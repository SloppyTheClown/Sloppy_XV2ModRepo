// ---------------------------------------------------------------------------
//  The in-game menu (Dear ImGui 1.92.4, third_party\imgui, MIT).
//
//  Drawn into the game's back buffer from the Present hook, so it runs on the
//  render thread together with the scripts. Nothing is set up until ~ is
//  first pressed or a script asks for the overlay; a player who never does
//  either pays one key check per frame.
//
//  The overlay: a script that calls ui.ShowOverlay() in a frame gets an ImGui
//  frame without the menu, in which every script's _OnOverlay draws windows
//  of its own (e.g. a search box over a game menu). It takes the mouse only
//  while the cursor is over one of its windows, and the keyboard only while
//  a text field is being typed into; the game keeps both otherwise. Mods'
//  search boxes (search_box.cpp) are drawn in the same frame and bring the
//  overlay up by themselves while they are shown.
//
//  Input is polled on the render thread (cursor position, mouse buttons and
//  the few keys a menu needs) instead of going through ImGui's Win32 backend:
//  that backend feeds ImGui from the window procedure, which belongs to
//  whichever thread made the window - not necessarily the render thread.
//  The window procedure is still subclassed, only to collect typed
//  characters and the mouse wheel (which can't be polled) and to keep the
//  mouse from the game while the menu is open (and, for the overlay, while
//  the cursor is over it).
//
//  The game polls the keyboard: xgcore calls GetKeyState for 256 keys every
//  frame (xgcore 0xddf90, call at 0xde01a; boot-and-camera.md "Text input"),
//  so blocking key messages doesn't hide them. xgcore's GetKeyState import is
//  redirected to KeyStateForGame instead: while a text field is active every
//  key reads as up, so typing doesn't also press the game's buttons. A key
//  that ends the typing (Enter) is polled by the render thread, which can see
//  it before the game does: every key down while typing, or still down when
//  typing ends, keeps reading as up until it is released.
//
//  The game's mouse camera lives in xgcore.dll: it puts the cursor back in
//  the middle of the window every frame (SetCursorPos, xgcore 0xca1c8) and
//  turns the camera from raw mouse input (WM_INPUT -> GetRawInputData,
//  xgcore 0xdf30e). While the menu is open both are held back: xgcore's
//  SetCursorPos import is redirected to KeepCursorWhileMenuOpen, and mouse
//  WM_INPUT never reaches the game's window procedure.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "host.h"
#include "overlay.h"
#include "search_box.h"

// The key left of 1 (` and ~ on a US keyboard; F1/F2 are the console's
// reload/hide keys). Other layouts put other characters on it.
constexpr int kMenuKey = VK_OEM_3;

static bool g_ready = false;                            // ImGui + its DX11 renderer set up
static bool g_setupFailed = false;
static bool g_menuOpen = false;
static bool g_drawing = false;
static bool g_overlayRequested = false;                 // a script called ui.ShowOverlay() this frame
// What the last ImGui frame wants; read by the window procedure (the window's
// thread), so each is a single bool written once per frame.
static volatile bool g_framesDrawn = false;             // an ImGui frame was drawn last time (menu or overlay)
static volatile bool g_overlayWantsMouse = false;       // the cursor is over an overlay window
static volatile bool g_typingText = false;              // a text field is active
static HWND g_window = nullptr;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static WNDPROC g_gameWindowProc = nullptr;
static LARGE_INTEGER g_lastFrameTime = {};
static std::string g_layoutFile;                        // UTF-8; ImGui keeps only the pointer

// Filled by the window procedure (the window's thread), read by the render
// thread at the start of each menu frame.
static SRWLOCK g_inputLock = SRWLOCK_INIT;
// Keys kept from the game until released (bit per virtual key): set by the
// game's key poll for keys down while typing and by the render thread for
// keys down when typing ends; cleared by the poll once the key is up.
static volatile LONG g_keysKeptFromGame[256 / 32] = {};
static std::vector<unsigned int> g_typedCharacters;
static float g_wheelSteps = 0.0f;
static float g_wheelStepsSideways = 0.0f;

// overlay.h documents these four.
bool OverlayMenuOpen() { return g_menuOpen; }
bool OverlayDrawing() { return g_drawing; }
void OverlayRequest() { g_overlayRequested = true; }

// True when the foreground window belongs to the game's process.
static bool GameHasFocus()
{
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    return foregroundProcess == GetCurrentProcessId();
}

bool OverlayGameHasFocus() { return GameHasFocus(); }

// True for the window messages of mouse buttons and wheels (what the game
// must not get while the menu, or an overlay window under the cursor, has
// the mouse).
static bool IsMouseButtonMessage(UINT message)
{
    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

// True for a WM_INPUT carrying mouse movement/buttons (reading only the
// header leaves the data for whoever reads it next).
static bool IsRawMouseInput(LPARAM lParam)
{
    RAWINPUTHEADER header = {};
    UINT size = sizeof(header);
    return GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_HEADER, &header, &size, sizeof(header)) ==
               sizeof(header) &&
           header.dwType == RIM_TYPEMOUSE;
}

// True while `virtualKey` (0..255) reads as up for the game (g_keysKeptFromGame).
static bool KeyKeptFromGame(unsigned virtualKey)
{
    return (g_keysKeptFromGame[virtualKey / 32] >> (virtualKey % 32)) & 1;
}

// Sets or clears the key's bit; atomic, as the render thread and the
// game's key poll both call it.
static void KeepKeyFromGame(unsigned virtualKey, bool keep)
{
    const LONG bit = 1L << (virtualKey % 32);
    if (keep) InterlockedOr(&g_keysKeptFromGame[virtualKey / 32], bit);
    else InterlockedAnd(&g_keysKeptFromGame[virtualKey / 32], ~bit);
}

// xgcore's GetKeyState (its keyboard poll): a key kept from the game reads as
// up (toggle bit kept) until it is really released.
using GetKeyStateFunction = SHORT(WINAPI*)(int);
static GetKeyStateFunction g_gameGetKeyState = &GetKeyState;   // until the redirect returns xgcore's own

static SHORT WINAPI KeyStateForGame(int virtualKeyCode)
{
    const SHORT state = g_gameGetKeyState(virtualKeyCode);
    const unsigned virtualKey = static_cast<unsigned>(virtualKeyCode) & 0xff;
    const bool down = (state & 0x8000) != 0;
    if (!down) {
        KeepKeyFromGame(virtualKey, false);
        return state;
    }
    if (g_typingText) KeepKeyFromGame(virtualKey, true);
    return KeyKeptFromGame(virtualKey) ? static_cast<SHORT>(state & 1) : state;
}

// Render thread: a text field became active or inactive. When typing ends,
// the keys still down (the Enter that ended it) stay kept from the game
// until released - the game may not have polled them yet.
static void SetTypingText(bool typing)
{
    if (g_typingText && !typing) {
        for (unsigned virtualKey = 1; virtualKey < 256; ++virtualKey)
            if (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) KeepKeyFromGame(virtualKey, true);
    }
    g_typingText = typing;
}

// The game window's procedure while the menu is set up: collects typed
// characters and the wheel for ImGui, and swallows the key, mouse and raw
// mouse messages the game must not see while the menu or overlay has them;
// everything else goes on to the game's procedure.
static LRESULT CALLBACK MenuWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    const bool mouseIsOurs = g_menuOpen || g_overlayWantsMouse;
    // the keys typed into a text field (the game's poll: KeyStateForGame)
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        if (g_typingText || KeyKeptFromGame(static_cast<unsigned>(wParam) & 0xff)) return 0;
    }
    if (g_menuOpen || g_framesDrawn) {
        switch (message) {
        case WM_INPUT:
            // the game turns the camera (and clicks its menus) from these;
            // DefWindowProc frees them
            if (mouseIsOurs && IsRawMouseInput(lParam)) return DefWindowProcW(window, message, wParam, lParam);
            break;
        case WM_CHAR:
            if (wParam == '`' || wParam == '~') break;   // the menu key's own character
            AcquireSRWLockExclusive(&g_inputLock);
            if (g_typedCharacters.size() < 256) g_typedCharacters.push_back(static_cast<unsigned int>(wParam));
            ReleaseSRWLockExclusive(&g_inputLock);
            break;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            const float steps = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            AcquireSRWLockExclusive(&g_inputLock);
            (message == WM_MOUSEWHEEL ? g_wheelSteps : g_wheelStepsSideways) += steps;
            ReleaseSRWLockExclusive(&g_inputLock);
            break;
        }
        case WM_SETCURSOR:
            // ImGui draws its own cursor while the menu is open
            if (g_menuOpen && LOWORD(lParam) == HTCLIENT) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        }
        // the mouse belongs to the menu while it is open, and to the overlay
        // while the cursor is over it
        if (mouseIsOurs && IsMouseButtonMessage(message)) return 0;
    }
    return CallWindowProcW(g_gameWindowProc, window, message, wParam, lParam);
}

// xgcore's SetCursorPos: does nothing while the menu is open, so the cursor
// stays where the player moves it instead of snapping back to the middle.
using SetCursorPosFunction = BOOL(WINAPI*)(int, int);
static SetCursorPosFunction g_gameSetCursorPos = &SetCursorPos;   // until the redirect returns xgcore's own

static BOOL WINAPI KeepCursorWhileMenuOpen(int x, int y)
{
    if (g_menuOpen) return TRUE;
    return g_gameSetCursorPos(x, y);
}

// Points `module`'s import of dllName!functionName at `replacement`; returns
// what it pointed at before (null when the module doesn't import it). Matched
// by name, not address: the Steam overlay may have hooked the function.
static void* RedirectImport(HMODULE module, const char* dllName, const char* functionName, void* replacement)
{
    auto* image = reinterpret_cast<BYTE*>(module);
    const auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    const auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dosHeader->e_lfanew);
    const IMAGE_DATA_DIRECTORY& imports = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress) return nullptr;
    for (auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + imports.VirtualAddress);
         descriptor->Name; ++descriptor) {
        if (_stricmp(reinterpret_cast<char*>(image + descriptor->Name), dllName) != 0) continue;
        if (!descriptor->OriginalFirstThunk) return nullptr;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(image + descriptor->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(image + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const auto* byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(image + names->u1.AddressOfData);
            if (strcmp(byName->Name, functionName) != 0) continue;
            DWORD oldProtection = 0;
            if (!VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &oldProtection))
                return nullptr;
            void* original = reinterpret_cast<void*>(slots->u1.Function);
            slots->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), oldProtection, &oldProtection);
            return original;
        }
    }
    return nullptr;
}

// Size of the swap chain's buffers (what ImGui draws into), which can differ
// from the window's client area when the game scales its output.
static bool BackBufferSize(IDXGISwapChain* swapChain, float& width, float& height)
{
    DXGI_SWAP_CHAIN_DESC description = {};
    if (FAILED(swapChain->GetDesc(&description))) return false;
    width = static_cast<float>(description.BufferDesc.Width);
    height = static_cast<float>(description.BufferDesc.Height);
    return width > 0 && height > 0;
}

// Creates ImGui and its DX11 renderer on the game's device, scales the style
// to the resolution, subclasses the game window and redirects xgcore's
// SetCursorPos / GetKeyState. Runs once, on the first frame that needs the
// overlay; false (logged) when it can't, and the menu then stays off.
static bool SetUp(IDXGISwapChain* swapChain)
{
    DXGI_SWAP_CHAIN_DESC description = {};
    if (FAILED(swapChain->GetDesc(&description)) || !description.OutputWindow) return false;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
        Log("menu the game's swap chain has no D3D11 device; no menu");
        return false;
    }
    g_device->GetImmediateContext(&g_context);
    g_window = description.OutputWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // keyboard navigation is switched on only while the menu is open (FeedInput)
    io.BackendPlatformName = "xv2_mod_loader";
    // window positions/sizes are remembered next to loader.json
    const std::wstring layoutPath = std::wstring(g_host->folder) + L"\\menu_layout.ini";
    const int size = WideCharToMultiByte(CP_UTF8, 0, layoutPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    g_layoutFile.assign(size > 0 ? size - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, layoutPath.c_str(), -1, g_layoutFile.data(), size, nullptr, nullptr);
    io.IniFilename = g_layoutFile.c_str();

    // ImGui's default font is 13 px, sized for a 720p-ish desktop: scale
    // everything with the game's resolution (1.5x at 1080p, 3x at 4K).
    float width = 0, height = 0;
    const float scale = BackBufferSize(swapChain, width, height) && height > 720 ? height / 720.0f : 1.0f;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;

    // ImGui's DX11 renderer compiles its two small shaders with
    // d3dcompiler_47.dll (delay-loaded: only here, on first use).
    if (!ImGui_ImplDX11_Init(g_device, g_context)) {
        Log("menu FAILED to set up the DX11 renderer");
        return false;
    }
    g_gameWindowProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&MenuWindowProc)));
    // xgcore may call the replacement before RedirectImport returns, so
    // g_gameSetCursorPos already holds a working SetCursorPos.
    void* engineSetCursorPos = nullptr;
    if (HMODULE engine = GetModuleHandleW(L"xgcore.dll"))
        engineSetCursorPos =
            RedirectImport(engine, "USER32.dll", "SetCursorPos", reinterpret_cast<void*>(&KeepCursorWhileMenuOpen));
    if (engineSetCursorPos) g_gameSetCursorPos = reinterpret_cast<SetCursorPosFunction>(engineSetCursorPos);
    else Log("menu xgcore.dll's SetCursorPos not found; the game may keep centring the cursor");
    void* engineGetKeyState = nullptr;
    if (HMODULE engine = GetModuleHandleW(L"xgcore.dll"))
        engineGetKeyState = RedirectImport(engine, "USER32.dll", "GetKeyState", reinterpret_cast<void*>(&KeyStateForGame));
    if (engineGetKeyState) g_gameGetKeyState = reinterpret_cast<GetKeyStateFunction>(engineGetKeyState);
    else Log("menu xgcore.dll's GetKeyState not found; keys typed into text fields also reach the game");
    QueryPerformanceCounter(&g_lastFrameTime);
    Log("menu set up (%.0fx%.0f, scale %.2f)", width, height, scale);
    return true;
}

// The keys ImGui's widgets use (navigation, text editing, Ctrl+C/V/X/Z/A).
static const struct { int virtualKey; ImGuiKey key; } kPolledKeys[] = {
    {VK_TAB, ImGuiKey_Tab},             {VK_LEFT, ImGuiKey_LeftArrow},    {VK_RIGHT, ImGuiKey_RightArrow},
    {VK_UP, ImGuiKey_UpArrow},          {VK_DOWN, ImGuiKey_DownArrow},    {VK_PRIOR, ImGuiKey_PageUp},
    {VK_NEXT, ImGuiKey_PageDown},       {VK_HOME, ImGuiKey_Home},         {VK_END, ImGuiKey_End},
    {VK_DELETE, ImGuiKey_Delete},       {VK_BACK, ImGuiKey_Backspace},    {VK_RETURN, ImGuiKey_Enter},
    {VK_ESCAPE, ImGuiKey_Escape},       {VK_SPACE, ImGuiKey_Space},       {VK_CONTROL, ImGuiMod_Ctrl},
    {VK_SHIFT, ImGuiMod_Shift},         {VK_MENU, ImGuiMod_Alt},          {'A', ImGuiKey_A},
    {'C', ImGuiKey_C},                  {'V', ImGuiKey_V},                {'X', ImGuiKey_X},
    {'Y', ImGuiKey_Y},                  {'Z', ImGuiKey_Z},
};

// Gives ImGui this frame's time step, display size, cursor, mouse buttons,
// wheel, polled keys and typed characters (render thread, before NewFrame).
static void FeedInput(IDXGISwapChain* swapChain)
{
    ImGuiIO& io = ImGui::GetIO();
    LARGE_INTEGER now, frequency;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    const float elapsed = static_cast<float>(now.QuadPart - g_lastFrameTime.QuadPart) / frequency.QuadPart;
    io.DeltaTime = elapsed > 0.0f ? elapsed : 1.0f / 60.0f;
    g_lastFrameTime = now;

    float width = 0, height = 0;
    if (!BackBufferSize(swapChain, width, height)) width = height = 1;
    io.DisplaySize = ImVec2(width, height);

    // keyboard navigation only in the menu: over a game menu the arrow keys
    // and Enter belong to the game
    if (g_menuOpen) io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    else io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

    const bool focused = GameHasFocus();
    io.AddFocusEvent(focused);
    POINT cursor;
    RECT client;
    if (focused && GetCursorPos(&cursor) && ScreenToClient(g_window, &cursor) && GetClientRect(g_window, &client) &&
        client.right > 0 && client.bottom > 0) {
        io.AddMousePosEvent(cursor.x * width / client.right, cursor.y * height / client.bottom);
    } else {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
    const int buttons[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON};
    for (int i = 0; i < 3; ++i) io.AddMouseButtonEvent(i, focused && (GetAsyncKeyState(buttons[i]) & 0x8000));
    for (const auto& polled : kPolledKeys)
        io.AddKeyEvent(polled.key, focused && (GetAsyncKeyState(polled.virtualKey) & 0x8000));

    AcquireSRWLockExclusive(&g_inputLock);
    std::vector<unsigned int> typed;
    typed.swap(g_typedCharacters);
    const float wheel = g_wheelSteps, wheelSideways = g_wheelStepsSideways;
    g_wheelSteps = g_wheelStepsSideways = 0.0f;
    ReleaseSRWLockExclusive(&g_inputLock);
    for (unsigned int character : typed) io.AddInputCharacterUTF16(static_cast<ImWchar16>(character));
    if (wheel || wheelSideways) io.AddMouseWheelEvent(wheelSideways, wheel);
}

// Draws ImGui's output into the swap chain's current back buffer, leaving
// the game's render targets as they were. (ImGui's renderer saves and
// restores the rest of the pipeline state itself.)
static void RenderToBackBuffer(IDXGISwapChain* swapChain)
{
    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))) return;
    ID3D11RenderTargetView* target = nullptr;
    const HRESULT created = g_device->CreateRenderTargetView(backBuffer, nullptr, &target);
    backBuffer->Release();
    if (FAILED(created)) return;

    ID3D11RenderTargetView* gameTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* gameDepth = nullptr;
    g_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, gameTargets, &gameDepth);
    g_context->OMSetRenderTargets(1, &target, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, gameTargets, gameDepth);
    for (auto* gameTarget : gameTargets)
        if (gameTarget) gameTarget->Release();
    if (gameDepth) gameDepth->Release();
    target->Release();
}

// Nothing drawn this frame: the game gets its mouse and keyboard back.
static void NoFrame()
{
    g_framesDrawn = g_overlayWantsMouse = false;
    SetTypingText(false);
}

// See overlay.h. Toggles the menu on ~, then draws one ImGui frame when the
// menu is open, a script asked for the overlay or a search box is up.
void OverlayFrame(IDXGISwapChain* swapChain, void (*drawSections)(), void (*drawOverlays)())
{
    static bool menuKeyWasDown = false;
    const bool menuKeyDown = GameHasFocus() && (GetAsyncKeyState(kMenuKey) & 0x8000);
    if (menuKeyDown && !menuKeyWasDown) g_menuOpen = !g_menuOpen;
    menuKeyWasDown = menuKeyDown;
    // a script asked for it, or a mod's search box is up (search_box.cpp)
    const bool overlay = g_overlayRequested || SearchBoxesVisible();
    g_overlayRequested = false;
    if ((!g_menuOpen && !overlay) || g_setupFailed) return NoFrame();
    if (!g_ready) {
        g_ready = SetUp(swapChain);
        g_setupFailed = !g_ready;
        if (!g_ready) return NoFrame();
    }
    // Only the game's own swap chain (its main window) gets the menu.
    DXGI_SWAP_CHAIN_DESC description = {};
    if (FAILED(swapChain->GetDesc(&description)) || description.OutputWindow != g_window) return;

    FeedInput(swapChain);
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor = g_menuOpen;     // over a game menu the game's cursor stays
    g_drawing = true;

    // every frame the overlay or the menu is up (scripts that didn't ask for
    // the overlay draw nothing then)
    DrawSearchBoxes();
    drawOverlays();
    if (g_menuOpen) {
        ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("XV2 Mod Loader")) {
            ImGui::TextDisabled("~ closes this menu");
            drawSections();
        }
        ImGui::End();
    }

    g_drawing = false;
    ImGui::Render();
    RenderToBackBuffer(swapChain);
    const ImGuiIO& io = ImGui::GetIO();
    g_framesDrawn = true;
    g_overlayWantsMouse = io.WantCaptureMouse;
    SetTypingText(io.WantTextInput);
}

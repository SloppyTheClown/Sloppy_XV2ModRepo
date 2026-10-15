// ---------------------------------------------------------------------------
//  Lua scripts - a LuaBackend-compatible runtime inside the loader.
//
//      <game>\XV2ModLoader\mods\<ModName>\*.lua
//
//  Every script gets its own Lua 5.4 state. As in LuaBackend
//  (https://github.com/Sirius902/LuaBackend), a script defines _OnInit()
//  (run once) and/or _OnFrame() (run every frame); require() finds modules in
//  <ModName>\io_packages\. Both functions run on the game's render thread,
//  right before each IDXGISwapChain::Present, so scripts keep step with the
//  game's frames and never run at the same time as each other.
//
//  _OnInit runs on the first frame, not at load: Steam DRM decrypts the
//  exe's code only after the game starts (exe-and-tooling.md), and by the
//  first frame that is long done.
//
//  Lua itself is XV2ModLoader\Lib\lua54.dll, delay-loaded: shared with the
//  libraries in Lib\ (sdk\modloader_lib.h), which add tables of their own to
//  every script (the in-game menu's "ui", "search", "files" - Lib\src) and
//  callbacks such as _OnMenu, through the Add* functions at the end.
//
//  A script whose _OnFrame (or a library's callback) raises an error is
//  unloaded, as in LuaBackend. With "lua_console": true in loader.json a console window shows what the
//  scripts print; F1 then reloads every script and F2 hides/shows it.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "loader.h"
#include "lua_scripts.h"

struct ModScript {
    std::wstring folder;                                // <game>\XV2ModLoader\mods\<ModName>
    std::wstring modName;
    std::wstring fileName;                              // <script>.lua
};

struct LoadedScript {
    lua_State* state;
    std::string name;                                   // <ModName>\<script>.lua, for messages
};

static std::vector<LoadedScript> g_scripts;
static bool   g_luaLoaded = false;                      // Lib\lua54.dll is loaded (StartLuaScripts)

// Added by the libraries while they are initialised, before any script or
// frame runs - so only read afterwards, without a lock.
static std::vector<void (*)(lua_State*)> g_luaRegistrars;
static std::vector<std::string> g_scriptCallbacks = {"_OnInit", "_OnFrame"};   // a script defining one is kept
static std::vector<void (*)()> g_unloadHandlers;
static std::vector<void (*)(IDXGISwapChain*)> g_frameHandlers;
static bool   g_console = false;                        // "lua_console": true in loader.json
static HANDLE g_consoleOut = INVALID_HANDLE_VALUE;

// ---------------------------------------------------------------------------
//  Text helpers
// ---------------------------------------------------------------------------

static std::string Narrow(const std::wstring& text, UINT codePage)
{
    const int size = WideCharToMultiByte(codePage, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(codePage, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

// Paths handed to Lua go through fopen/LoadLibraryA (io.open, require), which
// take the ANSI code page; names shown in messages are UTF-8.
static std::string AnsiPath(const std::wstring& path) { return Narrow(path, CP_ACP); }
static std::string Utf8(const std::wstring& text) { return Narrow(text, CP_UTF8); }

// LuaBackend's GAME_ID: the standard CRC-32 of the exe's file name.
static uint32_t Crc32(const std::string& text)
{
    uint32_t crc = 0xFFFFFFFF;
    for (unsigned char c : text) {
        crc ^= c;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

// ---------------------------------------------------------------------------
//  Output: console (if enabled) and loader.log
// ---------------------------------------------------------------------------

//  Console writes block while the window has a text selection (QuickEdit or
//  Edit > Mark): clicking into the console while alt-tabbing froze the game -
//  the render thread sat in WriteConsoleA under our Present hook (stack of the
//  hung game, 2026-10-07). So the render thread only queues text; a writer
//  thread does the console calls. The queue is bounded: while the console is
//  stuck, text past kConsoleQueueLimit is dropped and counted.
struct ConsoleChunk {
    WORD color;
    std::string text;
};

constexpr size_t kConsoleQueueLimit = 256 * 1024;      // bytes waiting; ~3000 lines
static std::deque<ConsoleChunk> g_consoleQueue;
static size_t g_consoleQueuedBytes = 0;
static size_t g_consoleDroppedChunks = 0;
static SRWLOCK g_consoleLock = SRWLOCK_INIT;
static CONDITION_VARIABLE g_consoleWake = CONDITION_VARIABLE_INIT;

static void ConsoleWrite(WORD color, const std::string& text)
{
    if (g_consoleOut == INVALID_HANDLE_VALUE || text.empty()) return;
    AcquireSRWLockExclusive(&g_consoleLock);
    if (g_consoleQueuedBytes + text.size() > kConsoleQueueLimit) {
        ++g_consoleDroppedChunks;
    } else {
        g_consoleQueue.push_back({color, text});
        g_consoleQueuedBytes += text.size();
    }
    ReleaseSRWLockExclusive(&g_consoleLock);
    WakeConditionVariable(&g_consoleWake);
}

// The writer thread: the only place that calls into the console for output.
static DWORD WINAPI ConsoleWriterThread(LPVOID)
{
    for (;;) {
        AcquireSRWLockExclusive(&g_consoleLock);
        while (g_consoleQueue.empty())
            SleepConditionVariableSRW(&g_consoleWake, &g_consoleLock, INFINITE, 0);
        ConsoleChunk chunk = std::move(g_consoleQueue.front());
        g_consoleQueue.pop_front();
        g_consoleQueuedBytes -= chunk.text.size();
        const size_t dropped = g_consoleDroppedChunks;
        g_consoleDroppedChunks = 0;
        ReleaseSRWLockExclusive(&g_consoleLock);

        DWORD written = 0;
        if (dropped) {
            constexpr WORD kRed = 12;
            const std::string note = "\n(console was blocked: " + std::to_string(dropped) + " outputs dropped)\n";
            SetConsoleTextAttribute(g_consoleOut, kRed);
            WriteConsoleA(g_consoleOut, note.data(), static_cast<DWORD>(note.size()), &written, nullptr);
        }
        SetConsoleTextAttribute(g_consoleOut, chunk.color);
        WriteConsoleA(g_consoleOut, chunk.text.data(), static_cast<DWORD>(chunk.text.size()), &written, nullptr);
    }
}

// io.write / io.stdout write to this DLL's CRT stdout, which is pointed at a
// pipe (OpenConsole); this thread moves what arrives into the console queue,
// so a blocked console can't block a script's io.write either.
static DWORD WINAPI StdoutPipeThread(LPVOID pipeReadEnd)
{
    constexpr WORD kGrey = 7;
    char buffer[4096];
    DWORD got = 0;
    while (ReadFile(pipeReadEnd, buffer, sizeof(buffer), &got, nullptr) && got)
        ConsoleWrite(kGrey, std::string(buffer, got));
    return 0;
}

void ScriptOutput(lua_State* state, int type, const std::string& text)
{
    // Console colours as in LuaBackend: name yellow, label by type, text grey.
    constexpr WORD kYellow = 14, kGrey = 7;
    static const struct { const char* label; WORD color; } kTypes[] = {
        {"MESSAGE: ", 11}, {"SUCCESS: ", 10}, {"WARNING: ", 14}, {"ERROR: ", 12},
    };
    std::string source = "Lua";
    if (state) {
        if (lua_getglobal(state, "LUA_NAME") == LUA_TSTRING) source = lua_tostring(state, -1);
        lua_pop(state, 1);
    }
    const char* label = (type >= kMessage && type <= kError) ? kTypes[type].label : "";
    Log("lua  [%s] %s%s", source.c_str(), label, text.c_str());
    ConsoleWrite(kYellow, "[" + source + "] ");
    if (*label) ConsoleWrite(kTypes[type].color, label);
    ConsoleWrite(kGrey, text + "\n");
}

// Ctrl+C / Ctrl+Break in the console would end the game: ignore them.
static BOOL WINAPI IgnoreConsoleControl(DWORD)
{
    return TRUE;
}

static void OpenConsole()
{
    if (!AllocConsole()) return;
    SetConsoleTitleW(L"XV2 Mod Loader - Lua scripts (F2 hides window)");
    // Closing a console window ends the process that owns it - the game (seen
    // 2026-10-07: closing the console quit XV2 at startup with no crash dump).
    // So the close button is removed; F2 hides the window instead.
    if (HMENU menu = GetSystemMenu(GetConsoleWindow(), FALSE)) DeleteMenu(menu, SC_CLOSE, MF_BYCOMMAND);
    SetConsoleCtrlHandler(IgnoreConsoleControl, TRUE);
    SetConsoleOutputCP(CP_UTF8);
    // QuickEdit starts a selection on any click into the window, and a
    // selection pauses all console output (see ConsoleWrite). Mark in the
    // window menu still can; the writer thread keeps that off the game.
    HANDLE consoleIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
    if (consoleIn != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(consoleIn, &mode))
            SetConsoleMode(consoleIn, (mode & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);
        CloseHandle(consoleIn);
    }
    g_consoleOut = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (g_consoleOut == INVALID_HANDLE_VALUE) return;
    if (HANDLE writer = CreateThread(nullptr, 0, ConsoleWriterThread, nullptr, 0, nullptr)) CloseHandle(writer);

    // io.write / io.stdout go to this DLL's own CRT stdout: a GUI process has
    // none, so open it on NUL, then put a pipe under it (StdoutPipeThread).
    // Unbuffered, so each io.write shows at once.
    FILE* stream = nullptr;
    HANDLE pipeRead = nullptr, pipeWrite = nullptr;
    if (freopen_s(&stream, "NUL", "w", stdout) == 0 && CreatePipe(&pipeRead, &pipeWrite, nullptr, 64 * 1024)) {
        const int pipeDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(pipeWrite), _O_WRONLY | _O_BINARY);
        if (pipeDescriptor != -1 && _dup2(pipeDescriptor, _fileno(stdout)) == 0) {
            _close(pipeDescriptor);                     // stdout keeps its own duplicate
            setvbuf(stdout, nullptr, _IONBF, 0);
            if (HANDLE reader = CreateThread(nullptr, 0, StdoutPipeThread, pipeRead, 0, nullptr)) CloseHandle(reader);
        }
    }
}

// ---------------------------------------------------------------------------
//  Loading and running scripts
// ---------------------------------------------------------------------------

static std::vector<ModScript> FindScripts()
{
    std::vector<ModScript> scripts;
    const std::wstring mods = std::wstring(g_dir) + L"\\mods\\";
    for (const auto& mod : ListFolder(mods + L"*", true))
        for (const auto& file : ListFolder(mods + mod + L"\\*.lua", false))
            if (HasExtension(file, L".lua")) scripts.push_back({mods + mod, mod, file});
    return scripts;
}

// Error handler for lua_pcall: the message plus a stack traceback.
static int Traceback(lua_State* state)
{
    luaL_traceback(state, state, lua_tostring(state, 1), 1);
    return 1;
}

// Calls the script's global function (if it defines one). False on an error,
// which is printed.
bool CallScriptFunction(lua_State* state, const char* function)
{
    lua_pushcfunction(state, Traceback);
    const int handler = lua_gettop(state);
    bool ok = true;
    if (lua_getglobal(state, function) == LUA_TFUNCTION) {
        if (lua_pcall(state, 0, 0, handler) != LUA_OK) {
            ScriptOutput(state, kError, std::string(function) + ": " + lua_tostring(state, -1));
            ok = false;
        }
        lua_settop(state, handler - 1);
    } else {
        lua_settop(state, handler - 1);
    }
    return ok;
}

static bool HasGlobalFunction(lua_State* state, const char* name)
{
    const bool found = lua_getglobal(state, name) == LUA_TFUNCTION;
    lua_pop(state, 1);
    return found;
}

static void SetGlobalString(lua_State* state, const char* name, const std::string& value)
{
    lua_pushlstring(state, value.data(), value.size());
    lua_setglobal(state, name);
}

static void SetGlobalInteger(lua_State* state, const char* name, lua_Integer value)
{
    lua_pushinteger(state, value);
    lua_setglobal(state, name);
}

// LuaBackend's globals: LUA_NAME, SCRIPT_PATH, LOAD_PATH, CHEATS_PATH,
// ENGINE_VERSION, ENGINE_TYPE, GAME_ID, BASE_ADDR; and package paths.
static void SetScriptGlobals(lua_State* state, const ModScript& script)
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const wchar_t* exeName = wcsrchr(exePath, L'\\');
    exeName = exeName ? exeName + 1 : exePath;

    SetGlobalString(state, "LUA_NAME", Utf8(script.fileName));
    SetGlobalString(state, "SCRIPT_PATH", AnsiPath(script.folder));
    SetGlobalString(state, "LOAD_PATH", AnsiPath(script.folder + L"\\io_load"));
    SetGlobalString(state, "CHEATS_PATH", "NOT_AVAILABLE");
    SetGlobalInteger(state, "ENGINE_VERSION", 5);
    SetGlobalString(state, "ENGINE_TYPE", "BACKEND");
    SetGlobalInteger(state, "GAME_ID", Crc32(AnsiPath(exeName)));
    SetGlobalInteger(state, "BASE_ADDR", static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))));

    lua_getglobal(state, "package");
    lua_pushstring(state, AnsiPath(script.folder + L"\\io_packages\\?.lua").c_str());
    lua_setfield(state, -2, "path");
    lua_pushstring(state, AnsiPath(script.folder + L"\\io_packages\\?.dll").c_str());
    lua_setfield(state, -2, "cpath");
    lua_pop(state, 1);

    // the mod folder in UTF-8, for the "files" functions' relative paths
    lua_pushstring(state, Utf8(script.folder).c_str());
    lua_setfield(state, LUA_REGISTRYINDEX, "xv2_script_folder");
}

// The libraries LuaBackend opens (base, package, coroutine, string, os, math,
// table, io, utf8 - its bit32 doesn't exist in 5.4); not debug.
static void OpenLibraries(lua_State* state)
{
    static const luaL_Reg libraries[] = {
        {LUA_GNAME, luaopen_base},          {LUA_LOADLIBNAME, luaopen_package},
        {LUA_COLIBNAME, luaopen_coroutine}, {LUA_STRLIBNAME, luaopen_string},
        {LUA_OSLIBNAME, luaopen_os},        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_TABLIBNAME, luaopen_table},    {LUA_IOLIBNAME, luaopen_io},
        {LUA_UTF8LIBNAME, luaopen_utf8},
    };
    for (const auto& library : libraries) {
        luaL_requiref(state, library.name, library.func, 1);
        lua_pop(state, 1);
    }
}

static bool ReadWholeFile(const std::wstring& path, std::string& contents)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") || !file) return false;
    char buffer[16384];
    size_t got = 0;
    while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0) contents.append(buffer, got);
    fclose(file);
    return true;
}

// Runs the script's top level; keeps it if it defines _OnInit, _OnFrame or a
// library's callback (g_scriptCallbacks).
static void LoadScript(const ModScript& script)
{
    const std::string name = Utf8(script.modName + L"\\" + script.fileName);
    std::string source;
    if (!ReadWholeFile(script.folder + L"\\" + script.fileName, source)) {
        ScriptOutput(nullptr, kError, name + ": can't read the file");
        return;
    }
    lua_State* state = luaL_newstate();
    if (!state) return;
    OpenLibraries(state);
    RegisterLuaApi(state);
    for (auto registrar : g_luaRegistrars) registrar(state);
    SetScriptGlobals(state, script);

    lua_pushcfunction(state, Traceback);
    const std::string chunkName = "@" + name;
    if (luaL_loadbufferx(state, source.data(), source.size(), chunkName.c_str(), nullptr) != LUA_OK ||
        lua_pcall(state, 0, 0, 1) != LUA_OK) {
        ScriptOutput(state, kError, std::string(lua_tostring(state, -1)) + "\nThe script was not loaded.");
        lua_close(state);
        return;
    }
    lua_settop(state, 0);
    bool hasCallback = false;
    std::string callbackNames;
    for (const auto& callback : g_scriptCallbacks) {
        hasCallback = hasCallback || HasGlobalFunction(state, callback.c_str());
        callbackNames += (callbackNames.empty() ? "" : ", ") + callback;
    }
    if (!hasCallback) {
        ScriptOutput(state, kWarning, "defines none of " + callbackNames + "; not loaded.");
        lua_close(state);
        return;
    }
    ScriptOutput(state, kSuccess, "loaded (" + name + ")");
    g_scripts.push_back({state, name});
}

static void UnloadScripts()
{
    for (auto handler : g_unloadHandlers) handler();
    for (auto& script : g_scripts) lua_close(script.state);
    g_scripts.clear();
}

// Loads every script, then runs each one's _OnInit. A failing _OnInit is
// reported but the script stays, as in LuaBackend.
static void LoadScripts()
{
    for (const auto& script : FindScripts()) LoadScript(script);
    for (auto& script : g_scripts) CallScriptFunction(script.state, "_OnInit");
    ScriptOutput(nullptr, kMessage, std::to_string(g_scripts.size()) + " Lua script(s) running");
}

// Edge-triggered key press, only while the game's window has the focus.
static bool KeyPressed(int key, bool& wasDown)
{
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    const bool down = foregroundProcess == GetCurrentProcessId() && (GetAsyncKeyState(key) & 0x8000);
    const bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

// Once per presented frame, on the render thread.
static void RunFrame()
{
    if (!g_luaLoaded) return;                           // the hook was installed for a library only
    static bool started = false;
    if (!started) {
        started = true;
        LoadScripts();
    }
    if (g_console) {
        static bool reloadDown = false, toggleDown = false, consoleShown = true;
        if (KeyPressed(VK_F1, reloadDown)) {
            ScriptOutput(nullptr, kMessage, "Reloading...");
            UnloadScripts();
            LoadScripts();
        }
        if (KeyPressed(VK_F2, toggleDown)) {
            consoleShown = !consoleShown;
            // Async: the window lives in conhost.exe, and a cross-process
            // ShowWindow waits for it - never wait on the render thread.
            ShowWindowAsync(GetConsoleWindow(), consoleShown ? SW_SHOWNOACTIVATE : SW_HIDE);
        }
    }
    for (size_t i = 0; i < g_scripts.size();) {
        if (CallScriptFunction(g_scripts[i].state, "_OnFrame")) { ++i; continue; }
        ScriptOutput(g_scripts[i].state, kError, "_OnFrame failed; the script was unloaded.");
        lua_close(g_scripts[i].state);
        g_scripts.erase(g_scripts.begin() + i);
    }
}

void ForEachScript(bool (*visit)(lua_State* state, const char* name, void* context), void* context,
                   const char* failure)
{
    for (size_t i = 0; i < g_scripts.size();) {
        if (visit(g_scripts[i].state, g_scripts[i].name.c_str(), context)) { ++i; continue; }
        ScriptOutput(g_scripts[i].state, kError, std::string(failure) + " failed; the script was unloaded.");
        lua_close(g_scripts[i].state);
        g_scripts.erase(g_scripts.begin() + i);
    }
}

// ---------------------------------------------------------------------------
//  Frame hook: IDXGISwapChain::Present, by patching its slot in the swap
//  chain's vtable. Every swap chain of that class shares the vtable (in
//  dxgi.dll's .rdata - DXVK's under Proton), so one made here on a hidden
//  window gives the game's too, whether or not the game has made it yet.
//
//  Steam's overlay (started from Steam) patches the first instruction of
//  dxgi's Present with a jump to itself and, after drawing, calls Present
//  through the vtable - which is us. Calling Present from there went back to
//  the overlay, back to us, until the stack overflowed (C00000FD in our hook,
//  2026-10-07; every call came from gameoverlayrenderer64.dll). So a nested
//  call goes to dxgi's code past the patch, rebuilt from dxgi.dll's file
//  (PresentPastPatches).
// ---------------------------------------------------------------------------

constexpr size_t kPresentSlot = 8;     // IUnknown (3) + IDXGIObject (4) + IDXGIDeviceSubObject (1)
constexpr size_t kEntryBytes = 32;     // how much of Present's start is kept from the file

using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
static PresentFunction g_previousPresent = nullptr;     // what the slot held when we patched it
static PresentFunction g_realPresent = nullptr;         // dxgi's own, from the file; null if not found
static unsigned char g_realEntry[kEntryBytes];          // its first bytes as in the file, unpatched
static void** g_presentSlot = nullptr;

// A loaded module's file on disk, for reading what its image held before
// anyone patched it in memory.
struct ModuleFile {
    std::string data;
    const IMAGE_NT_HEADERS64* nt = nullptr;
    uintptr_t loadedBase = 0;

    bool Open(const void* addressInModule)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(addressInModule), &module)) return false;
        wchar_t path[MAX_PATH];
        if (!GetModuleFileNameW(module, path, MAX_PATH)) return false;
        if (!ReadWholeFile(path, data) || data.size() < sizeof(IMAGE_DOS_HEADER)) return false;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(data.data());
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || data.size() < dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64)) return false;
        nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(data.data() + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
        loadedBase = reinterpret_cast<uintptr_t>(module);
        return true;
    }

    // Copies `size` bytes the file holds for loaded address `address`.
    bool Read(const void* address, void* out, size_t size) const
    {
        const uintptr_t rva = reinterpret_cast<uintptr_t>(address) - loadedBase;
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
            if (rva < section->VirtualAddress || rva + size > section->VirtualAddress + section->SizeOfRawData) continue;
            const size_t fileOffset = section->PointerToRawData + (rva - section->VirtualAddress);
            if (fileOffset + size > data.size()) return false;
            memcpy(out, data.data() + fileOffset, size);
            return true;
        }
        return false;
    }

    // A pointer as stored in the file (for the preferred base), moved to
    // where the module is loaded. Null if it doesn't point into the module.
    void* ReadPointer(const void* address) const
    {
        uint64_t preferredAddress = 0;
        if (!Read(address, &preferredAddress, sizeof(preferredAddress))) return nullptr;
        const uintptr_t loaded = static_cast<uintptr_t>(preferredAddress - nt->OptionalHeader.ImageBase) + loadedBase;
        if (loaded < loadedBase || loaded >= loadedBase + nt->OptionalHeader.SizeOfImage) return nullptr;
        return reinterpret_cast<void*>(loaded);
    }
};

// Length of one x64 instruction of the kinds compilers put at a function's
// start (push, mov/lea with a stack or register operand, sub rsp), or 0 for
// anything else - including rip-relative operands, which would point
// somewhere else once copied.
static size_t PrologueInstructionLength(const unsigned char* code)
{
    size_t length = 0;
    if ((code[0] & 0xF0) == 0x40) ++length;                       // REX prefix
    const unsigned char opcode = code[length++];
    if (opcode >= 0x50 && opcode <= 0x57) return length;          // push reg
    size_t immediate = 0;
    if (opcode == 0x83) immediate = 1;                            // sub/add r/m, imm8
    else if (opcode == 0x81) immediate = 4;                       // sub/add r/m, imm32
    else if (opcode != 0x89 && opcode != 0x8B && opcode != 0x8D) return 0;   // mov r/m,r / mov r,r/m / lea
    const unsigned char modrm = code[length++];
    const unsigned mod = modrm >> 6, rm = modrm & 7;
    if (mod != 3 && rm == 4) {                                    // SIB byte follows
        if (mod == 0 && (code[length] & 7) == 5) return 0;        // [disp32 + index]: not handled
        ++length;
    }
    if (mod == 0 && rm == 5) return 0;                            // rip-relative
    if (mod == 1) length += 1;                                    // disp8
    if (mod == 2) length += 4;                                    // disp32
    return length + immediate;
}

// Steam's overlay patches the start of dxgi's Present with a jump (E9 rel32,
// seen 2026-10-07) and, once it has drawn, calls Present through the vtable
// again - into us. So when Present's code no longer matches the file, we run
// its first instructions from a copy (taken from the file) and jump to the
// unpatched rest. Built once; null if the start can't be copied safely.
static PresentFunction PresentPastPatches()
{
    static PresentFunction trampoline = nullptr;
    static bool tried = false;
    if (tried) return trampoline;
    tried = true;

    // copy whole instructions until the longest jump a hook writes is covered
    // (FF 25 + 8-byte address = 14 bytes)
    constexpr size_t kPatchLength = 14;
    size_t copied = 0;
    while (copied < kPatchLength) {
        const size_t length = PrologueInstructionLength(g_realEntry + copied);
        if (length == 0 || copied + length > kEntryBytes - 16) {
            Log("lua  Present's start (byte %zu: %02X) can't be copied; nested Present calls go to the patched code",
                copied, g_realEntry[copied]);
            return nullptr;
        }
        copied += length;
    }
    auto* code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!code) return nullptr;
    memcpy(code, g_realEntry, copied);
    // jmp [rip+0] followed by the absolute address of the rest of Present
    const unsigned char jumpAbsolute[6] = {0xFF, 0x25, 0, 0, 0, 0};
    memcpy(code + copied, jumpAbsolute, sizeof(jumpAbsolute));
    const uintptr_t rest = reinterpret_cast<uintptr_t>(g_realPresent) + copied;
    memcpy(code + copied + sizeof(jumpAbsolute), &rest, sizeof(rest));
    DWORD oldProtection = 0;
    VirtualProtect(code, 64, PAGE_EXECUTE_READ, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), code, 64);
    trampoline = reinterpret_cast<PresentFunction>(code);
    Log("lua  Present's code is patched by another program (Steam overlay?); calling past it (%zu bytes copied)", copied);
    return trampoline;
}

// Where a call that must reach dxgi itself goes: Present, or the copy of
// its start when its code has been patched.
static PresentFunction RealPresentNow()
{
    if (memcmp(reinterpret_cast<const void*>(g_realPresent), g_realEntry, 5) == 0) return g_realPresent;
    if (PresentFunction trampoline = PresentPastPatches()) return trampoline;
    return g_realPresent;
}

static HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    static thread_local bool inside = false;
    // A nested call means a hook between us and dxgi (Steam's overlay) has
    // called back into us as its "original": go to dxgi itself, past any
    // patch on its code, or the two hooks call each other until the stack
    // runs out. Same when another hook has replaced us in the slot.
    const bool weAreOnTop = *g_presentSlot == reinterpret_cast<void*>(&HookedPresent);
    if (inside || !weAreOnTop) {
        if (g_realPresent) return RealPresentNow()(swapChain, syncInterval, flags);
        if (inside) return g_previousPresent(swapChain, syncInterval, flags);
    }

    inside = true;
    // DXGI_PRESENT_TEST only asks whether presenting would work; not a frame
    if (!(flags & DXGI_PRESENT_TEST)) {
        try {
            RunFrame();
            for (auto handler : g_frameHandlers) handler(swapChain);
        } catch (...) {                                 // never let an exception into dxgi
        }
    }
    const HRESULT result = g_previousPresent(swapChain, syncInterval, flags);
    inside = false;
    return result;
}

static bool PatchPresentSlot(IDXGISwapChain* swapChain)
{
    void** vtable = *reinterpret_cast<void***>(swapChain);
    void** slot = &vtable[kPresentSlot];
    if (*slot == reinterpret_cast<void*>(&HookedPresent)) return true;
    ModuleFile vtableFile, presentFile;
    if (vtableFile.Open(slot)) g_realPresent = reinterpret_cast<PresentFunction>(vtableFile.ReadPointer(slot));
    if (g_realPresent && !(presentFile.Open(reinterpret_cast<const void*>(g_realPresent)) &&
                           presentFile.Read(reinterpret_cast<const void*>(g_realPresent), g_realEntry, kEntryBytes)))
        g_realPresent = nullptr;
    Log("lua  Present slot holds %p, dxgi's own Present %p%s", *slot, reinterpret_cast<void*>(g_realPresent),
        g_realPresent && *slot != reinterpret_cast<void*>(g_realPresent) ? " (already hooked by something else)" : "");
    DWORD oldProtection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtection)) return false;
    // both set before the slot points at us
    g_previousPresent = reinterpret_cast<PresentFunction>(*slot);
    g_presentSlot = slot;
    InterlockedExchangePointer(slot, reinterpret_cast<void*>(&HookedPresent));
    VirtualProtect(slot, sizeof(void*), oldProtection, &oldProtection);
    return true;
}

static bool InstallPresentHook()
{
    // d3d11 is loaded here rather than imported, so a loader without Lua
    // scripts never pulls it in early.
    HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
    auto createDevice = d3d11 ? reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(
                                    GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain")) : nullptr;
    if (!createDevice) {
        Log("lua  FAILED to load d3d11.dll");
        return false;
    }
    WNDCLASSEXW windowClass = {sizeof(windowClass)};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"XV2ModLoaderPresentProbe";
    RegisterClassExW(&windowClass);
    HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16,
                                  nullptr, nullptr, windowClass.hInstance, nullptr);   // never shown

    DXGI_SWAP_CHAIN_DESC description = {};
    description.BufferDesc.Width = 16;
    description.BufferDesc.Height = 16;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 1;
    description.OutputWindow = window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    bool hooked = false;
    for (D3D_DRIVER_TYPE driver : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        IDXGISwapChain* swapChain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        if (SUCCEEDED(createDevice(nullptr, driver, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                   &description, &swapChain, &device, nullptr, &context))) {
            hooked = PatchPresentSlot(swapChain);
            swapChain->Release();
            context->Release();
            device->Release();
            break;
        }
    }
    DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    // d3d11.dll stays loaded: the patched vtable lives in the DXGI it pulled in
    Log(hooked ? "lua  frame hook installed (IDXGISwapChain::Present)" : "lua  FAILED to install the frame hook");
    return hooked;
}

// Set by whoever installs the frame hook first: StartLuaScripts, or a
// library (EnsureFrameHook - e.g. a DLL mod's first search box).
static volatile LONG g_frameHookClaimed = 0;

static bool ClaimFrameHook() { return InterlockedCompareExchange(&g_frameHookClaimed, 1, 0) == 0; }

static DWORD WINAPI InstallFrameHookThread(LPVOID)
{
    InstallPresentHook();
    return 0;
}

void EnsureFrameHook()
{
    // on a thread of its own: the caller may be one of the game's, and the
    // hook is installed through a throwaway D3D11 device
    if (g_frameHookClaimed || !ClaimFrameHook()) return;
    if (HANDLE thread = CreateThread(nullptr, 0, InstallFrameHookThread, nullptr, 0, nullptr)) CloseHandle(thread);
}

void StartLuaScripts(bool console)
{
    const size_t found = FindScripts().size();
    if (found == 0) return;                             // no scripts: no console, no hook
    Log("lua  %zu script(s) found", found);
    // Usually loaded already, by a library that links it. Every Lua call
    // goes through the delay-load stubs, which find it by name once loaded;
    // none may run without it (a missing delay-loaded DLL crashes the game).
    const std::wstring luaPath = std::wstring(g_dir) + L"\\Lib\\lua54.dll";
    if (!LoadLibraryExW(luaPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
        Log("lua  FAILED to load Lib\\lua54.dll (error %lu); Lua scripts will not run", GetLastError());
        return;
    }
    g_luaLoaded = true;
    g_console = console;
    if (g_console) OpenConsole();
    if (ClaimFrameHook() && !InstallPresentHook())
        ScriptOutput(nullptr, kError, "could not hook the game's frames; Lua scripts will not run.");
}

// ---------------------------------------------------------------------------
//  For the libraries (sdk\modloader_lib.h): called from their
//  ModLoaderLib_Init, on the loader thread, before StartLuaScripts.
// ---------------------------------------------------------------------------

void AddLuaRegistrar(void (*registrar)(lua_State* state)) { if (registrar) g_luaRegistrars.push_back(registrar); }

void AddScriptCallback(const char* functionName) { if (functionName) g_scriptCallbacks.push_back(functionName); }

void AddUnloadHandler(void (*handler)()) { if (handler) g_unloadHandlers.push_back(handler); }

void AddFrameHandler(void (*handler)(IDXGISwapChain* swapChain)) { if (handler) g_frameHandlers.push_back(handler); }

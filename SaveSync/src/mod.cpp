// ---------------------------------------------------------------------------
//  SaveSync - mod core: config, log, game version check, code hook helpers,
//  start-up. The sync itself is in savesync.cpp, the merge in merge.cpp.
//  Config/log/hook helpers come from this workspace's Chara Search and
//  Battle Styles.
// ---------------------------------------------------------------------------
#include "mod.h"
#include "savesync.h"
#include "json.h"
#include "default_config.h"
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

// ----------------------------------------------------------------- config ---
struct Config {
    bool enabled = true;
    bool logging = false;
};

static Config g_cfg;
uintptr_t     g_base = 0;
wchar_t       g_dir[MAX_PATH];
static std::vector<std::string> g_configErrors;

void Log(const char* fmt, ...)
{
    if (!g_cfg.logging) return;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\savesync.log", g_dir);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") || !f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "%02d:%02d:%02d.%03d  ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

// First run: write the shipped default config. CREATE_NEW - an existing file,
// even a broken one, is never touched.
static void EnsureConfig()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\savesync.json", g_dir);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(file, kDefaultConfig, (DWORD)(sizeof(kDefaultConfig) - 1), &wrote, nullptr);
    CloseHandle(file);
}

bool ReadWhole(const wchar_t* path, std::string& out)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    out.resize((size_t)min(size.QuadPart, (LONGLONG)(1 << 20)));
    DWORD got = 0;
    BOOL ok = ReadFile(file, out.data(), (DWORD)out.size(), &got, nullptr);
    CloseHandle(file);
    out.resize(got);
    return ok != FALSE;
}

static void LoadConfig()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\savesync.json", g_dir);
    std::string text, err;
    JVal root;
    if (!ReadWhole(path, text)) { g_configErrors.push_back("savesync.json not readable - defaults used"); return; }
    if (!JsonParse(text, root, err) || root.type != JVal::Obj) {
        g_configErrors.push_back("savesync.json: " + (err.empty() ? std::string("not an object") : err) + " - defaults used");
        return;
    }
    g_cfg.enabled = root.boolOr("enabled", true);
    g_cfg.logging = root.boolOr("logging", false);
}

// ------------------------------------------------------------ game code ---
// DBXV2.exe 1.26.01, from its PE header (readable before Steam's DRM has
// decrypted the code).
static bool IsGame12601()
{
    const uint8_t* image = (const uint8_t*)g_base;
    uint32_t pe = *(const uint32_t*)(image + 0x3C);
    return *(const uint32_t*)(image + pe + 8) == 0x6A63732E &&        // TimeDateStamp
           *(const uint32_t*)(image + pe + 24 + 56) == 0x1A1F000;     // SizeOfImage
}

// Executable memory within +/-2GB of `target`, for a 5-byte relative jmp.
static uint8_t* AllocNear(uintptr_t target, size_t size)
{
    SYSTEM_INFO info; GetSystemInfo(&info);
    uintptr_t granularity = info.dwAllocationGranularity;
    for (uintptr_t delta = granularity; delta < 0x40000000; delta += granularity) {
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t addr = dir ? target + delta : target - delta;
            addr -= addr % granularity;
            void* memory = VirtualAlloc((void*)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (memory) return (uint8_t*)memory;
        }
    }
    return nullptr;
}

static void AbsJmp(uint8_t* at, uintptr_t to)  // jmp [rip+0]; dq to   (14 bytes)
{
    at[0] = 0xFF; at[1] = 0x25; memset(at + 2, 0, 4);
    memcpy(at + 6, &to, 8);
}

// Function entry -> cave -> hook; the replayed first bytes + a jmp back form
// the trampoline returned. The jmp goes in with one 8-byte write, so an
// 8-byte aligned target is safe to patch while other threads run it.
void* HookCode(uint8_t* target, size_t size, void* hook)
{
    // Cave layout: +0 jmp to the hook; +16 the trampoline.
    uint8_t* cave = AllocNear((uintptr_t)target, 64);
    if (!cave) return nullptr;
    AbsJmp(cave, (uintptr_t)hook);
    memcpy(cave + 16, target, size);
    AbsJmp(cave + 16 + size, (uintptr_t)target + size);
    uint8_t code[8];  // target's first 8 bytes with the jmp written over them
    memcpy(code, target, 8);
    code[0] = 0xE9;
    int32_t rel = (int32_t)((intptr_t)cave - ((intptr_t)target + 5));
    memcpy(code + 1, &rel, 4);
    DWORD old;
    if (!VirtualProtect(target, 8, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    if (((uintptr_t)target & 7) == 0) InterlockedExchange64((volatile LONG64*)target, *(LONG64*)code);
    else memcpy(target, code, 5);
    VirtualProtect(target, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 8);
    return cave + 16;
}

// Waits until Steam's DRM has decrypted the page holding this code (pages
// decrypt one by one).
bool WaitForCode(uintptr_t rva, const uint8_t* expected, size_t size)
{
    for (int i = 0; i < 18000; ++i) {
        if (memcmp((const void*)(g_base + rva), expected, size) == 0) return true;
        Sleep(10);
    }
    return false;
}

static DWORD WINAPI ThreadMain(LPVOID)
{
    EnsureConfig();
    LoadConfig();
    Log("---- SaveSync v" MOD_VERSION " starting");
    for (const std::string& error : g_configErrors) Log("config: %s", error.c_str());
    if (!g_cfg.enabled) { Log("disabled in the config"); return 0; }
    g_base = (uintptr_t)GetModuleHandleW(nullptr);

    bool known = false;
    for (int i = 0; i < 2000 && !known; ++i) {
        known = IsGame12601();
        if (!known) Sleep(5);
    }
    if (!known) { Log("unknown game version - nothing is changed"); return 0; }
    SaveSync_InstallHooks();  // vtables only: before the slot menu's file can load
    return 0;
}

void Mod_Start(HMODULE self)
{
    GetModuleFileNameW(self, g_dir, MAX_PATH);  // ...\mods\SaveSync\SaveSync.dll
    if (wchar_t* slash = wcsrchr(g_dir, L'\\')) *slash = 0;
    if (HANDLE thread = CreateThread(nullptr, 0, ThreadMain, nullptr, 0, nullptr)) CloseHandle(thread);
}

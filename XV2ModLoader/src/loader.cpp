// ---------------------------------------------------------------------------
//  XV2 Mod Loader - winmm.dll
//
//  Windows loads winmm.dll from the game's bin folder before System32, so this
//  DLL is picked up by DBXV2.exe - and by every other DLL there that imports
//  winmm (EOSSDK, iggy, xgcore). It forwards all of winmm's exports to the
//  real system DLL (see winmm_exports.h) and exports nothing else. Then it
//  loads, in alphabetical order,
//
//      <game>\XV2ModLoader\Lib\*.dll               the loader's libraries
//      <game>\XV2ModLoader\mods\<ModName>\*.dll    the mods
//
//  and runs the mods' Lua scripts (lua_scripts.cpp). That is all the loader
//  does; anything more (the in-game menu, search boxes, game file edits) is
//  a library's (sdk\modloader_lib.h, Lib\src). A folder or file whose name
//  starts with "_" is skipped. Nothing is written unless loader.json says
//  "logging": true. The loader itself has no network or save file access;
//  Lua scripts in mod folders get Lua's io and os libraries.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include "winmm_exports.h"
#include "default_config.h"
#include "loader.h"
#include "lua_scripts.h"

wchar_t     g_dir[MAX_PATH];        // <game>\XV2ModLoader
static bool g_logging = false;
static bool g_luaConsole = false;   // "lua_console": true opens a console for Lua scripts
static SRWLOCK g_logLock = SRWLOCK_INIT;   // Log runs on the loader and render threads

void Log(const char* fmt, ...)
{
    if (!g_logging) return;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\loader.log", g_dir);
    AcquireSRWLockExclusive(&g_logLock);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "%02d:%02d:%02d  ", st.wHour, st.wMinute, st.wSecond);
        va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
    ReleaseSRWLockExclusive(&g_logLock);
}

// First run: create XV2ModLoader\mods and write the shipped loader.json, so a
// player who only copied winmm.dll has somewhere to put mods and a file to
// edit. CREATE_NEW - an existing loader.json is never touched.
static void EnsureLayout()
{
    wchar_t path[MAX_PATH];
    CreateDirectoryW(g_dir, nullptr);
    swprintf_s(path, L"%s\\mods", g_dir);
    CreateDirectoryW(path, nullptr);
    swprintf_s(path, L"%s\\loader.json", g_dir);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;          // already there (or unwritable)
    DWORD wrote = 0;
    WriteFile(h, kDefaultConfig, (DWORD)(sizeof(kDefaultConfig) - 1), &wrote, nullptr);
    CloseHandle(h);
}

// "key": true in loader.json; anything else or missing = false.
static bool ReadFlag(const char* json, const char* quotedKey)
{
    const char* value = strstr(json, quotedKey);
    if (!value) return false;
    value = strchr(value, ':');
    if (!value) return false;
    while (*++value == ' ' || *value == '\t') {}
    return strncmp(value, "true", 4) == 0;
}

// loader.json holds two flags; a full JSON parser would be overkill.
static void ReadConfig()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\loader.json", g_dir);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    char buf[4096]; DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
    CloseHandle(h);
    buf[got] = 0;
    g_logging = ReadFlag(buf, "\"logging\"");
    g_luaConsole = ReadFlag(buf, "\"lua_console\"");
}

bool HasExtension(const std::wstring& name, const wchar_t* extension)
{
    const size_t length = wcslen(extension);
    return name.size() > length && _wcsicmp(name.c_str() + name.size() - length, extension) == 0;
}

std::vector<std::wstring> ListFolder(const std::wstring& pattern, bool dirs)
{
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDir != dirs) continue;
        if (fd.cFileName[0] == L'.' || fd.cFileName[0] == L'_') continue;
        out.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    return out;
}

// What the libraries get (sdk\modloader_lib.h).
static void HostLog(const char* text) { Log("%s", text); }
static void HostScriptOutput(lua_State* state, int type, const char* text) { ScriptOutput(state, type, text ? text : ""); }

static const ModLoaderHost g_host = {
    kModLoaderHostVersion, g_dir, HostLog, HostScriptOutput,
    AddLuaRegistrar, AddScriptCallback, AddUnloadHandler, ForEachScript, CallScriptFunction,
    AddFrameHandler, EnsureFrameHook,
};

using LibraryInit = bool (*)(const ModLoaderHost* host);

// Lib\*.dll, before any mod: a library that exports ModLoaderLib_Init is
// handed the host; one that doesn't (lua54.dll) is just loaded.
static void LoadLibraries()
{
    const std::wstring folder = std::wstring(g_dir) + L"\\Lib\\";
    for (const auto& dll : ListFolder(folder + L"*.dll", false)) {
        if (!HasExtension(dll, L".dll")) continue;
        const std::wstring path = folder + dll;
        // ALTERED_SEARCH_PATH: a library's own dependencies (lua54.dll) resolve from Lib
        HMODULE library = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!library) { Log("FAILED   Lib\\%ls  (error %lu)", dll.c_str(), GetLastError()); continue; }
        auto init = reinterpret_cast<LibraryInit>(GetProcAddress(library, "ModLoaderLib_Init"));
        if (!init)               Log("loaded   Lib\\%ls", dll.c_str());
        else if (init(&g_host))  Log("loaded   Lib\\%ls (library)", dll.c_str());
        else                     Log("FAILED   Lib\\%ls  (ModLoaderLib_Init refused this loader)", dll.c_str());
    }
}

// Runs once DllMain has returned, so the libraries' and mods' own DllMains
// are not called under the loader lock.
static DWORD WINAPI LoadMods(LPVOID)
{
    EnsureLayout();
    ReadConfig();
    Log("--- XV2 Mod Loader v" MOD_VERSION " ---");
    LoadLibraries();
    const std::wstring mods = std::wstring(g_dir) + L"\\mods\\";
    int loaded = 0, failed = 0;
    for (const auto& mod : ListFolder(mods + L"*", true)) {
        for (const auto& dll : ListFolder(mods + mod + L"\\*.dll", false)) {
            if (!HasExtension(dll, L".dll")) continue;
            const std::wstring path = mods + mod + L"\\" + dll;
            // ALTERED_SEARCH_PATH: a mod's own dependencies resolve from its folder
            HMODULE m = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (m) { ++loaded; Log("loaded   %ls\\%ls", mod.c_str(), dll.c_str()); }
            else   { ++failed; Log("FAILED   %ls\\%ls  (error %lu)", mod.c_str(), dll.c_str(), GetLastError()); }
        }
    }
    Log("%d mod DLL(s) loaded, %d failed", loaded, failed);
    StartLuaScripts(g_luaConsole);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        // <game>\bin\winmm.dll -> <game>\XV2ModLoader
        wchar_t root[MAX_PATH];
        GetModuleFileNameW(hModule, root, MAX_PATH);
        for (int i = 0; i < 2; ++i) {
            wchar_t* slash = wcsrchr(root, L'\\');
            if (slash) *slash = 0;
        }
        swprintf_s(g_dir, L"%s\\XV2ModLoader", root);
        if (HANDLE t = CreateThread(nullptr, 0, LoadMods, nullptr, 0, nullptr))
            CloseHandle(t);
    }
    return TRUE;
}

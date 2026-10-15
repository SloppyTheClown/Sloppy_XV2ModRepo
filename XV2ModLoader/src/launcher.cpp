// ---------------------------------------------------------------------------
//  Launch Modded.exe / Launch Unmodded.exe  (built from this one file)
//
//  Sit in <game>\XV2ModLoader (the game folder itself also works).
//
//  Unmodded: every DLL in bin that is not one of the game's own gets
//            "_disabled" appended (winmm.dll -> winmm.dll_disabled), then the
//            game is started through Steam's normal entry, with EasyAntiCheat.
//  Modded:   every *.dll_disabled in bin is renamed back, then bin\DBXV2.exe
//            is started directly.
//
//  "The game's own" is the list below. Anything else in bin is treated as a
//  mod, so mods this launcher has never heard of are disabled too.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

#ifndef MODDED
#error define MODDED as 0 or 1
#endif

#if !MODDED
static const wchar_t* kVanillaDlls[] = {
    L"EOSSDK-Win64-Shipping.dll",
    L"iggy_w64.dll",
    L"oo2core_6_win64.dll",
    L"render_pass_x64.dll",
    L"steam_api64.dll",
    L"xgcore.dll",
};
#endif
static const wchar_t kSuffix[] = L"_disabled";
static const wchar_t* kTitle  = MODDED ? L"XV2 - Launch Modded" : L"XV2 - Launch Unmodded";

static bool EndsWithI(const std::wstring& s, const wchar_t* tail)
{
    size_t n = wcslen(tail);
    return s.size() >= n && _wcsicmp(s.c_str() + s.size() - n, tail) == 0;
}

#if !MODDED
static bool IsVanilla(const std::wstring& name)
{
    for (const wchar_t* v : kVanillaDlls)
        if (_wcsicmp(name.c_str(), v) == 0) return true;
    return false;
}
#endif

static bool GameRunning()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) };
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe))
        found = _wcsicmp(pe.szExeFile, L"DBXV2.exe") == 0;
    CloseHandle(snap);
    return found;
}

// Every file in `dir` - filtered by exact name below, because a "*.dll" search
// also matches "x.dll_disabled" through its 8.3 short name.
static std::vector<std::wstring> Files(const std::wstring& dir)
{
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

static int Fail(const std::wstring& msg)
{
    MessageBoxW(nullptr, msg.c_str(), kTitle, MB_OK | MB_ICONERROR);
    return 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring root(self);
    root.resize(root.rfind(L'\\'));
    const std::wstring here = root;
    auto hasGame = [](const std::wstring& dir) {
        return GetFileAttributesW((dir + L"\\bin\\DBXV2.exe").c_str()) != INVALID_FILE_ATTRIBUTES;
    };
    if (!hasGame(root) && root.rfind(L'\\') != std::wstring::npos)
        root.resize(root.rfind(L'\\'));   // <game>\XV2ModLoader -> <game>
    if (!hasGame(root))
        return Fail(L"DBXV2.exe was not found.\n\nThis launcher belongs in the XV2ModLoader folder"
                    L" inside the game folder. It is in:\n" + here);
    const std::wstring bin = root + L"\\bin";
    const std::wstring exe = bin + L"\\DBXV2.exe";
    if (GameRunning())
        return Fail(L"The game is already running. Close it first.");

    std::wstring errors;
    for (const std::wstring& name : Files(bin)) {
        std::wstring from = bin + L"\\" + name, to;
#if MODDED
        if (!EndsWithI(name, L".dll_disabled")) continue;
        to = from.substr(0, from.size() - wcslen(kSuffix));
#else
        if (!EndsWithI(name, L".dll") || IsVanilla(name)) continue;
        to = from + kSuffix;
#endif
        if (!MoveFileExW(from.c_str(), to.c_str(), 0))
            errors += L"  " + name + L"  (error " + std::to_wstring(GetLastError()) + L")\n";
    }
    if (!errors.empty())
        return Fail(L"Could not rename:\n\n" + errors +
                    L"\nNothing was launched. If a file of the same name already exists,"
                    L" remove one of the two.");

#if MODDED
    // Started from a Steam "non-Steam game" shortcut, we inherit SteamAppId,
    // SteamGameId etc. set to the shortcut's id. The game's Steam check sees a
    // foreign app id and has Steam restart it through its store entry
    // (START.exe, EasyAntiCheat). Drop them so it starts as from Explorer.
    std::vector<std::wstring> steamVars;
    if (wchar_t* env = GetEnvironmentStringsW()) {
        for (const wchar_t* p = env; *p; p += wcslen(p) + 1) {
            std::wstring var(p);
            size_t eq = var.find(L'=', 1);
            if (eq != std::wstring::npos && _wcsnicmp(var.c_str(), L"Steam", 5) == 0)
                steamVars.push_back(var.substr(0, eq));
        }
        FreeEnvironmentStringsW(env);
    }
    for (const std::wstring& name : steamVars)
        SetEnvironmentVariableW(name.c_str(), nullptr);

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exe + L"\"";
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        bin.c_str(), &si, &pi))
        return Fail(L"Could not start DBXV2.exe (error " + std::to_wstring(GetLastError()) + L").");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    // Steam's own entry for the game - the one that runs EasyAntiCheat.
    std::wstring appid = L"454650";
    HANDLE h = CreateFileW((bin + L"\\steam_appid.txt").c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char buf[32] = {}; DWORD got = 0;
        ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
        CloseHandle(h);
        std::wstring id;
        for (DWORD i = 0; i < got && buf[i] >= '0' && buf[i] <= '9'; ++i) id += (wchar_t)buf[i];
        if (!id.empty()) appid = id;
    }
    std::wstring url = L"steam://rungameid/" + appid;
    if ((INT_PTR)ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL) <= 32)
        return Fail(L"Could not ask Steam to start the game. Is Steam installed?");
#endif
    return 0;
}

// ---------------------------------------------------------------------------
//  EffectTextureLimit.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\EffectTextureLimit\
//
//  Raises the particle texture limit from 128 to 255 textures per effect
//  texture file (ptcl.emb). xgcore.dll's particle engine reads each EMP
//  texture sampler's EmbIndex as a SIGNED byte, so indices 128..254 become
//  negative, fail the unsigned bounds check and the particle draws without
//  its texture. One byte turns that read unsigned:
//
//    xgcore.dll+0x4c119  movsx ecx, byte ptr [rax+1]   0F BE 48 01
//                     -> movzx ecx, byte ptr [rax+1]   0F B6 48 01
//
//  (same RVA in game 1.25.02 and 1.26.01; Research_Notes\effects-and-vfx.md).
//  255 still means "no texture": the bounds check skips it while the file
//  has fewer than 256 textures. Trace effects (trc.emb) already read the
//  index unsigned. Files with <= 128 textures behave exactly as before.
//
//  Reads only effect_texture_limit.json's "logging" next to the DLL; writes
//  EffectTextureLimit.log only when that is true. No network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include "default_config.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr DWORD kBindIndexRva = 0x4c119;                               // movsx ecx, byte ptr [rax+1]
constexpr unsigned char kSigned[]   = { 0x0F, 0xBE, 0x48, 0x01 };     // as shipped
constexpr unsigned char kUnsigned[] = { 0x0F, 0xB6, 0x48, 0x01 };     // patched

std::wstring g_modDir;
bool g_logging = false;

void Log(const char *message)
{
    if (!g_logging) return;
    FILE *file = nullptr;
    if (_wfopen_s(&file, (g_modDir + L"EffectTextureLimit.log").c_str(), L"a, ccs=UTF-8") != 0 || !file) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    fwprintf(file, L"%02d:%02d:%02d %hs\n", now.wHour, now.wMinute, now.wSecond, message);
    fclose(file);
}

// "logging": true in the config; a missing config is written from the default.
void ReadConfig()
{
    std::wstring path = g_modDir + L"effect_texture_limit.json";
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) {
        if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file) {
            fputs(kDefaultConfig, file);
            fclose(file);
        }
        return;
    }
    char text[4096] = {};
    fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    const char *key = strstr(text, "\"logging\"");
    if (key) {
        const char *value = strchr(key, ':');
        g_logging = value && strncmp(value + 1 + strspn(value + 1, " \t\r\n"), "true", 4) == 0;
    }
}

// Patches the bind loop's EmbIndex read; xgcore is not DRM-encrypted, so the
// bytes are final as soon as the module is mapped.
void Patch()
{
    HMODULE xgcore = nullptr;
    // xgcore.dll is an import of DBXV2.exe, so it is normally mapped before
    // any mod; wait briefly (bounded) in case it is not.
    for (int attempt = 0; attempt < 100 && !(xgcore = GetModuleHandleW(L"xgcore.dll")); attempt++)
        Sleep(100);
    if (!xgcore) { Log("xgcore.dll not loaded - nothing patched"); return; }

    unsigned char *site = reinterpret_cast<unsigned char *>(xgcore) + kBindIndexRva;
    if (memcmp(site, kUnsigned, sizeof(kUnsigned)) == 0) { Log("already patched"); return; }
    if (memcmp(site, kSigned, sizeof(kSigned)) != 0) {
        char message[128];
        sprintf_s(message, "xgcore.dll+0x%lx is %02X %02X %02X %02X, not movsx ecx,[rax+1] - unknown xgcore, nothing patched",
                  kBindIndexRva, site[0], site[1], site[2], site[3]);
        Log(message);
        return;
    }
    DWORD oldProtect = 0;
    if (!VirtualProtect(site, sizeof(kUnsigned), PAGE_EXECUTE_READWRITE, &oldProtect)) { Log("VirtualProtect failed"); return; }
    site[1] = kUnsigned[1];
    VirtualProtect(site, sizeof(kUnsigned), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(kUnsigned));
    Log("patched xgcore.dll+0x4c119: particle textures 0..254 bind (limit 255)");
}

DWORD WINAPI Start(LPVOID)
{
    ReadConfig();
    Log("---- Effect Texture Limit v" MOD_VERSION " starting");
    Patch();
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        wchar_t path[MAX_PATH];
        DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        g_modDir.assign(path, length);
        g_modDir.resize(g_modDir.find_last_of(L"\\/") + 1);
        // Off the loader lock: the wait for xgcore must not block DllMain.
        HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
//  General_QoL.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\General_QoL\
//
//  Starts the mod on a background thread, reads its config next to the DLL,
//  writes only general_qol_saved.txt (camera zoom, title volume) unless
//  logging is switched on, and makes no network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include "mod.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Mod_Start(hModule);
    } else if (reason == DLL_PROCESS_DETACH) {
        Mod_Stop();
    }
    return TRUE;
}

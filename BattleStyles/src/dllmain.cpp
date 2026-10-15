// ---------------------------------------------------------------------------
//  BattleStyles.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\BattleStyles\
//
//  Starts the mod on a background thread, reads its config next to the DLL,
//  writes nothing but its log (when logging is on), makes no network calls.
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

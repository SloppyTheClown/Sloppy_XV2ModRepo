// ---------------------------------------------------------------------------
//  SaveSync.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\SaveSync\
//
//  Starts the mod on a background thread, reads its config next to the DLL,
//  writes only its log (when logging is on) and savesync.sav, makes no
//  network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include "mod.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Mod_Start(hModule);
    }
    return TRUE;
}

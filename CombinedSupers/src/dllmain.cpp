// ---------------------------------------------------------------------------
//  Built two ways from the same source:
//
//  CombinedSupers.dll  loaded by XV2 Mod Loader from
//                      <game>\XV2ModLoader\mods\CombinedSupers\
//  winmm.dll           (COMBINED_STANDALONE) sits in <game>\bin on its own.
//                      Windows loads winmm.dll from bin before System32, for
//                      the game and for its own DLLs that use winmm, so this
//                      version forwards every winmm export to the real one
//                      (winmm_exports.h).
//
//  Either way it starts the mod on a background thread, reads its config and
//  the game's own PSC file, writes nothing unless logging is switched on, and
//  makes no network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include "mod.h"
#if COMBINED_STANDALONE
#include "winmm_exports.h"
#endif

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

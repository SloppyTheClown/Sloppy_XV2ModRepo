// ---------------------------------------------------------------------------
//  XV2 Mod Loader library - XV2ModLoader\Lib\XV2ModLoaderLib.dll
//
//  Everything the loader (bin\winmm.dll) offers beyond loading DLL mods and
//  running Lua scripts, plugged in through sdk\modloader_lib.h:
//
//  * the in-game menu (~) and overlay - Dear ImGui over the game's frames
//    (overlay.cpp), with the scripts' _OnMenu / _OnOverlay and the "ui"
//    table (lua_ui.cpp);
//  * search boxes for mods - Lua's "search" table and the XV2Search_*
//    exports DLL mods use (search_box.cpp, sdk\xv2_search.h);
//  * game file patches - Lua's "files" table and the XV2Files_* exports
//    (file_patches.cpp, iggy_abc.cpp, sdk\xv2_files.h);
//  * menu callbacks - Lua's "menus" table: a patched menu script's messages
//    to a script and its answers (menu_callbacks.cpp).
//
//  sdk\modloader_lib.lua documents every Lua function for script authors.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include "lua.h"
#include "file_patches.h"
#include "host.h"
#include "lua_ui.h"
#include "menu_callbacks.h"
#include "overlay.h"
#include "search_box.h"

const ModLoaderHost* g_host = nullptr;

// A line in loader.log (printf-style). Dropped before ModLoaderLib_Init:
// a DLL mod may call an export (e.g. XV2Files_ReplaceFile) that logs before
// the loader has initialised this library.
void Log(const char* format, ...)
{
    if (!g_host) return;
    char line[1024];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    g_host->Log(line);
}

// A script's print/warning/error line, to the Lua console and loader.log.
void ScriptOutput(lua_State* state, int type, const std::string& text)
{
    if (g_host) g_host->ScriptOutput(state, type, text.c_str());
}

bool IsGame12601()
{
    const uint8_t* image = (const uint8_t*)GetModuleHandleW(nullptr);
    uint32_t pe = *(const uint32_t*)(image + 0x3C);
    return *(const uint32_t*)(image + pe + 8) == 0x6A63732E &&        // TimeDateStamp
           *(const uint32_t*)(image + pe + 24 + 56) == 0x1A1F000;     // SizeOfImage
}

std::string ScriptName(lua_State* state)
{
    std::string name = "Lua";
    if (lua_getglobal(state, "LUA_NAME") == LUA_TSTRING) name = lua_tostring(state, -1);
    lua_pop(state, 1);
    return name;
}

namespace {

// F1 reload: the scripts register their files and menu callbacks again.
void ClearScriptRegistrations()
{
    ClearScriptFilePatches();
    ClearScriptMenuCallbacks();
}

// True when the script's state has a global function called `name`.
bool HasGlobalFunction(lua_State* state, const char* name)
{
    const bool found = lua_getglobal(state, name) == LUA_TFUNCTION;
    lua_pop(state, 1);
    return found;
}

// Run the script's callback through the host (errors go to its console).
bool CallOnMenu(lua_State* state) { return g_host->CallScriptFunction(state, "_OnMenu"); }
bool CallOnOverlay(lua_State* state) { return g_host->CallScriptFunction(state, "_OnOverlay"); }

// One menu section per script that defines _OnMenu; counts them in *context.
bool VisitMenu(lua_State* state, const char* name, void* context)
{
    if (!HasGlobalFunction(state, "_OnMenu")) return true;
    ++*static_cast<size_t*>(context);
    return DrawScriptMenu(state, name, CallOnMenu);
}

// The script's _OnOverlay windows, when it defines one.
bool VisitOverlay(lua_State* state, const char* name, void*)
{
    if (!HasGlobalFunction(state, "_OnOverlay")) return true;
    return DrawScriptOverlay(state, name, CallOnOverlay);
}

// The menu's contents (overlay.cpp calls this while the menu is open).
void DrawScriptMenus()
{
    size_t sections = 0;
    g_host->ForEachScript(VisitMenu, &sections, "_OnMenu");
    if (sections == 0) DrawMenuNote("No Lua script has a menu yet (a script adds one by defining _OnMenu).");
}

// The scripts' own windows (every frame the menu is open or the overlay was requested).
void DrawScriptOverlays()
{
    g_host->ForEachScript(VisitOverlay, nullptr, "_OnOverlay");
}

// The host's per-frame callback (Present hook, render thread).
void OnFrame(IDXGISwapChain* swapChain)
{
    OverlayFrame(swapChain, DrawScriptMenus, DrawScriptOverlays);
}

// Puts this library's Lua tables into every script's state.
void RegisterTables(lua_State* state)
{
    RegisterLuaUi(state);
    RegisterLuaFiles(state);
    RegisterLuaSearch(state);
    RegisterLuaMenus(state);
}

}  // namespace

// Called by the loader (bin\winmm.dll) once, right after loading this DLL
// and before any mod DLL. Returns false (library unused) for a null host or
// an older host interface.
extern "C" __declspec(dllexport) bool ModLoaderLib_Init(const ModLoaderHost* host)
{
    if (!host || host->version < 1) return false;
    g_host = host;
    host->AddLuaRegistrar(RegisterTables);
    host->AddScriptCallback("_OnMenu");
    host->AddScriptCallback("_OnOverlay");
    host->AddUnloadHandler(ClearScriptRegistrations);
    host->AddFrameHandler(OnFrame);
    return true;
}

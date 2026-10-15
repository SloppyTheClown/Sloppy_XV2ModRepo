// The loader's services (sdk\modloader_lib.h), for the rest of this library.
#pragma once
#include <string>
#include "modloader_lib.h"

// Set by ModLoaderLib_Init (lib_main.cpp) before anything else here runs.
extern const ModLoaderHost* g_host;

// A line in loader.log (printf-style).
void Log(const char* format, ...);

// "[LUA_NAME] TYPE: text" to the Lua console and loader.log.
void ScriptOutput(lua_State* state, int type, const std::string& text);

// True when the running exe is DBXV2.exe 1.26.01 (PE TimeDateStamp and
// SizeOfImage; the same check as the DLL mods).
bool IsGame12601();

// The script's name (its LUA_NAME global; "Lua" without one): the owner of
// its file patches and menu callbacks.
std::string ScriptName(lua_State* state);

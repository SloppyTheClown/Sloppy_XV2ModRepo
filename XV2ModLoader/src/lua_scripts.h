// The Lua script runtime: lua_scripts.cpp runs the scripts, lua_api.cpp holds
// the functions they can call. Libraries (sdk\modloader_lib.h) extend it.
#pragma once
#include <string>
#include "modloader_lib.h"

// lua_scripts.cpp - called once from the loader thread after the libraries
// and DLL mods are loaded. Does nothing when no mod folder holds a .lua file.
void StartLuaScripts(bool console);

// Prints "[LUA_NAME] TYPE: text" to the console (when open) and loader.log.
// state may be null for the runtime's own messages.
void ScriptOutput(lua_State* state, int type, const std::string& text);

// lua_api.cpp - puts LuaBackend's and Cheat Engine's functions into a state.
void RegisterLuaApi(lua_State* state);

// The ModLoaderHost functions (sdk\modloader_lib.h) backed by the runtime.
void AddLuaRegistrar(void (*registrar)(lua_State* state));
void AddScriptCallback(const char* functionName);
void AddUnloadHandler(void (*handler)());
void ForEachScript(bool (*visit)(lua_State* state, const char* name, void* context), void* context,
                   const char* failure);
bool CallScriptFunction(lua_State* state, const char* functionName);
void AddFrameHandler(void (*handler)(IDXGISwapChain* swapChain));
void EnsureFrameHook();

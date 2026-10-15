// Game files the loader changes as the game reads them, for Lua scripts (the
// "files" table) and DLL mods (XV2Files_* exports, sdk\xv2_files.h) - see
// file_patches.cpp.
#pragma once

struct lua_State;

// Puts the "files" table into a script's state (a host Lua registrar).
void RegisterLuaFiles(lua_State* state);

// Drops every Lua script's patches (F1 reload: the scripts register again);
// DLL mods' patches stay. Copies the game was already given stay alive.
void ClearScriptFilePatches();

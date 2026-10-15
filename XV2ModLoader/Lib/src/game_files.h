// Reading the game's own data files (loose data\ first, then the .cpk
// archives) for Lua scripts (files.ReadGameFile) and DLL mods
// (XV2Files_ReadGameFile, sdk\xv2_files.h) - see game_files.cpp.
#pragma once

struct lua_State;

// files.ReadGameFile(path) - put into the "files" table by RegisterLuaFiles.
int FilesReadGameFile(lua_State* state);

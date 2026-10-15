// Shared between loader.cpp and the Lua runtime (lua_scripts.cpp, lua_api.cpp).
#pragma once
#include <windows.h>
#include <string>
#include <vector>

extern wchar_t g_dir[MAX_PATH];     // <game>\XV2ModLoader

// Appends a line to loader.log when loader.json says "logging": true.
void Log(const char* fmt, ...);

// Folders (dirs = true) or files matching a FindFirstFile pattern, sorted
// case-insensitively; names starting with "." or "_" are left out.
std::vector<std::wstring> ListFolder(const std::wstring& pattern, bool dirs);

// True when name ends in extension (".dll"), ignoring case. FindFirstFile's
// "*.dll" also matches "x.dll_disabled" through its 8.3 short name, so every
// listing by extension checks it again.
bool HasExtension(const std::wstring& name, const wchar_t* extension);

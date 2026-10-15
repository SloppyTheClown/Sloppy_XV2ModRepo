#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>

void Mod_Start(HMODULE self);
void Mod_Stop();

// Shared with style_menu.cpp (defined in mod.cpp).
extern uintptr_t     g_base;      // DBXV2.exe image base
extern wchar_t       g_dir[MAX_PATH];  // the mod's folder: config, log, equipped styles
extern std::wstring  g_gameDir;   // <game>
void  Log(const char* fmt, ...);
bool  ReadWhole(const wchar_t* path, std::string& out);
void* HookCode(uint8_t* target, size_t size, void* hook);
bool  CodeIs(uintptr_t rva, const uint8_t* code, size_t size);
bool  WaitForCode(uintptr_t rva, const uint8_t* expected, size_t size);  // CodeIs, retried up to 3 min
double MsSinceStart();

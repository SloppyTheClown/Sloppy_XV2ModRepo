#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

void Mod_Start(HMODULE self);

// Shared with savesync.cpp (defined in mod.cpp).
extern uintptr_t g_base;           // DBXV2.exe image base
extern wchar_t   g_dir[MAX_PATH];  // the mod's folder: config, log, savesync.sav
void  Log(const char* fmt, ...);
bool  ReadWhole(const wchar_t* path, std::string& out);

// Code hooks (mod.cpp). HookCode: the first `size` (5..8) bytes of `target`
// must be whole, position-independent instructions; returns the trampoline
// that runs the original, or null.
void* HookCode(uint8_t* target, size_t size, void* hook);
// Waits (up to 3 minutes) until Steam's DRM has decrypted the code at `rva`.
bool  WaitForCode(uintptr_t rva, const uint8_t* expected, size_t size);

// XV2 Mod Loader file patches (and game file reading), for DLL mods.
//
// The loader's library (XV2ModLoader\Lib\XV2ModLoaderLib.dll) can change a
// game file as the game reads it, so nothing in data\ has to be replaced and
// your mod doesn't need a file hook of its own. Copy this header into your
// mod; it needs only <windows.h>. Lua scripts get the same service through
// the "files" table (sdk\modloader_lib.lua).
//
//     XV2Files files;
//     if (files.Load()) {                           // false: no loader, or an older one
//         char reason[128];
//         if (!files.ReplaceFile("MyMod", "ui/texture/MY_MENU.emb", bytes, size, reason, sizeof(reason)))
//             log("not replaced: %s", reason);
//     }
//
// Rules shared by every function:
// * path: the game file, as the end of its path inside data\ - "ui/iggy/
//   ITEMLIST.iggy" matches "data\ui\iggy\ITEMLIST.iggy"; '/' and '\' are the
//   same, case is ignored, and the match must start at a folder boundary.
// * owner: your mod's name (any text). One patch per file: when another
//   owner already patches it, Replace* is refused; the same owner calling
//   again replaces its own patch.
// * A patch applies to every later read of the file (menus are read each
//   time they open; most other files once, at boot or at load screens), so
//   register before the game first reads it - DllMain is fine.
// * Only files the game has (in a cpk or loose) and reads through its normal
//   file loader; XV2Patcher's own reads (e.g. its .x2s files) don't pass.
// * Your data is copied: free it whenever you like.
// * Game version DBXV2.exe 1.26.01 only (other versions are refused).
// * Every function may be called from any thread, at any time.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

struct XV2Files {
    // Serves `data` instead of the game file at `path`, whole.
    //   owner, path   see above
    //   data, size    the new file's bytes (size > 0, at most 512 MB)
    //   reason        buffer for why it was refused, or null; reasonBytes = its size
    // Returns 1 when registered, 0 when refused (reason set).
    int (*ReplaceFile)(const char* owner, const char* path, const void* data, size_t size, char* reason,
                       int reasonBytes) = nullptr;

    // Swaps the ActionScript (AS3 ABC) of the .iggy menu at `path` for `abc`
    // - but only while the menu's own script is the one you built from
    // (vanillaAbcSize bytes with FNV-1a 64 hash vanillaAbcHash; the
    // ClaudeTools\iggy\*_abc.py builders print both), so a menu another mod changed
    // is left alone (Status then says "NOT patched: ...").
    // One script per original: registering again for another original (e.g.
    // the vanilla menu and xv2patcher's) adds it, for the same original
    // replaces it; each original the game loads gets its own.
    // Returns 1 when registered, 0 when refused (reason set as for ReplaceFile).
    int (*ReplaceIggyScript)(const char* owner, const char* path, const void* abc, size_t abcSize,
                             uint32_t vanillaAbcSize, uint64_t vanillaAbcHash, char* reason,
                             int reasonBytes) = nullptr;

    // Removes `owner`'s patch of `path`: later reads get the game's file.
    // Returns 1 when there was one, otherwise 0.
    int (*Remove)(const char* owner, const char* path) = nullptr;

    // Copies what happened when the game last read the file at `path`:
    // "registered, not loaded by the game yet", "served: ..." or
    // "NOT patched: <why>" - "" when no mod patches it.
    //   out, outBytes   as XV2Search::Query: cut to fit, always 0-terminated
    // Returns the text's full length in bytes (0 = no patch).
    int (*Status)(const char* path, char* out, int outBytes) = nullptr;

    // Copies the owner of the patch of `path` (the DLL mod's owner name or
    // the Lua script's LUA_NAME); "" when no mod patches it.
    // Returns the name's full length in bytes. Null with an older library.
    int (*Owner)(const char* path, char* out, int outBytes) = nullptr;

    // Reads a game file as the game finds it: loose data\ first (x2m and
    // Revamp files count), then the .cpk archives. Read-only; patches are not
    // applied. The first call reads the archives' file tables (about a second).
    //   path            inside data\ ("system/custom_skill.cus")
    //   out, outBytes   filled when the file fits; out may be null to ask the
    //                   size, which reads no file data
    // Returns the file's size in bytes, or -1 when it is found nowhere.
    // Null with a library older than this function (Load() still succeeds).
    long long (*ReadGameFile)(const char* path, void* out, size_t outBytes) = nullptr;

    // Where ReadGameFile would find `path`, without reading it.
    //   source, sourceBytes   gets "loose" or the .cpk's name (may be null);
    //                         cut to fit, always 0-terminated
    // Returns the file's size in bytes, or -1 when it is found nowhere.
    // Null with a library older than this function.
    long long (*FindGameFile)(const char* path, char* source, int sourceBytes) = nullptr;

    // Reads part of a game file: bytes [offset, offset + outBytes), cut at
    // its end. Compressed archive files decode from their end backwards, so
    // a part near the end of a big file (or in its first 256 bytes) is
    // cheap; whole-file reads should use ReadGameFile.
    // Returns the number of bytes copied, or -1 when the file is found
    // nowhere or offset is past its end. Null with an older library.
    long long (*ReadGameFilePart)(const char* path, unsigned long long offset, void* out, size_t outBytes) = nullptr;

    // Offline tools only (a test that loads the library outside the game):
    // the game folder to read from instead of two levels above the exe.
    // Returns 0 once a file has been read (too late). Null with an older library.
    int (*SetGameFolder)(const wchar_t* folder) = nullptr;

    // Finds the functions in the loader's library (loaded before any mod).
    // Returns true when all were found; false without the loader or with a
    // library older than this header.
    bool Load()
    {
        HMODULE library = GetModuleHandleW(L"XV2ModLoaderLib.dll");
        if (!library) return false;
        bool found = true;
        auto get = [&](auto& function, const char* name) {
            function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(GetProcAddress(library, name));
            found = found && function;
        };
        get(ReplaceFile, "XV2Files_ReplaceFile");
        get(ReplaceIggyScript, "XV2Files_ReplaceIggyScript");
        get(Remove, "XV2Files_Remove");
        get(Status, "XV2Files_Status");
        // newer than the others: optional, so older libraries still load
        ReadGameFile = reinterpret_cast<decltype(ReadGameFile)>(GetProcAddress(library, "XV2Files_ReadGameFile"));
        FindGameFile = reinterpret_cast<decltype(FindGameFile)>(GetProcAddress(library, "XV2Files_FindGameFile"));
        ReadGameFilePart = reinterpret_cast<decltype(ReadGameFilePart)>(GetProcAddress(library, "XV2Files_ReadGameFilePart"));
        SetGameFolder = reinterpret_cast<decltype(SetGameFolder)>(GetProcAddress(library, "XV2Files_SetGameFolder"));
        Owner = reinterpret_cast<decltype(Owner)>(GetProcAddress(library, "XV2Files_Owner"));
        return found;
    }
};

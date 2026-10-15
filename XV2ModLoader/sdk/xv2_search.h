// XV2 Mod Loader search boxes, for DLL mods.
//
// The loader's library (XV2ModLoader\Lib\XV2ModLoaderLib.dll) draws a text
// box over the game for any mod that asks for one, and tells the mod what was
// typed. Copy this header into your mod; it needs only <windows.h>. Lua
// scripts get the same boxes through the "search" table (sdk\modloader_lib.lua).
//
//     XV2Search search;
//     if (search.Load()) {                          // false: no loader, or an older one
//         // every frame (or tick) while your list is open:
//         unsigned changes = search.Show("mymod_list", "Search (Ctrl+F)", 0.22f, 0.17f, 0.30f);
//         if (changes != filteredFor) { ... filter again ...; filteredFor = changes; }
//         bool keep = search.Matches("mymod_list", "Name of an entry");
//     }
//
// Rules shared by every function:
// * id: any 0-terminated text naming one box; start it with your mod's name
//   so boxes don't collide. Mods that pass the same id share one box and one
//   query (Item Search and Battle Styles both use "equipment_search" on the
//   equipment screen). A null id is ignored (getters return their "unknown
//   box" value). Getters never create a box; setters do.
// * A box shows while you keep calling Show (at least every ~0.1 s) and hides
//   by itself when you stop - there is no "hide". What was typed is kept
//   while it is hidden.
// * The player clicks the box or presses Ctrl+F to type; while typing the
//   game gets no key presses. Enter (or a click elsewhere) ends typing, Esc
//   undoes the edit. A box holds up to 128 bytes of UTF-8.
// * Matching ignores case for A-Z only; other letters must match exactly.
// * Every function may be called from any thread, at any time (also before
//   the game shows its first frame).
#pragma once
#include <windows.h>
#include <type_traits>

struct XV2Search {
    // Shows the box this frame; call it every frame (or tick) the box should
    // be up.
    //   id     the box (see above)
    //   hint   greyed-out text shown while the box is empty; null = none
    //   x, y   where the box's top left corner first appears, as fractions of
    //          the game's picture (0..1); used only when the box first comes
    //          up in a session (the player can drag it)
    //   width  width of the text field, as a fraction of the picture's width
    // Returns the box's change count (see Changes); 0 for a null id.
    unsigned (*Show)(const char* id, const char* hint, float x, float y, float width) = nullptr;

    // Sets a greyed-out line under the box (e.g. "No item matches").
    //   note   UTF-8 text; null or "" removes the line
    // The line stays until set again (Show leaves it alone).
    void (*SetNote)(const char* id, const char* note) = nullptr;

    // Copies the typed text, in lower case (A-Z), UTF-8.
    //   out       buffer for the text, or null to only ask for its length
    //   outBytes  size of `out` in bytes; the copy is cut to outBytes - 1
    //             bytes and always 0-terminated (when outBytes > 0)
    // Returns the full length of the text in bytes, without the 0 (larger
    // than outBytes - 1 when it was cut). 0 for an empty or unknown box.
    int (*Query)(const char* id, char* out, int outBytes) = nullptr;

    // Whether `name` (UTF-16, 0-terminated) contains the box's text,
    // ignoring case (A-Z). Returns 1 when it does, when the box is empty or
    // unknown, or for a null id; otherwise 0. A null name counts as "".
    int (*MatchesW)(const char* id, const wchar_t* name) = nullptr;

    // Same as MatchesW for a UTF-8 name.
    int (*Matches)(const char* id, const char* name) = nullptr;

    // Returns 1 while the player is typing in the box, otherwise 0. Use it
    // to ignore your own hotkeys while they type.
    int (*Typing)(const char* id) = nullptr;

    // Puts the keyboard in the box (as Ctrl+F does) the next time it is drawn.
    void (*Focus)(const char* id) = nullptr;

    // Empties the box (e.g. when a different list opens). Counts as a change
    // when it held text.
    void (*Clear)(const char* id) = nullptr;

    // Replaces the box's text.
    //   text   UTF-8; null = ""; cut to 128 bytes
    void (*SetText)(const char* id, const char* text) = nullptr;

    // Returns a count that goes up by one every time the box's query (its
    // lower-case text) changes; 0 for an unknown box. Remember the value you
    // last filtered for and filter again when it differs.
    unsigned (*Changes)(const char* id) = nullptr;

    // Finds the functions in the loader's library (the loader loads it
    // before any mod, so DllMain may already call this). Returns true when
    // all of them were found; false without the loader or with a library
    // older than this header (then use none of them).
    bool Load()
    {
        HMODULE library = GetModuleHandleW(L"XV2ModLoaderLib.dll");
        if (!library) return false;
        bool found = true;
        auto get = [&](auto& function, const char* name) {
            function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(GetProcAddress(library, name));
            found = found && function;
        };
        get(Show, "XV2Search_Show");
        get(SetNote, "XV2Search_SetNote");
        get(Query, "XV2Search_Query");
        get(MatchesW, "XV2Search_MatchesW");
        get(Matches, "XV2Search_Matches");
        get(Typing, "XV2Search_Typing");
        get(Focus, "XV2Search_Focus");
        get(Clear, "XV2Search_Clear");
        get(SetText, "XV2Search_SetText");
        get(Changes, "XV2Search_Changes");
        return found;
    }
};

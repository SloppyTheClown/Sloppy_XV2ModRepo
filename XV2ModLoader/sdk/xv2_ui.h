// XV2UserInterfaceLib (XV2ModLoader\Lib\XV2UserInterfaceLib.dll), for DLL mods.
//
// Lets any number of mods edit the same menu. Each mod ships a whole edited
// copy of the menu's ActionScript (the AS3 ABC inside the .iggy, e.g. made
// with FFDec); the library works out what the mod changed against the script
// it was made from and merges only that into the menu as the mods before it
// left it - so two mods editing CHARASELE both apply instead of the last one
// replacing the other. Copy this header into your mod; it needs only
// <windows.h>. Lua scripts use the "uilib" table (sdk\modloader_lib.lua).
//
//     XV2UI ui;
//     if (ui.Load()) {
//         char reason[256];
//         if (!ui.EditScript("MyMod", "ui/iggy/CHARASELE.iggy", myAbc, myAbcSize,
//                            myBase, myBaseSize, nullptr, 0, 0, reason, sizeof(reason)))
//             log("menu edit refused: %s", reason);
//     }
//
// What a merge does (and doesn't): changed and new methods, traits, classes
// and scripts are applied; removals are not; a method an earlier mod also
// changed is merged instruction by instruction when the two edits don't
// overlap, otherwise this mod's version wins (noted in the status).
// Texture files (.iggytex / .iggyted / .emb) go per image: SetImage
// replaces or adds one image, other mods' images of the file apply too.
// Text files (.msg) go per entry the same way: SetMessage.
// A menu's timeline can be edited too: EditDisplay clones a sprite's placement
// (one more button) and sets a placement's matrix in a frame.
// Status: the loader's XV2Files::Status (sdk\xv2_files.h) for the menu,
// e.g. "MyMod: served: 3 changed (1 merged with an earlier edit), ...".
#pragma once
#include <windows.h>
#include <cstddef>
#include <type_traits>

struct XV2UI {
    // Registers a script edit of the .iggy menu at `path`.
    //   owner, path       as XV2Files
    // A menu may come in versions (the cpk's CHARASELE, XV2Patcher's loose
    // one): call once per version you made a script for, each with its
    // `game`. When the menu loads, the script whose `game` is the menu's
    // script byte for byte is used; else the one registered without `game`;
    // else the first, merged over its own `game`. The same owner and `game`
    // again replace that script; Remove drops them all.
    //   abc, abcSize      your whole edited script
    //   base, baseSize    what you edited, when it isn't the game's own
    //                     script byte for byte - with FFDec, the game's script
    //                     with the same classes recompiled unchanged
    //                     (ClaudeTools\iggy\ffdec_base.py); null = `game`
    //   game, gameSize    the game script `base` comes from, when the menu
    //                     the game loads may be another (e.g. you built from the
    //                     vanilla CHARASELE but XV2Patcher's is installed);
    //                     null = the menu's script as the game loads it
    //   order             lower runs earlier (0 = usual), as XV2Files::AddStage
    //   reason            why it was refused, or null; reasonBytes = its size
    // Your data is copied. Returns 1 when registered, 0 when refused.
    int (*EditScript)(const char* owner, const char* path, const void* abc, size_t abcSize, const void* base,
                      size_t baseSize, const void* game, size_t gameSize, int order, char* reason,
                      int reasonBytes) = nullptr;

    // Removes `owner`'s edit of `path` (its script, display list, or all its images or entries).
    // Returns 1 when there was one.
    int (*Remove)(const char* owner, const char* path) = nullptr;

    // Replaces one image of the texture file at `path`, or adds it when the
    // file has none by that name; your other images of the file stay, and
    // other mods' images of it apply too (the same image from two mods: the
    // later wins, noted in loader.log). Null with an older library.
    //   path        a .iggytex / .iggyted ("ui/iggy/CMNTITLE_en.iggytex") or
    //               .emb ("ui/texture/CHARA01.emb")
    //   name        iggytex/iggyted: the texture id as text ("14", the id the
    //               .iggy uses); emb: the entry's name ("FRZ_000.dds"), or
    //               "#<index>" in an EMB without names ("#<count>" adds one)
    //   data, size  the image (a DDS for iggytex; copied)
    //   order       lower runs earlier (0 = usual); the file's first call sets it
    // Returns 1 when registered, 0 when refused (reason set).
    int (*SetImage)(const char* owner, const char* path, const char* name, const void* data, size_t size, int order,
                    char* reason, int reasonBytes) = nullptr;

    // Drops one of `owner`'s images of `path`. Returns 1 when there was one.
    // Null with an older library.
    int (*RemoveImage)(const char* owner, const char* path, const char* name) = nullptr;

    // Sets the text of one entry of the .msg file at `path` (its first line;
    // other lines and the line's parameter count stay), or adds the entry
    // when the file has none by that name; your other entries of the file
    // stay, and other mods' entries of it apply too (the same entry from two
    // mods: the later wins, noted in loader.log). Null with an older library.
    //   path        "msg/lobby_text_en.msg" - one call per language file
    //   name        the entry's name ("name_4293"; the game looks text up by
    //               name, not id); plain ASCII
    //   text        plain UTF-8 ("Future's Doom"; ' " & < > are stored as the
    //               game's entities); copied
    //   order       lower runs earlier (0 = usual); the file's first call sets it
    // Returns 1 when registered, 0 when refused (reason set).
    int (*SetMessage)(const char* owner, const char* path, const char* name, const char* text, int order, char* reason,
                      int reasonBytes) = nullptr;

    // Drops one of `owner`'s entries of `path`. Returns 1 when there was one.
    // Null with an older library.
    int (*RemoveMessage)(const char* owner, const char* path, const char* name) = nullptr;

    // Edits the display list (the sprites' placements) of the .iggy menu at
    // `path`. Your commands replace your earlier ones for the path (they are
    // independent of EditScript's script edit); other mods' edits apply too.
    // Null with an older library.
    //   commands  text, one command per line, '#' starts a comment:
    //     clone  <sprite> <source> <name> <dx> <dy>
    //         adds a placement <name> to <sprite>: a copy of everything placed
    //         at <source>'s depth (with its later moves), shifted by (dx, dy),
    //         on a new depth. Nothing happens when <sprite> already has a
    //         placement <name> (so it can be registered every start).
    //     matrix <sprite> <target> <frame> <a> <b> <c> <d> <tx> <ty>
    //         sets <target>'s matrix in <frame> of <sprite>; '*' keeps a value.
    //     <sprite>: the instance name a placement gives the sprite
    //     ("edit_menu"; all placements of that name must be the same sprite)
    //     or "#<character id>"; <source>/<target>: a placement name inside
    //     the sprite, or "@<depth>"; <frame>: 1-based number or frame label.
    //     Refused: bad syntax (reason set). A command that can't be applied to
    //     the menu is reported in XV2Files::Status when the menu loads.
    //   order     lower runs earlier (0 = usual)
    // Returns 1 when registered, 0 when refused (reason set). Remove drops it.
    int (*EditDisplay)(const char* owner, const char* path, const char* commands, int order, char* reason,
                       int reasonBytes) = nullptr;

    // Finds the functions. False without the library (or an older one).
    bool Load()
    {
        HMODULE library = GetModuleHandleW(L"XV2UserInterfaceLib.dll");
        if (!library) return false;
        bool found = true;
        auto get = [&](auto& function, const char* name) {
            function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(GetProcAddress(library, name));
            found = found && function;
        };
        get(EditScript, "XV2UI_EditScript");
        get(Remove, "XV2UI_Remove");
        // optional: not counted in `found`, so older libraries still load
        SetImage = reinterpret_cast<decltype(SetImage)>(GetProcAddress(library, "XV2UI_SetImage"));
        RemoveImage = reinterpret_cast<decltype(RemoveImage)>(GetProcAddress(library, "XV2UI_RemoveImage"));
        SetMessage = reinterpret_cast<decltype(SetMessage)>(GetProcAddress(library, "XV2UI_SetMessage"));
        RemoveMessage = reinterpret_cast<decltype(RemoveMessage)>(GetProcAddress(library, "XV2UI_RemoveMessage"));
        EditDisplay = reinterpret_cast<decltype(EditDisplay)>(GetProcAddress(library, "XV2UI_EditDisplay"));
        return found;
    }
};

# Chara Search

Search and favorites for the character select grid in Dragon Ball Xenoverse 2.
Type part of a character's name or code to filter the grid. Favorites are
listed right after your own CaC.

A Lua script for [XV2 Mod Loader](../XV2ModLoader) (game version 1.26.01).

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first (a version with the
`menus` table). Put the `CharaSearch` folder from the download in the
game's `XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            CharaSearch\
                chara_search.lua
                CharaSele.abc
                CharaSele_XV2Patcher.abc
```

Coming from the older DLL version: close the game first and delete
`CharaSearch.dll` and `chara_search.json` from that folder (or run
`Build_Install.bat`, which does it). Your favorites are kept.

To remove it, delete the `CharaSearch` folder (your favorites are kept in
it, so keep `favorite_characters.sav` if you may come back).

## Using it

* **Searching:** on the character select, the loader's search box sits
  above the grid. Click it or press Ctrl+F, then type part of a name
  ("Goku") or a character code ("GOK"); "CaC" finds created characters.
  The grid shows only the matches. If nothing matches, it shows everything
  with the note "No character matches".
* **Favorites:** press F or click the left stick on a character to mark it
  as a favorite. Do it again to unmark it. The grid opens with the loaded
  CaC first, then your favorites, then everyone else.
* Favorites are kept per Steam account in
  `XV2ModLoader\mods\CharaSearch\favorite_characters.sav`. Favorites saved by
  older Battle Styles versions are copied over on the first start.
* Works with the character select file xv2ins installs (XV2Patcher 4.7), and
  alongside [Battle Styles](../BattleStyles).
* `chara_search.json` (written next to the script on the first start):
  `"debug_menu": true` shows its state under "CharaSearch" in the loader's
  `~` menu (off by default). F1 in game reloads it.

## Building

Nothing to compile. `Build_Install.bat` copies the script and the two menu
scripts (`iggy\*.abc`) into the game (found through Steam, or set
`XV2_GAME` to the game folder); `Build_Uninstall.bat` removes them again.
The `.abc` files are the compiled `as3\` sources.

## Credits

* [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and
  contributors (MIT licence): draws the search box, through the mod loader.
* [Lua](https://www.lua.org) by PUC-Rio (MIT licence): runs the script,
  through the mod loader.
* [XV2-Tools / Xv2CoreLib](https://github.com/LazyBone152/XV2-Tools) by
  LazyBone: the file layouts this mod reads (MSG and the save file's Steam
  id). Game files are read through the mod loader, whose CPK reader follows
  Xv2CoreLib's.
* [XV2Patcher / xv2ins](https://github.com/eterniti/xv2patcher) by Eternity:
  the mod also handles the character select menu file xv2ins installs.
* [JPEXS Free Flash Decompiler](https://github.com/jindrapetrik/jpexs-decompiler)
  (FFDec, GPL v3) decompiled and recompiled the character select's
  ActionScript.
* Everything else (the menu's callback and user data, the grid's layout) was
  found by reverse engineering the game.

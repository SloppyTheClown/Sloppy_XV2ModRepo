# Item Search

A search box over Dragon Ball Xenoverse 2's item lists (equipment screen:
torso, Super Soul, ...). Type part of an item's name and the list shows only
the items whose name contains it (upper/lower case doesn't matter).

Game version 1.26.01. Needs a current [XV2 Mod Loader](../XV2ModLoader)
(one with the `files` and `search` functions).

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `ItemSearch` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            ItemSearch\
                item_search.lua
                ItemList.abc           <- the item list menu's new script
```

No game file is replaced: the loader swaps the script into the game's own
item list menu as the game reads it. If another mod has its own
`ITEMLIST.iggy`, it is left alone: the search still filters, but the list
only shows the result when it is opened again.

To remove it, delete the `ItemSearch` folder (it also holds your
favorites, `favorites.sav`).

## Using it

Open an equipment list (Customize -> Change Equipment -> pick a slot). The
box appears above the list: click it (or press Ctrl+F) and type. Enter or a
click elsewhere gives the keys back to the list. The search stays while the
equipment screen is open and applies to every list opened there; clear the
box to see everything again. [Battle Styles](../BattleStyles)
uses the same box for its style list.

### Favorites

In the equipment and skill lists, press **F** or click the **left stick**
on an item to mark it as a favorite (again to unmark it). Favorites get a
gold star and sit at the top of the list, right after "None", whenever it
opens; the search filters them like everything else. They are kept per
Steam account in `XV2ModLoader\mods\ItemSearch\favorites.sav` (your game
save is not changed).

### Settings

`item_search.json` is written next to the script on the first start.
`"debug_menu": true` shows the search's state in the loader's `~` menu
(off by default). F1 in game reloads it.

## Building

Nothing to build. `Build_Install.bat` copies the script into the game
(found through Steam, or set `XV2_GAME` to the game folder);
`Build_Uninstall.bat` removes it again.

## Credits

* [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and
  contributors (MIT licence): draws the search box, through the mod loader.
* The script uses [LuaBackend](https://github.com/Sirius902/LuaBackend)'s
  script API by Sirius902 and Cheat Engine's Lua function names, as the
  loader provides them.
* [JPEXS Free Flash Decompiler](https://github.com/jindrapetrik/jpexs-decompiler)
  (FFDec, GPL v3) decompiled and recompiled the item list menu's script.
* The item list's structure and the menu script's protocol were found by
  reverse engineering the game.

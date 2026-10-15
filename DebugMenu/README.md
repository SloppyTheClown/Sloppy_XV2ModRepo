# Debug Menu

Adds a "Debug Menu" section to XV2 Mod Loader's in-game menu for Dragon Ball
Xenoverse 2 (game version 1.26.01): noclip and an item getter.

---

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `DebugMenu` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            DebugMenu\
                debug_menu.lua
```

This mod is a Lua script that the loader runs; nothing else needs to be
installed. To switch the menu off without removing it, rename the folder to
`_DebugMenu`.

To remove it, delete the `DebugMenu` folder.

## Using it

* **~** (the key left of 1) opens and closes the menu.
* **F1** reloads scripts, so a new version can be installed while the game
  is running.

### Item getter

Gives items and unlocks skills for the save that is loaded. The list is
read from the game's own files when the getter first shows (about a
second), so items and skills added by x2m mods or Revamp are in it; F1
reads it again after installing more.

* The first dropdown picks a group (tops, bottoms, gloves, shoes,
  accessories, super souls, materials, important items, capsules, super /
  ultimate / evasive skills, awoken skills (transformations)) or All.
* Type in the search box to filter by name or item ID, then pick the item
  in the second dropdown.
* Items: **+1**, **+10** and **+Max** add that many, never past the game's
  limit (99, important items 1). The menu shows how many you have.
* Skills: **Unlock** / **Lock**.
* Items and skills from a DLC you don't own can't be given or unlocked; the
  menu says "Needs DLC pack N - not owned" instead.

The game keeps the change with its next save (inventory and skills are
shared by every character slot of the save).

### Noclip

Tick **Noclip** to stop colliding with anything: walls, floors, ceilings,
other characters. You keep your normal controls (walk, run, fly). Works in
the lobby and in battle (Training and quests), and stays on through loading
screens until you untick it.

* You can leave the arena and the map; the game doesn't freeze you when you
  go past its edge.
* In the lobby the camera passes through walls too.
* The menu shows where you are (lobby or battle) and your collision layer
  (63 while noclip is on).

Untick it to collide again. Collision comes back once you **move**; the menu
says "move to turn collision back on" until then. If you untick it while
inside a wall, you stay stuck in that wall: tick it again, move out, then
untick.

### Camera

The menu also shows the current camera's position and the point it looks at.

---

## What it touches

* **Game memory:** while noclip is on, the player's collision layer and the
  physics world's collision table (put back when you untick it). The first
  time noclip is ticked it also changes the game's code in memory so a body
  on the noclip layer isn't frozen when it leaves the map's physics area;
  nothing else uses that layer. The game files on disk are never changed;
  everything is gone when the game closes.
* **Game memory (item getter):** the loaded save's inventory and skill
  list, changed through the game's own give-item and unlock functions;
  the game saves them with its next save.
* **Files:** none written. The game's data is only read.
* **Network:** none.

---

## Building

Nothing to build. `Build_Install.bat` copies the script into the game;
`Build_Release.bat` writes `DebugMenu.zip`. The game is found through
Steam, or set `XV2_GAME` to the game folder.

## Credits

* [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and
  contributors (MIT licence): draws the menu, through the mod loader.
* The script uses [LuaBackend](https://github.com/Sirius902/LuaBackend)'s
  script API (`_OnFrame`, `ReadLong`, `WriteExec`, ...), by Sirius902, as the
  loader provides it.
* Item getter: the save's inventory and skill layout from
  [Xv2CoreLib](https://github.com/LazyBone152/XV2-Tools) (`SAV_File.cs`) by
  LazyBone; the in-memory lists and the give/unlock functions were found by
  reverse engineering the game. The item list's IDB, CUS and msg
  formats are read after Xv2CoreLib.
* Physics: the game uses Havok Physics (hknp); the player's body, the
  collision filter and the freeze when leaving the map were found by
  reverse engineering the game.

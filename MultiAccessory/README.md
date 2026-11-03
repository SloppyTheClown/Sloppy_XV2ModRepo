# Multi Accessory

**Version 1.1.0** · for Dragon Ball Xenoverse 2 **1.27** · needs [XV2 Mod Loader](../XV2ModLoader/README.md) 1.1.0 or newer

An XV2 Mod Loader mod that lets a created character (CaC) wear up to two
extra accessories on top of the one equipped in the game, for example a hat
and glasses together. Pick them in the equip screen: the Accessory row has
two more boxes, each opening the accessory list. The extras show in the
equip screen, save select, character select, Conton City, battles and
cutscenes.

Online, in a room, players who also have the mod see your extras in the
room and in battle; players without it see your equipped accessory alone.

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `MultiAccessory` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            MultiAccessory\
                MultiAccessory.dll
                multi_accessory.json
```

To remove it, delete the `MultiAccessory` folder.

## Picking extras

In the equip screen's clothing tab, the Accessory row has three boxes: the
game's own accessory and two extras. Each extra box opens the accessory
list to pick an accessory for it. Picks are
saved per save slot and per preset in `multi_accessory.sav` next to the
DLL.

Saving or loading a preset in the lobby's Presets panel saves or loads its
extras too, and the panel lists them as "Accessory 2" and "Accessory 3".

## Settings

`multi_accessory.json` holds default extras, used for a save slot's preset
until something is picked in the equip screen:

```json
"extra_accessories": {
    "all": [],
    "slot_1": [835, 846],
    ...
}
```

* `slot_1` .. `slot_8`: the extras of the CaC in that save slot (in the
  order of the save select screen).
* `all`: extras for every CaC; they come before the slot's own.
* Each number is an accessory's **part set id**: the `Model` value of its
  entry in `system\item\accessory_item.idb` (for example 835 is a hat, 846
  glasses). At most two extras per CaC are used.
* `logging`: `true` writes `MultiAccessory.log` next to the DLL.

Restart the game after changing the file.

## Which accessories can be combined

* **Attached objects** (glasses, scouters, noses) combine with anything.
  A character holds at most 4 of their pieces in total; more are not drawn.
* **Headwear** (hats, wigs, helmets) is drawn on top of what the character
  already wears: two hats, a hat over a wig, a helmet with a hat all show.
  An extra never hides any part of the character. An extra identical to a
  part already drawn is skipped, and so are placeholder parts that only
  exist to hide the hair (for example a bald cap under a hat).
* Some transformations draw their own hair over an extra hat (Super
  Saiyan 3).
* An id the character's race doesn't have is ignored.

## Building

Needs CMake and Visual Studio 2022. Double-click `Build_Install.bat`
(build and install, with the game closed) or `Build_Uninstall.bat` (remove
the DLL, keep the settings).

## Credits

* How the game loads and draws accessories was found by reverse
  engineering the game.
* Save layout (CaC block offsets) cross-checked with Xv2CoreLib's
  `SAV_File.cs` from LazyBone's XV2-Tools
  (https://github.com/LazyBone152/XV2-Tools, MIT).
* The equip screen script is the game's AVATAR.iggy ActionScript,
  decompiled and recompiled with JPEXS Free Flash Decompiler (FFDec,
  https://github.com/jindrapetrik/jpexs-decompiler, GPLv3) and merged with
  other mods' edits by XV2 Mod Loader's UI library.

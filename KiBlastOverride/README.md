# Ki Blast Override

**Version 0.1.0** · for Dragon Ball Xenoverse 2 **1.27** · needs [XV2 Mod Loader](../XV2ModLoader/README.md) 1.2.0 or newer

An XV2 Mod Loader mod that lets a created character (CaC) choose the charged
ki blast. Customize > Skills gets a **Ki Blast Override** row under the
skills; it opens a list of the ki blast skills the game has loaded (the five
standard types, the special ones and any added by other mods; roster blasts that
are only a copy of a standard type are left out). Picking a blast means the super
soul no longer changes it. **Default** keeps the game's behaviour (the super
soul decides); **Race Default** keeps the blast of the character's race
whatever the soul. Transformations still swap the blast in battle, unless
`override_transformation_blasts` is on.

The choice is saved per Steam account, save slot and loadout preset (the
Presets panel copies it along with the rest of the loadout, and shows it, as does
the character select's skill popup). Online, everyone
in the match needs the mod: a player without it sees your super soul's blast.

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `KiBlastOverride` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            KiBlastOverride\
                KiBlastOverride.dll
                ki_blast_override.json
```

To remove it, delete the `KiBlastOverride` folder. Your picks are in
`equipped_blasts.sav` next to the DLL. Animations it converted stay in the
game's `data\skill\BLT` folders, listed in `converted_eans.txt`; they do nothing
without the mod, and those are safe to delete too.

## Settings

`ki_blast_override.json`:

* `"blast_search": true` adds a search box to the blast list.
* `"override_transformation_blasts": false`: a transformation that has its own
  ki blast replaces yours while it lasts. `true` keeps the blast you picked instead.
  A character on Default or Race Default, or with a pick the same as its soul's
  blast, always changes. Everyone in an online match needs the same setting.
* `"logging": true` writes `ki_blast_override.log` (the last run's log is kept as
  `ki_blast_override.prev.log`).

## Notes

* A blast with animations only for its owner, from the game or a mod, gets
  versions for CaCs converted automatically when the list first opens. They are
  loose files in the blast's `data\skill\BLT` folder; the owner keeps its own.
* Every player's game does this for itself; it only changes how the blast looks.
* The detail window of the equip screen still shows the blast type of the
  equipped super soul, not the one you picked.
* Tournaments still use the super soul's blast.
* Use Battle Styles 1.1.1 or newer with it; older versions gray out their row
  for a few minutes after the game starts.
* Works with Battle Styles and Multi Accessory in the same Customize screen
  (the menu edits are merged by XV2 Mod Loader's UI library).

## Building

Needs CMake and Visual Studio 2022. Double-click `Build_Install.bat`
(build and install, with the game closed) or `Build_Uninstall.bat` (remove
the DLL, keep the settings).

## Credits

* Where the game picks the ki blast, and how the Customize menu and its
  lists work, were found by reverse engineering the game.
* File layouts (CUS, CMS, MSG, save) cross-checked with Xv2CoreLib from
  LazyBone's XV2-Tools (https://github.com/LazyBone152/XV2-Tools, MIT).
* Mod blast names are read from the files LazyBone's XV2 Mods Installer leaves
  in the game's data folder. The zip and deflate reader is our own, written
  from PKWARE's APPNOTE and RFC 1951.
* The equip screen script is the game's AVATAR.iggy ActionScript,
  decompiled and recompiled with JPEXS Free Flash Decompiler (FFDec,
  https://github.com/jindrapetrik/jpexs-decompiler, GPLv3) and merged with
  other mods' edits by XV2 Mod Loader's UI library.

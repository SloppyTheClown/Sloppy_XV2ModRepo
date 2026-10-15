# Battle Styles

Equipable movesets for created characters in Dragon Ball Xenoverse 2. A
"Battle Style" changes a CaC's basic moves (attack strings, throw, guard,
dashes, idle stance and their camera) to those of a roster character. Skills,
supers and ultimates stay as they are.

Game version 1.26.01. Needs [XV2 Mod Loader](../XV2ModLoader).

## Installing

Needs XV2 Mod Loader installed first. Close the game, then put the
`BattleStyles` folder from the download in the game's `XV2ModLoader\mods`
folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            BattleStyles\
                BattleStyles.dll
                battle_styles.json                          <- its settings
                Convert_Moveset (Drag installers here).bat  <- optional, see below
                tools\                                      <- used by the converter
```

To remove it, delete the `BattleStyles` folder (it also holds your equipped
styles) and the game's `data\battle_styles` folder.

## Using it

* **Equipping:** Customize -> equipment tab has a **Battle Style** button
  under Super Soul. It opens a list of styles, sorted by name with the race
  movesets ("Human M Default", ...) at the end; pick one, or **Default** for
  the character's own moveset. By default it lists the characters you have
  unlocked in the character select (setting `styles`).
* **Style names** follow the character select: a character's default
  costume's moveset has the character's name; a moveset only some of its
  costumes use adds that costume's name, e.g. "Trunks (Future)" and
  "Trunks (Future) (Capsule Corp. Clothes 3)".
* **Searching:** while the style list is open, the loader's search box sits
  above it (click it or press Ctrl+F, type part of a name). The list shows
  the matching styles, plus Default and the equipped one. With
  [Item Search](../ItemSearch) installed it is the same box as the
  item lists', so one search covers both.
* **Favorites:** press F or click the left stick on a style to mark it as a
  favorite (again to unmark it). Favorites get a gold star and sit right
  after Default whenever the list opens; the search filters them too. They
  are kept per Steam account in
  `XV2ModLoader\mods\BattleStyles\favorite_styles.sav`.
* **Saved per character and per preset:** each CaC slot keeps its own style,
  and saving or loading a loadout preset saves or loads its style too. The
  style shows in the lobby Presets panel and in the character select skill
  popup, and the character select preview uses it.
* **Online:** other players see your style and fight against it (1v1 and
  Parallel Quests). They need the mod installed too.
* **Your save is never changed.** Styles are kept in
  `XV2ModLoader\mods\BattleStyles\equipped_styles.sav`, per Steam account
  (read from the loaded save), so the game's save works with or without the
  mod.
* **Fit to race:** a borrowed moveset is fitted to the CaC's leg length, so a
  short race's feet stay on the ground in a tall character's stance.
* **Props:** a style whose character holds something brings it along -
  Whis's, Vados's and Demigra's canes, Fu's, Future Trunks's, Trunks (Super
  Saiyan Rage)'s and Tapion's swords, Galena's and Towa's spears, Goku (Mini)'s Power
  Pole (it extends in attacks), Gamma's guns, Jaco's gun. Shown in the
  character select preview and in battle, not in the lobby. Made the first
  time a style is shown each session.
* **Moveset mods as styles:** a moveset mod can be added as an extra style
  instead of replacing a race's moveset. Drag the mod onto
  `XV2ModLoader\mods\BattleStyles\Convert_Moveset (Drag installers here).bat`
  (or double-click it and pick the mod's folder). It takes:
  * an LB Mod Installer mod (its exe or its folder),
  * loose moveset files (e.g. `MAF.ean`, `MAF_PLAYER.bac`) or `data` folders,
  * an `.x2m` character - only its moveset is used, with the character's
    own effects and sounds,
  * any of these as a downloaded `.rar`, `.zip` or `.7z`, unpacked or not.

  It writes `<name> Installer (Battle Styles Compatible).exe` next to the
  mod and starts it; install from there. The style shows in the list under
  the mod's name (download ids, race codes and versions are left out of it),
  on every race the mod supports, or on every race for a character's
  moveset. To remove it, run that exe again and uninstall. Problems are
  written to `Convert_Moveset.log` next to the .bat. Mods with only a
  costume or model are refused.
* **Character select search and favorites** are a separate mod,
  [Chara Search](../CharaSearch). Both can be installed together.

## Settings (`battle_styles.json`)

Written next to the DLL on the first start; read when the game starts.

| Key | Default | What it does |
|---|---|---|
| `enabled` | `true` | `false` turns the mod off |
| `logging` | `false` | writes `battle_styles.log` next to the DLL |
| `styles` | `"roster"` | which styles the list offers: `"roster"` characters unlocked in your character select (plus the race movesets), `"unlocked"` every character in the character select, unlocked or not (DLC you don't own still left out), `"valid"` every roster moveset a CaC can fight with, `"all"` every roster moveset, or a list of character codes (`["GOK", "VEG"]`) in the order to show them |
| `exclude_styles` | `["fake", "giants"]` | movesets never offered: `"fake"` (NPCs without attacks), `"giants"` (Great Apes, Cell Max...), `"race"` (the race movesets) or character codes |
| `style_search` | `true` | `false` turns the style list's search off |
| `style_names` | MAP, D00-D02 | names for styles by character code, over the automatic ones: `{"D00": "My Name"}`. Use it for installed characters whose name is missing or wrong; a style with no name shows its code |

## Known issues

* Giant-form and some NPC-only movesets don't work on a CaC; they are left
  out of the list unless you take them out of `exclude_styles`.
* Deleting a CaC doesn't clear its equipped style.

## Building

Needs Visual Studio 2022 (C++) and CMake. `Build_Install.bat` builds the
mod and copies it into the game (found through Steam, or set `XV2_GAME` to
the game folder); `Build_Release.bat` writes `BattleStyles.zip`;
`Build_Uninstall.bat` removes it again (settings and equipped styles are
kept).

## Credits

* [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and
  contributors (MIT licence): draws the search box, through the mod loader.
* Reference: the "EquipableMovesets" mod for game 1.25.02
  (`EquipableMovesets_1.25.2`, by DemonBoy going by its patch file's
  name). This mod is its own design and code, but its patch showed where
  the game builds a character's moveset paths and that the battle setup is
  what goes online.
* [XV2-Tools / Xv2CoreLib](https://github.com/LazyBone152/XV2-Tools) by
  LazyBone: the file layouts this mod reads (CMS, EAN/ESK, EMD, BCS, MSG, BAC, BDM,
  the CRI ACB / @UTF / AFS2 sound banks and CSO it merges, the character
  select table CST, and the save file's encryption and Steam id). Game
  files are read through the mod loader, whose CPK reader follows Xv2CoreLib's.
* [XV2Patcher / xv2ins](https://github.com/eterniti/xv2patcher) by Eternity:
  the mod also handles the character select menu file xv2ins installs, and
  reads XV2Patcher's slot list `XV2P_SLOTS.x2s` (eternity_common
  Xv2PatcherSlotsFile) to name the styles.
* [JPEXS Free Flash Decompiler](https://github.com/jindrapetrik/jpexs-decompiler)
  (FFDec, GPL v3) decompiled and recompiled the menus' ActionScript
  (equipment screen, character select, presets panel, style list).
* [LB Mod Installer](https://github.com/LazyBone152/XV2-Tools) by LazyBone
  (in XV2-Tools): the converter reads its installer format
  (`.installinfo`, InstallerXml bindings) and reuses the mod's own
  installer exe for the converted style. LB Mod Installer 3.32 (MIT) is
  included in `tools\lb_installer` for mods that bring no installer.
* [eternity_common / xv2ins](https://github.com/eterniti/eternity_common)
  by eterniti: the x2m format (`x2m.xml` character, CMS and CSO entries)
  the converter reads.
* The converter (`tools\moveset_to_style.exe`) is a Python script with
  [Python](https://www.python.org/) (PSF License, included as
  `moveset_to_style.PYTHON_LICENSE.txt`) bundled by
  [PyInstaller](https://pyinstaller.org/) (GPL 2 with the bootloader
  exception); versions in `tools\moveset_to_style.VERSION.txt`.
* Windows' `tar.exe` ([libarchive](https://www.libarchive.org/)) unpacks
  `.rar` / `.7z` downloads ([7-Zip](https://www.7-zip.org/) if installed).
* Mods the converter was built and checked against (none included):
  "Gun Fu Rush" by SSaYnt; Super Hero, Playful Brawler, Kick Tempest and
  Muay Thai race moveset mods; "Dark King Fu (Ultra Supervillain)" by
  CyberBlade117 and "Kaido" by TGFB x2m characters.
* Everything else (the moveset path hook, the battle and online setup
  records, the menus' data) was found by reverse engineering the game.

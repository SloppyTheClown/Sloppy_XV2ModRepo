# Revamp Bridge

**Version 1.0.0** · for Dragon Ball Xenoverse 2 **1.27** (also 1.26.01) with XV2Patcher 4.7

Installs **Revamp Xenoverse 2 5.1.1** (made for game 1.25.02) on game 1.27.
Revamp's own installer is run as it is; the bridge makes it work on today's
game and fixes what the game updates broke.

**Only for Revamp 5.1.1** - it does not work with any other Revamp version.
The release does not contain Revamp. It needs the Revamp download,
`Revamp Xenoverse 2 - 5.1.1.zip`, and builds everything it needs of Revamp's
from it on your PC.

> **Disclaimer:** Revamp Bridge is an unofficial converter, not made,
> supported or endorsed by the Revamp Xenoverse 2 team. If you run into
> problems with Revamp installed through it, **do not** report them to the
> Revamp team - open an issue here instead.

## Install Revamp

1. Follow Revamp's own pre-install steps (its `README.txt` in the zip):
   - start from a clean installation - uninstall your other mods first
     (Revamp's installer does not do this for you); reinstall them afterwards
   - run `xv2ins.exe` once if you never have (Revamp needs the files it creates)
   - for crashes after finishing a quest: LazyBone's XV2 Save Editor,
     Tools > *Remove Unused IDs*, then *Validate Flags (Experimental)*, save
2. Close the game. Extract the release zip anywhere you can write to (not
   Program Files) and put `Revamp Xenoverse 2 - 5.1.1.zip` in the
   `Revamp Bridge (Revamp 5.1.1 to XV2 1.27)` folder, next to
   `Revamp Bridge.exe` - either the zip itself or its extracted contents.
3. Run `Revamp Bridge.exe`. From the zip, the first run extracts Revamp's
   installer (about 5 GB, into `Revamp Bridge Files`); an extracted download
   is used where it is.
4. Use the Revamp installer as usual. When it closes, a second window lists
   the game update fixes that were applied.

Uninstall the same way (choose *Uninstall* in the Revamp installer); the bridge
removes the files it added. If the fixes window reports an error, fix the
cause and run `Reapply Fixes Only.bat`. The text file in the release folder
has the details.

## Why the Revamp installer fails on 1.26 and later

Its built-in file library (a fork of LazyBone's Xv2CoreLib) cannot read files
that changed after 1.25.02: item lists (`system\item\*.idb`, entries grew to
784 bytes), quest data (`quest\*\*.qxd`), the character select table
(`chara_select_table.cst`) and XV2Patcher 4.7's `XV2P_SLOTS.x2s` (11 fields per
slot). It stops with "IDB version not supported" before changing anything.

## What the bridge does

* **Runs the original installer** with Revamp's own library, rebuilt with the
  IDB, QXD, CST and character-slot readers of a current LB Mod Installer. The
  rest of the library is Revamp's, so its installer behaves exactly as it was
  written.
* **Ports Revamp's exe patches** (`RevampDynamicPartsets.xml`,
  `RevampExtra.xml`) to the game build it finds (1.27 or 1.26.01). As shipped
  they point at 1.25.02 code addresses and would crash the game.
* **Rebuilds `data\rvp.cpk`**. Revamp makes the game load it instead of
  `cpk\data2.cpk`; Revamp's copy carries 1.25.02's online config and system
  font. The rebuilt one has this game version's data2 files plus Revamp's
  intro movie and demo file.
* **Adds the new character image table** (`chara_image_TU22.hci`). The game
  now reads TU22; Revamp only writes TU21. TU22 is Revamp's table plus the
  entries the updates added.
* **Renumbers menu textures.** 1.26 renumbered the textures of several menus.
  Revamp's textures for them (title screen, character select, battle menu,
  the shared icon/font file, lobby HUD) are moved to the game's numbering.
  Without this those screens show corrupted graphics. The id maps are in
  `src\iggymaps_<build>.txt`.
* **Adds the text the updates added.** Revamp's text files (`data\msg`) are
  complete 1.25 copies, so newer characters had no names ("Unknown
  Character"). Every entry the game's file has and Revamp's lacks is appended;
  Revamp's own entries are kept.
* **Fixes "%ls" in text.** Revamp's text files lose the marker that tells the
  game to fill in a value, so lobby messages showed "%ls" instead of a
  number or item name ("You got TP Medals x %ls!"). The markers are copied
  back from the game's files.
* **Restores the lobby skill emotes.** Revamp's lobby action files for HUM,
  HUF, MAM, NMC and SYF lack the skill emotes (Kamehameha, Special Beam
  Cannon, ...); using one in an online lobby froze the game. The missing
  entries are copied from the game's files; Revamp's own are kept.
* **Vanilla lobby flight speed.** Revamp makes flying in the lobby 1.4x
  faster, which made boost flight (L2) in Conton City stop for a moment
  every few seconds. The bridge puts the game's own flight speed back.

## How the release stays free of Revamp's files

Two parts of the bridge are made from Revamp's files: the installer library
(`lib\`) and the ported exe patches (`epatches\`). `Revamp Bridge.exe` carries
binary patches (bsdiff) from Revamp's files to the bridge's instead, and on the
first run rebuilds them from your download: it extracts the zip's `Installer`
folder (or uses the one you extracted), takes the libraries embedded in Revamp's installer, applies the
patches and checks every result by SHA-256. Then it runs a copy of itself
named `Revamp Xenoverse 2 Installer.exe` (Revamp's installer opens the
`.installinfo` named after the running program).

## Known overlaps with newer game content

Revamp installs these at fixed IDs that 1.26 now also uses; Revamp's version
wins:

| File | ID | 1.26 added |
|---|---|---|
| `vfx\cmn\BTL_CMN.eepk` | effect 54 | a new common effect |
| `system\custom_skill.cus` | skillset VTB (141) costume 0 preset 1 | that preset (Revamp's optional roster expansion adds it too) |
| `chara\GKS\GKS.bcs` | part set 1 | a new part set |
| `system\chara_custom\MenuCostumeCustomList.oco` | partner 7 option 1, partner 24 option 3 | new partner costume options |

1.27 additionally: costume tops 551, 553-555 (Revamp's own tops), HUM.bcs part
set 981, Common.bev 188, and partner costume options (OCO partner 7 option 2,
OCC partners 56, 67, 101, 102).

## Build

Needs the .NET SDK, the .NET Framework 4.7 targeting pack, Python 3 with
`capstone` (and `bsdiff4` for a release: `pip install capstone bsdiff4`) and
ilspycmd:

    dotnet tool install -g ilspycmd --version 9.1.0.7988

Copy `RevampBridge.example.ini` to `RevampBridge.ini` and set:

* `RevampDir`: the Revamp `Installer` folder (contains
  `Revamp Xenoverse 2 Installer.exe` and its `.installinfo`)
* `LibDonor`: any LB Mod Installer 3.32 or newer (e.g. another mod's
  installer exe) - its library supplies the newer file readers

Then run `build.bat` (or the `Build_*.bat` wrappers):

    build.bat            build (the library only if missing)
    build.bat lib        rebuild the library too
    build.bat clean      rebuild everything
    build.bat release    build, then the release zip in ..\..\_Releases

`build.bat` decompiles both libraries, swaps in the new readers, compiles, and
fails if any attribute of the result differs from the originals. The build
folder's `Revamp Xenoverse 2 Installer.exe` is the bridge without the release
step: it runs the installer from `RevampDir` directly.

    "build\Revamp Xenoverse 2 Installer.exe" [--revamp <Installer folder>] [--game <game folder>]
    "build\Revamp Xenoverse 2 Installer.exe" --fix-only [--game <game folder>]

## Tools

For porting to a future game version:

| Tool | Does |
|---|---|
| `tools\dumpimage.py` | starts a DBXV2.exe and saves its decrypted image |
| `tools\epatch_port.py`, `codematch.py`, `sidebyside.py`, `globalmap.py`, `dumpfn.py` | find hook sites and data between two images |
| `tools\verify_epatches.py` | applies Epatches to an image like XV2Patcher and checks every jump |
| `tools\idcollide.cs` | lists a mod's fixed IDs that an update now uses |
| `tools\inspect.cs` | prints vanilla entries from a game folder |
| `tools\changedvanilla.py` | lists loose `data\` files whose game (cpk) version changed between two game versions |
| `tools\iggypairs.py` | finds menu texture files that no longer match their menu after an update; writes the iggymaps |
| `tools\iggytexdiff.py` | compares a mod's .iggytex with the game's in two versions, texture by texture |
| `tools\iggyids.py` | tells which game version's texture numbering a menu (.iggy) uses |
| `tools\exeiggy.py` | extracts the menus (.iggy) built into a program, e.g. xv2ins.exe |
| `tools\msgdiff.py` | lists text (.msg) entries a mod's file lacks compared with the game's |
| `tools\embdiff.py` | compares a mod's UI image file (.emb) with the game's in two versions, by name |
| `tools\ddspng.py` | DDS to PNG (stdlib only), side-by-side sheets |
| `tools\iggystate.py` | classifies loose menu files against the game's in two versions |
| `tools\cpkls.py` | lists a CPK's files |
| `tools\lbybacdiff.py`, `lbyflight.py` | lobby action file (BAC) compare / vanilla flight speed |

## Credits

* **Revamp Xenoverse 2** by its team - the mod this installs (not included;
  you need their download).
* **LB Mod Installer** and **Xv2CoreLib** by LazyBone
  ([XV2-Tools](https://github.com/LazyBone152/XV2-Tools)) - the installer Revamp's
  is built on, and the IDB/QXD/CST/character-slot readers the bridge swaps in.
* **XV2Patcher** by Eternity - the Epatch format the exe patches use.
* [ILSpy](https://github.com/icsharpcode/ILSpy) (`ilspycmd`, MIT) - decompiles
  the libraries at build time.
* [Capstone](https://www.capstone-engine.org/) (BSD) - disassembly for porting
  the exe patches.
* [bsdiff](https://www.daemonology.net/bsdiff/) by Colin Percival (BSD), via
  [bsdiff4](https://github.com/ilanschnell/bsdiff4) - the release's binary
  patches.
* Costura (Fody, MIT) - the embedded-library format read from the installer.
* Everything else (exe patch ports, texture maps, file fixes) was found by
  reverse engineering the game.

# SaveSync

Shares unlock progress between your created characters (CaCs) in Dragon Ball
Xenoverse 2. Turn SaveSync on for some save slots. Every synced slot then
gets everything any of them has unlocked, and the best score and rank on
every quest.

Game version 1.26.01. A mod for XV2ModLoader.

## Installing

1. Install XV2ModLoader first.
2. Close the game.
3. Extract `SaveSync.zip` into the game folder (the one containing `bin`).
   This creates `XV2ModLoader\mods\SaveSync\`.

To uninstall, delete `XV2ModLoader\mods\SaveSync\`.

## Using it

The save slot screen gets a "SaveSync" column next to the slot list, with
ON / OFF for every slot, empty ones included.

* Press RIGHT to move into the column, then ENTER to switch the selected
  slot ON or OFF. LEFT or ESC goes back to the slots.
* The toggles are stored per Steam account in
  `XV2ModLoader\mods\SaveSync\savesync.sav`.

Syncing happens only when you play a synced slot:

* **Loading a synced slot:** the slots that are ON are merged, then the game
  loads the slot you picked. The game saves the merged slots.
* **A new character in a synced slot:** after character creation, the first
  game save merges it with the others. The character still plays the intro,
  then has everything unlocked.

Slots that are OFF are never changed.

`savesync.json` in the same folder has `"enabled"` (false turns the mod off)
and `"logging"` (true writes what each sync changed to `savesync.log`).

## What gets synced

* Story and lobby flags (story progress, shops, mentor badges, X-ultimate
  talks, emotes, notices).
* Every quest list: story, parallel quests, time rifts, mentor lessons,
  expert missions, raids, Elder Kai, Frieza siege, Infinite History, and the
  newer story and parallel quests. For each quest the furthest state, best
  rank and best score are used.
* Mentor friendship, dual-attack gauge and mentor flags.
* Characters wished for from Shenron.
* Roster characters, their costume variants and stages, since those follow
  the story and quest progress.

Not synced: appearance, level, presets and play statistics. Items, clothes,
skills and Zeni are already shared by all CaCs in the save file.

## Without the mod: the offline script

`offline\savesync.py` (in the source, needs Python 3 and `pip install
cryptography`) does the same merge on the save file while the game is
closed. Slots are numbered 0-7 from the top of the save slot screen.

Show what syncing slots 0, 1 and 4 would change (writes nothing):

    python offline\savesync.py --slots 0,1,4

Sync them. The script first copies `DBXV2.sav` to
`DBXV2.sav.savesync-backup-<date>-<time>` next to it:

    python offline\savesync.py --slots 0,1,4 --write

Undo (puts the newest backup back):

    python offline\savesync.py --undo

`--save <path>` picks a save file. By default the script uses the save the
game loads, the newest `DBXV2.sav` under
`<Steam>\userdata\<your id>\323470\remote\DBXV21`. The game also writes a
copy under `%LOCALAPPDATA%\BANDAI NAMCO Entertainment`, but it does not read
that one.

## Credits

* [XV2-Tools / Xv2CoreLib](https://github.com/LazyBone152/XV2-Tools) by
  LazyBone: the save file layout and its encryption (`SAV_File.cs`,
  `Encryption.cs`, including the header checksums `offline\sav_crypto.py`
  writes).
* [JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler) (GPL-3.0):
  decompiling and rebuilding the save slot menu's script.
* The in-game addresses (save parsing, saving, the save slot menu) were
  found by reverse engineering the game.
* The flag names come from the game's `lobby_event_manage.tsd`, read with a
  port of Xv2CoreLib's TSD parser. Which data is per CaC and how it merges
  was found by comparing save slots.

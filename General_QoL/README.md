# General QoL

A mod for XV2 Mod Loader (Dragon Ball Xenoverse 2, game version 1.26.01).
Each feature can be switched on or off in `general_qol.json`; some start
off (see [Settings](#settings-general_qoljson)):

* **Transfer shop:** every Time Machine destination is unlocked, from the
  start.
* **Flight:** you can fly in Conton City from the start. The save's
  flight flag is left alone; the "you can fly now" scene still comes with
  the story. Flying in the lobby is also faster (setting
  `lobby_flight_speed`).
* **Story requirements:** quests that wait for part of the main story
  (parallel quests, expert missions, training) and the mentors are
  available from the start, and the shops sell every skill, super soul,
  clothing item and accessory from the start. So are the shops, quest and
  battle desks, the expert mission rifts, and the desks that wait for the
  prologue or the story: Advancement Tests, radio, Photo Mode, Crystal
  Raids, Info Robot, Kame House, Multiplayer Modes, voting and the Festival
  of Universes lobby's desks - without the parallel quest tutorial. The
  main story itself still plays in order.
* **Parallel quests:** every parallel quest is available, without clearing
  the ones before it.
* **Expert missions:** every expert mission is available, without clearing
  the ones before it, from the rift that lists them all.
* **Level cap:** the maximum level is 199 from the start, without the Elder
  Kai and Whis level cap events. This is saved for the whole save file, so
  every character on it keeps it.
* **TP medal shop:** the items that normally rotate with the seasons are all
  sold all year.
* **Raid characters:** the Master Raid bosses (Supreme Kai of Time,
  Corrupted Fused Zamasu, Demigra, Final Form Mira and Super Buu) are
  unlocked, without buying their gifts from the TP medal shop for Fu or
  playing the Master Raid tutorial. This is saved for the whole save file.
* **Online rooms wait for everyone:** pressing Ready no longer starts the
  15 second countdown that sends the room on; the room starts once every
  player is ready. Every player in the room needs the mod.
* **Transformation names in battle:** for awoken skills installed from an
  x2m (e.g. New Transformations), the battle HUD names the stage the next
  transform goes to, instead of always the last stage.
* **Boot:** the logo slideshow and the intro movie at startup are skipped,
  and the title screen plays at your in-game volume settings instead of
  full volume.
* **Camera zoom:** hold `-` to move the camera further away, `+` to bring
  it closer (main keyboard or keypad), `Backspace` to go back to the
  normal distance. Battle, lobby, menus (character select and creation)
  and cutscenes each keep their own zoom, and it is remembered between
  sessions.
* **DLC Master:** one NPC in the Time Nest (Hercule by default, standing
  where Dastar stands) lists the DLC story quests in story order: those the
  character's DLC content allows and has reached. He replaces the
  replay robots Brashy, Spots and Culina, and Dastar.

## Installing

Needs [XV2 Mod Loader](../XV2ModLoader) installed first. Close the game,
then put the `General_QoL` folder from the download in the game's
`XV2ModLoader\mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            General_QoL\
                General_QoL.dll
                general_qol.json       <- its settings
```

To remove it, delete the `General_QoL` folder.

## Settings (`general_qol.json`)

| Setting | Default | Meaning |
|---|---|---|
| `enabled` | `true` | `false` turns the whole mod off |
| `unlock_teleports` | `false` | unlock every transfer shop destination |
| `unlock_flight` | `true` | flying in the lobby is always allowed (the save is not changed) |
| `lobby_flight_speed` | `2.5` | flight speed in the lobby, as a multiple of the normal speed (`1` = normal, 0.1 to 10); battles are not changed |
| `clear_story_flags` | `false` | the story requirements above: quests, mentors, desks and shop items |
| `unlock_all_parallel_quests` | `true` | parallel quests do not wait for other parallel quests |
| `unlock_all_expert_missions` | `true` | expert missions do not wait for other expert missions |
| `unlock_level_cap` | `true` | maximum level 199 from the start (whole save file) |
| `tp_shop_all_seasons` | `true` | the TP medal shop sells every seasonal item all year |
| `mentor_names_on_map` | `true` | the Conton City map shows the names of the mentors you have discovered next to their icons |
| `reveal_hidden_conditions` | `false` | parallel quest details show every condition instead of "???" |
| `raid_characters` | `false` | unlock the Master Raid bosses (whole save file) |
| `wait_for_all_ready` | `true` | online rooms start only once every player is ready, with no 15 second countdown |
| `transform_hud_next_stage` | `true` | the battle HUD names the next stage of an x2m awoken skill |
| `all_dragon_balls` | `false` | you always count as holding all seven Dragon Balls, so Shenron can be summoned any time (the balls do not show in your inventory) |
| `skip_logos` | `true` | skip the logos at startup |
| `skip_intro` | `false` | skip the intro movie at startup |
| `title_volume` | `true` | the title screen uses your volume settings (see below) |
| `automatic_flag_fixer` | `true` | stops broken save flags from breaking the game, see below |
| `camera_zoom` | `true` | camera zoom, see below (`false` turns it off) |
| `replay_npc` | on | the DLC Master, see below |
| `logging` | `false` | write `general_qol.log` next to the DLL |

A setting missing from the file counts as `false` (`enabled` counts as
`true`).

The game only reads your volume settings from the save once you press
Start, so `title_volume` uses the ones it saw the last time a character was
loaded. Until it has seen them once, the title screen has no music.

### `automatic_flag_fixer`

Save editors' flag tools (for example "Validate Flags" in Xenoverse 2 Save
Editor 3.5.0) can switch on story flags your character has not earned. The
game then gets stuck. With this on, the game copes instead; your save is
not changed:

* Conton City no longer reloads forever when the save says episodes 1 and 2
  are cleared before you have finished them.

### `camera_zoom`

| Setting | Default | |
|---|---|---|
| `enabled` | `true` | Zoom on/off |
| `zoom_in_keys` | `["+", "num+"]` | Hold to bring the camera closer |
| `zoom_out_keys` | `["-", "num-"]` | Hold to move it away |
| `reset_keys` | `["backspace"]` | Back to the normal distance |
| `speed` | `1.8` | How much the distance changes per second while a key is held (1.8 = 80% further after one second) |
| `min` | `0.5` | Closest, as a share of the normal distance |
| `max` | `4.0` | Furthest, as a multiple of the normal distance |

Keys: a letter or digit, `+` `-` `num+` `num-` `num*` `num/` `num0`..`num9`,
`backspace` `tab` `enter` `space` `insert` `delete` `home` `end` `pageup`
`pagedown` `up` `down` `left` `right` `[` `]` `,` `.` `/` `;` `'` `\` `` ` ``,
`f1`..`f24`, `mouse4` `mouse5` `middlemouse`, or a Windows key code like
`"0x6B"`. Keys only count while the game window is in front.

The camera zoom and those volume settings are kept in
`general_qol_saved.txt` next to the DLL. Delete it to reset the zoom.

### `replay_npc`

A setting left out uses the value in the "Default" column.

| Setting | Default | |
|---|---|---|
| `enabled` | `true` | The DLC Master on/off. Off leaves Dastar and the robots as they are |
| `model` | `"STN"` | Character code (the three letters of `data\chara\<code>`); `STN` is Hercule |
| `costume` | `0` | Costume number |
| `idle_action` | `140` | Lobby action looped while idle; 140 is Hercule's pose |
| `talk_motion` | `"Mot_Stand"` | Animation while talking |
| `name` | `"DLC Master"` | Name shown above the NPC |
| `greeting` | | What he says when you talk to him |
| `option` | `"Relive an adventure"` | The choice that opens the quest menu |
| `menu_line` | | Line just before the quest menu |
| `farewell_go` | | Line after picking a quest |
| `farewell_stay` | | Line after choosing not to go |
| `menu_title` | `"DLC Adventures"` | Header of the quest menu (the game's is "Future Saga"); the big "Future Saga" title above it is hidden |
| `talk_voice` | `"BDQR_MST_000"` | Voice line when you start talking to him, a cue name in the game's `CAQR_ALL_VOX` voice bank; `BDQR_MST_000` is Hercule's. Empty keeps Dastar's |
| `hide_npcs` | `[987, 85, 86]` | Lobby NPCs to hide: Brashy, Spots, Culina |
| `quests` | every DLC story quest | The menu's quests, by quest name, in order |

`quests` is not in the shipped file. To choose your own, add it inside
`replay_npc`, e.g. `"quests": ["CTP_22_00", "CTP_22_01"]`. The DLC story
quests' names start with `CTP_14` to `CTP_27`; a name the game does not
have is skipped (and listed in the log).

Leave a text setting empty (`""`) to keep the game's own line.

The time rifts in Conton City still start each DLC story the first time.
Hiding Spots and Culina also hides where their stories normally begin;
those quests are in the DLC Master's list instead.

On any game version other than 1.26.01 the mod changes nothing.

## Building

Needs Visual Studio 2022 (C++) and CMake. `Build_Install.bat` builds the
mod and copies it into the game (found through Steam, or set `XV2_GAME`
to the game folder); `Build_Uninstall.bat` removes it again.

## Credits

* [XV2-Tools / Xv2CoreLib](https://github.com/LazyBone152/XV2-Tools) by
  LazyBone - QXD, IDB, TSR, TSD and TNL file layouts.
* [eternity_common](https://github.com/eterniti/eternity_common) by Eternity -
  item field names (IDB) and XV2Patcher's character slot keys.
* [XV2Patcher](https://github.com/eterniti/xv2patcher) by Eternity - the
  pre-baked.xml aura data (CusAuraData) x2m awoken skills use.
* [JPEXS Free Flash Decompiler](https://github.com/jindrapetrik/jpexs-decompiler)
  by Jindřich Pető - compiles the lobby map's ActionScript.
* Everything else (where the game checks flags, quests, DLC and the raid
  characters, the boot sequence, the camera, sound): found by reverse
  engineering the game.

# Save data, flags, unlocks and the level cap

Covers: where the loaded character's flags and quest progress live, the
account flags, which flag unlocks what (shops, desks, flight, teleports,
expert missions), the level cap, the script expression registry and
mentor progress (friendship) in the save file, what is per CaC and how
slots merge (SaveSync), the inventory and skill lists in memory and
the game's give-item / skill-unlock functions (DebugMenu item getter),
the Dragon Ball count / Shenron wish (General_QoL `all_dragon_balls`).
Used by General_QoL. Game 1.26.01; mgr = `[0x1568310]`.

Moved to their own files (2026-10-22):
* `savesync.md`: SaveSync: what is per CaC and how slots merge (2026-10-11); SaveSync in game: the live save image (2026-10-11)
* `save-inventory-and-skills.md`: Inventory and skills in memory; giving items (2026-10-12)

## Layout
* **Decrypted save image in memory: `[0x15681a8] + 0x68`** (save/system
  singleton, vtable 0x1108de0), the whole decrypted DBXV2.sav (0x12a1f8
  bytes, matched byte for byte, 2026-10-06). Starts `#SAV`, version byte +5
  (40), **Steam id at +8 XOR 0x7468656265636F6E** ("thebecon", Xv2CoreLib
  SAV_File.cs). Save file the game loads: `<Steam>\userdata\<32-bit account
  id>\323470\remote\DBXV21\DBXV2.sav` (Steam Cloud). The game also writes an
  identical copy to `%LOCALAPPDATA%\BANDAI NAMCO Entertainment\DB
  Xenoverse 2\Saved\SaveGames\<id>\DBXV2.sav` but does not read it: a merge
  written only there never showed in game and was overwritten on the next
  save (2026-10-11). Edit the userdata one, game closed. AES-CTR
  (Encryption.cs). `ClaudeTools\save\sav_steamid.py` decrypts a file and prints
  the id; `ClaudeTools\save\steamid_probe.py` finds it in the running game. Dead
  end: the quest manager [0x1568310] (first 0xa8000 bytes) holds no copy.
* Loaded slot = int at mgr+8+0x44 (already valid at the title screen).
  Per-character save block stride 0xe880.
* SYSFLAG (script) = per character: 2048+ bits at mgr+0xe710+slot*0xe880 (1.26: 0xe700)
  (accessors 0x828ca0 / 0x433e0; getter 0x83fa20 = block + (id>>5)*4, no
  bound - flags above 2048 continue past the first 256 bytes; the game itself
  sets 7326..). `ClaudeTools\sysflags.py` reads the first 256 bytes only.
* Quest progress: mgr+0x11ae0+slot*0xe880; TPQ = vector of 0x48-byte records,
  id +0xc, state +0x10 (3 = cleared).
* ACCFLAG = save-wide 288 bits at mgr+8+0xcc (getter 0x834be0).
* WRONG, do not use: mgr+8+0x884 (getter 0x826750) is some other save-wide
  array. Builds up to 2026-09-29 ORed bits 60..69 into it and they were
  saved; what it holds is unknown.
* Options: byte at mgr+8+0x8c+4+id (see sound.md "Volume options").
* `ClaudeTools\snapdiff.py` diffs the manager's first 0x4000 bytes bit by bit.

## Flag numbers (checked against lobby_event_manage.tsd's constant section)
    30  CTP_EPISODE_00_CLEARED      36  CTP_EPISODE_04_CLEARED
    60..69 UNLOCK_TELEPORT_POINT_01..10 (each robot sets its own on first talk)
    114 SHOP_TELEPORT_FIRST_TALK    300-305 CTP_Prologue_*
    306-316 CTP_FLBYTUTORIAL_*      401-403 CTP_EP01_LOCK*
    801 CTP_FLYINGLOCK_TOK_FIRST_TALK
    2400 HLQ_BUNCH_EVENT_CLEAR      5213 ONBATTLETOUR
    UNLOCK_*: ONLINE_LOBBY 0, SHOP 1, ODEKAKE 3, PARTY 4, BATTLE 5, QUEST 6,
      ONLINE_BATTLE 7, ONLINE_QUEST 8, XENLON 10, PRESENT 11, LEADERBOARD 12,
      TENKAICHI 13, UNLOCK_Buku 16 (flight), MAXIMUM_LV_STAGE1-4 17-20
    QUEST_CLEAR = 3, GAME_VERSION_PRODUCTS = 0

## What unlocks what
* Shops and desks check the character's UNLOCK_* SYSFLAGs, not the story
  (triggers like `SYSFLAG(CONST(UNLOCK_SHOP))==0` -> SHOP_LOCKED;
  receipt_quest_* scripts: `IF SYSFLAG(v1)==FALSE -> TAG_WARNING_UNLOCK_YET`).
* The lobby script sets them on load (setup_pre_positioning ~line 1560):
  EP 00 cleared -> SHOP, BATTLE, QUEST, ONLINE_BATTLE, LEADERBOARD; TMQ_1400
  cleared -> ONLINE_QUEST; EP 1 and 2 -> ONLINE_LOBBY, ODEKAKE, PARTY,
  TENKAICHI, PRESENT; EP 13 -> XENLON; EP 4 (flag 36) -> UNLOCK_Buku 16.
  General_QoL sets them from its flag thread instead.
* Flight: flag 36 (-> 16) is enough. The flight scene (flyng_tutorial.tsr
  CTP_FLYINGLOCK_TOK) only shows TUTORIAL 434, sets 801 and saves - it grants
  nothing. A watchpoint on CTP_04_04's quest state while trying to fly found
  only the script QUEST() evaluator reading it.
* Flight without the flag (General_QoL `unlock_flight` since 2026-10-14):
  **0x859490 = "can fly"** = SYSFLAG 16 UNLOCK_Buku (block getter
  0x828ca0(mgr, -1), bit getter 0x83fa20) || 0x834c10(account manager, 2).
  Only caller 0x4b0d9c in the lobby player update 0x4b0cd0: a 0 sets bit
  0x1000 ("flight locked") in [player+0x3c8]. Patched to `mov eax,1; ret`
  (prologue `48 89 5c 24 10 57 48 83 ec 20`); no flags written, no script
  edits, the flight scene comes with the story as in vanilla. Not seen in
  game yet; whether a script also gates flight on flag 16 is unknown.
* Teleports: flags 60..69 alone do not help before the prologue - the transfer
  robots (NPCs 65..80) also have SHOP_TELEPORT_LOCKED while `CTP_00_01!=3`
  (lobby-quests-and-shops.md).
* Expert missions: flag 2400 (set by the HLQ_BUNCH_EVENT scene after
  HLQ_1100) shows the HLQ_List rift (object 266); setting it skips the scene.
* Raid rifts (objects 163-180) have empty conditions and RBQ quests no
  requirements - they appeared once the UNLOCK flags were set (hypothesis:
  shown from the UNLOCK flags).
* DLC16/17 notices (setup_pre_positioning 52/56/73) and the battle tour
  unlock scene also check those flags.

## Level cap
* Max level = 0x856ee0: 199; ACCFLAG 34/33/32/31/30 off -> 180/160/140/120/99;
  9/8/7/6 off -> min 95/90/85/80; 0x2ba8d0 (Lite?) -> min 50. Only account
  flags, nothing per character. The user's save already had all nine (and
  35..287 all set - unknown why). General_QoL `unlock_level_cap` sets 6-9 and
  30-34.
* Scripts: levels 80->99 in 5 stages gated by ACCFLAG in shop_level.tsr; past
  99 notices at 99/120/140/160/180 once TCQ_WIS_00 (Whis) is cleared.
  UNLOCK_MAXIMUM_LV_STAGE1-4 (17-20) are never set by any script.

## Script expression registry
Registry in .data, entries {handler, id, name[16]}; all go through dispatcher
0x3c98e0 (jump on id-0x5f, byte table 0x3ca94c, dword table 0x3ca8e0).
SYSFLAG id 0x61, ACCFLAG 0x7c -> 0x3c9c38. Commands: SETSYSFLAG id 0xd,
SETACCFLAG 0x11. Script constants QUEST_LOCK < QUEST_OPEN < QUEST_PLAYED <
QUEST_CLEAR (=3); LOCK = 0 is inferred from the ordering, not read.

## Save Editor v3.5.0 "Validate Flags" bug (2026-10-04)
* Decompiled (ilspycmd): rules in `SaveEditor\manual\SysFlags.xml`; per rule
  the flag is set if Conditions1 or Conditions2 are all cleared, else cleared
  when ChangeIfSet. `CaC.IsFlagTrue()` returns true for an EMPTY list, and
  212 of the 217 non-"Other" rules have `Conditions2=""` -> Validate Flags
  sets every episode-cleared (30-47), ADD_MSG_EP*, prologue/tutorial
  first-talk (300-316), CTP_xx_xx_FIRST, TEACHER_*_TCQ_*_CLEAR_FIRST and
  TMQ_TUTORIAL flag regardless of progress. Seen live on Claotzu (slot 4,
  only CTP_00_01 cleared): 212/217 set.
* Fix: `ClaudeTools\fix_save_editor_flags.py` puts a nonexistent quest name in
  empty lists (`Quests.IsComplete` is false for unknown ids); `--undo`.
* That character then reloaded Conton City forever (lobby shows, no
  control, notices, fade, load; the game saves during it). Cause: SYSFLAG 34
  CTP_EPISODE_01_AND_02_CLEARED on while story quest CTP_99_01 is not
  cleared. "Is Conton TV unlocked" 0x3d54d0 = flag 34 || flag 80
  PUT_CONTONTOTV || CTP_99_01 cleared; while flag 80 is off and that is true
  the lobby (callers 0x3e5bd5 / 0x3e5cd4 -> TV manager [0x15851d8]
  0x4b01b0 / 0x4aa130; also 0x3e0665 in CControl::ProcSceneInitialize)
  sets the TV up and reloads, but CTV_UNLOCK_EVENT (sets 80) is only
  reserved by setup_pre_positioning.tsr once CTP_99_01 is cleared. The
  script only sets 34 from CTP_99_01, so a consistent save never hits this.
* Fix (General_QoL `automatic_flag_fixer.conton_tv_loop`): the flag id byte
  of `mov dword [rsp+0x38], 0x22` at 0x3d54e0+4 -> 0x50 (test 80 twice).
  Confirmed in game by writing that byte live, then setting 34: no loop.
* Method: `ClaudeTools\loopwatch.py` (flags/quests over time) + `log_files` showed
  no script and no flag change per cycle (monitoring_flag.tsr is only the
  idle gimmick monitor); then bisected live with `ClaudeTools\flagclear.py`
  (writes SYSFLAGs; the check is continuous, the loop stops/starts within
  seconds of a change - no reload needed). Exe readers of constant flag ids:
  calls to 0x83fa20 with `mov dword [rsp+N], id; lea rdx,[rsp+N]`.
  0x4432f6..0x443394 (flags 45/44/41/36/34) only picks the lobby BGM.

## Mentor progress (friendship) in the save file (2026-10-08)
* Decrypted file, per character (stride 50888): mentor entries at 125956,
  8 bytes {u16 friendship 0..1000, u16 dual-attack gauge 0..4000, u32 flags},
  index -> mentor as Xv2CoreLib MentorToCharaId (0 Krillin, 1 Tien, 2 Yamcha,
  3 Piccolo, 4 Raditz, 5 Gohan kid, 6 Nappa, 7 Vegeta, ... 24 Goku, 25 Whis).
  Lesson states are separate: TCQ records at 108592 (24 B: i32 type 3, qxd
  quest id, state (1 open, 2 played, 3 cleared), rank (seen 3-6), flag (1 once
  cleared), best score). The friend's save (2026-10-08) still had a rank and
  score on all 53 cleared lessons while Play Data showed "?" under every
  mentor, so that screen's "?" is not missing save data; cause still open.
* Flag bits = SET_MASTER_FLAG names in the exe's string order (image
  0x10981a8): 0 FIRST_CONTACT, 1 LICENSED, 2 DISMISSAL, 3 EXAM_RECEIPT,
  4 X_ULTIMATE, 5-7 FREE, 8-11 NOTICE_QUEST_00..03, 12+ TALKED_COS_01..10.
* Friendship sources: scripts only ADD_MASTER_FP (+150 per talk, quest-based
  amounts in osq_talk_teacher_present*); no script lowers or resets it.
  teacher_Lesson_result (every lesson's result event) adds none. The qxd
  field +140 (100/200/300/400 for lessons 00..03) is NOT credited on a clear:
  a save with TCQ_PIC_00 cleared had Piccolo at 0. Its meaning is unknown.
* Gates: lessons via MSTQUEST(STATE, mentor, n) == QUEST_OPEN/PLAYED (exe
  side; TCQ qxd requirements are all -1); X_ULTIMATE talk needs badge 02 +
  MASTER_INFO(m,FRIENDLY_POINT)==1000; badges 350/800/1000, talk tiers >350..>900.
* `ClaudeTools\save\sav_mentors.py <sav>` prints friendship/gauge/flags and the
  cleared lessons per character.
* Case (a player's save, 2026-10-08), reported as "mentor
  training reset": the save has 53 lessons cleared (all 4 for 10 mentors),
  skills and training level intact; friendship ~0 (Krillin 180, Tien 60,
  Piccolo 740), gauge 0, no X_ULTIMATE; Play Data 0% and "?" circles. An
  earlier save (2026-09-29: only PIC_00 cleared, all friendship 0) shows
  friendship was never built up - lesson clears give none and the friend
  rushed lessons. So nothing in the save was reset; Play Data's completion
  follows friendship/X_ULTIMATE. Ruled out: our DLLs (General_QoL's flag
  thread only ORs SYSFLAG/ACCFLAG bits, bounds-checked slot; BattleStyles
  only reads the save manager); the preset copy 0x506f80 overflow (presets
  1-7 never used); RevampBridge's 2026-10-05 TSD fix (moves lobby trigger/
  event entries only - Future Gohan's lessons, Android 16's last lesson
  result + X-ultimate talk, XV1 robot, DLC receptions; no save writes).

## Dragon Balls and the Shenron wish (2026-10-14)
Static analysis of a decrypted 1.26.01 image (`ClaudeTools\exe`); General_QoL
`all_dragon_balls` built on it - confirmed in game (2026-10-14).
* Balls = items type 7 (important), ids 1..7 (IDB entry 1 = "One-Star
  Dragon Ball"), count 1 each.
* **Item count 0x856d00(type, id, flags)**: types 0x10..0x13 / 0x11 go to
  flag functions; types 0..9 -> `0x83b450(list, type, id, flags)` on the
  inventory list. The script expression `ITEM(type,id)` calls it
  (0x3ca004, flags 0); ~30 other callers (shops, quest details, ...).
* Script: `dragon_ball.tsr` CALL_DRAGON summons only if
  `ITEM(7,1..7)>=1` (also: not in a party, SYSFLAG v0, not between
  CTP_10_01 and CTP_11_00), then the command `DRAGONBALL` (lobby event
  class `CEventDragonBall`, vtable 0x1075ce0). `shop_db.tsr` = the STP
  medal Shenron (buys the missing balls; says "full" when all 7 are held).
* **Wish end 0x4f2890** (caller 0x4ed137): counts held balls with
  0x856d00(7, 1..7, 0), builds event 0x31 with `7 - held`
  (0x6a66d0, obfuscated int) and sends it via 0x733c90 (network/telemetry
  sender, 12 callers) - i.e. how many balls were bought with STP medals.
  Then **takes balls 1..7: 0x855e40(7, id, 1)** x7 at 0x4f1f55..0x4f1fba.
* **Take 0x855e40(type, id, count)** -> remove `0x83b080(list, type, id,
  count)`: returns 0 without touching anything when the id is not in the
  list or count > held; else subtracts, posts event 2, frees the node at 0
  (unless node +0x20 flags).
* Why General_QoL hooks the count instead of skipping the script check:
  with no balls held the wish would report 7 bought with STP medals to the
  server side. Counting all 7 as held reports 0 (a normal wish); the takes
  fail harmlessly; nothing goes into the save. Real balls a player still
  picks up are taken by the next wish as usual.

## Shenron wish characters are a count, not a flag (2026-10-14)
Found while SaveSync left wish characters locked on another CaC (static
analysis, `ClaudeTools\exe`; data from the PC save).
* **Wish counts**: 32 bytes per CaC in PlayData, file 126276 + 168
  (= 126444 + slot*50888), one byte per wish type of
  `data/event/lobby/wish_script.dbw`. Byte 7 = "more characters" wishes
  made (4 on a slot after four wishes, every other slot 0/1).
* **check_unlock 0x5e3090** (0x5e28af..0x5e2983) walks the 16 entries of
  the wish-character list `[0x15545b8]`; 0x295660 (code) / 0x295620
  (costume) only look at the first N, N = 0x858e80 = 0x84d300(PlayData, 7)
  = byte at PlayData + 0x68 + 7. So the unlock follows that counter; no
  SYSFLAG/ACCFLAG is set.
* PlayData in memory: getter 0x828c70 = manager + 0x12670 + slot*0xe880 (1.26: 0x12660)
  (current slot); the save loader 0x869ce0 fills it with 0x84dd00 from file
  126340 (PlayData + 64), a 1:1 copy, so struct + 0x68 = file 126444.
* `wish_script.dbw`: "#DBW", 16-byte header (0x13 types, 0x11), records
  {i32 type, 5 x (id, kind, count)}, unused = (-1, 10, -1). Type 7 has 4
  records of kind 9 (character groups); type 0 = 500000 zeni; others items.
* SaveSync merges only byte 7 (highest wins): the other wish types grant
  items when made, so raising their counts would skip the reward.
* Omega Shenron is a DLC roster character, not a wish character.

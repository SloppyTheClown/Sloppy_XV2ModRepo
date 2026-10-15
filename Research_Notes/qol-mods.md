# General_QoL - features, mechanisms, state

Covers: what General_QoL changes and how (file edit / flag / code patch),
config keys incl. renamed ones, offline tests, and what is user-confirmed vs
untested. The game-side facts behind each feature are in the topic files
linked per row.

## Overview
* `General_QoL.dll`: story/desk/shop/flight/teleport/level-cap unlocks, the
  replay NPC and the rest of the table below.
* Uses the shared file-request hook (exe-and-tooling.md), chained with any
  other mod that hooks it.
* Replay list: filters like vanilla (DLC check 0x59b570 + "reached" filter:
  quest state >= 1), always; it looks for no other mod. Quests whose DLC
  check passes still need state >= 1, so unreached quests are not listed.
  Log: "N listed, M still DLC-locked" / quests held back.

## Raid characters (General_QoL `raid_characters`, 2026-10-12)
* What the user wanted: the Master Raid bosses unlocked (TP shop gift route
  is unreasonable), still only in raids. The first build (owned-mask cmove
  NOP, below) made them selectable everywhere instead - replaced the same day.
* The raid boss slots carry DLC key 0x10000000 (eternity's `PRB`; CST
  DlcFlag1 and x2s dlc_key 268435456): STD 0/1, ZMD 0/1, DMG 0/1, MRN 0/1
  (costume/preset; preset 0 of DMG/MRN/ZMD is the normal base-game slot) and
  SBM costume 3 preset 1. CST UnlockIndex 0, no QXD Chara_Unlock
  (`ClaudeTools\qxd_chara_unlocks.py`).
* **The unlock = account flags.** 0x5e4140 (called from 0x5e276f) has a
  table {code, costume (-1 any), ACCFLAG} and hides a slot while its flag
  (getter 0x826ba0, edx = entry +0x24) is off: TOW 1 -> 14, DMG -> 20,
  MRN -> 21, ZMD -> 22, STD -> 24, SBM 6 -> 24, TE0 1 -> 25.
  Script names (lobby_event_manage.tsd constants): 20/21/22 =
  ACCFLAG_ORE_RAID_UNLOCK_DMG/MIR/ZMD, set by npc_talk_present_fuu.tsr when
  Fu gets the boss's gift (items 6/70 DMG, 6/68 MIR, 6/69 ZMD, from the TP
  shop); 24 = ACCFLAG_PRB_FIRST_UNLOCK, set in setup_pre_positioning.tsr once
  PLAYED_ORE_RAID_TUTORIAL is on. General_QoL sets 20, 21, 22, 24 from the
  flag thread (save-wide, like the level cap). ACCFLAG 23 is read at
  0x554407 / 0x6026cd / 0x60ddc9 - not checked what for.
* Raid-only part (kept vanilla now): SelectCharacter's owned key mask
  0x5e3a60 (only caller 0x5e6105) sets bit 0x1c only if the current quest's
  type (`[0x15545b0] -> 0x81ad40 -> +0x34`, getter 0x2da2a0) == 0xf or menu
  +0x27e4 != 0: `0f ba e8 1c / 85 ff / 0f 44 c3` at 0x5e3c9e. NOPing the
  cmove at 0x5e3ca4 shows the raid slots in every select.
* Dead ends: the 12 other `test ..,0x10000000` / `bt ..,0x1c` sites
  (0xc5c4e, 0xc8e8b, 0xf6cb3, 0x11f7ff, 0x269b42, 0x26c978, 0x7b4b2f,
  0xa40261, 0xaa8c30, 0xb1dc9a, ...) are battle state flags, not the roster.
* State: see CLAUDE.md open items.

## General_QoL features
| key | mechanism | notes (topic file) |
|---|---|---|
| `clear_story_flags` | mentors (NPCs 3..36, type-1 triggers only) + desks/robots -> `AVATAR(LEVEL)>=1` / always / never-locked; QXD story requirements cleared (type 0, incl. CTP_00_01); IDB AVAILABILITY 30..46 -> 0x7fff; flag thread sets UNLOCK_* SYSFLAGs | lobby-quests-and-shops, save-flags-and-unlocks |
| `unlock_flight` (old `unlock_flight_after_prologue` still read) | code: "can fly" 0x84b380 -> `mov eax,1; ret`; flags and lobby files untouched (until 2026-10-14: flags 36 + 16 + script/TSD trigger 311 edits) | save-flags-and-unlocks "Flight without the flag" |
| `lobby_flight_speed` (number, 1 = off) | cave at 0x4b3d63 in PhysChar@Lobby's step: xmm10 (velocity factor) x multiplier for the local lobby player while airborne | lobby-physics-and-debug-menu "Flight speed" |
| `unlock_teleports` | flags 60..69; robots' SHOP_TELEPORT_LOCKED `!=3` -> `==9` | |
| `unlock_all_parallel_quests` | clears QXD type-1 requirements; tmq_data.qxd unlock kind (+80) -> 0 for every record | requirements alone left PQs hidden behind their parent quest / story-event kind 3 (lobby-quests-and-shops.md "QXD quest data") |
| `unlock_all_expert_missions` | clears type-4 (HLQ) requirements, sets flag 2400 (list rift) | per-mission rifts still chain (TSD unchanged) |
| `unlock_level_cap` | ACCFLAG 6-9, 30-34 | |
| `tp_shop_all_seasons` | EventShopTbl.est: all items every season | **not tested in game** |
| `replay_npc` (+ `quests`, `menu_title`, `talk_voice`) | Dastar's slot -> "DLC Master" (Hercule): TSD/TNL/msg, menu hooks 0x599ba0/0x5996c0, menu title text + CMNTITLE art blanked, voice cue remap | built-in list of 60 quests; title-art blanking **not confirmed in game** |
| `skip_logos` | NOP the je at 0x57e63d | boot-and-camera |
| `skip_intro` | WaitInstall Start -> jmp Stop | |
| `title_volume` (old `mute_title_music`) | at boot set categories 0/2/1 from options 3/4/5 remembered in `general_qol_saved.txt` (taken only when categories equal options); none yet -> mute cat 0 | sound; with the PC save (options 0/1/2) title music stays silent anyway |
| `camera_zoom` (object; `true`/`false` accepted) | SetMainCamera/SetMainPrevCamera import hooks + calcViewMatrix + culling 0x33b610/0x31dd70/0x31d980 hooks; keys `+`/`-`, Backspace reset, configurable keys/speed/min/max, saved per mode | boot-and-camera; clips through walls by design |
| `automatic_flag_fixer` (object or bool; `enabled`, `conton_tv_loop`) | exe byte patch 0x3d5234 (Conton TV unlock check tests flag 80 instead of 34) at boot | save-flags-and-unlocks; live patch confirmed, built DLL not yet run |
| `mentor_names_on_map` | exe: append call 0x576904 -> cave (mov r9, rdi) -> NPC name (npc+0x42) as `,c0=..` on each map "Event:" line; file: LOBYHUD.iggy gets a new AS3 block (`as3\LobbyMap.as`, built by `ClaudeTools\iggy\lobyhud_abc.py` into `src\lobyhud_abc.h`) labelling po_icn_sensei/sensei2 icons | ui-iggy-and-textures; works in game |
| `reveal_hidden_conditions` | exe: NOP the jl's at 0x59f397 / 0x59f406 so PQ details show condition lines 2/3 instead of "???" | lobby-quests-and-shops "Quest details"; works in game (2026-10-12) |
| `wait_for_all_ready` | exe: mov at 0x5cfb6c -> jmp, so the online room's 15 s Ready countdown never starts; the host still starts once all are ready | lobby-quests-and-shops "Online room countdown"; confirmed in game (PC <-> Deck) |
| `wait_for_all_ready_raid` | exe: jb at 0x5cfa2c -> jmp, the 60 s countdown of room modes 4/5/9/12 (which rooms those are is unknown - maybe Raid) | lobby-quests-and-shops "Online room countdown"; installed on the PC, not seen in game |
| `all_dragon_balls` | exe hook on the item count 0x848bf0: Dragon Balls (type 7, ids 1..7) always count 1, so Shenron's script check passes and the wish reports 0 balls bought | save-flags-and-unlocks "Dragon Balls and the Shenron wish"; confirmed in game |
| `log_files` (debug, not in the shipped json) | logs every loaded path | |

* `HookCode()` is shared by all code hooks (8-byte atomic jmp write).
* `general_qol_saved.txt` is per machine: sync it between machines no more
  than the logs.
* Each file edit is gated by its own setting.

## Offline tests
* `General_QoL\tests\run.bat`: patches the archive originals, decodes with
  `ClaudeTools\gamefiles\tsrdump.py` + check.py (asserts only the intended triggers
  change: e.g. 508 type-1 NPC triggers, 75 desk/robot triggers, 606 QXD values).
* `ClaudeTools\gamefiles\verify_dll_files.py`: runs General_QoL's `npctest`
  CMake target (EXCLUDE_FROM_ALL - build first) on the archive originals, checks every result with independent parsers.
* run.bat also runs `maptest` (LOBYHUD.iggy patch == lobyhud_abc.py's,
  name encoding cases).

## State (2026-10-03)
* User-confirmed: replay NPC + menu + Hercule's
  voice, teleports, flight, mentors/shops/quest desks/expert list rift/raid
  rifts on a pre-prologue character (no softlock), shops' AVAILABILITY fix,
  skip_logos + title mute, logos/intro skipped, camera zoom (all bugs fixed).
* `mentor_names_on_map` confirmed 2026-10-05: names show, only for mentors
  already discovered (the game sends no map line for the others). Label
  styled after that: 2/3 size, dark yellow (seen); text pulled back 40% of
  the icon's width (the icon's bounds are wider than its circle).
* Not confirmed: `tp_shop_all_seasons` (watch for a missing tail, crash or
  list cap: 289 entries vs max 166), menu title art blanking, title_volume
  restoring music for a save whose BGM is above 0, interplay with Revamp's
  lobby files.
* Unknown: why the flight scene did not play on first load (played on a
  later reload).

# Lobby NPCs, scripts, quest lists, quest/item data, shops

Covers: lobby files (TNL, TSD, TSR) and how NPC visibility works, which NPC
id is what, the quest-list menus and their DLC check, QXD quest
requirements, item shop availability, the TP medal shop, the online room
Ready countdown, and the pitfalls
hit while editing them (softlock, white screen). Used by General_QoL. Game 1.26.01. Decoders: `ClaudeTools\gamefiles\` (tsrdump, tsd, tnl,
qsf); `XV2_RevampBridge\tools\lobbyquests.py`.

## Lobby files
* `lobby_npc_list.tnl`: NPCs (CMS, costume, lobby name, position, action).
  TNL I32_4 52/53 = an expert mission's "new" rift, 54/55 its "cleared" one.
* `lobby_event_manage.tsd`: triggers (TNL_ID, Event_ID, condition string) and
  events (script, function, 20 args, NPC ids); section type 4 = constants
  (i32 len + name, i32 type, i32 len + value).
  - Trigger +12 = type: 6393 are type 1 (player talks), 19 are type 2 (start
    by themselves). Seven type-2 triggers on NPCs 11-17 are chase gimmicks
    (lby_gimmick_chase*.tsr) gated on CTP_06_00 etc. Opening them made a
    chaser run at a flying character with controls locked = SOFTLOCK. Edit
    only type-1 triggers.
  - A lobby NPC exists only while one of its triggers is true. A scene whose
    actor's NPC is missing waits forever: white screen with wind sound (the
    flight scene's actor 1 = Supreme Kai of Time, TNL 289, trigger 311).
  - TSD conditions may carry spaces (trigger 425 is `( AVATAR(LEVEL)>=1)`);
    no TSR condition in the game has spaces.
* `data\event\lobby\*.tsr` lobby scripts: XOR-encoded (tsrdump.py, port of
  XV2-Tools TSR_File). In-place same-length edits are safe; General_QoL's
  strings are in `src\tsr.h`.
  - setup_pre_positioning.tsr: `IF (QUEST(TPQ,STATE,CTP_04_04)==CONST(QUEST_CLEAR)),
    :-1, :TAG_CTP_FLYINGLOCK_END` before RESERVE_SCENEINIT_EVENT
    CTP_FLYINGLOCK_TOK (CTP_04_04 appears 6 times - match the IF by its label).
  - Script order: CTP_PROLOGUE scene, EP1 lock, TMQ tutorial, then flight.
  - Pre-prologue lobby = scripted tutorial: triggers test
    `QUEST(TPQ,STATE,CTP_00_01)==0` / `!=0` and CTP_EP01_LOCK.

## NPC ids (TSD/TNL)
* Mentors 3..36 (story-gated `QUEST(TPQ,STATE,CTP_NN_NN)==3`; Beerus 33 after
  CTP_12_00). NPCs 11-17 also carry the chase gimmicks.
* Transfer robots 65..80; offline quest counters 63/64 (unconditional
  triggers 104/105). Desks: 81 Advancement Test, 94 radio, 95 Photo Mode, 96
  Crystal Raids (CTP_04_04 + its tutorial flag), 97 Info Robot (CTP_99_00 +
  TTL_START_NOTICE), 148 Kame House, 976 Multiplayer Modes (TMQ_1400), 982
  votes, 1007-1046 Festival desks, 1309 Polisher, 1006 Universe Lobby
  entrance (also CTP_99_01, FLBYTUTORIAL_END, festival demo flags - left alone).
* Robots: Spots 85 (CTP_14 story), Culina 86 (CTP_15), Waipa 87, 981 (DLC
  story receptions - their triggers pick the next story step, left alone),
  Brashy 987 (SELECT_EXTPQ = -3), Dastar 1294 (SELECT_EXTPQ_FUTURE_SAGA = -5).
  Dastar's triggers 55/57/61/64 carry their own DLC_Flag. Rewiring Dastar to
  Brashy's SELECT_EXTPQ skipped all dialogue (unknown why).
* Expert mission rifts = lobby objects 208-273 chaining on HLQ clears (first
  needs CTP_05_04 + CTP_06_02); 266 = HLQ_List rift (receipt_HLQ_LIST:
  QUEST_MENU "HLQ"). Raid rifts 163-180.
* The replay NPC ("DLC Master", General_QoL `replay_npc`) = Dastar's slot:
  TNL 1294 model (default Hercule STN, costume 0, action 140), event 2493 keeps
  SELECT_EXTPQ_FUTURE_SAGA (arg 1 = talk motion), name name_4293 and 5 lines
  (CHOICE, CHOICE_01, SELECT, YES, NO) in every lobby_text_*.msg. Voice:
  sound.md. Menu title art: ui-iggy-and-textures.md.

## Quest-list menus
* QUEST_MENU "TPQ",-1,<list>: -2 TPQ_EX1/2, -3 TPQ_ADD, -4 TPQ_FM(_NO_PV),
  -5 TPQ_FUTURE (QuestSort.qsf). Menu +0xa4 = list id, +0xa0 = context.
* Builders 0x599ba0 (-3) and 0x5996c0 (-5), same layout: candidates come from
  the SAVE's progress ([0x1554458] list +0xb10), then a quest is kept if its
  QXD record (0x7fb090([[0x1554458]+0x928], q+8, q+0xc), raw 292 bytes) +0x108
  (Update_Requirement) bit 28 is clear, 0x59b910 (TPQ: state >= 3; state =
  quest object vt+0x18), and 0x59b570(menu, record+0x10c DLC_Flag) passes,
  then only TPQ_ADD members. So editing QuestSort.qsf alone shows nothing.
* DLC check 0x59b570: each DLC bit -> content pack id -> 0x2b9c30(pack, 1) ->
  0x2ba520 = ownership, hooked by XV2Patcher (XINPUT1_3.dll). DLC_Flag is
  ownership, not story progress. Quests with DLC_Flag 0: CTP_17_00, 18_*,
  19_01/02, 20_99, 21_99, 26_99, 27_99.
* General_QoL's replacement list: 0x81ac70([0x15545b0], -1) -> ctx;
  0x834600(ctx, 0x59a5a0([menu+0xa0]), name) -> quest; append with 0x3def0.

## QXD quest data (read from the data; XV2-Tools names in quotes)
* Quest record 292 bytes. +0x10c (+268) DLC_Flag; +106 star rank (0-5).
* Requirements: 4 types at +48 (0 = TPQ list, 1 = TMQ, 4 = HLQ, 6 = CHQ,
  8/9/10 TTQ/TFB), 4 quest indices at +64 (-1 = none). TPQ index 50 =
  CTP_00_01; CTP_* = 51..175 (FEP 106-107, TPQ_50_* 108-118 between). The
  game reads +64 (clearing story requirements made story-gated PQs appear).
  Type 1 = chained on other PQs (185 TMQ, 1 OSQ); type 4 = HLQ chain (19).
* TMQ = the 186 parallel quests. TPQ/CTP/FEP are NOT parallel quests.
* Game record offsets = raw file offsets (record copied as is).
* **Unlock kind +80 (+0x50)** - clearing the requirements is NOT enough for
  PQs (fresh CaC was missing PQ 1, 6, 9, 12, 14, 15 with them cleared):
  * 0x83e160 (unlock evaluation, sets state 1 via vt+0x10(1,0)): kind 0 =
    unlock at once; 1 = all 4 requirements state 3 (null quest = met, so
    cleared slots pass); 2 = flag (+0x30/+0x40 via 0x82d2f0/0x82da30);
    4 = level (+0x54). Kind 3 is not handled = never unlocks there.
  * 0x83e050 (shown at the counter; called by 0x83b560, a switch on quest
    type): kind 0 -> shown. Else for TMQ looks up the **parent** quest =
    shorts +0x28 type / +0x2a index (= the first requirement, a duplicate
    the requirement clear leaves alone) and shows the quest only if the
    parent's state != 0; with +0x2c (I_44) == 1 only if the quest itself is
    cleared. Other types check the quest's own state.
  * 0x59b910 (list filter, context 0x59a5a0([menu+0xa0]) == 1): kind 3 is
    listed only once cleared (state >= 3).
  * TMQ kinds (vanilla = Revamp): 0 x11, 1 x181, 3 x10 (TMQ_0301, 0401,
    0506, 0700, 0803, ... = the I_44 = 1 ones, also the exe string list at
    0xf917e0). HLQ: 0/1 only. Fix (General_QoL `QxdUnlockFromStart`): every
    TMQ record kind -> 0.

## Quest details: hidden conditions ("???")
* Quest detail builder 0x59e660 (CMN_QUEST quest.iggy, DetailSetPage1 via
  0x5a8f50 at 0x59ff6c: 3 success + 3 failure lines, each split on '
').
  Condition texts come from 0x6174c0(kind, quest type, quest index, -1, 0)
  (kind 0 = title, 1 = success text, 2 = failure text).
* TMQ only (quest +8 == 1): line 2 = msg kind 4 if the quest object's
  vt+0x58 >= 2 (cmp eax,2 / jl at 0x59f394), line 3 = kind 5 if >= 3
  (0x59f403); otherwise msg QUE_STR_10_000 via 0x59b070 = "???". What
  vt+0x58 counts is not checked (it grows as the hidden conditions are met).
* General_QoL `reveal_hidden_conditions` NOPs both jl's (7c 34 / 7c 44) at
  boot. A quest without that msg gets the lookup's default string. Works
  in game (2026-10-12).
* The reward page (DetailSetPage2Reward 0x5aa2c0) has no condition text;
  QXD reward flags 2 = not listed at all (0x5a0d6d loop skips them).

## Items and shops
* skill_item.idb (Eternity's loose .idb.xml names the fields): +14 DLC_FLAG
  (XV2DataEdit "I_10", a content-pack id: 29 = base game, 64 = newest DLC),
  +20 AVAILABILITY (u16, "I_12"): 0x7ffe = TP shop, 0x7fff = zeni shop from
  the start (vanilla 0x7fff items have BUY 0; X2M custom skills use it with
  BUY 10000), 30..46 = story-gated shop items rising with price, 32766/32767/
  65535 not sold, 0 on 3 always-sold items. Setting 0 HID items; 30..46 ->
  0x7fff is what works (407 items in 7 files).
* TP medal shop `data\system\EventShopTbl.est` (+ `_freemium.est` for Lite;
  which one the game reads is not checked): "#EST", u16 0xfffe, u16 0, i32
  count, i32 0, then count x {i32 item type, i32 item id, i32 season 0..3} in
  4 season blocks (166/154/143/154). Types 0-3 costume parts, 4 accessory, 5
  super soul, 10-12 skills, 16/17 unknown. 289 distinct items (201 Lite), 135
  in one season only. What picks the season (clock? server?) is unknown.
  General_QoL `tp_shop_all_seasons` lists all in every season (7420 -> 13888
  bytes, the only size-changing edit) - not tested in game.

## Online room countdown (ROOM.iggy)
Found statically in the old decrypted image (`ClaudeTools\_claude_xv2mod\dump`,
`ClaudeTools\exe\`), 2026-10-12. Not yet seen in game.
* ROOM.iggy (online PQ/battle room, `Menu::RoomUI`, vtable 0x10bd640) only
  draws the timer (AS `RoomTimer`: TimerStart/TimerEnd/TimerSetTime). The
  exe looks the AS callbacks up once (0x5dcef0, name table 0x14aa220, 49
  names; handles at ui+0x1f8 + 8*i, Timer* = i 46/47/48).
* Room timer on the room control object: +0x9a8 running, +0x9b0 start
  (ms, GetTimeBaseCount), +0x9b8 seconds, +0x9bc shown. Start(seconds)
  0x5dc5b0, Stop 0x5dc480, seconds left 0x5dc4d0, expired 0x5dc580 (checked
  by Event procedures 0x44c810.. 0x44fcb0 and RoomControl 0x5cff70, which
  then sends everyone on).
* Events come from `Lobby::CLobbyNetP2P` (singleton [0x1573e98]; deque at
  +0xd68, push 0x474360, pop 0x4719b0; event {u32 type, ...}). Handler
  0x5cf900 (jump table 0x5cfc58, types 0..5): 0 -> stop if [p2p+0xe44];
  1 -> stop if not [p2p+0xe44], then as 3; 3 -> start **60 s** if room mode
  (0x43140 on 0x476b0(room)) is 4/5/9/12 (bitmask 0x1230) and members
  (0x47790) >= 2 (`cmp eax,2 / jb` at 0x5cfa29, `mov edx,0x3c` 0x5cfa32);
  5 -> start **15 s** for modes 1,2,6,7,8,11,15,18..22 (table 0x5cfc78).
  Event 3 is queued at 0x475cb4 right after member+0x149 = 1 (the ready
  flag: 0x472610 counts members with +0x149 set).
* Everyone ready: 0x5cf7d0 - when this player is the host (0x433e0 ==
  0x92950) and 0x472610 says all ready, 0x472b90 sends everyone on, timer
  or not. So never starting the timer = the room waits for all.
* General_QoL `wait_for_all_ready`: 0x5cfa2c `0f 82` -> `90 e9` (the jb's
  rel32 0x147 now a jmp to 0x5cfb79), so events 1/3 never start it. Each
  player's game runs its own timer: everyone in the room needs it (an
  unpatched host's timer still sends everyone on). Untested: whether
  something other than Ready (e.g. a join) also raised event 1/3, and
  whether a player who never readies can still be kicked/left normally.
* **The countdown players see on Ready is the 15 s one** (user, in game,
  with only the 60 s patch: still 15 s, still forced). Event 5 is queued
  by P2P message 6 (message switch in 0x475640, `movzx byte [rbp-0x50]`-1,
  table 0x47718c, 20 cases; case 6 = 0x475b65, no condition, also sets
  [p2p+0xe44] = 1); message 3 (case 0x475bd1) is the member-ready one ->
  event 3. Event 5's case reaches `mov edx,15` at 0x5cfb6c (only via the
  byte table; the 60 s path enters at 0x5cfb71). Second patch: 0x5cfb6c
  `ba 0f 00 00 00` -> `eb 0b 90 90 90` (jmp 0x5cfb79, past Start). Expired
  (0x5dc580) is false while +0x9a8 (running) is 0, so nothing forces. The
  all-ready path 0x5cf7d0 has no mode check. **Confirmed in game** (PC <->
  Deck, both patched: no countdown, the room started once both were
  ready). Who sends message 6 was not traced.
* Config split: `wait_for_all_ready` = the 15 s patch (0x5cfb6c),
  `wait_for_all_ready_raid` = the 60 s patch (0x5cfa2c). Which rooms are
  modes 4/5/9/12 is unknown (user guesses the raid room); the 60 s patch
  has not been seen to do anything in game.

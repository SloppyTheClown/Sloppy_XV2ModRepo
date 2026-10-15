# Battle styles (equipable movesets)

Covers: BattleStyles overview - the goal and user decisions, the
1.25.02 "EquipableMovesets" reference mod and its sites ported to
1.26.01, the CMS path getter, the phase plan and state log, style names,
which movesets are listed (unlocked / giants / fake / missing files).

The rest of the Battle Styles research is split by area - open only the
one you need:

| File | Read when the task involves |
|---|---|
| [battle-styles-fit.md](battle-styles-fit.md) | fitting a borrowed EAN / cam EAN / BAC / BDM to the CaC's race (fit_to_race, pelvis, merged EAN builds 5-6, tail merge, keyframe alignment crash, race-only BAC entries) |
| [battle-styles-sound-and-effects.md](battle-styles-sound-and-effects.md) | the merged character SE bank, the style's character EEPK / effect owner (build 7), the cue limit crashes |
| [battle-styles-props.md](battle-styles-props.md) | props the style character holds (canes, swords, spear, pole, guns): prop bones, prop meshes as physics objects, sizing and back placement (builds 8-14) |
| [battle-styles-loading-and-online.md](battle-styles-loading-and-online.md) | the battle setup list, the character select setup record (online sync), which player the CMS getter loads, the CSS preview, presets in memory + the sidecar save |
| [battle-styles-ui.md](battle-styles-ui.md) | the equip screen slot (AVATAR.iggy / EditChara / AVATAR_LIST), style list search + favorites, the CSS skill popup + lobby Presets row, the FFDec roundtrip bug |
| [chara-select-search.md](chara-select-search.md) | CharaSearch: character select search + favorites (split out of Battle Styles) |

## Goal (user request)
A "Battle Style" slot on the equip screen; saved per character and per
preset; new styles addable through the XV2Patcher / xv2ins ecosystem; only
non-skill moves change (basic attacks, throw, idle, ...).
User decisions:
* A style = `data\battle_styles\<Name>.xml` listing, per CaC race, the EAN /
  BAC / BCM / BDM / cam.ean (optionally effects and sounds); it can point at
  an existing character or at its own files. Plus a testing option that
  lists every roster character's moveset.
* Online: synced - other players see and fight your style.
* Saves: never inside the save (vanilla saves must keep working). Sidecar
  file per character slot and per preset.

## Reference mod: EquipableMovesets_1.25.2 (game 1.25.02)
`EquipableMovesets_1.25.2.zip` (a community mod for 1.25.02): an Epatch
(`DemonBoyEquipableMoveset.xml`) + an xv2ins installinfo (CMS entries per
race with AutoCharaID 10000-10099, files in `chara/<RACE>/MST/`, cso/ers/msg,
a patched `AVATAR_LIST.iggy`). How it works:
* The chosen moveset id is written into the **CaC costume field of the battle
  setup list** (the game forces it to 0 for CaCs, so it is free, and the list
  is what goes over the network).
* The CMS lookup for the moveset fields is redirected to the moveset's CMS
  entry; ERS (EEPK), CSO (sounds) and the EEPK chara-id getter are patched to
  use it too; extra fixes for Purification awoken.
* Relative CMS values like `../CL4/CL4` make the getter build
  `chara/%s_PLAYER.%s` instead of `chara/%s/%s_PLAYER.%s`.

### Sites, 1.25.02 -> 1.26.01
Found with `XV2_RevampBridge\tools\codematch.py` (most reference byte
patterns no longer match); probe scripts `locate_em.py` / `locate2.py` were
scratch - re-derive with codematch if needed.

| Reference name | 1.25.02 | 1.26.01 | Notes |
|---|---|---|---|
| MovesetSetupCss | 0x6142a1 | **0x5e705d** | exact |
| MovesetSetupSP | 0x680150 | **0x652210** | exact; setup-list writer, see below |
| CmsChange | 0x2ded2f | **0x2c5597** | inside the CMS path getter 0x2c5510 |
| ErsChange | 0x9bedaa | **0x9563eb** | |
| CsoChange | 0x2afc18 | **0x298d68** | |
| VfxFix (EEPK chara-id getter) | 0x120860 | **0x121370** | field offset +0x15a0 became +0x15c0 |
| CostumeIdCacCheck | 0x6960d3 | unresolved | candidates 0x6673bb / 0x6672f0 (few votes) |
| CostumeFix | 0x695bec | unresolved | candidate 0x668ab2 (20 votes, 160-byte window) |
| Purification fixes | 0x8f686 / 0x158f92 | unresolved | code became a 0x1b9c memcpy near 0x92d00 / 0x155557 |

Globals: 0x1698df8 -> **0x156b558** (battle setup list), 0x1681bc8 ->
**0x1554408**, 0x169d118 -> **0x156eb60**, 0x1681c08 -> 0x1554448.

## CMS path getter 0x2c5510 (1.26.01)
`char* fn(cms rcx, charaId edx, field r8d, out r9)` - finds the entry with
that id and formats its string field into `out` ("chara/%s_PLAYER.%s"
0xf97c40 or "chara/%s/%s_PLAYER.%s" 0xf97c58, via 0x32a20); returns out or
null. Refuses field >= 13 (0x2c558e). `_PLAYER` is appended for fields 7, 8,
12 (`lea eax,[r8-7]; test eax,0xFFFFFFFA`).
* Entry: `40 53 55 56 57 | 48 81 EC D8 00 00 00` - 5 bytes of pushes, 16-byte
  aligned: hookable with HookCode n=5.
* Only caller: wrapper **0x2c54b0** (`out` = its 0x80-byte stack buffer, then
  "data/%s" into the caller's buffer). Wrapper callers: 0x113e74, 0x113f54,
  0x113f8d, 0x1144d0 (fn 0x113d40), 0x19e994, 0x2496d8, 0x2982ea, 0x298894,
  0x2b355d, 0x2b358e, 0x2b35bf, 0x3711ac.
* CMS layout: count at +8, table offset at +0xc, 0x54 bytes per entry, id
  +0, code +4, string offsets at +0x20 + field*4. Fields: 0 BCS, 1 EAN,
  3 FCE_EAN, 4 FCE, 6 CAM_EAN, 7 BAC, 8 BCM, 9 BAI, 12 BDM.
* CaCs are ids 100..107 (HUM, HUF, SYM, SYF, NMC, FRI, MAM, MAF).
* A CMS value starting with "../" takes a separate branch: "chara/%s.%s"
  (0xf97c70) from value+3 - the getter strips the "../" itself, so the game
  never puts ".." in a request path on its own.

## Phase plan
1. **Done (confirmed in game):** hook 0x2c5510; config `test_style`
   (CMS code) gives every CaC that character's moveset fields
   (`test_parts`). Checks: does a CaC load and fight with e.g. GOK's
   moveset; lobby/menus with the swapped EAN; skills unaffected.
2. Per-player style: know the player being loaded; carry the style id in
   the setup-list costume field (0x652210) so it syncs online; ERS/CSO/EEPK
   for effects and sounds of the borrowed moveset.
3. "Battle Style" equip slot: AVATAR.iggy AS3 + exe-side list (built).
4. Sidecar storage per character slot and per preset (per Steam account).
5. Style XMLs (`data\battle_styles\styles\*.xml`) + moveset mod converter
   (built, see "Phase 5" below), plus the "all roster movesets"
   test list.

## State
* 2026-10-05: phase 1 confirmed in game - only CaCs change (Piccolo
  normal). fit_to_race build 4 (leg-length scale + intro bone-length tracks
  dropped) confirmed in game: FRI and MAM CaCs (different heights/widths)
  stand on the ground, intro no longer stretched. So the per-CaC body scale
  does apply on top of a race-fitted EAN. Open: tail (not animated), neck
  (parked). Next: phase 2.
* 2026-10-05: phases 3 and 4 (per character slot) built, installed on the
  PC, **not yet run in game**. `src/style_menu.cpp`:
  - AVATAR.iggy served with `as3\Avatar.as` (button 8 "Battle Style\t" -
    the tab hides msg-id replacement - user data 96 label / 97 count,
    sendType 240). Offline: patched file identical to avatar_abc.py's.
  - EditChara user-data count byte 0x5e -> 0x60; vtable slot 17 hooked:
    240 opens AVATAR_LIST like 0x4fcac0 (default = equipped), 107 is
    followed by pushing 96/97; poll 0x4fd9d0 hooked: when +0x2f8 goes
    240 -> 0x192 the last result is equipped and 96/97 pushed again (ends
    the AS wait). Assumption to check: a cancel leaves the result at the
    default entry.
  - List: CMS entries with EAN, BAC and BCM, first of each distinct
    (EAN, BAC, BCM) set = 218, names `chara_<CODE>_000` from
    proper_noun_character_name_en.msg (code when missing, "(CODE)" added
    to duplicate names), "Default" first. Config `"styles"`: "all" or codes.
  - Choice in `<mod folder>\equipped_styles.sav` {"slot": "CODE"}; the
    CMS hook uses it for every CaC when `test_style` is empty (local only
    until phase 2).
  - To check in game: slot shows under Super Soul, list opens/scrolls,
    confirm/cancel, label updates, battle uses the style, other equipment
    unaffected.
  - First run in game (18:46): no slot. Two causes, both fixed (rebuilt):
    (1) file requests carry BACKSLASH paths (`data\ui\iggy\AVATAR.iggy`,
    seen in live request objects, vtable 0xf9b270) - the suffix compare
    used '/' only, so the file was never served (General_QoL normalises
    slashes, hence LOBYHUD worked); (2) the count byte and poll pages were
    still encrypted at 643 ms although the CMS hook's page was ready -
    every code patch now waits for its own bytes. Dead end: AVATAR.iggy
    loads through the generic menu file loader 0x2c7f40 (194 callers,
    LOBYHUD too) - there is no separate load path to hook.
  - Second run: file served, patches in, still no slot. The in-game
    Customize menu runs in **mode 3** (EditMenuClothInfoList, request 103),
    not 7 (CusCloth, 107) - read live with `ClaudeTools\battle_styles\editchara.py`
    (EditChara +0x1f4 = 3, AS user data [0] = 3; user data vector at
    +0x160/+0x168, 16-byte {valid, type, value}; SetUserDataStr 0x2c8240
    writes type 6 + the wchar_t pointer). The slot is
    now in both lists and 96/97 are pushed after 103 and 107.
  - Third run: slot shown but grey until the tab was re-shown, and a
    click crashed - AVATAR_LIST missing in mode 3 and cloth push not
    hooked (battle-styles-ui.md "Exe"). Both fixed.
* 2026-10-05: **phases 3 and 4 confirmed in game.** Slot active at once,
  list opens/scrolls, equip works, the style loads in training. Tested:
  Krillin and Broly work. Great Ape Vegeta and Reception Robot equip and
  load but are badly broken (below).
* 2026-10-05: **per-character style built, installed on the PC, not yet
  run in game.** Hook 0x16bfa0 (battle-styles-loading-and-online.md) sets the loading player; in battle
  a CaC gets the style equipped on its entry's save slot (+8), a file of
  another chara than the entry's own keeps its moveset; outside the
  loader (lobby, menus, CSS preview) the loaded slot's style as before.
  Config `apply_to_all_cacs` (testing) = the old behaviour, `test_style`
  still wins over both. Log: one "battle player N: chara .. costume ..
  save slot .." line per loaded player - to check: +8 really is the save
  slot (two CaCs from different slots), what NPC CaCs hold there.
  Known gap: online, a remote player's +0x78 is *their* slot index, so we
  would apply our own slot's style to them (desync) - phase 2.
* 2026-10-05 run: **per-character battle confirmed** (slot 3 NMC GNY,
  slot 7 SYM STN, each its own in an Offline Battle; log "battle player 0
  ... save slot 3", "player 3 ... save slot 7" - entry +8 is the save slot).
  The CSS preview still showed the loaded slot's style on every CaC -> hook
  0x5e66d0 added (battle-styles-loading-and-online.md), built, not yet run.
* 2026-10-05 20:22 run: **CSS preview per slot confirmed** (user: "working
  great"). Log while browsing Offline Battle CSS: "chara 106 ... (character
  select, save slot 5) -> fit/GRD.MAM.ean", slot 7 STN, slot 3 GNY, slots
  0/1/2/4/6 "own moveset" - menu +0x450 is the previewed CaC's save slot,
  never -1 there. So per-character (phase 4 + battle + CSS) is done offline.
  Still on the loaded slot's style: lobby, other menus using 0x657ff0
  (SelectHero, EditSlot, PartnerCustomize, RecordMain) - **not a problem**:
  the user confirmed CaCs there stand in a neutral pose and show no moveset
  idle, so nothing to hook. Next: broken-moveset exclusion list, then phase 2 (online:
  carry the style in the setup list instead of reading the local sidecar).
* 2026-10-06: user's online test PC <-> Deck: the opponent loaded the
  wrong moveset (remote save slot read in the local sidecar). Phase 2
  built and installed on the PC, not yet run: style code carried in the
  setup record name tail (battle-styles-loading-and-online.md "Character select setup
  record"). Log
  lines to check: "record sent", "record applied", "battle player N ...
  setup record: yes ... style". Not covered: online modes that don't go
  through this record (Parallel Quests?), the CSS preview of a remote
  player.
* 2026-10-06 run: **phase 2 confirmed online PC <-> Deck** (user: "works
  perfectly"). 1v1: both sides load each player's style, and the pre-battle
  preview was synced too. **Parallel Quests also works** - it goes through
  the same setup record. Next: broken-moveset exclusion list, phase 5.

## Style names (2026-10-12, built + installed, NOT yet seen in game)
User report: the "Trunks (Future)" select slot has sword and no-sword
costumes, but the list showed one "Trunks (Future)". Both were there: a
moveset is one CMS (EAN, BAC, BCM) set, and the slot mixes two codes -
TRX (sword, Long Coat; also TRF shares it) whose `chara_TRX_000` is just
"Trunks", and TR1 (`chara_TR1_000` "Trunks (Future)"). The select shows the
slot's first costume TRX/2 -> `chara_TRX_002` "Trunks (Future)".
* Slots: `data\XV2P_SLOTS.x2s` (`{...}` per slot, `[code,costume,preset,...]`
  per costume) or, without XV2Patcher, `chara_select_table.cst` (entry +0
  = 0 starts a slot, code +4, costume u16 +8, preset u16 +0xa; Xv2CoreLib
  CST_File.cs).
* Names: `chara_<CODE>_<costume 3 digits>` in proper_noun_character_name,
  falling back to `chara_<CODE>_000` (most costumes have no own line - e.g.
  GHL/1 heads the Gohan slot); costume names
  `variation_<CODE>_<costume>_<preset>` in proper_noun_variation_name.
* Rule (style_menu.cpp MovesetName, mirrored by
  `ClaudeTools\battle_styles\stylenames.py`, which prints every name): a moveset
  heading a slot gets that slot's name; else the first costume using it
  gets its shown name, plus " (<costume name>)" when that equals the
  slot's name (HIK stays "Hit (Awoken)"); in no slot -> `chara_<CODE>_000`;
  same name twice -> " (CODE)" as before. 1.26.01 + Revamp changes: TRX
  Trunks -> Trunks (Future), TR1 -> Trunks (Future) (Capsule Corp. Clothes
  3), VG1 -> Vegeta (Battle Armor 9), GH1 -> Gohan (Youth) (Battle Armor 1),
  SV1 -> Videl (Super) (Space Waitress Outfit), SHE -> Android 18 (Super)
  (Tracksuit 3), SBM Kaiwareman -> Saibaman, APL Nabana -> Appule, COL
  Metal Cooler -> Cooler.
* Not changed: msg text keeps entities (`Future&apos;s Doom`); not checked
  how the list draws them.

## Unlocked styles + giants (2026-10-13, built + installed, NOT seen in game)
Config `"styles"`: `"all"`, `"valid"` (adds the "fake" + "giant" groups to
`exclude_styles`), `"unlocked"` (valid + only characters unlocked in the
character select; race movesets always; shipped default) or a list of codes.
Renamed 2026-10-15: that setting is now `"roster"` (default); `"unlocked"`
is now roster without the unlock check (every select character, DLC gate
still applied - `UpdateUnlockedStyles` only skips `FindCheckUnlock`).
The list is sorted by name (`_wcsicmp`), race movesets last in
kRaceMovesets order, Default first; a config list keeps its order.

**Giants: CMS entry +0x10** (eternity CmsFile.h unk_10) is a size class,
0..6 in 1.26.01: 5 = Great Apes OSV OSB OBB OSN + Shenron SIN XAS, 6 = Cell
Max SIM SIH; every fighter is 0..4 (Broly, Dodoria, Guru 4). XA8/XOD hold
0xBACABACA filler (no EAN - skipped anyway). Giant = >= 5. Fits the data
(`ClaudeTools\battle_styles\giantmovesets.py`); not proven to be what the game
reads. Dead end: nothing else in the CMS (cam / u16 / u1a) separates them.

**Unlocked: the character select's check_unlock.** Called at 0x5e4ae2:
setup `44 8B 87 3C 04 00 00 48 8B CF 8B 97 38 04 00 00` (r8d = costume,
rcx = menu, edx = CMS id), call at 0x5e4af2. XV2Patcher's `UnlockCharaMods`
hooks that call (so it can point elsewhere - follow the E8 target), and
`unlock_chara_all` overwrites it with `41 B8 01 00 00 00 90 90` (= all
unlocked). The original 0x5e2560(menu, CMS id, costume) reads only menu
+0x27e8, +0x444, +0x27e4; its sub-checks 0x5e4140, 0x5e3cc0, 0x5e3f70
overwrite rcx before use. So the mod calls the live target with a zeroed
0x3000-byte stand-in menu under __try (fault -> log, show everything),
once per list open. A style is unlocked when any select costume (CMS id,
costume of every code sharing the moveset) is. Styles in no select slot are
dropped in "unlocked" mode; Default and the equipped style always show.
Uses the local save, but only for the UI list - the equipped style is
still sent online by code, so no desync.

## DLC characters (2026-10-13, reworked 2026-10-14; installed, NOT seen in game)
check_unlock does **not** gate DLC: the select drops DLC costumes in its
script (CharaSele.as "delete chara" loop: hidden when `(owned1 & DlcFlag1)
== 0 && (owned2 & DlcFlag2) == 0` and both single-unlock keys miss). The
masks come from 0x5e5fb0: owned1 = 0x5e3a60(menu) (call at 0x5e6105, iggy
var 0x224a; reads only menu +0x27e4; bit 0x1c = raid bosses only in a raid
quest, type 0xf), owned2 inline from 0x5e611a (var 0x224b): pack ownership
0x2b9c30(pack, 0) for 0x2e/30/32/37/39/3b/3d/40 and update level 0x2b9850()
(clamped TU version from [0x156efac]) >= 0x13..0x27. Battle Styles reads
the slot keys from the x2s fields 7/8 (eternity Xv2PatcherSlotsFile dlc
low/high) or the CST +0x14/+0x18.
**2026-10-14 rework (one way, every mode):** Battle Styles no longer copies
the mask logic or knows about other mods. owned1 = the live call target at
0x5e6105 called directly; owned2 = the game's own block 0x5e611a..0x5e623e
run in place: a stub (RWX page) pushes rbx..r15, sets esi = 0 / r14d = 1
(the block's inputs, 0x5e5fd6 / 0x5e6069), jumps in; a hook at 0x5e623e
(`44 8B C7 BA 4B 22 00 00`, mov r8d, edi; mov edx, 0x224b) returns edi to
the stub while its `running` flag is set, else replays the 8 bytes for the
select. So any patch there (another mod's, XV2Patcher's) is followed. Raid bit
0x10000000 always ORed into owned1. Gate applies in all/valid/unlocked.
Log: "styles: N of M not shown (DLC checked, owned 0x.. 0x.., unlock check
on|off, .. ms)". Single-unlock keys
(0x5e3260 / 0x5e2c20, vars 0x224c/d) not read. Live x2s on the PC: 380
costumes key (1, 0), the rest DLC keys, none (0, 0).

Unnamed movesets (name = code): NP1/NP3/NP4/SYF/FRI/MAM are race
movesets (named "Human M Default" etc.); CHL/YJB fake; SLM/PGS/YRN/GNE/URN
files missing; MAP, D00, D01, D02 are loose Revamp characters (data\chara,
no chara_X_000 name in the msg). Named by the config's `"style_names"`
(defaults in mod.cpp Config::styleNames, shipped in battle_styles.json).

MAP (CMS id 0x6c, `chara\map\`) is the Majin **Purification** awoken
form: the awoken CUS entry "MAP" (ID1 25004, at 0x18b38 in vanilla
custom_skill.cus) has CharaSwapId1 = 0x6c and SkillsetChange1 = 1, so the
transform swaps the CaC for that character (Kid Buu attacks, its own
skills). The other CUS "MAP" (ID1 192, 0x5c44) is an unrelated entry with
no swap - decode the right one before concluding "no link". Confirmed by
the user in game (Purification replaces the whole moveset).

## Broken movesets / to do: categorise + exclusion list
Not fixed yet (user's call, 2026-10-05). The list currently offers all 218
distinct roster movesets, and some are not humanoid-fighter movesets:
* **Giants** (e.g. Great Ape Vegeta): the CaC lies on the ground as its
  idle and can't move properly. Guess (unverified): giants load their
  model/animations from a different id list or setup than normal
  characters, so the borrowed EAN/BAC doesn't fit a CaC skeleton.
* **NPCs with fake movesets** (e.g. Reception Robot): a broken copy of
  Goku's moveset; the robot has no head or legs, so its animation scales
  those bones and the CaC's head expands without end until off screen.
Wanted:
1. Categorise movesets automatically where possible (e.g. humanoid
   fighter / giant / NPC-or-prop). Ideas, none checked: compare the EAN's
   bone list against the CaC skeleton (missing head/leg bones -> NPC
   prop), giant flags or the transformation/giant data in CUS/BCS/PSC,
   CMS fields, whether the character appears in the character select
   roster (CSO/char select list).
2. An exclusion list in the config that takes **groups** (categories,
   e.g. "giant", "npc") **or single CMS codes**, applied when building
   the offered list (StyleMenu_Build, config `"styles"`).
* 2026-10-05: **per-preset + per-account storage built, not yet run in
  game** (battle-styles-loading-and-online.md "Presets in memory"). To check: save the main loadout to a
  preset, equip another style, load the preset back -> the style returns;
  log lines "slot N: preset A -> B, style X" and "Steam account ...";
  the old file migrates (slots 3/5/7 kept as main presets).
  Gap: deleting a CaC (EditSlot -> 0x81af30) leaves its styles in the file,
  so a new CaC made in that slot inherits them.
* 2026-10-05: **presets confirmed in game**, including the preset picked
  in the character select (battle + CSS preview use that preset's style;
  user: "It works perfectly!").
* 2026-10-05: **Battle Style row in the CSS skill popup and the lobby
  Presets panel built, installed on the PC, not yet run in game** (see
  battle-styles-ui.md "Skill popup / Presets panel UI"). To check: row placement (log has the
  layout dumps), the right preset's style, non-CaCs show no row.
* 2026-10-06, first in-game look: Presets row shows (under the skills) but
  its value was squeezed illegible and had no backing box; the CSS popup
  had no row because xv2ins's own CHARASELE.iggy was loaded, not the cpk's
  (both causes below). Fixed + built, not yet installed / seen.
* 2026-10-11: **movesets with missing files left out automatically** (installed
  on the PC 2026-10-11, not yet seen in game). StyleMenu_Build
  checks `chara/<ean>.ean`, `chara/<bac>_PLAYER.bac`, `chara/<bcm>_PLAYER.bcm`
  (loose data, then the cpk TOCs; `GameFileExists` in cpk.cpp, no reads) and
  drops a set whose file exists nowhere; log "styles: URN left out - ... not
  found". On 1.26.01 + Revamp (PC) that is 5 of 218, all missing the EAN:
  URN (Baba), SLM, PGS, YRN, GNE; every BAC/BCM exists
  (`ClaudeTools\battle_styles\missingfiles.py` lists them offline). A slot that
  already had one equipped plays Default (StyleMenu_PresetCode returns "").
  Giants and fake-NPC movesets still to do.
* 2026-10-13: **fake movesets left out automatically** (installed on the PC,
  not yet seen in game). The user's confirmed fakes: Cheelai XAC, Reception
  Robot HST, Tokitoki TKT, Elder Kai ROK, Shenron XAS. Signal found
  (`ClaudeTools\battle_styles\fakemovesets.py`): every fighter EAN has `_ATC_`
  (attack) animations (Fu 15, Goku 40+); the fakes have none - HST only
  `HST_MOB_*`/`HST_SHP_*` walk/shop, TKT/XAC cutscene `CMN_DEM_*`, XAS/SIN/
  CML/RBL/CHL/YJB a stand pose (1-8 animations). They borrow real BAC/BCMs
  (HST: GHS's, XAC: G18's), so BAC/BCM can't tell. Elder Kai is different:
  ROK = HUF/HUF/HUF, the race's own moveset (all 8 race sets show up under
  NPC codes: ROK FOF NP1 NP3 NP4 SYF FRI MAM). Result: 9 "fake" + 8 "race"
  left out of 213. Config `"exclude_styles": ["fake", "race"]` (default when
  missing) also takes CMS codes (any code sharing the moveset); left-out
  codes already equipped play Default (`g_leftOutStyles`).
  Later the same day the user chose to keep the race movesets: default is
  now `["fake"]`, and a moveset whose EAN, BAC and BCM are all one race's
  is named after the race ("Human F Default" for ROK, "Frieza Race
  Default", ...; `kRaceMovesets` in style_menu.cpp) instead of the NPC.
  Cost: the 213 EANs are 1.2 GB decompressed and every one in the cpks is
  CRILAYLA. CRILAYLA decodes back to front and the EAN name table is at the
  file's end (header +28), so `ReadGameFileEnds` (cpk.cpp) reads the 0x100
  raw prefix (header) + a 64 KB window at the stored end and decodes only
  down to the name table (window x4 if short). Checked against full reads
  (GOK HST XAC TKT XBG HUF SIN BDK byte-identical, ~0 ms each).
  Giants not done: SIM (Cell Max) has attacks but no `_BAS_BUKUU`/`_BAS_BOOST`
  (flight) animation - a possible "giant" signal; OSV (Great Ape) has
  `OSV_BAS_BUKUU_F_S` though, so unproven.
* **Props missing** (Whis's cane, swords, ...): fixed - see
  battle-styles-props.md.

## Phase 5: custom styles from moveset mods (2026-10-14, Gun Fu Rush confirmed in game)
Goal: a moveset mod that replaces a CaC race's (or a roster character's)
moveset installs as an extra style instead, listed under every `styles`
setting, nothing vanilla replaced.

**LB needs folder entries in the .installinfo zip** (first in-game try,
Gun Fu Rush): the converted installer added its CMS/ERS/CMN merges but no
JUNGLE1 file, so the style's EAN was missing and the DLL left it out
("910 left out - chara/910/910.ean not found"). LB's `JungleCheck`
(Install.cs) tests `ZipManager.Exists("JUNGLE1/")` = `ZipArchive.GetEntry`,
which only finds an explicit directory entry; Python's zipfile writes
none. The converter now adds an entry for every folder
(`add_folder_entries`), as LB's own packer does.

**Any input (2026-10-14, offline only):** the converter also reads x2m
characters, bare moveset files and archives (`read_mod`). Findings:
* Windows' own `%SystemRoot%\System32\tar.exe` (bsdtar/libarchive 3.8.4)
  lists and extracts .rar (rar4 + rar5) and .7z; Git Bash's GNU tar can't.
  7-Zip/WinRAR are not on the PC.
* x2m `<Entry>` (eternity CmsEntryXV2) -> CMS binary fields: U_10 = I_16
  (+0x10), LOAD_CAM_DIST, U_16 = I_22, U_18 = I_24, U_1A = I_26, U_1C =
  I_28; I_08 always 0. CHARACTER = BCS, AI = BAI, STR_50 = BDM. Paths
  without "../" are the x2m's own folder (not installed) -> HUM's.
* x2m `<CsoEntry SE=...>` may name a roster bank (Dark King Fu: XBF's) and
  `CHAR_EEPK` a roster EEPK (chara/XBF/XBF.eepk); the JUNGLE then still
  ships an unused CAR_BTL_<own>_SE.
* CSO: "#CSO", count +8, table +0xc, 32-byte entries (chara id +0,
  costume +4, SE name offset +8).
* **cpk keys include `data/`** (`data/system/char_model_spec.cms`); the
  converter's `Game` had looked them up data-relative and silently read
  nothing from the cpks (Gun Fu worked only through Revamp's loose files).
* Download names: Xenoverse Mods prefixes an upload id
  (`1729306997_Kick-Tempest-MAF.rar`, `1789314554921-Kaido_Final.rar`);
  race moveset mods are often just `MAF.ean` + `MAF_PLAYER.bac/bcm/bdm`
  at the archive root.

* **Style XMLs** `<game>\data\battle_styles\styles\*.xml`:
  `<BattleStyle code="910" name="Gun Fu Rush"><Race race="HUM" moveset="910"/>...`.
  `StyleMenu_MovesetFor` picks the line: exact race -> `ANY` -> same sex ->
  first line. Each line's moveset must be a CMS entry with its files; a CMS
  code used only by custom styles is not listed on its own.
* **Converter** `BattleStyles\tools\moveset_to_style.py` (wrapper
  `Convert_Moveset (Drag installers here).bat`): reads an LB installer (`.installinfo` / its exe)
  or loose `data` folders, takes every option of every step, groups
  identical race variants (Gun Fu Rush: HUM=SYM, HUF=SYF) and gives each
  variant a new CMS code ("9" + 2 base36 chars from crc32(name), skipping
  CMS codes and other style xmls). Writes `_Releases\Battle Style - <name>\`
  with the mod's own LB exe renamed + a new `.installinfo`:
  JUNGLE1 = `chara/<NEW>/<NEW>.ean/_PLAYER.bac/bcm/bdm`, `vfx/chara/<NEW>/`,
  `sound/SE/Battle/Chara/CAR_BTL_<NEW>_SE.acb`, the style xml; CMS + ERS
  entries merged; other merges (BSA, shot BDM) kept as is; the mod's own
  CMS/CSO/ERS edits dropped.
* **CMS entry:** numbers copied from the source entry (I_16, I_22..I_28 ...);
  moveset fields (EAN, BAC, BCM, BDM) = the new code's own files; every
  other field points back at the source: the getter maps value "X" to
  `chara/<code>/X` and "../A/B" to `chara/A/B`, so `../HUM/HUM` etc.
* **LB facts (source in `ClaudeTools\XV2-Tools\LB Mod Installer`):** CMS Entry ID
  and ERS SubEntry ID take bindings: `{AutoCharaID=(400;999), SetAlias=X}`
  then `{GetAlias=X}` (ERS sub-entries go through a second binding pass,
  Install.cs InstallSubEntries). **CSO Chara_ID is an int - no binding**, so
  the converter writes no CSO entry and the DLL finds a style's SE bank by
  name: `StyleSeBankOf` = CSO bank, else `CAR_BTL_<code>_SE` when the acb
  exists. JUNGLE1 is copied into data as-is after the Files. The installer
  identifies a mod by Name: running it again uninstalls.
* **Packed effect files:** an EEPK's .emp/.etr files live inside its
  `<name>.pbind.emb` / `.tbind.emb` containers (Gun Fu Rush's 8 .emp are in
  `HUM.pbind.emb`), so the converter's missing-asset check looks inside them.
* **Side installers:** Gun Fu Rush ships optional "Effects Installer for CaCs"
  (race ERS entries) and "Projectiles Installer" (CMN.bsa + shot.bdm merges)
  for drag-and-drop users; the main installer already carries the same files.
* Not seen in game: LB 3.27's CMS/ERS/BSA/BDM merges on 1.26, fit + SE merge
  + effects with a custom code, online with a custom style (the CMS id comes
  from AutoCharaID, so it may differ between two PCs; the code is the same).

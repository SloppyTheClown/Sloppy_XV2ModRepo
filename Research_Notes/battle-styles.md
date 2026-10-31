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

Moved to their own files (2026-10-22):
* `battle-styles-roster.md`: Style names (2026-10-12, built + installed, NOT yet seen in game); Unlocked styles + giants (2026-10-13, built + installed, NOT seen in game); DLC characters (2026-10-13, reworked 2026-10-14; installed, NOT seen in game); Broken movesets / to do: categorise + exclusion list
* `battle-styles-custom.md`: Phase 5: custom styles from moveset mods (2026-10-14, Gun Fu Rush confirmed in game)

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
| MovesetSetupSP | 0x680150 | **0x656020** | exact; setup-list writer, see below |
| CmsChange | 0x2ded2f | **0x2c5597** | inside the CMS path getter 0x2c5610 |
| ErsChange | 0x9bedaa | **0x9563eb** | |
| CsoChange | 0x2afc18 | **0x298d68** | |
| VfxFix (EEPK chara-id getter) | 0x120860 | **0x121430** | field offset +0x15a0 became +0x15c0 |
| CostumeIdCacCheck | 0x6960d3 | unresolved | candidates 0x66b28b / 0x6672f0 (few votes) |
| CostumeFix | 0x695bec | unresolved | candidate 0x668ab2 (20 votes, 160-byte window) |
| Purification fixes | 0x8f686 / 0x158f92 | unresolved | code became a 0x1b9c memcpy near 0x92d00 / 0x155557 |

Globals: 0x1698df8 -> **0x156b558** (battle setup list), 0x1681bc8 ->
**0x1554408**, 0x169d118 -> **0x156eb60**, 0x1681c08 -> 0x15681a8.

## CMS path getter 0x2c5610 (1.26.01)
`char* fn(cms rcx, charaId edx, field r8d, out r9)` - finds the entry with
that id and formats its string field into `out` ("chara/%s_PLAYER.%s"
0xf97c40 or "chara/%s/%s_PLAYER.%s" 0xf97c58, via 0x32a20); returns out or
null. Refuses field >= 13 (0x2c558e). `_PLAYER` is appended for fields 7, 8,
12 (`lea eax,[r8-7]; test eax,0xFFFFFFFA`).
* Entry: `40 53 55 56 57 | 48 81 EC D8 00 00 00` - 5 bytes of pushes, 16-byte
  aligned: hookable with HookCode n=5.
* Only caller: wrapper **0x2c55b0** (`out` = its 0x80-byte stack buffer, then
  "data/%s" into the caller's buffer). Wrapper callers: 0x113e74, 0x113f54,
  0x113f8d, 0x114590 (fn 0x113e00), 0x19e994, 0x2496d8, 0x2982ea, 0x298894,
  0x2b355d, 0x2b358e, 0x2b35bf, 0x3711ac.
* CMS layout: count at +8, table offset at +0xc, 0x54 bytes per entry, id
  +0, code +4, string offsets at +0x20 + field*4. Fields: 0 BCS, 1 EAN,
  3 FCE_EAN, 4 FCE, 6 CAM_EAN, 7 BAC, 8 BCM, 9 BAI, 12 BDM.
* CaCs are ids 100..107 (HUM, HUF, SYM, SYF, NMC, FRI, MAM, MAF).
* A CMS value starting with "../" takes a separate branch: "chara/%s.%s"
  (0xf97c70) from value+3 - the getter strips the "../" itself, so the game
  never puts ".." in a request path on its own.

## Phase plan
1. **Done (confirmed in game):** hook 0x2c5610; config `test_style`
   (CMS code) gives every CaC that character's moveset fields
   (`test_parts`). Checks: does a CaC load and fight with e.g. GOK's
   moveset; lobby/menus with the swapped EAN; skills unaffected.
2. Per-player style: know the player being loaded; carry the style id in
   the setup-list costume field (0x656020) so it syncs online; ERS/CSO/EEPK
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
  - AVATAR.iggy served with `as3\Avatar.as` (through the loader's UI
    library since 2026-10-25, like every Battle Styles menu edit) (button 8 "Battle Style\t" -
    the tab hides msg-id replacement - user data 96 label / 97 count,
    sendType 240). Offline: patched file identical to avatar_abc.py's.
  - EditChara user-data count byte 0x5e -> 0x60; vtable slot 17 hooked:
    240 opens AVATAR_LIST like 0x4fd5a0 (default = equipped), 107 is
    followed by pushing 96/97; poll 0x4fe4b0 hooked: when +0x2f8 goes
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
    seen in live request objects, vtable 0xfa92f0) - the suffix compare
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
    +0x160/+0x168, 16-byte {valid, type, value}; SetUserDataStr 0x2c8340
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
  run in game.** Hook 0x16c060 (battle-styles-loading-and-online.md) sets the loading player; in battle
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
  0x5e7210 added (battle-styles-loading-and-online.md), built, not yet run.
* 2026-10-05 20:22 run: **CSS preview per slot confirmed** (user: "working
  great"). Log while browsing Offline Battle CSS: "chara 106 ... (character
  select, save slot 5) -> fit/GRD.MAM.ean", slot 7 STN, slot 3 GNY, slots
  0/1/2/4/6 "own moveset" - menu +0x450 is the previewed CaC's save slot,
  never -1 there. So per-character (phase 4 + battle + CSS) is done offline.
  Still on the loaded slot's style: lobby, other menus using 0x65be10
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

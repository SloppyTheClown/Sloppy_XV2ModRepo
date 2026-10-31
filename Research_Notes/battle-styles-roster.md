# Battle styles: which movesets are listed

Covers: style names, unlocked styles + giants, DLC characters, broken movesets and the exclusion list.
Split out of `battle-styles.md` (2026-10-22).

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

**Unlocked: the character select's check_unlock.** Called at 0x5e5612:
setup `44 8B 87 3C 04 00 00 48 8B CF 8B 97 38 04 00 00` (r8d = costume,
rcx = menu, edx = CMS id), call at 0x5e4af2. XV2Patcher's `UnlockCharaMods`
hooks that call (so it can point elsewhere - follow the E8 target), and
`unlock_chara_all` overwrites it with `41 B8 01 00 00 00 90 90` (= all
unlocked). The original 0x5e3090(menu, CMS id, costume) reads only menu
+0x27e8, +0x444, +0x27e4; its sub-checks 0x5e4c70, 0x5e47f0, 0x5e4aa0
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
masks come from 0x5e6ae0: owned1 = 0x5e4590(menu) (call at 0x5e6c35, iggy
var 0x224a; reads only menu +0x27e4; bit 0x1c = raid bosses only in a raid
quest, type 0xf), owned2 inline from 0x5e6c4a (var 0x224b): pack ownership
0x2b9d50(pack, 0) for 0x2e/30/32/37/39/3b/3d/40 and update level 0x2b9970()
(clamped TU version from [0x156efac]) >= 0x13..0x27. Battle Styles reads
the slot keys from the x2s fields 7/8 (eternity Xv2PatcherSlotsFile dlc
low/high) or the CST +0x14/+0x18.
**2026-10-14 rework (one way, every mode):** Battle Styles no longer copies
the mask logic or knows about other mods. owned1 = the live call target at
0x5e6c35 called directly; owned2 = the game's own block 0x5e6c4a..0x5e6d7c
run in place: a stub (RWX page) pushes rbx..r15, sets esi = 0 / r14d = 1
(the block's inputs, 0x5e6b06 / 0x5e6b99), jumps in; a hook at 0x5e6d7c
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
  Gap: deleting a CaC (EditSlot -> 0x828ed0) leaves its styles in the file,
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

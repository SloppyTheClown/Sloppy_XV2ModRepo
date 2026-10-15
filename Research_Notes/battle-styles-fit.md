# Battle styles: fitting a borrowed moveset to the CaC's race

Covers: BattleStyles' fit_to_race - a style's EAN, cam EAN, BAC and
BDM rewritten per race (`src\ean_fit.cpp`): pelvis / leg-length scaling,
intro bone-length tracks, the merged race + style EAN (build 5), merged
BDM (build 6), the race tail track merge, the keyframe alignment crash,
race-only BAC entries. Overview: battle-styles.md; sounds/effects:
battle-styles-sound-and-effects.md; props: battle-styles-props.md.

## Borrowed EAN on a CaC (fit_to_race)
In game (FRI CaC, test_style GOK): the CaC floats a little and the neck looks
stretched; the tail does not move.
* **Floating:** every animation stores b_C_Pelvis as an absolute position, so
  Goku's file lifts a shorter race. All 92 GOK animations have a pelvis
  position track. Other bones with position tracks in every animation:
  b_C_Base, g_R/L_Foot, g_C_Pelvis, g_R/L_Hand, g_C_Head, g_x_LND (one GOK
  animation has position tracks on all bones).
* Idle pelvis height (b_C_Pelvis Y, first key of <EAN>_BAS_STAND; MAM.ean's
  is BUL_BAS_STAND), 1.26.01 + Revamp, `ClaudeTools\battle_styles\racepelvis.py`:
  HUM 0.6162, HUF 0.5791, SYM 0.6650, SYF 0.5322, NMC 0.7969, FRI 0.6030,
  MAM 0.6206, MAF 0.6533; GOK 0.6450.
* Dead end (test 2, 2026-10-05): marking the path "chara/FRI/../GOK/GOK.ean"
  - the game resolves ".." and requests plain `data\chara\GOK\GOK.ean`
  (backslashes), once, shared by every character that uses it. So a fitted
  copy needs a real file name of its own; the file-request hook cannot do it.
  All 8 race EANs are requested at the CSS/lobby whether used or not.
* Fix built (build 3, `src\ean_fit.cpp` + `src\cpk.cpp`, config
  `fit_to_race`): the getter hook, on a CaC's first EAN request per session,
  reads the style EAN (loose data first, else the cpks - C++ port of
  XV2DataEdit cpk.py, byte-identical on FRZ/SYM/BRL/GOK), scales its pelvis
  position keys (build 3: race / style idle height; build 4: race / style
  leg length, see below), writes
  `data\battle_styles\fit\<STYLE>.<RACE>.ean` (only if the bytes differ),
  and returns that path. Same inputs -> same file on every machine.
  Per-CaC height/width are not used: native CaCs share one race EAN at all
  sizes, so the game applies the body scale on top (confirmed in game, build 4).
* **Stretched during the battle intro:** GOK_DEM_START positions 114 bones
  (every other animation only the 9 above), forcing Goku's bone lengths onto
  the CaC. The fitted copy drops every position track on a bone
  *_BAS_STAND does not position (GOK: 110 tracks) by taking it out of its
  node's track table; the CaC's own skeleton then gives those lengths.
* **Build 3 in game (2026-10-05):** intro stretch gone; FRI and MAM CaCs
  still float well clear of the ground, although the fitted idle pelvis
  matched the race's exactly. Cause: legs only get rotations, their lengths
  come from the CaC's skeleton. Goku idles with knees bent (pelvis 0.645 on a
  0.851 leg); the same angles on FRI's 0.622 leg end the feet ~0.13 up.
  Build 4 scales by leg length = |b_L_Leg2| + |b_L_Foot| + |b_L_Toe.y| from
  each EAN's skeleton block (skeleton +12: 48-byte transforms), race EAN =
  the CaC's own CMS path. GOK -> HUM/SYM x1.000 (same skeleton), HUF/SYF
  0.800, NMC 1.202, FRI 0.731, MAM 0.699, MAF 0.824. Caveat: ankle-to-sole
  height is not in the skeleton (MAM idles with the pelvis 0.621 on a 0.595
  leg), so a race with big feet may sink/float slightly.* **Neck stretch:** user: may just be how the animation looks; parked.
  Cause unknown. EAN skeleton rest values differ (chest Y
  GOK 0.079 vs FRI 0.024, neck 0.146 vs 0.104; otherwise FRI is about 0.72x
  GOK). Re-check after the pelvis fit; next suspect is the g_C_Head position
  track or the borrowed skeleton block's rest transforms.
* **Tail - race track merge (2026-10-12, built + installed, NOT yet seen
  in game):** `ean_fit.cpp` `AppendTailNodes`. HUM/HUF (Saiyan tail of
  SYM/SYF) have the same `X_T_TAIL1..10` chain as FRI; NMC/MAM/MAF none.
  Every race base animation keys all 10 bones except id 2 (WALK) and 78
  (`ClaudeTools\battle_styles\tailprobe.py`). For each style animation, tail bones
  it doesn't animate get nodes resampled from the race: the same base id
  when the race one has the tail and (D-1)/(T-1) is within 0.5..2
  (stretched, so loops stay loops), else race id 0 (BAS_STAND) looped with
  period D-1. One key per frame, in the style's key sizes; still tracks
  (scale) get 2 keys; quaternions lerped + normalised. New node table
  appended after the animation block. Offline (`ean_fit_test`): GOK:FRI and
  GOK:HUM give all 92 animations the 10 bones, first/last keys valid, float
  values 16-aligned; FRI:HUM keeps Frieza's own tail (2 animations filled).
  Log note ends "race tail (10 bones) added to N animations (M nodes)".
  First in-game report: tail "wiggling fast" in the save slot screen and
  lobby - not this merge: the log shows nothing served before the character
  select (lobby is excluded since 4b), and those screens load the race's own
  EAN (FRI from the cpk; HUM/HUF/SYM loose ones are Revamp's, 2025). The
  user dropped it ("probably my imagination"). In battle the merged tail
  is "serviceable for now" (2026-10-12) - SCD physics stays the fallback
  idea if it ever needs more.
  Earlier notes:
  FRI.ean has 160 bones incl. the tail,
  GOK.ean 130, so a borrowed EAN never animates the tail. Idea: merge the
  race's own tracks for bones the style lacks (e.g. FRI_BAS_STAND's tail
  loop into every animation, or a matching race animation by name).
  Tail bones: `X_T_TAIL1`..`10` (one chain, in FRI.ean, not in GOK.ean).
  Vanilla FRI tail has no SCD; it is keyframed only.
  * **Physics option (2026-10-12, offline look only):** BCS physics
    objects take an SCD (Files EMD/EMM/EMB/EAN/bone/SCD; Xv2CoreLib only
    resolves the path, no parser). SCD magic `#SCD`, small (~3 KB), names
    its chain bones `SCD_<name>_<chain>_<link>` + anchor-prefixed copies
    (`ASCD_`/`CSCD_`); they are bones of the physics object's own ESK
    (e.g. `fri_310_mant_scd.esk`). Closest tail template:
    `buu_000_tentacle.scd` (3332 B, one 8-link chain anchored on
    b_C_Head). 285 vanilla SCDs (obi, mant, hair, skirt...). Would need:
    tail mesh cut out of the body EMD into a physics object (as for props),
    bones renamed to the SCD chain, an SCD written or cloned, and the body's
    own tail hidden. Format and whether SCD swings unanimated bones not proven.
* `ClaudeTools\battle_styles\eanprobe.py STYLE RACE [anim]`: skeleton rest and
  first position keys side by side; `racepelvis.py [codes]`: the idle-height table above (build 3 only; build 4 no longer uses it).

### Build 5: merged fit (2026-10-11, confirmed in game)
User report: Turn Giant plays wrong animations or can't work; idle only:
NMC + ZBN hands flipped 180 deg into the wrists, teeth float in front of the
face with almost every style; MAM + Ribrianne face upside down (mouth on the
forehead); MAM + ZBN wrists flipped and head/torso scrunched into the body.
A modder's explanation (quoted by the user): Turn Giant reads the
character's own EAN at fixed ids; Namekian/Majin CaCs have extra arm bones
for stretching. Measured with `ClaudeTools\battle_styles\eantracks.py STYLE RACE
[anims]` (STYLE may be a file, e.g. an `ean_fit_test` output):
* **Race EAN ids:** every race has the same base set at 0..81 (BAS_STAND ..
  BAS_STAND_01_S), then its own transformation set: HUM/HUF 117 animations
  (KIN, BEA), SYM/SYF 97, NMC 138 (GNT at 82..125), FRI 105 (GRD), MAM 96
  (MTF, BEA), MAF 98. A roster EAN uses 82+ for its own moves (GOK 82 =
  NAG_A, DEM_START ...), so with a borrowed EAN Turn Giant played those.
  Race cam EANs have 3 animations (0 START, 1 the transformation camera,
  2 `*_CAM_000_MGT`); roster cam EANs (ZBN, GOK) only id 0.
* **Idle only:** build 4 kept position tracks on every bone *_BAS_STAND
  positions - but style idles position nearly everything (ZBN_BAS_STAND
  116 bones; VGT, ZMG, EIB, G13 position all 46 face bones). Race body EANs
  never animate `f_*` bones (the face EAN does), so the style's own face
  rest (teeth, mouth) was forced on the CaC. NMC/MAM/MAF hang b_*_Elbow off
  x_*_ArmZoom5 and b_*_ArmRoll off x_*_ArmZoom10 (ZBN off b_*_Arm1/Arm2),
  so ZBN's elbow/forearm positions land in the wrong parent frame. Rest
  rotations of shared bones do NOT differ ZBN vs NMC (dead end: the flip
  is not a rest-frame rotation difference).
* **Fix (`src\ean_fit.cpp`):** the fitted EAN is the race's file (header +
  skeleton, its animations) with the style's put over it: style ids < 82
  replace the race's, style ids >= 82 move up by (race count - 82) to sit
  after the race's, other ids keep the race's animation (so GNT/KIN/GRD/MTF
  are there at their ids). Bone indices are remapped by name. Tracks: none
  on bones the race EAN never animates; position tracks only on b_C_Base,
  b_C_Pelvis, g_* and on bones the race positions too whose parent chain
  (by name, root excluded - ZBN_BAS_BONE vs NMC_BAS_BONE, MAM's is
  BUL_BAS_BONE) matches; pelvis keys x leg ratio as before, other kept
  position keys x race rest length / style rest length (ZBN's b_L_Hand
  0.282 vs NMC 0.303, MAM 0.313). Cam EAN merged the same way (base 1, no
  track rules). The style BAC (`*_PLAYER.bac`) gets type-0 records on the
  character EAN (EanType 5) with id >= 82 and type-10 records on the
  character cam EAN (EanType 4) with id >= 1 shifted by the same amounts,
  written as `fit\<STEM>.<RACE>.bac`, only when the EAN fits were used and
  moved something. Offline: `build\Release\ean_fit_test.exe <game> <out>
  ZBN:NMC GOK:HUM ...` (tests\ean_fit_test.cpp). ZBN -> NMC: 692 tracks +
  272 nodes dropped, BAS_STAND 116 -> 65 bones, no face; GOK -> HUM: 8 BAC
  refs moved up 35 (NAG/DEM at 117+), PIC -> NMC: arm stretch kept, refs
  at 138+; every shifted ref resolves in the merged EAN.
* **Unverified:** a style without face tracks (GNY) still had floating
  teeth; guess: the game takes the EAN skeleton's rest for unanimated bones
  (GNY ToothTop rest z -0.0047 vs NMC 0.0225), fixed by the race skeleton
  now. Ribrianne's code not found (not RBR/RIB). Tail still not animated
  (the race's tail tracks aren't merged into style animations).
* **In game:** MAM + ZBN/Ribrianne idle fine; NMC Turn Giant plays every
  animation right with ZBN or ZMS.

### Build 6: merged BDM + character SE dropped (2026-10-11, NOT yet seen in game)
User report after build 5: NMC Giant's 1st and 3rd weak swing and its strong
attack hit nothing (ZBN and ZMS styles); ZMS spams wrong sounds on almost
every move. Measured with `ClaudeTools\battle_styles\bacrefs.py BAC [BDM ...]`
(per BAC entry: hitboxes as BDM file:id, sounds as ACB:cue; then which
character-BDM ids each BDM lacks; BAC/BDM may be files on disk):
* **Turn Giant files:** `skill/MET/002_GOK_GNT/`: 002_GOK_GNT.bac (the
  transformation), 002_GOK_GNT_AFTER.bac (the giant's moves),
  002_GOK_GNT_PLAYER.bcm, 002_GOK_GNT_AFTER_PLAYER.bcm (refs BAC 0, 300-302,
  330, 360, 362, 375, 377, 460, 461, 600-602). The AFTER BAC's hitboxes use
  the **character** BDM (type 1 record, low nibble of +19 = 1) ids 0-4 and
  35-42 - all in NMC_PLAYER.bdm (0-15, 35-43), none of 1-4/35-42 in
  ZBN_PLAYER.bdm (only 0) or ZMS's. So the style's BDM replaced the race's
  and the Giant swings found no damage entry.
* **Sounds:** `system/chara_sound.cso` (32-byte entries: chara id,
  costume, SE, VOX, AMK, skill code) gives CaCs 100-107 their race's
  CAR_BTL_*_SE bank (SYM none); a roster BAC's type-11 records on AcbType 2
  (character SE) play cue ids of the *style's* bank in the race's bank.
  ZMS_PLAYER.bac has 39 (cues 0-6), ZBN 4, GOK 7, PIC 3, NMC/BRL none. No
  way found to point a BAC at another character's bank.
* **Fix (`src\ean_fit.cpp` FitBdmToRace, FitBacToRace):** CMS field 12
  (BDM) is served as `fit\<STEM>.<RACE>.bdm`: the race's header + entries,
  then the style's; a style id the race uses moves to max(both)+1 upwards
  (BDM header 16 bytes, count +8, entries +12, 1284- or 1084-byte entries,
  id i32 at +0). The fitted BAC (now always made) points character-BDM
  hitboxes at the moved ids and drops AcbType-2 sound records (record count
  at type list +2 lowered, the array compacted). Trade-off: the style's own
  swing/whoosh SE is gone; voice (AcbType 3) and skill/common SE stay.
* **Offline:** ean_fit_test also writes `STYLE_PLAYER.RACE.bdm`. ZBN:NMC
  26 entries, ZMS:NMC 44 (3 ids moved, 8 hitboxes remapped, 39 sounds
  dropped), GOK:HUM 27 (9 moved), BRL:MAM 153; bacrefs on the merged BDMs:
  the AFTER BAC's ids 0-4/35-42 all present, the fitted ZMS BAC's ids all
  present, no character SE left.
* Ribrianne's code is probably BRL (its fit to MAM matches the report).
* **In game:** Giant hitboxes fixed (NMC + ZBN/ZMG). The user's "Zamasu"
  style is **ZMG** (Fused Zamasu, chara 142); its CSO SE bank is
  CAR_BTL_ZMS_SE (VOX CAR_BTL_ZMG_VOX). The user rejected dropping the SE.
* **Which sounds collide** (`ClaudeTools\battle_styles\acbwave.py ACB OUT CUE..`
  extracts cues to .wav; ffmpeg decodes the HCA): ZMG light combo BAC
  300-304 play character SE cue 0, other moves cue 2. CAR_BTL_ZMS_SE cue 0
  KenFuri (sword swing), 2 KenFuri_L; CAR_BTL_NMC_SE cue 0 M002_GNT_Furi,
  1 GNT_Furi_2, 2 GNT_Henshin_End, 3 GNT_Henshin_Start, 4 GNT_Makouhou,
  5 GNT_Shout. Turn Giant itself plays NMC bank cues 2, 3 (002_GOK_GNT.bac)
  and 7, 9 (AFTER bac) - so switching the CaC to the style's bank breaks
  the Giant; both banks are needed (merge, like the BDM).
* **Missing beam sword:** ZMG 300-304 spawn character-EEPK (BAC type 8,
  48-byte record, EepkType +8 = 2, effect id i32 +16) effects 1 and 12
  (304: 1, 10). The CaC's character EEPK is the race's
  (`vfx/vfx_spec.ers`: chara/NMC/NMC.eepk; ZMG -> chara/ZMG/ZMG.eepk).
  NMC_PLAYER.bac, HUM_PLAYER.bac and both Turn Giant BACs use no
  character-EEPK effect. The reference mod redirects ERS (0x9563eb) and the
  EEPK chara-id getter (0x121370) for this.

### Crash fix: keyframe alignment (2026-10-11, built, NOT yet seen in game)
* User report: Cellin (CLN) and Videl (VDL) styles crash in battle; game a
  bit slower at times. Dumps in `%LOCALAPPDATA%\CrashDumps` (cdb `-z`, see
  data-files-and-xv2dataedit.md): both in xgcore
  `AnimationSequencer::CalcNodeCurrentPosition` on `movaps` from an address
  8 mod 16, 18-35 s into the fight.
* Cause: float keyframes (value size 2) are read with movaps, so they must
  be 16-aligned in the file. Both styles' EANs are loose (mod-made) and start
  some animations at 8 mod 16 (VDL 2 of 82, CLN 51 of 92); their keys are
  aligned in the source. The fit copied every animation to a 16-aligned
  offset, moving those keys off by 8 (VDL fit 72 tracks, CLN 3029). cpk EANs
  start every animation at 0 mod 16, so they never showed it.
* Fix (`src\ean_fit.cpp`): each copied animation keeps its source offset
  mod 16. `ClaudeTools\battle_styles\eanalign.py EAN..` lists misaligned float
  tracks; offline VDL/CLN -> NMC now 0, ZBN.NMC.ean byte-identical to before.
* Same session, separate crash (20:15, exe 0x9bba67, ~4 min in): first put
  down to the URN style (Fortune Teller Baba, chara 205; its CMS EAN
  "chara/URN/URN.ean" exists nowhere - still a battle-styles.md "Broken movesets" case). It
  was the SE bank merge instead, see battle-styles-sound-and-effects.md "Cue limit crash".

### Race-only BAC entries (Dual Ultimate rush) (2026-10-11, built, NOT yet seen in game)
User report: a styled CaC's Dual Ultimate initiating melee rush has no
hitbox (any Dual Ultimate). Measured offline (`bacrefs.py`, a BCM/BAC dump):
* Dual Ultimates are `skill/ULT/<id>_..V` (e.g. 008_GOK_CKMV, 164_VGT_FNFV;
  names "DUAL ..." in proper_noun_skill_ult_info, category "Dual Ultimate"
  in proper_noun_skill_table_name). Their BCM points at skill BAC 0/1/10/11/
  21/22; entry 0 plays character-EAN animations 4, 11, 19, 1 and has **no
  hitbox** (types 0,2,3,4,5,7,8,10,11,12,15,19,23,24); only 1/11 hit
  (common BDM 300). So the rush's hit lives outside the skill files.
* CaC race BACs have 901 entries (NMC_PLAYER, HUM_PLAYER), roster ones
  fewer (CLN 723): 475-529, 600-632, 725/726, 770-773 exist only for CaCs
  (490 common BDM 465, 501 466-468, 505 480/481, 512-515 490/491 ...). The
  fitted BAC was the style's, so all of them were missing (or flagged empty
  = 0x80000000) - the exe calls some of these by number. Which one is the
  rush was not pinned down.
* **Fix (`src\ean_fit.cpp` AddRaceEntries, from FitBacToRace):** every
  entry the race BAC has and the style's lacks (past its count or flagged
  empty) is copied in as it is: type list + record arrays appended (record
  array length = up to the next type list / record array / entry table, as
  Xv2CoreLib does), a new entry table at the end when the count grows. Only
  with the body EAN merged: race animation ids, character-BDM ids and
  character SE cues all keep their ids in the merged files, so the copies
  need no remapping. Style entries are never replaced.
* Offline (ean_fit_test also reads the race BAC): CLN:NMC 39 race entries
  added, ZMG:NMC 41, VDL:HUM 35; fitted BACs have 901 entries, the copies'
  hits/sounds/effects identical to the race's (490, 501, 505, 726, 770
  checked), style entries unchanged apart from the old remaps.

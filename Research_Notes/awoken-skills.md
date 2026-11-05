# Awoken skills (transformations)

Covers: what a CUS awoken skill swaps (model, skillset, moveset), skillset
presets, the after BAC/BCM paths, the files-loaded flags, shipping fixed-path
skill files in an x2m, Purification's parts; Refinement_Transformation
(was XV2_PurificationControlled; workspace root, outside the repo) state.

Found by reverse engineering the game's 1.26.01 data, with
`ClaudeTools\purification\cus_awoken.py` (dumps every awoken CUS entry from
the cpks), `cus_skillsets.py` (skillset presets per char id) and `ClaudeTools\purification\ean_names.py` (an EAN's animation
names by id), the XML serializer for BAC/BCM, and eternity's
`X2mFile.cpp` / `CusFile.cpp` (`ClaudeTools\eternity_common`) for what xv2ins
does with an x2m.

## The CUS entry (custom_skill.cus, awoken section, 92-byte entries)
Layout from Xv2CoreLib `CUS\Parser.cs`; field names as the x2m `<Skill>`.
* PATHS (7 strings): EAN, CAM_EAN, EEPK, SE, VOX, AFTER_BAC, AFTER_BCM.
  NULL = the default file in the skill folder. The game builds the after
  paths as `data/skill/%s.bac` / `data/skill/%s.bcm` (format strings in the
  exe), so the value has no `data/skill/` and no extension.
* U_0E = files loaded flags: CamEan 0x2, Eepk 0x4, CharaSE 0x10,
  CharaVOX 0x20, Bdm 0x400, CharaSpecificEan 0x800, AfterBac 0x2000,
  AfterBcm 0x4000. The skill EAN (and `_F.ean` for female races) loads
  without a flag: vanilla MAP has 0x22 and plays 004_GOK_MAP(_F).ean.
* TYPE: BAC 0x10, BCM 0x20, EAN 0x40 (+ flags 1, 2, 7 on every awoken one).
* MODEL (chara swap) = a CMS id: the character becomes that roster
  character - body, EAN, BAC, BCM, BDM, all of it.
* CHANGE_SKILLSET = N loads preset N of the CaC race's skillset in
  custom_skill.cus (32-byte entries, count i32 @8, offset i32 @12: char_id
  i32, costume i32, 9 skill u16 - 4 supers, 2 ultimates, evasive, blast,
  awoken - then the preset index u16 @26, which is what N matches;
  `ClaudeTools\purification\cus_skillsets.py` dumps them). 0xffff = keep
  the CaC's skills.
* **Partial skill swaps**: a preset slot can say "keep the CaC's own":
  65534 for supers, 4998 for ultimates, 9998 for evasive, 24998 for awoken.
  FRI preset 1 (vanilla Golden Form) keeps everything and changes only the
  blast (21085); NT's Turn Golden uses CHANGE_SKILLSET 2 = the existing FRI
  preset 2 (blast 21091). Majin (106/107): preset 1 = Kid Buu's full kit,
  preset 2 = keep-all + blast 21091 (MGT's, as FRI preset 2); **no Majin preset
  has only blast 21086**, so a Purification with only the MAS blast needs a
  new preset. A skill x2m cannot add one (eternity `X2mFile.cpp`: SkillSet
  is only for NEW_CHARACTER), and custom_skill.cus in JUNGLE would wipe other
  mods' presets (data/system is in xv2ins's dangerous-jungle list).
* PUP_ID (powerup_parameter.pup) and AURA are plain ids.

Vanilla awoken skills with after files (`cus_awoken.py`): KIN, GNT, GLD,
SGM, SGF, VMG (uses GLD's), SFM. None swaps the model; their after BACs play
every moveset animation from the **Character** EAN (the CaC's own race EAN),
hit through the Character BDM (Turn Giant: NMC.ean ids 82..125, NMC_PLAYER.bdm
ids 0-4 and 35-42). No vanilla after BAC uses the Skill EAN or Skill BDM,
and **in game they don't reach the awoken skill's files** (Purification
(Controlled) 1.0): attacks lunged with no animation or hitbox, idle froze on
one frame, and supers' animations replayed at idle - the Skill EAN looks like
whichever skill was used last.

An after BAC **overrides per entry**: only its non-empty entries replace the
character's (KIN_AFTER 697 entries / 119 used, GNT 901 / 142, SFM 913 / 155;
MAP_PLAYER.bac 887 / 164 vs MAM_PLAYER.bac 901 / 179). Entries MAP lacks
(211, 212, 250-257, 284, 285, 290, 293, 376, 378, 470, 475, 476, 487, 496,
501, 503-510, 512-515, 725, 726, 770-773) stay the CaC's.

## Purification (MAP, id1 25004, id2 4)
race_lock 0x90 (MAM|MAF), type 0xf6, U_0E 0x22, PUP 108, AURA 14,
MODEL 108 (chara MAP = Kid Buu), CHANGE_SKILLSET 1, no after paths.
* Moveset: from the model swap - MAP.ean (106 animations, BUS_* names),
  MAP_PLAYER.bac/.bcm/.bdm. The invincible block and the other moveset
  quirks are in that BAC/BCM.
* Skills: MAM/MAF skillset preset 1 = Kid Buu's kit: supers 371/373/372/593,
  ults 5370/5371, evasive 10370, blast 21086 (MAS, the special ki blast),
  awoken 25004. MAP's own preset 0 holds only blast 21086. So **the special
  ki blast belongs to the skill swap**, not the moveset or the buff.
* Buff: PUP 108, which also gives the hidden 5-star super soul 218 (talisman
  effect type 5, activation 1, multiplier 0.3).
* Skill files: 004_GOK_MAP.bac (the transformation; plays Skill EAN 0/1 =
  MAM_MTF_JYN_000/001, and Character 0 - Kid Buu's stand after the swap),
  .cam.ean, .ean / _F.ean (only those 2 animations), _PLAYER.bcm.
* MAP_PLAYER.bac references: animations 273 Character, 6 Common,
  9 CommonTail, 248 FaceA, 24 FaceForehead (face ids are laid out the same
  for every character); hitboxes 17 Character BDM, 74 Common; effects
  Character/Common/StageBG EEPK (ERS maps CaC 106/107 to MAP.eepk); cameras
  26, one on the Character cam EAN.
* EAN sizes: MAM.ean 96, MAF.ean 98, MAP.ean 106 animations; the CaC races
  share ids 0..81 (Battle Styles `kRaceBaseAnimations`).

## Shipping skill files at a fixed path in an x2m
xv2ins copies the CUS PATHS verbatim and renames `SKILL/NNNN_CCC_SSSS*` into
`<id2>_<cms of id2/10>_<skill name>` in a folder it picks at install, so an
after path cannot point into the x2m's own skill folder (an x2m that ships
`SKILL/..._AFTER.bac` with paths at a vanilla skill's after files installs
copies nothing reads). Files under `JUNGLE/` are copied verbatim into
the game folder: New Transformations' "Turn Golden (Final Form).x2m" ships
`JUNGLE/data/skill/MET/LB_New_Transformations/LB_GFF_AFTER.bac` +
`..._AFTER_PLAYER.bcm` and points its paths there.

## Refinement_Transformation (was XV2_PurificationControlled)
Renamed to "Refinement" in 1.3 (skill RFN, blast copy RFB, after paths
`MET/Refinement/RFN_AFTER*`); the notes below keep the 1.0-1.2 names.
1.3's pink aura: effects-and-vfx.md "Refinement's pink aura".
`build.bat` writes `_Releases\Purification (Controlled).x2m` (skill PUC):
vanilla MAP's CUS fields without MODEL/PARTSET/CHANGE_SKILLSET, U_0E 0x6022
(CamEan, CharaVOX, AfterBac, AfterBcm), after paths
`MET/PurificationControlled/PUC_AFTER` and `..._PLAYER` in JUNGLE.
* 1.1 (Turn Giant's pattern, after 1.0's Skill refs failed): the race files
  get Kid Buu added, shipped in JUNGLE: MAM.ean / MAF.ean = own animations,
  padded to 100 with copies of 004_GOK_MAP(_F).ean (MAM 96 + 2x2, MAF 98 + 2),
  then MAP.ean fitted with Battle Styles' FitEanToRace (base 0 = append) at
  MAP id + 100; MAM/MAF_PLAYER.bdm = own entries (149 / 145, ids < 263) +
  MAP_PLAYER.bdm's 19 at id + 300. After BAC = MAP_PLAYER.bac with Character
  EAN ids +100 and Character BDM ids +300; after BCM = MAP_PLAYER.bcm. Skill
  BAC = 004_GOK_MAP.bac with only its Character 0 stand -> 100; skill
  EAN/cam/BCM vanilla.
* Works without Battle Styles: its fitter is only compiled into
  `tools\fit_eans.exe` at build time. With Battle Styles installed its merged
  race EAN/BDM are built from the loose race files, so ids 100+ / 300+ should
  survive - not checked.
* JUNGLE race files replace any other mod's loose MAM/MAF EAN/BDM (xv2ins
  removes them on uninstall).
* x2m trade-off: no MAS ki blast (21086) - needs a new Majin skillset preset
  (above). Character SE/VOX in the BAC play from the CaC's banks.
* 1.1 (x2m) confirmed in game, except the ki blast.
* 1.2 = LB Mod Installer package: adds MAM/MAF preset 10 (keep-all + blast)
  through a CUS xml (LB merges Skillsets by chara_costume_preset and removes
  them on uninstall). Preset swap + uninstall confirmed in game. **Blasts
  play Character animations too**: 1086_cmn_mas.bac plays Character 96..100
  (MAP.ean's MAM_BLT_000/001_JYN_*), so with the vanilla blast the CaC played
  whatever its race EAN has there (the transformation intro). The package
  installs a copy of the blast (new CUS blast entry, AutoSkillID Blast) with
  those ids +100; confirmed in game (kick + shot). Its leg portal is
  Character EEPK effect 1 (MAP.eepk Leg_Warp, on b_L_Foot), which a Majin
  CaC loads too (vfx_spec.ers maps 106/107 to MAP.eepk); the leg "vanishes"
  by scale (b_L_Leg2 Y 0.017, b_L_Foot Y x15), which survives the EAN fit.
  On a Majin the portal looked like it was inside the torso (ground and
  air): the foot's transform matches Kid Buu's, but male Majin pants are
  ~1.2 x 1 m (Kid Buu 0.4 x 0.25) and the ring ~0.8 x 0.6 m, so the leg
  hides it. The copy ships its own eepk (MAP.eepk with the portal x2,
  KiBlastSkill effects, FilesLoaded Eepk) - confirmed in game. LB order gotcha: within one cus
  xml, Skillsets are resolved before the skill lists, so an alias set by a
  skill's AutoSkillID must come from an earlier file entry.

## BAC facts from the Refinement rework (2026-11-03)
Function types are printed in decimal by the serializer; names are in `Xv2CoreLib\ValuesDictionary\BAC.cs` `BacFunctionNames` (hex): 13 = Activate Transformation, 6 = Invisibility, 35/36 = Spawn/Knock Away Pebbles, 40 = Invisible Opponents. `AuraType` in the same file also has a 6 (HenshinEnd), which is unrelated. Vanilla 004_GOK_MAP pairs Function 13 with Function 6 and Common 612 at the end of its entry.

An effect Off placed past the animation end never runs, so LoopCancel parts keep looping after the transformation: keep Offs before Function 13 and the last frame.

A BCM link entry is only checked inside a BAC BcmCallback window, and `InTransformedState` is evaluated then: for a transformation whose intro chains into a second action through the BCM, Function 13 must come before the callback (entry 0: Function 13 at 274, callback 275). The reverse order silently skips the second action. Found from the data; check the result in NEXT_STEPS.

Skill files with their own id inside: an installer replaces 0xBACA (47818) with the installed id in BAC and BDM `Skill_ID` (LB `InstallSkillFolder` -> `ChangeNeutralSkillId`, also .bsa; xv2ins `ChangeReferencesToSkill`), renames `NNNN_CCC_SSSS` in file names, and copies the rest verbatim. So a skill edited in place needs its id back to 0xBACA before it ships, and the eepk's container names (`<prefix>.ptcl.emb`) need a fixed name, which works in a skill folder.

A skill's `_F.ean` can be regenerated from its male `.ean` with `Tools\EanRaceFit` (Battle Styles' fitter as a standalone tool, `skill` mode). Confirmed in game (female Majin, 2.0).

A BAC Camera event's Duration must cover the whole entry when the next action links at the end: entry 0's camera ran 250 of 276 frames and the gameplay camera came back for the last 26, which XenoKit does not show.

**The ki charge is not in an after BAC (2026-11-04, tested in game):** sounds + steam added to after BAC 68-73 (`BUS_BAS_STUN_*`, BAC.cs "Recover KO") changed nothing on a charge. Charging is a super skill with its own BAC, so a transformation's after BAC never runs for it. `ClaudeTools\refinement\bac_kiaicharge_scan.py` lists every BAC with a KiaiCharge aura event. The charge family:
* All share one pattern: Ultimate Charge (598 KTU, 2093 KTG), Burst Charge (599 KTB), Indomitable (2321 KFC), Energy Charge (591, 1105), Full Power Charge (592), Maximum Charge (593, 1123, 2343), Final Charge (1573), Pretty Charge (1564). 598/599 use the EAN + SE of 593 (`SPA/593_APL_KTH`, `CAR_BTL_S593_KTH_SE`); 2321 uses the SE only. That EAN has 2 animations, `APL_SPA_KTN_C` (0) and `APL_SPA_KTN_S_C` (1), 140 frames, played in 15-frame looping pieces. Which of 0/1 is ground or air wasn't checked.
* Events: `Character_VOX` cue 88 @0, `Skill_SE` cue 1 @16 (StopWhenParentEnds), StageBG effects, Functions 35 + 4 (+22 in KFC) every 15 frames from 25, `Aura KiaiCharge` @25. Ending: `Aura KiryokuMax`, `Aura KiaiCharge` + DisableAura; the recovery plays `Character` EAN 0.
* "Charge" 50 TGK / 54 GTGK is hitboxes and movement, an attack rush, not a ki charge.
* MAM/MAF (and other CaC player BACs) have charge-shaped entries 484/486 (Common EAN 249/250, same 15-frame loop, KiaiCharge @25) that MAP_PLAYER doesn't. They are the guard + heavy charge, below.

**Guard + heavy charge (2026-11-04, read from the files; a copy in Refinement 2.11 fires and charges in a transformed form without `InTransformedState`, seen in game):** Ultra Instinct's built-in charge is in the CaC files, not in its skill. Each CaC `*_PLAYER.bcm` (seen in MAM, MAF, HUM, HUF, SYM, NMC, FRI) has 4 roots with one child each, in MAM root positions 40-43 of 48:
* Root: `Heavy, Guard` + ActivatorState `Idle`, or `Heavy` + `Guarding`; HoldDownConditions 0x20000; PrimaryActivatorConditions `Standing|Floating, InTransformedState, Unk11, Idle, KiBelow100`; CUS_AURA 30 (gates it to UI's aura); BAC 484 (ground) / 486 (air); ReceiverLinkID 0x2.
* Child (the release): same buttons, HoldDownConditions 0x4, `InTransformedState, Unk11, Idle`, BAC 485 / 487, ReceiverLinkID 0x3.
* BAC 484/486: Common EAN 249/250 in 15-frame looping pieces from 25 to 430, end animation 430-507, then Character EAN 0/1. Function 4's ki ramp equals Ultimate Charge 598's (-1.0 at 25, up to -7.0 at 385-415), so it needs no skill EAN. Common_SE 179@16 is the loop, 180@470 the burst. Females use Common 247/248 (`GKS_SPA_KTM_F`, `_S_F`); that is the only MAM/MAF difference.
* The "Built-In Ki Charge" mod (Sportiax) installs the same entries for other forms with `InTransformedState` removed and CUS_AURA -1, which shows both gates can go.
* In an after BAC the ids must not collide: MAP_PLAYER.bac uses 484-486 itself. CaC BACs have empty slots up to DefaultEmptyEndIndex 900, so Refinement uses 640-643. Custom sounds there have to be Character_SE (Skill_SE has no bank in an after BAC entry).

**What can change a charge only while transformed:**
* The BAC event `Flags` are only Both/CaC/Roster (`ValuesDictionary/BAC.cs`), with no transformation condition. The EAN and SE come from the skill, and a skillset preset replaces slots without looking at what's in them. So no data route was found for animation or sound.
* The aura's KiaiCharge slot is the one conditional hook in the data: the skill fires the slot of whatever aura is active. Aur 14 holds effect 143, Revamp's aur 2500 holds 153. Editing the slot would affect anything else drawing that aur. Not tried.
* Otherwise a DLL: at the BAC Animation/Sound event, or entry start 0xf3da0, check `[char+0x22ec]` + the active skill `[char+0x364]`. Animation event handler not found yet.

Sounds in an after BAC must be Common SE or Character SE, not Skill SE; new Character SE cues need merged CSO rows, which an x2m can't do (`xenokit-audio-library.md`). Skill effects need type 12 + the neutral id, so only an after BAC inside the skill folder gets them.

## Kid Buu's guard: hard-coded CUS aura 14 (game 1.27, found 2026-11-03)
**Result:** the transformed guard is not data. `[char+0x22ec]` holds the current CUS aura / trans_control: BAC Function 13 (0xfc860, `cmp cx,0xd` at 0xfd446) passes `[cusEntry+0x3a]` (CUS_Aura, a short) + stage to 0xca320, which writes it at 0xca64a. The getter is 0xc8bb0 (`mov eax,[rcx+0x22ec]; ret`, 104 callers). Nine places compare it with 14, vanilla MAP's CUS_Aura:
* `call c8bb0; cmp eax,0xe` at 0x11b274, 0x11b6a5 (guard input 0x4000: if 14, `[char+0x58]&3`==0 and `[char+0x4e8]` non-null with vcall+0x40 nonzero -> state 10 = Kid Buu's block, after BAC 450+rand%3; otherwise 0x118940 -> state 9, the normal guard), 0x1230f1 (BAC 0x1c6 if flags&3), 0x123aef (0x1c8), 0x123e82 (0x1c7), 0x14f490, 0x14f6bb, 0x259f31 (hit handling).
* Inlined at 0xcce45: `cmp dword ptr [rdi+0x22ec],0xe`, sets state 10 through 0xc4570. Found by searching for the direct compare after the eight getter sites; other forms (a switch) are not ruled out.

So a skill with its own CUS_Aura never gets the guard, whatever its BAC, BCM, EAN or pre-baked entries say. XV2Patcher's BH10/11/13/64/Int2/Int3 cover other sites. There is no hard-coded "MAP" chara code check.

**Character layout used:** skills at `[char + 0x268 + slot*16]` (CUS entry pointer, 9 slots, 0xc5670/0xc569b), short name at entry +0, CUS_Aura at +0x3a. 0xeea10(`char+0x11d0`) returns the *active* skill (index from `[+0x364]`, or a global's `[+0x340]`), so it is the right entry while Function 13 runs but not at guard time. Other aura-by-chara-code writes: 3XK/5XK/0XK -> 4, ULA -> 10, KIN -> 0xb.

**Evidence that this is the gate:** the x2m (CUS_Aura 14) has the guard unpatched, the installer version (own id) did not, and a test DLL that changed nothing but these nine checks gave male and female the guard, invincible during the animation as on Purification (confirmed in game, 2.1).

**Current approach (2.3):** no exe change; a BAC dodge instead, see "Super souls cannot trigger the auto-dodge" below.

**Dropped, works on paper (2.2, never reported on in game):** an XV2Patcher Epatch making the checks compare only the low byte: getter sites `83 F8 0E` -> `3C 0E 90` (cmp al,0xe; nop), inline site `83 BF EC 22 00 00 0E` -> `80 BF ...` (cmp byte ptr). Every following jcc reads ZF only. It needs the skill's aura id fixed at 270 = 0x10E (vanilla ids are below 256, -1 has low byte 0xFF; XV2Patcher's limit is 0x2000, LOOKUP_SIZE), and an Epatch that doesn't match stops the game at start (XV2Patcher main.cpp), so every game update moving these bytes needs a new file.

**Rejected:** a run-time DLL (the first fix: read the id from the skill slot holding "RFN") adds a mod loader dependency. Setting CUS_Aura 14 on the skill: pre-baked data is per aura, so it gets vanilla's aura (0x9c4 under Revamp) and loses the smoke and outline. A getter patch in its padding fits but needs a per-install id. A data-only BCM route to BAC 450 would have no random variant, none of the hit sites and unproven priority over the engine guard.

**Dead end:** pre-baked BEHAVIOUR_66/64 (255/0xff until 1.9, 14/0xe in 2.0) has no effect on the guard. Comparing the CUS entries of MAP and RFN is what pointed here.

## Super souls cannot trigger the auto-dodge; BAC Functions do (game 1.27, found 2026-11-03)
**Result:** the automatic dodge is BAC data, not a soul effect, and no soul effect type can start it. A skill can have it from its after BAC alone, with no Epatch, DLL or aura id.

**Soul effects:** dispatcher 0x554a0 (from 0x549f0, from 0x69710; 0x6c4e0 reads the effects at entry+0x40+0xf0*i). It refuses type > 0x7c (124) at 0x554d0 and activation > 0x8a at 0x55508; jump table 0x55ce0 maps type to an internal buff kind, 0x54a20 creates the instance. Types 29, 30, 80, 82, 121 have no kind. So a new effect type needs new exe code; IDB data can only pick from the existing ones. `ClaudeTools\refinement\soul_effects.py` lists types, talismans and which awoken skills reach which talisman.

**The Functions (BAC type 15, the handler fragment 0xfe7ed of 0xfc860):**
| Function | Sets | Condition | Params |
|---|---|---|---|
| 0x43 | `[char+0x5c]` bit 0x100 (0xff3fb) | none | Param1 stamina cost |
| 0x5f | `[char+0x60]` bit 0x10 (0xffce3) | hp% <= threshold (0xcfef0) | Param1 stamina cost, Param2 health threshold |
| 0x65 | `[char+0x60]` bit 0x200 (0xfffaa) | CUS aura == Param3 (0xfff64), hp% <= threshold (0xcfeb0) | Param1, Param2, Param3 aura; ebx 2 clears |

Those three sites are the only writers of the flags. The stamina store 0xd7110 (`[char+0x6634]`) is called only from 0xff3f2, 0xffcce, 0xfff95, the threshold store 0xd7100 (`[char+0x6638]`) only from 0xffcda, 0xfffa1. The soul code has no call to the BAC player 0xc3be0 and no dodge flag writes. BAC state exit 0xf5c00 clears at 0xf5f7e.

**Vanilla use** (`ClaudeTools\refinement\bac_dodge_scan.py`, data/chara + data/skill; 0x65 x445, 0x43 x84, 0x5f x72): every CaC race BAC (HUM/HUF/SYM/SYF/NMC/FRI/MAM/MAF_PLAYER) has 0x65 with (100, 100, 30) in 56 entries, so a CaC dodges when its CUS aura is 30: vanilla awoken MGT 1510 / MGQ 1511 (pup 122). That aura gate is why those skills dodge, and neither has a soul that does it. GKS_PLAYER uses 0x43 (mostly stamina 100, some 25/50/0), FOK_PLAYER uses 0x5f (health 50). Entries in HUM: 0-6, 8, 9, 14, 16, 18, 20, 21, 23, 25, 26, 28, 31, 33, 38, 44, 50-52, 54-65, 190, 191, 211, 212, 300, 330-332, 373, 410, 411, 415, 425, 430, 435, 770-773.

**The dodge itself:** state `[char+0x15c0]` 0x1b -> flag 0x100, 0x1c -> 0xcfef0, 0x1d -> 0xcfeb0, 0x1e -> 0x5c bit 0xc (0xc4fec..0xc5049). State classes 0x1375a0, 0x137740, 0x137890 play 770-773 (tables 0xf71fb0, 0xf71ff8, 0xf72040, order by hit direction 0x11f210 + random/alternation); 0x1379e0 plays 765-768 (0xf72088). 0xc8280 reads the stamina cost, vtable +0x278 applies it. Which state uses which class is not verified.

**Hit handling 0x259d20:** 0x259f25 `[+0x58]&0x100` + aura 14 ignores the hit (Kid Buu's block); 0x259f40: attack `[rsi+0x32]` <= 0x5f with `[+0x58]&0x2000000` ignored; dodge flag set -> 0xcffe0 (`[+0x22c8]`>0 and `[+0x22cc]`>0). Reaction selection is around 0xcc6d5 / 0xcbf40 / 0xd147c; dodge versus guard priority not read.

**Class 0x1375a0 in detail:** picks the entry from its table by hit direction * 2 + a random bit (if the current entry is already 770-773 it alternates), plays it through 0xc3be0, then applies the cost from 0xc8280 (`[char+0x6634]`) through vtable +0x278. No gate on Param1 seen, and vanilla has cost 0 Functions (GKS_PLAYER 56, 59 have a second one). Vanilla timing: GKS_PLAYER and MAM 54-59 run the Function from frame 0 for the entry's whole length; MAM 770 is invulnerable from frame 0 for 21 and has Function 0x43 again at 21 for 21, which chains dodges.

**Data-only custom dodge, tried in Refinement 2.3, dropped:** Function 0x43 cost 0 in the after BAC's guard entries 54-59, and new entries 770-773 = Kid Buu's blocks with invulnerability from frame 0. In game it played the normal guard animation most of the time and needed no timing. Refinement 2.4 keeps CUS aura 14 instead (real guard) and draws its own aura as a skill effect: effects-and-vfx.md "Refinement's pink aura".

**Installer limits:** LB installs IDB and PUP entries with AutoID and tracks them for uninstall, but `PUP_Entry.Talisman_ID_1/2` are plain ints with no binding (`PUP_File.cs`), so a new soul attached by a PUP needs a fixed talisman id; no binding is shown on the CUS `PUP` field either (`CUS_File.cs:563`, not tested). A skill sharing a PUP with another also shares its souls. LB cannot compute or edit Epatch contents.

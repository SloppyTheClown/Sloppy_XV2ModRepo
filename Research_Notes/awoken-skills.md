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

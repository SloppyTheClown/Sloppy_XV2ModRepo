# New Transformations (LazyBone, 3.4.5) on 1.26 / Revamp

Covers: what NT's installer and its .x2m skills change, the battle HUD stage
names ("???"), the Hair Change face/hair swap, BCS hair/eye colour indices vs
Revamp, XV2_NewTransformationsBridge. The 128-texture installer limit and
the game patch are in effects-and-vfx.md.

## What NT installs
* The LB installer (`New Transformations Installer.exe` + `.installinfo`,
  zip with `InstallerXml.xml`) adds only shared data:
  * HUM.bcs, HUF.bcs and FRI.bcs (colours, part sets 297/298);
  * cmn.bpe, custom_skill.cus (skillsets 105-107), talisman IDB,
    chara_sound.cso;
  * BTL_AURA textures (134 total on Revamp).

  Options:
  * HairChange (vanilla SS skills' part sets 297/298: None / Hair /
    Hair+Eye);
  * Aura style;
  * AutoDodge;
  * Kaioken drain;
  * Tail anims.

  readme line 60: any later mod that installs HUM.bcs or HUF.bcs wipes NT's
  colours, and NT's installer has to be run again.
* The transformations themselves are separate .x2m awoken skills installed
  with xv2ins:
  * folder `0. Hair Change` and `1. No-Hair Change` hold the same skills;
  * the skill GUID is identical in both, and only the costume attachment
    (`COSTUME_ATACHMENT/<guid>.x2m` inside the x2m) differs;
  * variants in subfolders (No Health Requirements, Moveset Swapping) reuse
    the skill GUID.
* NT's multi-choice system (Controls.txt): pressing an extra button during
  the transform animation picks another stage. It does not change HUD names.

## Battle HUD stage names - "???"
* The battle HUD names awoken stage N after `BHD_MET_<ID2 4 digits>_<N>` in
  `msg\quest_btlhud_<lang>.msg`. ID2 is the MET skill's
  (`data\skill\MET\<id2>_<code>`). Vanilla is 1:1: BHD_MET_0000_0..2 = SS,
  SS2, SS3.
* xv2ins writes one entry per TRANS_NAME_<LANG> of the x2m. Five NT skills
  have fewer names than stages, so their later stages show "???". This is
  NT's own bug and happens on vanilla too. The five skills, by x2m GUID:

  | Skill | GUID starts | Stages | TRANS_NAMEs |
  |---|---|---|---|
  | Super Saiyan 4 | a8202e67 | 4 | 1 |
  | Super Saiyan All (Rose + SS4) | ddb436a7 | 12 | 1 |
  | SSB Kaioken (4 stages) | 64f7fe35 | 4 | 3 |
  | SS Rose Kaioken (4 stages) | b5d68fd4 | 4 | 3 |
  | Frieza Transformations | f60cc82d | 4 | 1 (changelog: stage 4 = Augmented Form) |

* The per-stage names in `XV2_NewTransformationsBridge\tools\ntfix.py` were
  read from each stage's CusAuraData (aura, hair colour, hair part set).
  For AL4R:
  * 0 = UI -Sign- (UI aura, eyes 98, part set 270);
  * 1-4 = SS..SS4;
  * 5 = SSG (hair 99);
  * 6-9 = Rose, Rose KK, KK X10, KK X20;
  * 10 = UI -Sign-;
  * 11 = UI.

  **In game, AL4R showed "Ultra Instinct" (index 11) on every stage.** The
  names are fine; the HUD never asks for any other index. See next section.

## Which stage the HUD names (exe 1.26.01, found by reverse engineering)
* Name lookup: 0x1c1cc0(out, skill_id, stage) formats `BHD_MET_%d%d%d%d_%d`
  (0xf78728); stage -1 = `BHD_MET_RELEASE` (0xf78718). The skill panel
  0x1c1050 calls it (stage kept at +0x8dc). Its caller is 0x1b8d80
  (players <= 2), and that is called from fn 0x15e230 at 0x160607.
* fn 0x15e230, awoken slot (slot 8), 0x160083-0x160436:
  * `r13d` = skill id, `[rbx+0x3c]` = CUS Num_Of_Transforms.
  * `r15d` = 0x11ffc0(char) = current stage, -1 if untransformed.
    XV2Patcher hooks this (CusAuraPatchInt2, so likely pre-baked INTEGER_2).
  * Ki thresholds per stage are in `[rbp+0x1c0..]`. XV2Patcher swaps in its own
    array (RedirectTransformArrayPatch at 0x1603a1, presumably pre-baked
    KI_REQ). It also raises the stage cap from 3 to 100 (ChangeUiMaxTransform,
    0x1603b0).
  * The loop runs from the top stage down, and 0xc66a0 checks
    `0 <= threshold <= [char+0x10c]`.
    * `r12b` = 0 (default): it takes the **highest stage you could afford now**.
    * `r12b` = 1: it takes only the next stage (current + 1). Only skill ids
      25005, 25040, 25800 and 26350 set this (0x1600a4-0x1600d0).
  * Result `edi`: -1 if it is not above the current stage, else the stage
    whose name is shown.
* **Cause:** every CusAuraData in pre-baked.xml has KI_REQ 0 (all x2m auras,
  not only NT's). So every stage is "affordable", and the HUD always names
  the top stage (Num_Of_Transforms - 1).
  * NT's multi-choice system (its Controls.txt) ignores Ki: the button goes
    to the next form, and extra input redirects it.
  * So the HUD name is the top stage's on any stage. Before the fix that index
    had no msg entry ("???"); now it is "Ultra Instinct".
  * Nothing is broken. It is the vanilla Ki rule meeting zero Ki
    requirements, and no msg data can make the name change per stage.
* Vanilla 0x11ffc0 (seen behind XV2Patcher's jmp, from 1.25): if
  `[char+0xb0] & 0x10` (transformed), read the aura via 0xc8bb0
  (`mov eax,[rcx+0x22ec]`) and map it through a byte table; XV2Patcher
  swaps in the CusAuraData's INTEGER_2. AL4R's INTEGER_2 repeats
  (0,1,2,3,3,4,5,6,6,7,8,9), so `r15d` is not 1:1 with its 12 stages, and
  option "next-stage mode" (`xor r12b,r12b` at 0x160083 -> 1) would have
  named stages twice.
* **Fix built (General_QoL `transform_hud_next_stage`, installed on the PC,
  not seen in game):** a hook at 0x160436, where every path of the pick
  has converged with the result in `edi` (16 bytes: `test byte [rsi+0xb0],
  0x10 / je / cmp r13d,0x61ad`, replayed in the cave; none of XV2Patcher's
  patches 0x1603a1/0x1603b0/0x11ffc0 overlap). It takes `rsi` = char,
  `r13d` = skill id, `[rbp-0x50]` = Num_Of_Transforms (0x160370), and
  replaces `edi` with: stage 0 untransformed, else the current aura's stage
  index + 1, -1 on the last stage.
  * Stage index = position of the aura among the skill's CusAuraData
    entries in pre-baked.xml (`CUS_ID2_OWNER` = ID2, skill id = 25000 +
    ID2); xv2ins writes them in order, matching TRANS_STAGE (AL4R 0x4a..0x55
    = 0..11).
  * Only when the skill's pre-baked entry count equals Num_Of_Transforms
    and the current aura belongs to it - vanilla skills with one edited
    aura (pre-baked has ID2s 6, 31, 240 with one entry) keep the game's
    choice.
  * Unverified: that `[char+0x22ec]` holds the pre-baked `cus_aura_id`
    under XV2Patcher. The log ("hud stage: skill ..., aura ... -> names
    stage N (game picked M)") shows it.
  * Offline check: `General_QoL\tests\hudtest.cpp` on the PC's pre-baked.xml
    (70 entries, 30 skills) passes.
* **In game (index + 1 build):** the aura at `[char+0x22ec]` IS the pre-baked
  id (log: 0x4b..0x53 -> 2..10). Every middle stage of AL4R was right, but
  base named "UI -Sign-" (stage 0) and KK X20 (stage 9) named "UI -Sign-"
  (10) instead of "Deactivate". Index + 1 is wrong for NT: AL4R's stage 0
  is a dummy the button never goes to, and stages 10/11 are HP-gated.

### BCM chain (the rule now used)
* What the skill button really does is in the skill's BCM
  (`data\skill\MET\<ID2>_<CHAR>_<CODE>\<folder>_PLAYER.bcm`, else
  `<folder>.bcm`). Entry (112 bytes, Xv2CoreLib BCM/Parser.cs): conditions
  u32 @24 (0x80 InBaseForm, 0x100 InTransformedState; AL4R base 0xc80,
  transformed 0xd00), BAC entry i16 @32, sibling @48, child @52, KiRequired
  u32 @92, HealthRequired f32 @96 (< 0 = at most -x of max HP, > 0 = above x;
  NT's stats.txt), TransStage i16 @100 (0-based target stage).
* Transform entries = depth 0-1 (root siblings + their children; deeper ones
  are NT multi-choice follow-ups) with 0x80/0x100 whose BAC entry has a type
  15 row with function 0xd (Activate Transformation). Revert entries (AL4R
  BAC 23, STR BAC 3/4) only have 0xe, and TransStage 0 is valid (S20 base ->
  0), so "TransStage > 0" is not the test.
* File order is the priority (2500_TR1_STR: base #1 Ki 400 -> stage 1
  comes before #2 Ki 300 -> stage 0).
* Named stage: untransformed = first base entry whose gates pass (none ->
  the cheapest base entry); transformed at s = first transformed entry with
  TransStage > s whose gates pass, none = -1 (Deactivate). Skills without a
  BCM (1511, 1570, 2082, the vanilla 6/31/240/400/410/801) keep index + 1.
* AL4R: base -> 1, 0..8 -> next, 9 -> 10 only at HP <= 60 %, 10 -> 11 only
  at HP <= 40 %, 11 -> Deactivate. All 20 BCM chains on the PC look sane
  (`ClaudeTools\new_transformations\bcmchain.py <game>`, `bcmtree.py`,
  `bacfuncs.py`).
* The game's own BCM check was not traced; the rule is inferred from the
  data. Ki / HP read off the character: Ki f32 `[+0x10c]` (0xc66a0 compares
  a cost <= it), HP f32 `[+0x100]` / max `[+0x104]` (0xcfdf0 / 0xcfe30) -
  **unverified on the HUD's character**; the "hud stage" log line prints
  `ki .., hp cur/max`. An unreadable ratio counts as full HP. Ki units vs
  BCM KiRequired (STR: 300/400) also unverified - AL4R's gates are all 0.
* XV2Patcher's source is not kept locally, and a web search did not find it.
* xv2ins rewrites the msg files on every skill install or uninstall, so the
  fix must be run again afterwards.

## Hair Change = another face
* The `0. Hair Change` attachment's part sets (one per stage, HUM + HUF)
  set Face_forehead, Face_eye and Hair to model IDs 238-299 (270, 297, 298,
  256, 249, 253, 271), with NULL files, i.e. `HUM_<id>_<part>.emd`.
* NT's readme expects players to copy their own CaC's files into those IDs.
  That is per player, so online the other side sees something else. With
  NT's defaults the CaC gets a different face and hair - the user's report
  on Super Saiyan All.
* The `1. No-Hair Change` attachment has empty part sets except SS3 (HUM
  forehead model 299 with HUM_008 files, eye HUM_003; hair 65535 on both
  races) and the bust 256 stage. It keeps the CaC's face and hair and
  changes colours only.
* Detecting the installed variant: the attachment GUID appears in
  `data\X2M_COSTUME.xml` (xv2ins's costume registry). Match on the skill
  GUID too (`data\skill\*\*\X2M_SKILL.ini`), since files share attachments.
* `ClaudeTools\new_transformations\attsum.py <x2m.xml>` prints each part set's
  parts.

## BCS colours vs Revamp (no change made yet)
* CusAuraData BCS_HAIR_COLOR / BCS_EYES_COLOR index BCS PartColor groups 4
  and 5. HUM and HUF share the indices.
* Revamp: HUM group 4 has 107 entries, 98-106 extra (98 cyan, 99 Rose pink,
  100 blue, 101-106 light blue); HUF groups 4/5 have 98 entries (vanilla).
* NT's installer replaces entries 0..103 of groups 4/5 in HUM and HUF. 0-97
  equal vanilla; 98-103 are NT's (98 silver, 99 hot pink, 101 Rose). So
  Revamp's HUM 98-102 are overwritten and 103-106 survive.
* NT's skills get NT's intended colours. Nothing found uses Revamp's 98-102:
  no BCS ColorSelector, no Revamp pre-baked entry (Revamp's SSB/SSG/UI/
  Beast auras use hair -1). The CaC creator palette is not checked.
* Possible fix if it matters: append NT's colours at free indices (107+)
  in HUM and HUF, restore Revamp's 98-106, and remap NT's pre-baked
  (`data\pre-baked.xml`, owners 1591-1603) colour indices.

* **Checked 2026-10-13 on the PC:** NT's colours are NOT in the installed
  HUM/HUF.bcs. HUM group 4 = Revamp's 107 entries, group 5 = vanilla 98;
  HUF 98/98 = vanilla. NT's installer xml has 104/104 in both. So NT's
  skills that use hair/eye 98-103 currently get Revamp's HUM hair colours
  (98 cyan, 99 Rose pink, ...) and out-of-range eye indices. That may be
  the "hair colour off" report. Re-running NT's installer would restore
  them but overwrite Revamp's 98-102 (the fix above avoids that).

* **Correction + fix, 2026-10-13** (`ClaudeTools\new_transformations\bcscolors.py`
  compares an installed BCS with NT's installer xml; `auracolors.py` now
  reads pre-baked.xml by regex - xv2ins's `<!------>` comments break
  ElementTree):
  * NT's 98-103 are Revamp's colours shifted by two plus two new ones:
    NT 98 silver, 99 hot pink (new); NT 100-103 = Revamp HUM 98-101 (cyan,
    Rose pink, blue, light blue). Eyes (group 5) use Color4 only. NT's
    0-97 differ from vanilla only by float rounding.
  * So on the PC NT's Rose stages (hair 101) showed light blue on HUM,
    silver (98) showed cyan, hot pink (99) showed Rose pink; HUF hair and
    all eyes 98+ pointed past the end of the list.
  * Only pre-baked.xml uses 98+: no part ColorSelector in the installed
    HUM/HUF.bcs does (scanned, 165 selectors, max index 67).
  * NT's LB install did run (its JUNGLE bust 256 files are installed); a
    later BCS write (Revamp or xv2ins) dropped its colours.
    `install_error.txt` in the NT folder is an older failed run
    (FileCacheManager.SaveStreamFiles, HUF_256_Bust.emb); the .installinfo
    zip itself tests fine.
  * Fix in `ntfix.py` (Build_Fix.bat): append NT's six colours after the
    longest list (107-112) in HUM/HUF groups 4 and 5, padding shorter lists
    from the other race (HUF hair gets Revamp's 98-106) or their last
    colour; the new list goes at the end of the file and the group's
    count (+10) and offset (+12) are repointed. NT skills' CusAuraData
    (owner = installed skill whose GUID is an .x2m in NT's folder) remapped
    98-103 -> 107-112 (28 values, 18 skills). Undo data
    `data\ntbridge_colours.json`; apply/re-apply/undo byte-identical on a
    copy. Applied on the PC, not seen in game.

## Vanilla Beast (skill 801) - not NT
* User report: Beast changes the CaC's face, not just the eye colour.
* NT touches nothing Beast uses. Its BCS installs are colour groups 4/5
  and part sets 297/298 (absent on the PC: "None" option). Beast's CUS
  entry equals the cpk's, and there are no loose MET 801 files.
* The only non-vanilla Beast data is Revamp's `data/cac/pre-baked.xml`.
  It overrides vanilla cus aura 0x18 (aur 0x15, owner 801; Revamp's comment
  says "Super Saiyan Blue", which is a copy-paste). Values: behaviours
  0x18/24, hair -1, **eyes 255**. It is the only override of an id below
  0x33 in the installed pre-baked.xml. Eye index 255 exists in no HUM/HUF
  colour group (max 98-107).
* The vanilla values of entry 0x18 (the exe table) were not looked up. The
  test is to drop that one entry from `data\pre-baked.xml` and compare in
  game. xv2ins rewrites pre-baked.xml on every install, so the edit is
  temporary.

## Probe scripts
`ClaudeTools\new_transformations\`:

| Script | What it does |
|---|---|
| msgdump.py | dump msg entries by regex |
| ntinstalled.py | installed NT skills, stages vs names |
| ntstages.py | per-stage aura/hair/eye from an x2m |
| auracolors.py | pre-baked entries with colours >= 98 |
| bcscolors.py | BCS hair/eye colours vs NT's installer xml; colour selectors >= 98 |
| attsum.py | attachment part sets |

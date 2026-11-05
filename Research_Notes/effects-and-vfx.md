# Effects / VFX (EEPK, particles, effect textures)

Covers: EEPK containers (PBIND/TBIND/CBIND), how their texture EMBs load,
the 128 particle-texture limit (where the game enforces it and the one-byte
fix), Xv2CoreLib/LB Mod Installer's own 128 checks, New Transformations vs
Revamp's BTL_AURA. Addresses: DBXV2.exe 1.26.01 RVAs, or `xgcore.dll` RVAs
(image base 0x180000000; xgcore is not DRM-encrypted).

## The problem that started this
New Transformations 3.4.5 (LB Mod Installer 3.32, Costura-packed) fails:
"EEPK install phase (vfx/cmn/Custom_Aura/BTL_AURA_CustomAura.vfxpackage)
AddPbindDependencies: The maximum allowed amount of textures has been
reached". Revamp's `data\vfx\cmn\BTL_AURA.ptcl.emb` holds 123 of 128; each NT
aura option adds ~10-11 (Custom 10, Mixed 10, Vanilla 11, UI 1). All 123 are
unique and used (Xv2CoreLib MergeDuplicateTextures / RemoveUnusedTextures
freed 0).

## Where the game enforces 128 - FOUND (static, not yet tested in game)
`xgcore.dll+0x4c119` (file offset 0x4b519, identical in 1.25.02 and
1.26.01 xgcore), in the particle engine's per-node texture bind loop:

    movsx ecx, byte ptr [rax+1]      ; 0F BE 48 01  EMP texture sampler +1 = EmbIndex, SIGNED
    cmp   ecx, dword ptr [r15+0x14]  ; EmTextureContainer count
    jae   skip                       ; unsigned: index >= 128 -> negative -> skip
    mov   rax, [r15+8] ; mov rcx, [rax+rcx*8] ; ... call 0x81910 (bind texture)

rdi = EMP node: word +0xb2 = texture sampler count, +0xb4 = offset of the
sampler offset list. An EmbIndex of 128..254 sign-extends, fails the check
and the particle draws with no texture; 255 (= none) is -1 the same way.
**Fix: `0F BE` -> `0F B6` (movzx) at +0x4c119.** New limit 255 textures
(indices 0..254; 255 still means none as long as the EMB has < 256).

Trace (ETR, TBIND) path `xgcore+0x887e6` already uses movzx (sampler list at
ETR +0x6e count / +0x70 offset) - no 128 cap in the game there.
`sxscan.py` (byte load followed by cmp [x+0x14]) over all of xgcore found
only these two plus two unrelated `[r14-3]` reads; no signed EmbIndex read in
DBXV2.exe.

## Loading is not capped (dead ends, kept so nobody re-checks)
* EmTextureContainer (xgcore, exports) is dynamic: array +8, capacity +0x10,
  count +0x14, grows via 0x30e70. AddTexture 0xb8210, Create 0xb7fa0
  ("#EMB" check), CreateAdd 0xb8020, SetTexture 0xb8260 (unsigned bound),
  GetTexture 0x16c60 (ICF-folded), GetSize 0x2a260, vtable 0x350218.
* Exe EEPK setup 0x958e80 (from state machine 0x958460): `[this+0xc0]` file
  collection by type (1 pbind, 2 ptcl.emm -> +0xe0, 4 tbind, 5 trc.emm ->
  +0xf0, 6 trc.emb -> +0xf8 Instantiate+Create, 7 -> 0x949ad0, 8 ->
  0x9481f0). PBIND entries [+0x60..+0x68] stride 0x20, 0x951650 stores
  {emp, emm, emb}; TBIND entries +0x78 via 0x9536f0. PBIND's emb `[+0xe8]` =
  `[this+0x168]`, from the shared path-keyed texture cache 0x2cfdb0 (map
  0x156f0a0, lock 0x156f0f0, factory [0x156f0e0]).
* Exe IAT for EmTextureContainer: Instantiate 0xf4e6a8, Create 0xf4e6a0,
  CreateAdd 0xf4e650, GetTexture 0xf4e648 (used at 0x8855b1 with index 3/4 -
  not EMP), GetSize 0xf4e640, SetTexture 0xf4e698.
* Method that found it: `opscan.py xgcore.img 0 400000 "movsx \w+, byte ptr
  \[\w+ \+ 1\]"` - a signed read of byte +1 was the hypothesis (128 = sign
  bit). Searching for EMP field reads (+4..+0xa together) and "#EMP" found
  nothing (the engine reads fields lazily, no signature check).

## Tool side (LB Mod Installer / Xv2CoreLib / EEPK Organiser)
`EMB_File.MAX_EFFECT_TEXTURES = 128` (a const - compiled into the IL as
`ldc.i4 0x80` wherever used): EMB_File.cs:287, EffectContainer.cs 1297/1356
(Pbind/TbindLinkTextureAndMaterial), 1876-1882 (ValidateTextureContainers),
2213, 2593-2597, 3270, 3348, EMO_File.cs:296. With the game patched, an
installer whose Xv2CoreLib uses 255 is needed. NT's installer = LB Mod
Installer 3.32 with Costura-embedded Xv2CoreLib; RevampBridge already runs an
installer with a replaced Xv2CoreLib (revamp-on-1.26.md "Loading").
xv2ins/eternity_common has no 128 check.
Xv2CoreLib also has `MergeAllTexturesIntoSuperTextures_PBIND` (atlases) -
an alternative that needs no exe patch; now the default ("Texture merge
instead of 255" below).

## Tool side - patched in place (XV2_NewTransformationsBridge, was in EffectTextureLimit)
The installer's own Xv2CoreLib (Costura resource
`costura.xv2corelib.dll.compressed`) is IL-patched, no recompile: every
`ldc.i4 0x80` (20 80 00 00 00) -> 0xFF in EMB_File.Add,
EffectContainerFile.{Pbind,Tbind,SDBH_Pbind}LinkTextureAndMaterial
(`sbyte.MaxValue + 1` folds to the same constant), ValidateTextureContainers,
CreateAwokenOverlayEntry, AssetContainerTool.Add{Pbind,Tbind}Dependencies -
11 sites in NT 3.4.5's lib. Not touched: AssetContainerTool..ctor
(AssetSpawnLimit 128), EMO_File.ConvertToEmo, ValidateContainerFileNames
(path length). `libpatch <dll>` lists every method holding ldc.i4 128.
Served by a RevampBridge-style launcher (AssemblyResolve ahead of Costura's),
now XV2_NewTransformationsBridge\launcher\; NT's skill problems are in
new-transformations.md.

## Tool side - Xv2CoreLib in the workspace (2026-10-18)
`Tools\XV2 Tools\Xv2CoreLib` (what XenoKit and the other tools here build
against) now allows 255 everywhere instead of 128, matching the game patch:
`EMB_BaseFile.MAX_EFFECT_TEXTURES = 255` (marked "Local change") and the three
link checks (`Pbind`/`Tbind`/`SDBH_Pbind`LinkTextureAndMaterial) accept
`EmbIndex < byte.MaxValue`, so 0..254 link while 255 keeps meaning "no texture".
Without it XenoKit could not load Revamp+NT's `vfx/cmn/BTL_AURA.eepk` at all
(637 effects; `PbindLinkTextureAndMaterial: EMB_Index is out of range (128)`),
which is what the Effect Library reported as a bare "could not load".
Verified offline: `ClaudeTools\effects\eepk_load_probe.ps1` loads BTL_AURA (637
effects) and BTL_CMN (525) with the patched build; demo EEPKs whose loose
`.ptcl.emm` is missing still fail the same way as before (not a regression).

## Texture merge instead of 255 (2026-10-18) - the default now
Files past 128 break other tools (LB Mod Installer, EEPK Organiser and
friends call them corrupt), so 255 is opt-in now (EffectTextureLimit
`"raise_limit"`, bridge `RaiseTextureLimit=1`). The default follows the
community guide "Optimize Your Auras!" (Google Doc 13erQ3iCNZPjsIZfEQhxbGgVKw5hu4kVhg7aL8g4IbZw;
read through its `/mobilebasic` URL - the edit view gives no text, `/export`
was refused): EEPK Organiser > File > Load from game > cmn > BTL_AURA >
Tools > Optimize Textures, back up vfx\cmn first.
* Code: `EffectContainerFile.MergeAllTexturesIntoSuperTextures_PBIND()`
  (EffectContainer.cs ~2338), returns `{merged, superTextures}`. Packs small
  ptcl textures into atlases and rewrites the EMP samplers' scroll/scale.
* Headless: `XV2_NewTransformationsBridge\optimizer\` (AuraOptimizer.exe).
  Load like EEPK Organiser: set `SettingsManager.Instance.Settings.GameDirectory`,
  `FileManager.Instance.LoadFile<EffectContainerFile>(rel, false, true, true)`
  (loose then cpk; a plain `Load(path)` fails on cpk-only assets such as
  Knt_Wind.emo), `LoadedExternalFiles.Clear()`, set `Directory`, `Save()`.
  Save writes every referenced asset too, so save to a scratch folder and copy
  back only `BTL_AURA.*`.
* The merge's undo step calls `UndoManager.RefreshUndoRedoState`, which uses
  `Application.Current.Dispatcher`: create a WPF `Application` first or it
  throws NullReferenceException.
* NT 3.4.5's bundled Xv2CoreLib can't do it (no FileManager.LoadFile, and the
  installer's Costura resources lack the image libraries - Pfim,
  CSharpImageLibrary, WriteableBitmapEx): build against the workspace
  Xv2CoreLib (net48, XenoKit's `LocalBuild\net48-only.targets`).
* Result, PC (Revamp + NT, 637 effects): 134 -> 18 textures (123 merged into
  7 super textures), ~2 s; ptcl.emb 19 -> 34 MB. A second run finds nothing
  to merge; restore from the backup is byte-identical. Applied to the test
  game 2026-10-18; not seen in game yet.

## Refinement's pink aura (2026-10-19) - a recoloured copy of a game aura
2.4 (seen in game) stops installing all of this as an aura, to keep CUS aura 14 and its hard-coded guard (awoken-skills.md "Kid Buu's guard"): the aura value picks both the guard and the aura the engine draws. The package's HenshinStart copy is now a skill eepk effect (109), started by the skill's BAC at the end of the intro and stopped in its revert entry. Guard, manual and forced revert work; a forced revert doesn't leave it running. The effect does disappear on jumping. The rest of this section is how 1.3-2.3 did it.
Stopping effects (seen in game, 2.5): a skill eepk effect ends on both jumps, though the jump entries have no event that stops it and none of its parts has OnGroundOnly. MAP_PLAYER entry 24's `Common 0 41 Off` isn't the cause: removing it changed nothing, and it's not in the directional jumps. Cause unknown; engine side not read. Function 6 (Invisibility) lasts for its event's Duration (MAP_PLAYER entry 44: 0+1, 2+1, 4+1, 6+43). No vanilla after BAC (001 kin, 002 gnt, 003 gld, 030, 031, 1350, 2020) has a skill eepk effect; 2.6 tried `Skill_ID 0, Use_Skill_ID False` in one: nothing happened.

Quieter aur slot for one skill (Refinement 2.13, seen in game): an aur row is shared by everything drawing it, so don't edit the row. Drop the BAC `Aura` event and run a thinned copy of the slot's effect from the skill eepk over the same frames (`RefinementAura.exe thineffect`). EEPK `Deactivation`: Never 0, Immediate 1, LoopCancel 2. In XenoKit's `VfxAsset.Terminate`, LoopCancel on a particle or Tbind part stops emitting and lets particles finish, and other asset types end at once; an Off + Default restart on LoopCancel parts should overlap rather than pop (assumed the same in game).

EEPK part orientation (seen in game, Refinement 2.13): the serializer's `RotateOnMovement` is byte +4, `Orientation`. None ignores the bone's rotation (both arms' vents shared one world direction); AttachmentBone makes a part follow the bone, as XenoKit's `VfxAsset.GetAdjustedTransform` reads it.

BAC effect event type resolution (game 1.27, handler 0xf9240-0xf98a4): EepkType 3 (AwokenSkill) goes through the skill record in use (0xeea10, from chara+0x364, -1 outside a skill = null). The record's +0xd low nibble n picks the type (0/1 -> 5, n -> n+4, so n=6 -> 12 = NEW_AwokenSkill) and word +0xA (ID2) the id. Vanilla Purification: +0xd = 0xf6, ID2 4 -> (12, 4). With no skill in use, type 3 spawns (3, event id), which finds nothing: that's why a type 3 effect in an after BAC is silent while the transformed character isn't using a skill. Other types take the id straight from the event's dword at +0xc (Skill_ID u16 + Use_Skill_ID u16, so Use_Skill_ID must be True = 0). So an after BAC must name type 12 and the real ID2. LB rewrites the neutral id 0xBACA only in the BACs of a SkillDir, so such an after BAC must sit in the skill's own folder, and the CUS path then needs a known folder: `AutoSkillID=(Awoken; alias; <CMS>)` fixes the parent, and `{GetAlias=alias, Format=3}_<CMS>_<short>` builds the name. SpawnEffect 0x965080(manager 0x1568368, request, type, id, flags): flag bit 1 (Off) -> lookup 0x9652f0 + stop 0x968cf0, else start 0x9693b0. Refinement 2.7 restarts its aura this way; not yet seen in game. The engine's own stop on jumping wasn't found.
Looping body outline from a BAC (scan of every vanilla BAC: `ClaudeTools\refinement\bac_screeneffect_scan.py`): a ScreenEffect (type 16) on a BPE with I_00=1, Duration 1; start I_14 0 or 1, stop = same id with 0x2 (DisableEffect) added, also in another entry of the skill (2070_fit_cor 1 -> 5) and often at frame 0. Vanilla looping pink: 302 (0.43,0,0.157)-(0.51,0,0.196) a0.4, 320/350/371 darker. Vanilla's looping pairs with a stop use start 0x1, stop 0x3 (sim_player 282, 2122_sim_mba 0, 2120_sim_dra 0); 0x1's meaning unknown. 302 at a0.4 F_32 0.04 with start 0 before Function 13 (frame 275) showed no outline on a transformed Majin (2.5, seen in game); vanilla transformations stop their outlines before Function 13 (030_god_sgm BPE 274, 2020_fod_sfm BPE 284). Not yet known which of these was the cause.
2.0 (seen in game, colour kept): the smoke is white/grey, no longer pink; colours map to grey of the same brightness (`ToSmokeGrey`, `LightToSmokeGrey`, replacing the `ToLightPink` named below) and the package's saturation is -1 (HueAdjust adds saturation and clamps at 0). The outline stays pink. Particles keep their size on a small body, so a female Majin got thick, glaring smoke; 2.1 (not yet seen in game) multiplies colour and light brightness by `SmokeBrightness` 0.7 and particle transparency (`Color1/2_Transparency`) by `SmokeOpacity` 0.55.
How a skill's aura is chosen: CUS skill `CUS_Aura` (a CUS aura id) ->
XV2Patcher's `data\pre-baked.xml` `<CusAuraData cus_aura_id aur_aura_id>`
(+ behaviour flags; installed copy on the PC for cus 14: BEHAVIOUR_11/10/13/64
0xe, BEHAVIOUR_66 14, INTEGER_3 1, REMOVE_HAIR_ACCESSORIES 0xff, hair/eye
colour 0xffffffff, rest 0) -> `system\aura_setting.aur` aura (7 slots: BoostStart,
BoostLoop, BoostEnd, KiaiCharge, KiryokuMax, HenshinStart, HenshinEnd ->
effect ids) -> `vfx\cmn\BTL_AURA.eepk` effects. Purification = cus/aur aura 14
("Evil"): effects 140, 141, 142, 143, 144, 230, 232.
* What colours what (EEPK Organiser-style dump, `RefinementAura.exe detail`):
  the body's **purple tint** is the ECF node's rim colour (`Evl_Aura.ecf`,
  used by 141, 143, 230; not an outline, see "Body outline = BPE" below);
  the **smoke's** colour is its texture (`AuraEvl.dds`, dark
  purple), so the particle colours alone can't make it light pink.
* Tool: `Refinement_Transformation\tools\aura` (C#, net48, against
  `Tools\XV2 Tools\Xv2CoreLib`): loads the effects from the cpks, copies them
  into a .vfxpackage with assets renamed `RFN_*` (recoloured copies must not
  keep vanilla names: LB matches assets/textures by name and reuses the
  game's), hue-shifts particles/ECF/lights to 330 (pink) and recolours only
  coloured textures; grey textures (RGB spread < 0.05) keep their name and
  are shared with the game's (7 new textures instead of 24 - BTL_AURA's 128
  texture cap, vanilla has 81).
* Xv2CoreLib's recolour is async and resumes on the WPF dispatcher: a
  console tool needs `[STAThread]`, a `System.Windows.Application` (undo
  manager), a DispatcherSynchronizationContext, and must pump the
  dispatcher while waiting (`Await`), or it deadlocks.
* LB installer: a SourcePath ending in `.vfxpackage` = FileType.VfxPackage
  (InstallerXml.cs), installed into the InstallPath's eepk via Install_EEPK
  (merges duplicate textures, tracks effect ids for uninstall). Its AutoID
  for package effect ids doesn't work, so the effects get fixed ids
  (47810.., the tool checks they're free in the cpk and loose BTL_AURA).
  AUR aura `ID="{AutoID, SetAlias=..}"` works (Install_AUR runs
  ParseProperties); CusAuraData `{AutoCusAura}` (GetFreeCusAuraID, from
  0x33) and `{GetAlias=..}` resolve in XmlParser's first pass. Bindings
  output decimal and CusAuraData reads `"81"` as 81 (`"0x81"` = 129), checked
  with the installer's YAX options. CUS_ID2_OWNER is filled in by LB
  (FixCusAuraOwners) for the mod's awoken skill. Order: vfxpackage -> AUR ->
  pre-baked -> CUS.
* Refinement 1.3 in game: the Evil-aura recolour came out hot magenta, not
  smoky, body tinted purple, outline not visible, no trail. Evil isn't a
  good base: its smoke is a dark texture and HueSet (Xv2CoreLib ChangeHue)
  skips white/grey colours.
* 1.4 (not seen in game): base = aura 36 (Ultra Instinct, effects 350-354,
  356, 358; white/pale blue). Every EMP colour remapped to a light pink of
  the same brightness (own `ToLightPink`, not HueSet). UI has no ECF, so the
  Evil aura's ECF part (effect 141) is copied in with a pink rim = outline.
* Trail: EffectPart Flags1 `InstantUpdate` (0x04) makes particles follow
  the bone each frame; a copy of a PBIND part **without** it leaves its
  particles where they were emitted = a world-space trail (read in
  XenoKit's code). Seen in game (1.4): one pelvis copy leaves only a few
  puffs; copying every limb's part (Refinement 1.8, 16 parts) is the try
  for a body-shaped trail (seen in game: works, wanted more often).
  Puff rate = the EMP's emitter nodes' `BurstFrequency` (frames between
  bursts; HenshinStart's 20-40, mostly 24). The trail copies share their
  EMP with the bone-following aura, so instead of editing it Refinement 1.9
  adds a second copy per part with `StartTime` + half that period = twice
  the puffs, body aura unchanged.
* An outline BPE entry can animate: keyframes at Start_Time 0/30/60 with
  different colours blend between them (a user-tuned magenta <-> pink entry,
  Refinement 1.8); full-strength colours like (1, 0, 0.5) work as well as
  the tiny Ultra Instinct values.
* Behaviour flags 64/66 (0xe / 14 in cus aura 14) were guessed to be the
  Evil body tint; 1.4 sets them to 0xff. Wrong guess: 1.4 still darkened. 2.0 goes back to Purification's 14/0xe, because the 255/0xff values are the suspect for losing Purification's guard (not proven; the tint was the ECF, below).
* **ECF = the body tint** (1.4 in game: body darker + pulsing). Shader inputs
  (XenoKit VfxColorFade): g_vFadeMulti_PS = multi colour x its transparency,
  w = blending factor; g_vFadeRim_PS = rim x transparency; g_vFadeAdd_PS.
  Multi multiplies the body colour (Evil's is black -> dark body), a track on
  the blending factor (parameter 3; Evil 0.5 -> 0.8 -> 0.5) pulses it. A pure
  outline = multi white, add black, rim colour, blend 1, no tracks (Mgt_Aura,
  effect 847, is the closest vanilla one). Survey of every BTL_AURA ECF:
  `RefinementAura.exe ecfs "<game>"`. Refinement 1.5 did this: in game the
  whole body came out washed-out white, with no outline. **An ECF rim is
  not an outline**: it tints the body from the silhouette inward
  (the Evil ECF's rim made 1.3's body purple).
* **Body outline = BPE** (`data\pe\cmn.bpe`, `BPE_SubEntry Type="BodyOutline"`,
  keys: colour, transparency, near/far fade). XV2Patcher's pre-baked.xml
  `<AuraExtraData aur_id bpe_id flag1 flag2>` ties one to an aur aura: aura
  35 (Ultra Instinct) -> BPE 86, a steady pale cyan outline (0, 0.098, 0.118);
  small values like this already render bright. LB installs BPE entries
  (`{AutoID, SetAlias}` on Index works, Install_BPE parses bindings) and
  AuraExtraData (tracked by aur_id). List them:
  `ClaudeTools\refinement\bpe_outlines.py <cmn.bpe.xml>`. Refinement 1.6
  ships a pink copy of 86, confirmed in game.
* **aur_id means the aur aura drawn, after any remap:** a pre-baked
  `CusAuraData` can send a CUS aura elsewhere (Revamp: `cus_aura_id="0xe"` ->
  `aur_aura_id="0x9c4"`, 2500; written in hex, so search for that too). An
  outline on aur 14 then never shows. Refinement 2.9 lists both ids (untested).
* Xv2CoreLib's EMA light ChangeHue only recolours at the red channel's key
  times; G/B keys elsewhere keep their colour. Recolour all channels' keys
  yourself (Refinement's `LightToLightPink`).

## State
* Game patch = `EffectTextureLimit\EffectTextureLimit.dll`; since 1.1.0 only
  with `"raise_limit": true` (off by default); visual on/off comparison not
  done yet. NT 3.4.5 on
  Revamp gives BTL_AURA 134 textures; 128..133 are used only by NT's Ultra
  Instinct (Sign) aura EMPs (LB_AuraLoop_UI_1, LB_Aura_*GKS*,
  LB_AuraBurst_LineUI) and LB_ZMG_Aura01 (`ClaudeTools\effects\emp_high_textures.py`
  lists them for any effect file). EMP sampler table: header +14 count,
  +20 offset, 28-byte entries, +1 EmbIndex.
* Players without the patch see particles using index >= 128 untextured;
  the patch changes nothing for files with <= 128 textures.

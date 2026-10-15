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
an alternative that needs no exe patch.

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

## State
* Game patch = `EffectTextureLimit\EffectTextureLimit.dll`; applies in
  game (log line), visual on/off comparison not done yet. NT 3.4.5 on
  Revamp gives BTL_AURA 134 textures; 128..133 are used only by NT's Ultra
  Instinct (Sign) aura EMPs (LB_AuraLoop_UI_1, LB_Aura_*GKS*,
  LB_AuraBurst_LineUI) and LB_ZMG_Aura01 (`ClaudeTools\effects\emp_high_textures.py`
  lists them for any effect file). EMP sampler table: header +14 count,
  +20 offset, 28-byte entries, +1 EmbIndex.
* Players without the patch see particles using index >= 128 untextured;
  the patch changes nothing for files with <= 128 textures.

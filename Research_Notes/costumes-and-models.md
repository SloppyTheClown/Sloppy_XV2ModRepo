# CaC costumes, models and porting (XV2_ClownNose; formats for XV2_EliteKnight)

Covers: building a new-costume x2m by script (x2m.xml schema, BCS part sets,
IDB entries, file naming), EMD/EMM/EMB/ESK via the XV2 Xml Serializer, XV2
character shaders vs full-colour textures, accessories as BCS physics
objects ("Accessory without replacing the hair", XV2_ClownNose), where the
x2m sources/tools live locally and how to check an installed x2m. The
Elite Knight port (Dark Souls PTDE models, fitting to the XV2 skeleton,
other races, its version history and the texture-by-UV shader findings)
is in the Elite Knight project's own notes. Projects: `ModelSwaps\XV2_EliteKnight\` (pipeline in
`tools\make.py`), `ModelSwaps\XV2_ClownNose\` (`tools\build.py`).

## Costume x2m (xv2ins 4.7, from eternity_common `DBXV2/X2mFile.cpp`)
* Zip: `x2m.xml` + one folder per race code `HUM` (= HUM+SYM), `HUF`
  (= HUF+SYF), `NMC`, `FRI`, `MAM`, `MAF`. Files in a race folder are copied
  to `data/chara/<RACE>/`; the first `10000..10000+n` in a file name becomes the
  installed part-set number (3 digits), e.g. `HUM_10000_Bust.emd` ->
  `HUM_<n>_Bust.emd`. Files without such a number are skipped.
* Root: `<X2M type="NEW_COSTUME">`, params `X2M_FORMAT_VERSION` (26.0 ok),
  `MOD_NAME`, `MOD_AUTHOR`, `MOD_VERSION` (float), `MOD_GUID`, `UDATA`
  (mandatory from 6.51: base64 of 64 arbitrary bytes). Params are
  `<NAME value="..." />`.
* `<X2mItem>`: `ITEM_NAME_EN`, `ITEM_DESC_EN` (mandatory), optional other
  languages / `ITEM_HOW_*`, `ITEM_TYPE` TOP|BOTTOM|GLOVES|SHOES|ACCESSORY,
  and one `<IdbEntry id="0" type="...">` (IdbFile.cpp Decompile layout; ids
  and name/desc ids are assigned at install). `MODEL` = index of the x2m's
  part set (0-based), not a BCS number.
* `<X2mPartSet>`: `RACE` (index into the list above) + one `<PartSet>` with
  exactly 10 `<Part idx="0..9">` (Face_base, Face_forehead, Face_eye,
  Face_nose, Face_ear, Hair, Bust, Pants, Rist, Boots) or none; an empty part
  has no `MODEL`. All races used must have the same number of part sets.
* Part params (Eternity name = LB XML name): MODEL/MODEL2/TEXTURE,
  U_10 = Shader, U_18 = PartFlags, U_1C = HideParts bits, U_20 = HideMats
  bits (bits: FaceBase 1, Forehead 2, Eye 4, Nose 8, Ear 0x10, Hair 0x20,
  Bust 0x40, Pants 0x80, Rist 0x100, Boots 0x200), F_24/F_28 = F_36/F_40,
  U_2C/U_30 = I_44/I_48, NAME = chara code (<= 3 chars), FILES = "EMD, EMM,
  EMB, EAN" with empty entries written `NULL` (empty = default name
  `<NAME>_<model>_<Part>.*`). MODEL 10000+k is resolved like file names.
* Vanilla HUM: helmets/hats are Hair-only part sets; clothes use
  Bust/Pants/Rist/Boots part sets (often with HideMats, e.g. Pants hiding the
  Bust's tucked material).
* IDB starter entries (first entry of costume_*_item.idb / accessory_item.idb)
  are fine templates for the stat effects but NOT for shop fields: they have
  DLC_FLAG 0xFFFF, AVAILABILITY 0x7FFE = TP-medal-shop only, so the item was
  sold nowhere (seen in game). Use DLC_FLAG 29 + AVAILABILITY 0x7FFF (zeni
  shop from the start, see lobby-quests-and-shops.md "Items and shops"). RACELOCK
  bits: HUM 1, HUF 2, SYM 4, SYF 8, NMC 0x10, FRI 0x20, MAM 0x40, MAF 0x80.

## Sources and tools kept locally (don't re-download)
* eternity_common (xv2ins/x2m source: `DBXV2/X2mFile.cpp`, `X2mCostumeFile.cpp`,
  `BcsFile.cpp`, `IdbFile.cpp`, `UtilsStr.cpp`): shallow clone at
  `ClaudeTools\eternity_common` (commit 1a8e138, 2026-09-22).
  `git -C ClaudeTools\eternity_common pull` to update.
* LazyBone's XV2-Tools / Xv2CoreLib (C# file layouts, BCS flag names):
  `ClaudeTools\XV2-Tools`. Its built XML serializer:
  `ClaudeTools\xmlserializer\XV2 Xml Serializer.exe` (EMD/EMM/EMB/ESK/BCS
  <-> XML; run it on a copy, it writes `<file>.xml` beside it).
* Game files: `XV2DataEdit\xv2data\cpk.py` `CpkSet(<game>\cpk)`; names in
  `list(prefix)` are **lower case** (`data/chara/huf/huf_846.emd`); `read(name)`.
* `ClaudeTools\bcs_partset.py <RACE> <id>...` prints part sets from the installed
  (or cpk) BCS; `--physics` tallies every physics object kind with an example.
  Use it to copy a vanilla pattern before inventing one.

## Checking an installed x2m (fast path)
* xv2ins writes loose files: `data\chara\<RACE>\<RACE>_<n>.*` (n = next free part
  set, Clown Nose got **2005**), edits `data\chara\<RACE>\<RACE>.bcs` in place
  and appends the IDB entry (Clown Nose: accessory_item.idb index 202, id 160).
  Sort `data\chara\<RACE>` by date to find n. SYF uses HUF's files and BCS.
* Then: `ClaudeTools\bcs_partset.py <RACE> <n>` for the BCS side; the IDB entry's model
  (u32 at +0x30 of each 0x310-byte entry after the 16-byte header) must be n.
* Reinstalling an x2m with the same MOD_GUID updates it in place.

## Accessory without replacing the hair (XV2_ClownNose, 2026-10-10)
Belmod's Wig = accessory_item.idb index 182 (id 190, IDB MODEL at entry +0x30
= 1406, stars 5, AVAILABILITY 0x7FFE = TP medal shop). BCS part set 1406 puts
`<race>_1406_hair.emd` in the **Hair** part (PartFlags 0x4
DytRampsFromTextureEmb, no hides), which is the only reason it replaces the
hair. EMD model `c1406_wig`: submesh `Wig_A` = hair, `Wig_B` = the nose (136
verts, 236 tris, 100 % b_C_Head, one piece on all 6 races; NMC/MAM/MAF have an
extra skin/hair Model before `c1406_wig` in the same file, so search all
models). EMM `Wig_B` material keeps MatScale1X 1 (its .dyt.emb line).

Pattern for a head prop that leaves hair/face alone = vanilla glasses (part
set 846): Face_base part with MODEL/MODEL2 -1 (x2m: 65535), PartFlags 0, no
hides, plus a `PhysicsObject` (x2m params U_00/U_02 Model1/2 = 0xffff, U_04
texture, U_18 flags, U_1C/U_20 hides, NAME = race code, STR_28 = six names in
the order **EMD, EMM, EMB, EAN, bone to attach, SCD** - Xv2CoreLib BCS
Parser.cs offsets 40-60; empty = `NULL` (UtilsStr.cpp GetMultipleStrings
turns NULL into ""), empty EMM/EMB default to the EMD name. xv2ins swaps
`10000` inside STR_28 too (X2mFile.cpp ResolvePartSetString runs over
`ph.unk_28`).
* **Physics object flags follow the shader** (HUF.bcs tally + `samplers`
  comparison, 2026-10-10): 0x4 DytRampsFromTextureEmb = non-UNIF shaders
  (TOON_STAIN1/2_DFD: glasses 846/844/941, hats 835, scouters 854) whose ramp
  is a small 2176-byte texture inside the model's own `.emb`, sampled with
  Clamp (e.g. glasses sampler 0 -> EMB 1 Clamp, sampler 1 -> EMB 0 Wrap); no
  `.dyt.emb`. TOON_UNIF_* shaders need a `.dyt.emb`; which flag finds it
  depends on the parent part (`ClaudeTools\bcs_physics_dyt.py`: HUF objects by
  flags x own dyt x parent model, 2026-10-11):
  * flags 0: all 203 vanilla uses sit under a parent part **with** a model
    (`*_mark_SCD` on a Bust). Under a model-less part (the nose) it doesn't
    draw at all (Clown Nose v1.0 and v1.2).
  * 0x1 (Xv2CoreLib: "seemingly does nothing") is what objects under a
    model-less part use: `HUF_299_hair_scd` = TOON_UNIF_STAIN1_DFD + own
    `.dyt.emb`, Hair part Model -1, no LODs - the closest vanilla match to
    the nose.
  * 0x2 DytFromTextureEmb: dyt named after the object's EMB field
    (`HUF_491_mark_SCD` has no dyt of its own, EMB = HUF_490_mark_SCD, which
    has one).
  * Every vanilla b_C_Head object is non-UNIF, hence all 0x4 there. Regular
    parts differ: the wig's Hair part has 0x4 with UNIF + `.dyt.emb`.
* **What the exe does with the flags** (1.26.01, static disassembly of the
  running decrypted image via `ClaudeTools\exe\dump_running.py`, 2026-10-11):
  * `0x29ae30` loads one physics object: LOD = `0x29ef60(race)` (0 if
    flags & 4); dyt name = EMD name (EMB name if flags & 2) + `.dyt.emb`;
    its own dyt is loaded **only if the 5th argument (an external dyt) is
    null**, otherwise that one is stored at +0x210.
  * `0x29c070` = regular part loader (call at 0x29c6c8): skips objects with
    flags & 4; with flags & 1 passes the part's dyt (rdi+0xd0) as the
    external one; runs only for a part that has a model.
  * `0x6588a0` = accessory loader (calls 0x658b8f / 0x658d03): loads **only**
    flags & 4 objects; external dyt = the part object `0x29d1e0(part index)`
    vtable +0x30 = the CaC's **face** dyt; no part object -> skipped.
  * So on an accessory part set without a model, only 0x4 loads at all, and
    a TOON_UNIF_* shader then reads the face's ramps (washed out). The
    working combination is the glasses': 0x4 + non-UNIF shader + ramp inside
    the `.emb`.
  * Symptom of a non-loading accessory: equipping it leaves the previous
    accessory drawn (the appearance doesn't refresh) and the slot is empty
    after logging in again. The IDB entry was not the cause
    (`ClaudeTools\idb_compare.py` diff vs the wig: only ids/DLC/price/model).
* Belmod's nose ramp = line 1 of `<race>_1406_hair.dyt.emb` #0 (128x128 DXT1,
  16 rows per line): rows 16-19 100000 -> 290810 -> 5a1018 -> c72031 ->
  ff344a, 20-23 green/yellow, 24-31 black. Vanilla in-.emb ramps (huf_846
  #1, huf_853 #0/#1) are 128x16 DXT5 2176 B with the same row layout, so the
  16-row slice drops in (kept as DXT1: 3-colour blocks don't convert 1:1).
  Wig `.emb` = [32x32 mask (black at the nose's UVs), 512x512 lines (near
  white there)]. There is no non-UNIF STAIN2_MSK shader; TOON_STAIN2_DFD
  (scouters) samplers = [ramp Clamp, lines Wrap], ramp line = MatScale1X 0.
* Clown Nose history: v1.0 flags 0 + wrong ESK tree, v1.2 flags 0, v1.3
  flags 0x1: never loaded (part loader, no parent model - see above). v1.1
  flags 0x4 + vanilla ESK tree: in the right spot but washed out, black on
  the equipment screen (UNIF shader on the face's dyt). v1.4: flags 0x4,
  TOON_STAIN2_DFD, `.emb` = [wig lines, nose ramp], samplers [1 Clamp, 0
  Wrap], no `.dyt.emb`: stays equipped, same in menu and field, but **all
  black** - the wig's line texture (near white, opaque DXT1) is ink
  everywhere for a `_DFD` shader. v1.5: line texture = blank 32x32 DXT5
  (black, alpha 0), ramp re-encoded as DXT5 alpha 255 like the vanilla ones
  - **works in game** (right colour, stays equipped, same in menu and
  field). It has no sheen, unlike the wig's nose. That is probably the
  shader change (the wig uses an MSK material, the nose `_DFD`), but this
  wasn't checked. Accepted as it is.
* `_DFD` line textures (`ClaudeTools\emb_texture_stats.py` over hum_853/846):
  black RGB, ink in alpha (scouter #3 alpha mean 17, glasses #0 16); MSK
  shaders' line textures are near white instead, so they don't carry over.
* Colourable accessory instead of a fixed ramp: no vanilla accessory has
  colour selectors, and a 0x4 object's dyt is the face's, so a picker would
  recolour from the face's lines - not pursued.
* The physics model has its own skeleton `<name>.esk`: glasses' is
  EXP_<RACE>_846 with **two children**: b_C_Base, and x_x_SkinRoot >
  c846_glasses; all identity (Version 37508, Flag 1, UseUnk2). Nesting
  x_x_SkinRoot under b_C_Base (v1.0) makes the object invisible. The EXP root name need not match the
  file name (33 of 1828 vanilla ESKs reuse another, e.g. huf_700_skirt_scd ->
  EXP_HUF_513_skirt_SCD), so xv2ins not renaming it is fine.
* Vertices are head-local and skinned to b_C_Base (glasses: X +-0.077, Y
  0.015..0.06, Z -0.087..0.018 on HUF; -Z = forward).
* Body space -> head space: b_C_Head has identity rotation in every race's
  `<race>_000.esk`, AbsoluteTransform Line4 Y = -h: HUM 0.6260537, HUF
  0.4189977, NMC 0.7527669, FRI 0.4876199, MAM 1.101364, MAF 0.4181729.
  Head-local Y = body Y - h (X/Z unchanged). Nose result (HUF: Y -0.008..0.029,
  Z -0.107..-0.074) sits just below and in front of the glasses on all 6 races.
* Lesson: copy every field of the closest vanilla example (flags, ESK tree,
  Files) and diff our serialized files against it before building; "an
  example with these flags exists elsewhere" was not enough.

## Model files via XV2 Xml Serializer
* LB's `XV2 Xml Serializer` (XV2-Tools) converts EMD/EMM/EMB/ESK/BCS both
  ways. The checkout's csproj references NuGet YAXLib 4.5, which breaks it at
  runtime (`YAXAttributeForClassAttribute` missing): point it at
  `..\DLLs\YAXLib.dll` instead. A working build is in
  `ClaudeTools\xmlserializer\` (moved from ModelSwaps\XV2_EliteKnight\tools 2026-10-10).
* EMD XML: Model > Mesh > Submesh (name = EMM material name) with
  TextureSamplerDefs (TextureIndex = EMB entry), Triangles (each list has its
  own `<Bones>` name table; vertex BlendIndexes index into it; we keep <= 24),
  Vertexes (Position, Normal, Texture1, BlendIndexes/BlendWeights x4).
  `tools\xv2emd.py` reads/writes it.
* UVs: vanilla V values are negative = DirectX V - 1; with Wrap sampling DS1's
  DirectX UVs can be used unchanged. (Reasoned, not yet seen in game.)
* HUM skeleton (HUM_000.esk): Y up, character faces -Z, T-pose; pelvis at
  origin, head joint +0.63, feet -0.78, toe -0.85. Body deform bones b_C_*,
  b_L/R_*, fingers h_L/R_{Thumb,Index,Middle,Ring,Pinky}1-3, helpers
  *_ArmHelper, *_Elbow, *_Knee, *_LegHelper, b_L/R_ArmorParts.

## Shaders (adam_shader technique_*_sds.emz, zlib raw after 16 B header)
* Character shaders (TOON_UNIF_STAIN*_DFD ...) take colour from the DYT ramp
  per material; the EMB texture only gives line art (A), scratch (R) and
  blood (G) - a painted diffuse is not shown. (XV2AutoShader's Blender
  reimplementation, github.com/Nyxifer-prog/XV2AutoShader.)
* Skinned textured non-toon techniques exist (`T1_C_W` once in
  stage/lby/lbya23, `T1_C_L_SM_W`, `SpcBump_UV1_W`, `BgChara_W`) but **a CaC
  part with `T1_C_W` is drawn with the toon pipeline anyway** (seen in game,
  v1.0): the DS diffuse was read as line/scratch/blood masks and coloured from
  the copied DYT's placeholder green/yellow ramps. Dead end for costumes.
* Vanilla CaC clothing (HUM_300): one material per colour region, shader
  `TOON_UNIF_STAIN2_DFD` (skin `STAIN3`, alpha-tested `..._DFDAth`), params
  MatCol0=(0,0,0,1) line colour, MatCol1=(0,0,0,1), MatCol3=(0,0,1,1),
  MatScale0=(1,1,1,0.028), MatScale1=(line,1,1,1), Glare 1. Each submesh has 2
  sampler defs with the same TextureIndex = EMB entry; DYT entry n pairs with
  EMB entry n. EMB DDS: A = ink lines (bright lines + soft halo on black),
  R = scratches, G = blood (battle damage), B unused.
* DYT DDS 128x128; MatScale1X picks a 16-row block. Fixed-colour block (e.g.
  HUM_509_Hair, Piccolo's turban): rows 0-3 the colour ramp along x (stepped:
  dark x<64, mid <112, light), rows 4-7 a green->yellow aux ramp, rest black.
  Stain blocks (CC materials) hold a different pattern; copy vanilla ones.
* **Each colour group needs its own DYT line** (MatScale1X): the game writes
  a picker's colour into the line its materials use. v1.1 had every CC group
  on line 0 -> only the last picker did anything and it recoloured the whole
  part (seen in game). Vanilla: CC00 line 0, CC01 line 1, SKIN line 2 ...
* `TOON_UNIF_ENV` = matcap shine: its EMB entry is a sphere map (HUM_1414
  suit: a painted purple sphere), its DYT entry is mostly white; params
  ReflectCoeff/ReflectFresnelBias/ReflectFresnelCoeff 1, MatOffset1 0. No line
  art on those triangles (the EMB entry is the sphere map).
* Recolouring: material name prefix = BCS PartColors name: `CC00_BUST_` ..
  `CC03_BUST_` (HUM PartColors index 6-9), PANTS 10-13, RIST 14-17, BOOTS
  18-21, SKIN_ 0, HAIR_ 4, eye_ 5, PAINT_A-C 22-24. The part needs a
  ColorSelector (PART_COLORS = that index, COLOR = default entry) per group to
  show the picker. Palette entries: Color1 lit colour, Color2/3 shadow,
  Color4 = Color1.
* Accessories: IDB MODEL = a HUM.bcs part set whose Hair part is the
  headwear (accessory 0 Piccolo's Turban -> 509). Every vanilla accessory Hair
  part has PartFlags `DytRampsFromTextureEmb` (0x4) and no colour selectors.
  v1.0's helm (flags 0, T1_C_W) did not show at all and its hide flags had no
  effect - cause not isolated (v1.1 uses flag 0x4 + STAIN2: untested).
* Winding: XV2 triangles agree with vertex normals (cross(b-a, c-a) . n > 0);
  DS1 FLVER triangles are the opposite - reverse them or the game culls the
  outside (v1.0 looked see-through).
* XenoKit renders with the game's own adam_shader files, so it is a valid
  preview.

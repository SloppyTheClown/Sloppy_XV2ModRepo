# Sparking! ZERO -> XV2 (Angry Explosion)

Covers: porting from Sparking! ZERO (UE5 paks via CUE4Parse): animation/camera retargeting, rebuilding Niagara effects as EMP, WindIN, x2m skill id placeholders.
Split out of `cross-game-effects.md` (2026-10-22).

## Sparking! ZERO -> XV2 (Angry Explosion)

Done for Majin Buu's ultimate "Angry Explosion" (a new CaC ultimate, ShortName
AEX). Nothing loads as-is; animation, camera and textures are read out of the
paks and rebuilt as XV2 files. State: built, not seen in game (2026-10-20).

### Reading the paks
* SZ is UE 5.1 IoStore (`SparkingZERO\Content\Paks`, AES-encrypted, Oodle).
  CUE4Parse reads it with `EGame.GAME_UE5_1`, the AES key from LostImbecile's
  open-source SZ-Audio-Modding-Tool (`utoc_generator.c`) and a `.usmap` from
  TheNaeem's Unreal-Mappings-Archive (without mappings, unversioned
  properties don't parse). Probe: `ClaudeTools\szexport` (.NET 10 console app:
  list / json / refs / tex / anim / refskel / mesh / loc / raw).
* Buu = chara 0170. Ultimate clips are `..._0170_00_ult..._ry1_*` AnimSequences
  (one per phase: 0DEM build-up 471 frames, 3HT1 blow-up, 4LP1 hold, 6OT2
  recovery; Ah1P/Bh2P/COT0 are other branches). Sampled per frame with
  CUE4Parse's anim conversion; SZ runs ~60 fps, so 1 SZ frame = 1 XV2 frame.
* The cut camera is a level sequence `LSCAM_0170_00_actULT_SRPSD`: track
  BP_BattleCutCamera2 = camera transform, BP_LookAtTarget2 = look-at point,
  CameraComponent CurrentFocalLength = zoom, all bound under BINDING_ORIGIN
  (the character actor: UE axes +X forward, +Y right, Z up, cm, follows root
  motion). Keys baked per frame, tick resolution 24000 (400 ticks = 1 frame).
  Rotation channel 0 = roll.
* The head-hole steam is not in the ultimate: it is a story cutscene effect,
  PS_EF_Evn_04_3_03_30_00 / _01 (`out\event\PS_Evn_00.json` / `_01`). These
  are **Cascade** ParticleSystems, not Niagara (corrected 2026-10-21), so
  every module value is in the dump and converts to EMP numbers (see
  "Cascade -> EMP" below). _01 = _00 played once (EmitterLoops 1). The
  cutscene (EF_05B looping, EF_06 once) attaches one copy per head socket
  with component scales 00 0.7, 01 1.0, 02 0.8, 03 1.0, 04 0.7, 05 0.9;
  no copy sits on the arm sockets.
* Head holes = six mesh sockets on BPCHR_0170_00_00's skeletal mesh
  (`socket_Buu_00..05`, on HEAD, offsets in cm + FRotator; steam flies along
  the socket's +X).

### Sound effects
**Dumping recipe (any character, re-run and matched byte for byte on Buu's 10):**
1. `szexport raw <regex> <outdir>` saves `BTLSE_<chara>_00.awb` (effect sounds) +
   `.uasset` (the ACB, with cue names). Voices are separate: `BTLCV_<chara>_00_*.awb`.
   HCA type 56, encrypted. Buu = 0170.
2. Key = `13238534807163085345 * ((subkey<<16) | (uint16)(~subkey+2))` mod 2^64
   (`hcakey_generator.c` in SZ-Audio-Modding-Tool), subkey = the AWB's u16 at 0x0E
   (61451 for Buu, key 0x40eb19e8a58cbab6). It differs per AWB. `combined_key` in
   `ClaudeTools\refinement\sz_audio\hca_plain.py`.
3. Split the AWB into `.hca` files, named `<awb id>_<cue id>_<cue name>`.
4. Put a `.hcakey` next to the HCAs (the key as 8 big-endian bytes) and run
   `ClaudeTools\vgmstream\vgmstream-cli.exe -i -o out.wav in.hca`. `-i` = one pass,
   ignoring the loop; `-m` prints sample rate, channels and loop points.

**Traps:**
* **Without the `.hcakey`, vgmstream still gives a WAV of the right size, but its
  samples differ.** Compare against a known-good decode if a dump sounds wrong.
* **ffmpeg and VGAudio garble these files; use vgmstream only.** The HCAs (v3.0) have
  min_resolution 0: resolution-0 bands cost no bits and get noise fill (clhca.c
  `calculate_resolution`, `reconstruct_noise`). ffmpeg's hcadec.c and VGAudio have
  no min_resolution handling (peak correlation with vgmstream only 0.37-0.80), and
  agree with each other, which looks like confirmation but isn't. `dump_awb.py`
  still decodes with ffmpeg: use it for the split + names only.
* The 0xFFFF frame sync proves nothing: the cipher table maps 0xFF to 0xFF under
  every key. `test_key.py` checks scale factors instead: 0 bad frames here, wrong
  keys score many.
* Output paths: vgmstream wrote nothing to a mixed-slash `%TEMP%` path from Git Bash.
  Run it from the folder with relative names.

**Buu's sounds** (48 kHz; cue names say what each is for, `SPC` = the steam move):

| Sound | Length | Use |
|---|---|---|
| `000_15531 actSPC_2_LOOP` | 6.357 s, mono | HCA loop over the whole file (305152 samples) |
| `001_15530 actSPC_1_IN` | 2.733 s, mono | |
| `002_15532 actSPC_3_MAX` | 1.393 s, mono | |
| `003_12101 actSPM1_SD_2_LD` | 1.017 s | |
| `004_12120 actSPM1_HD1P_1` | 6.349 s | |
| `005_12100 actSPM1_SD_1` | 1.437 s | |
| `006_12102 actSPM1_SD_3_OT` | 2.265 s | |
| `007_14520 actULT_HD1P_1` | 8.243 s | |
| `008_14500 actULT_SD_1_32F` | 2.880 s | |
| `009_14501 actULT_SD_2_205F` | 5.319 s, stereo | |

**Into an XV2 bank:** WAV -> HCA with VGAudio's `ConvertStream.ConvertFile` (encoding
is fine, only its decoding of these files is wrong), then `ACB_File.AddCue(name, id,
ReferenceType.Sequence, hca, false, loop, EncodeType.HCA, out cue)`, as XenoKit's Add
Cue does. The `loop` argument only sets the waveform's LoopFlag: the HCA needs its own
loop chunk too, `EditLoopOnWaveform(waveform, true, startMs, endMs)`.
`ReplaceTrackOnCue(cue, hca, false, EncodeType.HCA)` swaps a cue's sound in place. A
save can renumber AWB ids (memory and streamed sounds share one id space), so compare
banks per waveform row, not per id.
* A CUS `ACB_SE` path + CharaSE gives a skill a bank at a fixed name
  (`sound/SE/Battle/Skill/<path>.acb`), which auto-assigned ids can't change.

### Rebuilding
* Retarget (`AngryExplosion_Ultimate\tools\retarget.py`): per mapped bone in
  world space `target = source * source_bind^-1 * A * target_bind`, A = the
  rotation taking the target's bind bone direction onto the source's, so
  A-pose vs T-pose differences don't leak in. The source bind must be the
  shared SZ skeleton's reference pose (the clips are authored against it,
  not against Buu's mesh bind). Root NULL -> b_C_Base, WAIST offset ->
  b_C_Pelvis, scaled by leg length (CaC 0.78 m vs Buu 0.59 m). Helper bones
  copy the rule seen in vanilla CaC EANs: LegHelper = 50% of Leg1, ArmHelper
  = 80% of Arm1, ArmRoll = the hand's twist, g_x_LND = -b_C_Base position.
* Camera EAN (`tools\camera.py`): one bone "Node"; Position = camera,
  Rotation XYZ = target point, Scale X = roll, Scale Y = **vertical** FOV,
  both radians (XenoKit Camera.cs). Space = the character's (faces -Z)
  around the BAC Camera item's BoneToFocusOn - use b_C_Base so it follows
  the root motion as SZ's does. FOV = 2 atan(sensor height / 2 focal length)
  with UE CineCamera's default filmback (13.365 mm high; the sequence only
  keys the focal length).
* Socket mapping (`tools\sockets.py`): SZ HEAD -> XV2 b_C_Head differ by a
  fixed rotation once retargeted, so `local_xv2 = R_xv2^T * C * R_sz *
  local_sz` at any frame (checked: identical at frames 0/200/400); Buu's head
  is much bigger, offsets scaled by 0.0065 (unverified in game). Buu's
  skeletal mesh (`out\buu\SKEL.json`) also has arm holes socket_Buu_06-08
  (SHOULDER_L x2, SHOULDERROLL_L) and 09-11 (right side), pitch +-90 = steam
  along the arm bone's local Z; mapped to b_L/R_Arm1 (anchor SHOULDER_*),
  scaled by upper-arm length (SHOULDER->ELBOW vs Arm1->Arm2).
  Placing them: one EMP per hole (emitters at the origin firing +Y), the
  hole's offset and aim on its own EEPK part (Position = bone-local metres,
  Orientation X/Z in degrees, same convention as an EMP Rotation), so each
  is movable with XenoKit's part gizmo. Keep the part flag
  **UseBoneDirection** (0x20) off: XenoKit's
  `VfxAsset.GetBoneAttachTransform` then re-aims the bone frame's Y at a
  child bone, which turned the head jets front/back and the arm jets 90 deg
  above the shoulder (user, XenoKit, 2026-10-21).
  Vanilla does the opposite: of bone-attached parts with a non-zero Position in a third of the skill eepks, nearly all have the flag on (277 with AttachmentBone + both updates; off only a handful, mostly on-axis offsets). Big off-axis offsets with it off are uncharted in game.
  These parts have **Orientation None** (byte +4; the old serializer's XML
  calls it `RotateOnMovement`). XenoKit's `VfxAsset.GetAdjustedTransform` then
  keeps only the bone's position, so X/Z above are a world direction and don't
  follow the bone. For aim that follows the bone use AttachmentBone (2). It
  would explain Refinement 2.12's tilted arm vents coming out wrong in game
  (one world direction, both arms); the switch in 2.13 is not yet seen in game.
* Retarget on Male Majin: Buu's clavicles shrug 40-80 deg (vanilla CaC
  ults 15-30); MAM's Arm1 sits 0.43 m out from b_*_Shoulder (HUM 0.21), so
  the full shrug lifted the arm roots 0.40 m above Neck1 (past the head).
  retarget.py `DAMPED` scales the clavicles' local rotation to 0.3; Arm1's
  world rotation stays Buu's. CaC skeletons all have identity bind
  rotations; skill EANs key Position only for b_C_Base, b_C_Pelvis and
  g_x_LND, so each race's bone lengths apply.
* Foot sliding after the retarget (2026-10-22): rotation-only retargeting
  onto shorter/longer legs slides planted feet 2-20 cm.
  `AngryExplosion_Ultimate\tools\footlock.py` finds contacts on the
  **source** feet (SZ TOE1 = ankle, TOE2 = toe; the target's toes sit too
  high to detect from), pins the target ankle at its span mean, solves
  Leg1/Leg2 by two-bone IK (pole from the animated knee), gives Foot its
  animated world rotation back, fades the pin over 6 frames and lowers the
  pelvis (capped at 6 cm) where a pin is out of reach. Dead ends: forcing
  every pin reachable alone made 30 cm/frame pops between spans; lowering
  the pelvis alone dipped it 33 cm. Drift -> 0.00 cm offline
  (`ClaudeTools\aex_footcheck\footcheck.py`); fitted to the CaC skeleton's
  leg lengths only.
* **One CaC EAN, six skeletons** (2026-10-22): XenoKit (AnimationPlayer.
  UpdateNode) composes local = ESK_rel * EAN_rel^-1 * key and moves a
  shared EAN's b_C_Pelvis bind Y by -(BCS F_48[0] - 1) / 2 (offset 48 of
  `<R>.bcs`: HUM 1.0, HUF 0.78, MAM 0.73, MAF 0.82, NMC 1.205, FRI 0.745),
  so a race's pelvis = key + (F - 1) / 2 (MAM 13.5 cm lower, NMC 10.25
  higher) on its own leg lengths (all leg rest rotations identity). Feet
  grounded for HUM float/sink on the others except near one knee bend (HUM
  pelvis ~0.5 m); vanilla AGE (pelvis 0.62) floats the small races ~3 cm
  too. A skill can't ship per-race EANs without replacing the race EANs.
  footlock.py's race fit picks per-frame pelvis drop + per-foot lift over
  all six skeletons' soles (`ClaudeTools\aex_footcheck\skinned.py` checks
  by skinning each race's default boots/pants; `race_legs.py` dumps the leg
  data). Straight legs are where the races differ most (5-9 cm).
* SZ's **battle** steam asset was not found (don't re-hunt): socket_Buu_*
  appear only in the story cutscene effects (EF_CO_04_3_03_30_05B/06 =
  PS_Evn_00/01). The ult's own LSG_0170_00_actULT_SD has particle systems
  PS_00-03 (sparks, cracks, vibration lines) firing at frames 210 / 254 /
  349 / 450 (tick resolution 24000 at 60 fps = 400 ticks a frame); PS_70/72
  are used only by the hit LSGs (HD1P, HI) = explosion smoke, not steam. So
  the steam is the cutscene's PS_Evn_00, timed to the SD LSG's first
  particle (210). (A first hand-made look, short puffs + random wisp cells,
  read as "janky" next to SZ; replaced by the converted values.)
* **PS_Evn_00 (Cascade) values**, all `bUseLocalSpace`, velocity along the
  socket's +X, cm / s:
  * Bomb_Star Sprite_0 (MI_EF_Equ_Tra_Smoke775 = Unique079_E 5x4, unlit
    translucent, Linear_Blend SubUV): life 0.45-0.5, size 10-12 x
    SizeMultiplyLife_11 (1 -> 2.94 at 90 % -> 5), colour (6, 8.67, 10),
    alpha 0.9 held to 70 % then -> 0, 300 cm/s, drag 5, rotation +-0.25
    turn, rate +-0.01 turn/s, SubUV 5 -> 50 (wraps the 20 cells), 2/s + 1
    per 0.1 s loop.
  * Bomb_Star Sprite_13: life 0.35-0.65, size 4-7 x SizeMultiplyLife_2
    (1 -> 3.44 at 10 % -> 6), same colour/alpha, 200-300 cm/s, drag 3,
    sphere spawn r 7, rotation 0-1 turn, rate +-0.05, SubUV 3 -> 21 eased,
    8/s + 1 per 0.27 s loop.
  * Smoke_S Sprite_12 (MI_..._Smoeke773 = Smokel_01 8x8, PSA_Velocity):
    life 0.3-0.5, size X 3 / Y 30 x (7, 2) linear, colour (3,3,3) ->
    (4,6,6), alpha 0.5 -> 0.98 at 94 % -> 0, 100 cm/s, SubUV 0 -> 63 eased
    in-out, 10/s.
  * WindIN x2 (LockedAxis_ZAxisFacing / _NegativeZAxisFacing): mesh
    particles SM_EF_Equ_UniqueMS002, material MI_EF_Equ_Pt_Unique113 (parent
    M_EF_Cmn_Pt_VertexMeshAnime: masked, clip 0.3333, unlit, two-sided,
    1_FramesOfMeshes 32, texture T_EF_Equ_Smoke_000 8 x 4). Life 0.25-0.35,
    StartSize [3,3,3]..[5,2,2] x SizeScale 0.1, colour (1.1, 2, 2.5) x 1.8,
    alpha 1, 50-100 cm/s +X, 7/s + 1 per 0.3 s, local space;
    ParameterDynamic_6 param 0 -> 1 over life picks the mesh frame.
    The mesh (`szexport mesh`: 1764 verts, 1531 tris) is flat (Z = 0, X
    0..65, Y -8.6..19 cm) and holds 30 frames as separate triangle sets:
    frame = floor(UV2.x), UV0 maps frame f into texture cell f. Flat + turned
    about the jet axis to face the camera = an EMP velocity-oriented quad, so
    it is baked into a flipbook (`AngryExplosion_Ultimate\tools\wind.py`)
    instead of an EMO (EMP's Mesh type can't switch frames). XenoKit's
    velocity quad spans -0.5..1.5 half-lengths forward (ParticlePlane moves
    it by +ScaleV/2) and its top vertices take the cell's top V, so the cell
    top is the far end. Not seen in game yet.
  Cascade dump gotchas: a FRawDistribution's baked `Table` is what runs
  (sampled evenly over TimeScale/TimeBias; EntryStride = components), unless
  `Distribution` points at a non-bakeable object; SubUV indexes past the cell
  count wrap; PSA_Square uses size X only; sprite sizes are full widths.
* **Cascade -> EMP** (`tools\steam.py`): one parent VerticalDistribution per
  emitter (speed x 0.0065 m per cm, cone 0 as SZ, sphere spawn -> a 10 deg
  cone since EMP's PositionOffset_AddedRandom is 0..+max per axis), child
  AutoOriented sprite (radius = size / 2 x 0.0065 x hole scale, Scale X keys
  = radius x the SZ curve, AutoOriented_Component Rotation / SpeedRotation
  as Base = -range, AddedRandom = 2 x range, deg and deg/frame), Smoke_S as
  `AutoOriented_VisibleOnSpeed` (Age_FireLine's "line" layout: UseScale2,
  Flags_33 Unk5+Unk7, Scale X = half width, Y = half length, Z = base -
  ParticlePlane's first vertex uses the base for U, so Z follows X), drag
  as sub-group 11 with Cascade's value as is (units unverified). Colour:
  XV2 clamps at 1, so the colour is white and the texture carries the
  brightness. Don't flatten the RGB to white (first builds; "solid white"
  next to SZ): the puff sheet Unique079_E is cel-shaded (light ~200, shadow
  ~140) and SZ shows that shadow despite its 3-10 colour, so build.py
  stretches it by levels (200 -> 255, 140 -> 150); Smokel_01's RGB is plain
  white anyway. Flipbooks: EMP can't blend cells;
  one keyframe per run of frames along the SZ curve over the average life,
  Time = frames shown - 1 (XenoKit ParticleUV holds a key Time + 1 frames).
  **Never Time 0**: a flipbook with Time-0 keys (a cell a frame) froze the
  game the moment the particles spawned (2026-10-21); vanilla's lowest is
  1, so steam.py holds a cell >= 2 frames (fix not yet seen in game).
  EEPK InstantUpdate = Cascade local space.
* Vanilla AGE effect 0 (Age_CrgCor/Ring/wind/Base + LIGHT Age_Crg, on
  b_C_Pelvis) is the pink charge glow that fills the screen; with it, StageBG
  410/156 and effect 3 (Age_CrgStar sparks) on early, the steam was
  unreadable ("huge pink haze"). ptcl.emm material 1 = "nml" (plain alpha).
* EMP facts used (XenoKit Engine/Vfx/Particle): speeds m/s (moved speed/60
  per frame), times in frames, Scale animation keys = absolute sprite radius;
  emitter Rotation applies roll Z then pitch X to +Y. Children of one parent
  all spawn together, so each spawn rate needs its own parent emitter. An EMP
  Texture's TextureIndex = the index in the skill's ptcl.emb (shared by every
  PBIND part). EEPK effect with AttachFlags PositionUpdate + RotateUpdate and
  no InstantUpdate: the emitter follows the bone, spawned particles stay in
  the world; with InstantUpdate the spawned particles follow too (used since
  2026-10-21, as SZ's local space).
* Textures: SZ PNG (from CUE4Parse) -> uncompressed A8R8G8B8 DDS, box
  downscaled (pure Python, `tools\textures.py`).

### XV2 skill side
* Vanilla AGE = CUS ULT 352 (`data/skill/ULT/352_BUL_AGE/`), BAC entries 0
  (use), 1 and 3 (hit / end branches), 360 frames. Entries 1 and 3 continue
  entry 0's timeline: their skill animation starts at StartFrame 332 / 320
  and their items sit at 320+, so a retime must map them the same way.
* BAC Function 0x26 (38) "Pass BSA Entry": Param1 = BSA condition (must equal
  the BSA entry's `Bac_Condition`, 352 here - **not a frame, don't remap**),
  Param2 = skill type (6 = ultimate), Param3 = skill id.
* x2m placeholders xv2ins rewrites in the x2m's BAC/BSA/BDM (eternity_common
  X2mFile.cpp): 0xBACA (47818) = the new skill's id2, 0xBACB (47819) = id1.
  BAC ChangeReferencesToSkill covers effect items (type != common),
  projectiles, Function 0x25/0x26 Param3 and 0x4E.
* CUS mapping between eternity's x2m `<Skill>` and Xv2CoreLib's XML:
  NUM_TRANSFORMS (u32) = Xv2CoreLib NumTransformations (low 16) + I_66 (high
  16; AGE: 0x04070001); U_52 = Xv2CoreLib NEW_I_86. U_0E file flags for a
  CaC copy: drop CharaVOX (Buu's own voice bank) and the chara-specific
  EAN/cam (`352_bul_age_bul.*`, Buu's version); keep CharaSE with an explicit
  ACB_SE path (`CAR_BTL_U352_AGE_SE`), as vanilla B1080 skills do.
* Serializer gotcha: given a relative path the XV2 Xml Serializer writes next
  to the drive root, fails and waits for a key - always pass absolute paths
  (and stdin = DEVNULL from scripts).

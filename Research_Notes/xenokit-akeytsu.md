# XenoKit: Akeytsu export/import

Covers: the XenoKit "Akeytsu" menu - exporting the primary/victim/playing animation and BAC cameras to .akt (AktBridge), hand/foot IK, toon colours, importing back by name, and its state.
Split out of `xenokit.md` (2026-10-22).

## Akeytsu export/import (2026-10-21)
Top menu "Akeytsu": **Export to .akt...** writes the primary actor (+ the victim
when the victim is enabled and set) with its meshes and adds hand IK and
reverse-foot IK. Since 2026-10-22 (second design) the primary gets **every
animation of the Animation tab's selected EAN** in its animation bank (nothing
needs playing; the victim keeps its playing one), and **every Camera-tab
animation goes onto one separate camera character** (akeytsu's own layout, see
"Separate camera" below), each with its own length. **Import from .akt...**
bakes the .akt's animations back into the same-named EAN animations and the
camera character's animations into the same-named cam.ean animations (one Undo
step "Akeytsu import").

Design:
* XenoKit is net48, the .akt code (`Z:\Projects\Misc\Akeytsu\MyAkeytsu\src\MyAkeytsu.Core`)
  is .NET 9, so `AktBridge.exe` compiles MyAkeytsu.Core's sources (never edits
  them) and XenoKit runs it as a process: `export <in.xk> <out.akt>`,
  `import <in.akt> <out.xk>`. The .xk ("XKAK" v3: characters, the camera
  object's name + any number of camera animations, PNG textures; v1/v2 files
  still load) is a temp file in %TEMP%.
  Needs the .NET 9 runtime. `LocalBuild\build.bat` publishes it to
  `bin\Release\net48\AktBridge\`.
* Units: XenoKit metres <-> akeytsu centimetres (x100). Each actor's placement
  (`BaseTransform`, the victim stands away) is baked into its root bones;
  every akeytsu character sits at the origin (a character transform gave the
  IK controllers odd worlds in a sample).
* Pose (from `AnimationPlayer.UpdateNode` / `UpdateAbsoluteMatrix`): XenoKit's
  bone = keyframe(S*R*T) * inv(EAN rest) * ESK RelativeMatrix, so export writes
  `local = keyed * inv(eanRest) * eskRelative` per frame and import inverts it
  (`key = local * inv(eskRelative) * eanRest`). EAN rest pelvis Y is lowered by
  `(BCS F_48[0]-1)/2` for non-IsCharaUnique EANs, as XenoKit does. Bones the
  EAN doesn't animate use their ESK rest.
* IK (AktBridge `AktExporter`): `IK_b_L/R_Hand` on Arm1 -> Arm2 -> Hand,
  `RF_b_L/R_Toe` reverse feet on Leg1 -> Leg2 -> Foot -> Toe with 7 shoe
  locators. Every key is an FK key (joints authoritative, controller follows),
  so the animation is exact; keys carry their RefArray and, for the feet, the
  roll angle from the toe bend.
* Roll sign (found with MyAkeytsu.Cli `rfkeycheck`): the heel rolling up by
  +a bends the ball by -a against the foot, so roll = -bend about RollAxisPlant
  (`RollSign`). A toe curled *down* has no reverse-foot angle -> roll 0; those
  keys are exact as FK, but dragging the controller there flattens the toe.
* IK placement fixes after the first GUI export (2026-10-21, the user saw huge
  sideways hand rings and foot controllers above the hips):
  - Hand ring `DecoratorAxis` = the hand's basis row most parallel to the
    forearm (akeytsu's own Add IK rule). akeytsu's samples run bones along Y,
    XV2's along X, so the old fixed 1 turned the ring 90 degrees.
  - `TargetRenderSizeRatio` = forearm / 3 / scene TargetRenderSize (8): about
    an 8 cm ring on HUM. The sample ratio 4 drew 32-42 cm rings.
  - The reverse-foot controller + shoe locators sit at the sole's lowest
    vertex Y. XV2's rest has the hips at y = 0 and the feet ~60 cm below,
    so the old y = 0 put them at hip height.
* **Separate camera (2026-10-22, format 3).** akeytsu's camera is its own
  object, not a character's child: the user's sample
  `Z:\Projects\Misc\XV2\Akeytsu\MAM_BUL_ULT_AEX_C_TestEdit_WSeparateCamera.akt`
  (camera added in akeytsu) has a `CCharacter 'Camera_01'` with
  `__BlendshapeControllers`, `__Anims`, `__BindPoses`, `__RefPose`,
  `__RefPoseOld`, `__RigAnim` (stack: Camera Info, MRS Track, Looker_VK
  Track_02), `CCameraAnimated` > `CHideJointForLooker 'Default name'`, and
  `CCameraTarget 'Camera_Target_01'` - no bones. AktExporter builds exactly
  that (named `<cam.ean name before the first dot>_Camera`), one CAnimation
  per XkCamera, each its own length. AktImporter treats a character whose
  joints are all CHideJointForLooker and that has a CCameraAnimated as the
  camera (one XkCamera per animation, named like it); the old
  camera-under-a-character is read only when there is no camera character.
  XenoKit side (`AkeytsuExchange.ExportCameras` / `BindCamera`): each
  Camera-tab animation with a "Node" is baked over its own FrameCount; its
  space (target character + facing) comes from a BAC entry of
  `Files.SelectedMove`, else the primary's moveset, with an active Type10 on
  that camera (EanIndex = the animation's IndexNumeric, `GetCamEanFile`
  holds it) and an active full-body Type0 playing one of the exported
  animations from the same EAN; camera frame c -> BAC frame `StartTime + c -
  StartFrame` -> that body animation's frame (BacPlayer's skip rule as
  below). No such entry: the primary at frame 0. Empty Camera tab: the older
  single-camera sources below, as one camera animation.
  akeytsu plays every character at the same scene time, so since 2026-10-22
  each bound camera sits on its body animation's timeline: `CameraBinding.Offset`
  = `BodyFrameAt(0)` (the body frame of camera frame 0); the export holds the
  camera's frame 0 for the first Offset akeytsu frames (the space still follows
  the body frame), import maps akeytsu frame a -> camera frame a - Offset and
  drops the held frames, so the cam.ean keeps its own length. Negative Offset
  (the cut starts at a camera frame past the body's frame 0): those leading
  camera frames aren't exported and the import keeps them (replaces only the
  written range). Unbound cameras: Offset 0.
* Camera (export, before 2026-10-22 / empty Camera tab): baked per frame of the
  character it follows, FOV in `CameraKey.Fov`. Source, first
  that exists: (1) the camera animation XenoKit is playing
  (`Viewport.Instance.Camera.cameraInstance`); (2) the primary's BAC entry's
  camera cuts (Type10, Flags 0 or CaC 1/else 2, `Files.GetCamEanFile`, target
  actor 1 when FocusOnTarget) - needed because a BAC camera clears itself after
  its Duration (`Camera.UpdateCameraAnimation` -> `ClearCameraAnimation`), so the
  first akeytsu export (Action tab, paused) had **no camera at all**; the export
  frame maps to the BAC frame through the Type0 playing the exported animation
  (StartTime + f - StartFrame; f itself when StartFrame != 0 at StartTime 0,
  BacPlayer's skip), the latest cut started is shown, holding its last frame;
  with no BAC entry playing (animation played from the Animation tab) the
  entry is looked up: the first entry of `Files.SelectedMove`, else the
  actor's moveset, with an active Type10 and a Type0 playing the exported
  animation (same EanIndex, and `GetEanFile(EanType)` holds that animation) -
  added 2026-10-22 after the user's export opened with a camera named
  "Viewport_Camera" that never moved (fallback 3);
  (3) the viewport camera (`CameraState`) held still, named "Viewport_Camera"
  (that name in akeytsu = no camera animation was found).
  **Akeytsu side:** a camera without its target makes akeytsu open an empty
  scene (`CCameraAnimated::GetCameraTarget` has no null check, Akeytsu
  `research_notes/camera-animation.md`), so AktBridge (`AddCamera` /
  `CameraTracks` in `AktExporter.cs`) writes akeytsu's own rig, copied from the
  user's sample `Z:\Projects\Misc\XV2\Akeytsu\MAM_BUL_ULT_AEX_C_MyOwnCameraTest.akt`
  (our export + a camera added in akeytsu), into the main .akt:
  - `CCameraAnimated` (child of the character): Flags 1, Constraint = target,
    CameraType -1533556440, RenderMode 0, RenderFlags 4152, `_targetDistance`
    700, `_ratio` 1, Mode 0, Ratio 3, Near 15, Far 30000.
  - `CHideJointForLooker` "Default name" (child of the camera): Flags 130,
    local translation (0, 0, -distance), Constraint = target, SmoothMethod 0.
  - `CCameraTarget` (child of the character): Flags 1, Constraint null,
    TargetRenderSizeRatio 1, JointsString [camera, hide joint], Parents [],
    DecoratorAxis 2. Classes CLooker : CControllerIK, CCameraTarget : CLooker,
    CHideJointForLooker : CJoint, CAnimTrackLooker_VK : CAnimTrackIK_VK, none
    with members (`IKSchema.cs`).
  - Controllers: appended after the character's joints/IK/locators in the
    order camera, joint, target. **The hide joint also goes last in
    `JointArray`** (sample: 143 = 142 bones + it). The first rig export left it
    out and akeytsu opened an empty scene again (user, 2026-10-22); the other
    differences fixed with it: camera + target are the character's last
    children (after the meshes), the camera track is named "Camera Info" with
    mask 0x81FF (MyAkeytsu's TrackFactory writes 0x803F, no scale bits). The
    importer skips CHideJointForLooker when reading bones.
    **Schema names:** that export still opened empty. MyAkeytsu's
    `CameraSchema.Ensure` (Core/Akt/CameraSchema.cs, not ours to edit) adds
    CCameraAnimated, CAnimTrackCamera, CKeyCamera and CTangentInfoListCamera
    without akeytsu's "class " name prefix and with Mode before Ratio; the
    sample has `class CCameraAnimated` Ratio:int, Mode:int, NearClipPlane,
    FarClipPlane, and the prefix on all four. `IKSchema.Ensure` (runs first)
    now declares them the sample's way, so Ensure finds them and adds nothing
    (2026-10-22, checked offline with a raw schema-name dump, not seen).
    Tracks in every stack (incl. `__RigAnim`),
    after the bone tracks: CAnimTrackCamera (mask 33279), CAnimTrackMRS for the
    hide joint (511), CAnimTrackLooker_VK (575, CKeyIK, RefArray = [camera
    local, hide joint local]). Keys use Flags 0 (FK), so the camera plays its
    own track and the target follows (MyAkeytsu's evaluator solves IK only
    with Flags bit 0).
  - Rest pose = the camera at frame 0; the hide joint and the target sit at
    XenoKit's eye-to-target distance (40 cm when it is 1 cm or less), keyed
    per frame. Only the first animation with frames is keyed; the others get
    rest (identity) keys.
  XenoKit's player (`Camera.ProcessCameraFrame`): node "Node", Position = eye,
  Rotation XYZ = target, Scale X = roll and Y = vertical FOV (radians; no Scale
  -> FOV `EAN_File.DefaultFoV`, roll 0); world = raw rotated by the target's
  facing + the target bone's world (+ primary BCS `F_48[1] - 1` on Y for a
  non-IsCharaUnique EAN). Facing is rebuilt from the baked frames as b_C_Base's
  turn since frame 0 x the placement's rotation. BAC camera modifiers are not
  included. View = LookAt(eye, target, Y) x RotZ(roll); akeytsu gets the
  inverse as the camera's local matrix.
* Camera (import): eye = the camera's translation, view = -Z row, roll = the
  angle between its up and the unrolled up; akeytsu has no target, so the
  target goes at the old key's eye-to-target distance along the new view
  (3 m when there is none). Written back to the export's source
  (`CameraCuts`, shared with export): the playing camera, else the primary's
  BAC camera cuts - each akeytsu frame goes to the cut + camera frame it was
  sampled from (`CutAt`); frames the export only held (before the first cut,
  past a cut's Duration or its animation's end) are dropped. Per camera
  animation the keys over the written range of "Node" are replaced (Position,
  Rotation, Scale X/Y; old W and Scale Z/W kept), in the same Undo step.
  Until 2026-10-22 import only wrote into a *playing* camera, so a BAC-cut
  export came back without its camera ("Regular animation imports back but
  not the camera", user). Export and import now also take each cut's space
  from that cut's own target character (export used the first cut's for all).
* Textures: each submesh's first sampler (`TextureSamplerDefs[0].EmbIndex`)
  into its part's EMB (`PartSet.TextureFile`, a Local change) is encoded to
  PNG (WPF `PngBitmapEncoder`) and embedded as a `CTexture` under
  `__Textures`, set as the material's Diffuse texture (colour white). A
  material name reused by another image gets `_2`, `_3`. The raw EMB of a toon
  material is only masks (A = ink lines), so the first export showed a
  see-through "ghost" with only the outfit's lines. Now (`MaterialTexture` in
  `AkeytsuExchange.cs`) TOON_* materials are baked: the DYT texel at x 120 (light
  step), row MatScale1.X * 16 + 1, from the DYT XenoKit draws with
  (`Xv2Submesh.CustomColorDyt` = the BCS/CaC custom-colour copy, else
  `part.Dyts[ForceDytOverride or Part.Texture]`), lerped to MatCol0 by the EMB
  alpha, alpha 255. Other shaders keep the raw EMB.
  UVs go across unchanged: akeytsu's V runs top-down like XV2's (Akeytsu
  `research_notes/data-model.md` "UV orientation"). The first bake export wrote
  `1 - v` - flat colours looked right, the ink lines were scattered (user,
  2026-10-22); fixed in `AktExporter` the same day.
* Import matches characters by name (ShortName, victim `<ShortName>_Victim`),
  else by order; animations by name + occurrence (an EAN may repeat a name;
  `SameNamed`), legacy 1 -> 1 when both sides have one. A bone whose imported
  frames all match its current pose (5e-4 m, quaternion dot 1e-5, scale 1e-3)
  is left alone, so importing a whole bank doesn't densify untouched
  animations; a changed bone is keyed on every frame (old keys removed), scale
  only where the node had scale or it changed. Cameras: each camera-character
  animation goes into the same-named Camera-tab animation through the same
  `BindCamera` space, replacing all of its "Node" keys; unchanged cameras
  (eye/target 1e-3 m, roll/FOV 1e-3 rad) are skipped. A single camera with no
  name match (old exports) uses the old cut path (`ApplyCutCamera`). Report:
  `<Name>: n of m animations changed: ...`, `Camera <object>: n of m
  animations changed: ...`, plus the names not found.
  **New animations (2026-10-22):** an akeytsu animation of the primary with no
  name match is added to the Animation tab's EAN (`EAN_File.AddNewAnimation`,
  next free id, the akeytsu name; b_C_Base always keyed so the length holds),
  reported as `Added to the EAN: [id] name (n frames)`; the victim's extras are
  still skipped (its EAN is often a shared one). A camera-character animation
  with no match goes into the Camera tab's cam.ean (`AddNewCamera`), placed
  unbound (primary's b_C_Base, frame 0 space, Offset 0) - except when the file
  has a single camera and none matched (old exports), which still takes the
  legacy cut path. Same Undo step.

Offline check (scratchpad harness, not kept): HUM_000.esk + HUM.ean
GOK_BAS_WALK (`AngryExplosion_Ultimate\build\probe\*.xml`), primary + turned
victim, 122 bones, 70 frames: export -> import round trip max error 6e-8 m /
6e-8 rotation; `ikkeycheck` 280/280 hand keys solve to the FK pose;
`rfkeycheck` every key with a roll matches the shoe solve (102/280 solve, the
rest are toe-curl keys with roll 0, see above).
Camera + texture check (2026-10-21, scratchpad, not kept): MAM_BUL_BAS_STAND.akt
imported, a 104-frame orbiting camera (roll 0-10, FOV 40-30) and a PNG on a
test mesh added to the .xk, exported ("Embedded 1 textures", "Camera ... on
MAM") and imported back: eye, roll and FOV exact, target 1 m along the view.
Re-checked with the camera rig in the main .akt (2026-10-22): 1 CCameraAnimated,
1 CHideJointForLooker, 1 CCameraTarget, 2 CAnimTrackLooker_VK, camera at
controller slot 142 of 145; import back: 104/104 frames, eye 8e-7 m, view
direction 3e-5 deg, roll 5e-6, FOV exact.
Separate camera check (2026-10-22, scratchpad `xkroundtrip`, not kept; AktBridge
copied off Z: first, see CLAUDE.md pitfall): 2 animations (10/25 frames) + 2
cameras (30/12 frames) -> .akt tree matches the sample's Camera_01 node for
node (`CCharacter 'TEST_Camera'` with CAM_A/CAM_B under `__Anims`) -> import:
ANIM_A=10, ANIM_B=25, CAM_A=30, CAM_B=12; eye 5e-7 m, roll 4e-6 deg, FOV exact.
The XenoKit side (ExportedAnimations, BindCamera, ApplyScene by name) is
built, not run.

## Keyframe (cleaned) export (2026-10-24)

Akeytsu menu: **Export to .akt (baked)...** (unchanged: a key every frame) and
**Export to .akt (keyframes)...** (new). The exchange file is XKAK v4, which
adds one `Keyframes` flag (v3 still loads as baked); everything else is the
same. The UI only sets the flag; the cleanup runs in AktBridge
(`AktBridge\KeyCleaner.cs`) in the bridge process, so the XenoKit UI stays
responsive. Baked mode still writes every frame and all tracks with the same
values as before.

Keyframes mode: akeytsu is pose-to-pose (all tracks of an animation are keyed
at the same `CAnimStack.TimeArray` times), so the cleaner picks one shared set
of frames. It is Ramer-Douglas-Peucker over the whole baked pose: start with
the first and last frame, find the frame where akeytsu's linear/slerp
interpolation between the current keys misses the baked pose the most, add it,
and repeat until every frame is inside the tolerances. Keys use linear
tangents, which akeytsu plays as straight lines for translation/scale and
slerp for rotation - the model the cleaner measures. Tolerances
(`XkKeyframeTolerance`, shared with the UI text): 3 mm / 0.75 deg / 0.5 %
scale for bones, 3 mm / 0.75 deg for camera eye, direction, roll and FOV. The
stack keeps the usual end key one frame past the last real frame, replaying
key 0. Bones whose delta from rest is identity at every key get no track
(b_C_Base always keeps one); reverse-foot locator identity tracks are skipped;
IK and reverse-foot controller tracks are keyed at the same sparse times and
still carry the FK keys/RefArrays/roll angles. Camera animations are cleaned
the same way from their scene-space frames. Consecutive key rotations are
flipped onto one quaternion hemisphere because akeytsu slerps without flipping.

Offline check (`ClaudeTools\xkkeys`: `ean_to_xk.py` writes an XKAK scene from an
EAN, `PoseDiff` compares the evaluated poses of two .akt files):
BUL_ULT_AGE_C 361 frames -> 134 keys, 65 tracks instead of 130, max local
2.93 mm / 0.734 deg, max world 7.8 mm at a fingertip; AEX_ULT 654 -> 328 keys,
max world 8.5 mm; the IK build keeps 69 tracks and `ikkeycheck` reports
268/268 hand keys solving (6e-7); a 240-frame test camera -> 49 keys, eye
1.97 mm, direction 0.044 deg, FOV 0.025 deg. Import -> baked export round trip
is within 0.07 deg for bones and 0.02 deg for the camera. Not seen in
XenoKit/akeytsu yet.

Limits: one flat lit colour per toon material (no shading steps, scratch/blood
masks ignored); import keys every frame of a changed bone; the
victim must have its own animation playing to be exported/imported (only the
primary exports the whole bank); a camera is lined up with the first BAC entry
pairing it with a body animation only (another body animation in akeytsu plays
it at that offset too); edits to a camera's held lead-in frames are dropped;
frame rate 60; the cleaned-up keyframe timing is export-only - the import still
keys every frame of a changed bone, so a keyframe export/import round trip
comes back baked.

## State (2026-10-22)

* Akeytsu export/import: built (XenoKit + `AktBridge\`), checked offline only
  (above). First GUI export failed: "Access to the path '%TEMP%\XenoKit_Akeytsu_*.xk'
  is denied" - the checkout carries a Low integrity label, so XenoKit.exe ran as a
  Low process (no writes to %TEMP%, the game folder or Documents; AktBridge too).
  `Launch XenoKit.bat` now sets both exes to Medium (`icacls /setintegritylevel M`)
  on every launch; start XenoKit through it, not the exe. The user's export then
  worked; the IK rings/foot controllers were misplaced -> fixed (see "IK placement
  fixes"), and camera + textures were added. **Hand/foot IK confirmed good in
  akeytsu (2026-10-22).** The same export showed the character see-through (raw
  line masks) and had no camera -> texture bake + camera fallbacks + the
  `_CameraTest.akt` split built 2026-10-22 (checked offline: cam.xk -> main .akt
  without, `_CameraTest.akt` with the `CCameraAnimated`). Seen in akeytsu
  (2026-10-22): baked colours generally right, ink lines scattered -> the V flip
  removed (AktBridge republished, not seen yet). `_CameraTest.akt` opened as an
  **empty scene** (no crash), though its file is as big as the main .akt - akeytsu
  drops everything when the camera lacks its target. The user wants character +
  camera in **one scene** (needed to animate both), so the split goes once the
  layout is known. From the user's sample (`MAM_BUL_ULT_AEX_C_MyOwnCameraTest.akt`)
  the camera rig now goes into the main .akt and `_CameraTest.akt` is gone
  (2026-10-22, checked offline, AktBridge republished). That export **still
  opened as an empty scene**; diffing it against the sample (MyAkeytsu.Cli
  `tree`/`dump`) found the hide joint missing from `JointArray` + the mask,
  name and child-order differences -> fixed (see the camera rig above),
  AktBridge republished 2026-10-22. Offline: the sample imported, exported and
  imported again (654 camera frames), the new .akt has JointArray 143 ending in
  the hide joint, "Camera Info" tracks with mask 33279. **Still empty** (user,
  2026-10-22) -> the camera classes' schema names/member order fixed (see
  "Schema names" above), XenoKit + AktBridge rebuilt; the round trip's .akt
  shows `class CCameraAnimated` (Ratio, Mode, Near, Far), `class
  CAnimTrackCamera`, `class CKeyCamera`, `class CTangentInfoListCamera` like the
  sample. Same build: export/import show a progress dialog (MahApps,
  indeterminate) with AktBridge's phase lines (materials, each character,
  keying, writing). **Confirmed (user, 2026-10-22): character + camera open in
  one scene, ink lines and colours right, progress dialog good.** The camera
  didn't move: it was "Viewport_Camera" (no camera animation found, fallback
  3) -> the BAC entry lookup without a playing entry added (see the camera
  sources above), XenoKit rebuilt 2026-10-22, not seen.
  **Confirmed (user, 2026-10-22): the camera animation comes into akeytsu
  and looks great; the character animation imports back.** The camera didn't
  import back (import only wrote into a playing camera) -> import writes
  into the BAC camera cuts like the export reads them (see "Camera (import)"),
  XenoKit rebuilt 2026-10-22. **Confirmed (user, 2026-10-22): the camera
  imports back.** The user then asked for the second design (whole Animation
  tab in the bank, Camera tab on a separate camera character; see "Separate
  camera") -> built 2026-10-22 (XenoKit + AktBridge), AktBridge round trip
  checked offline, not seen in XenoKit/akeytsu.
  Waiting on the user: pick the EAN on the Animation tab and the cam.ean on
  the Camera tab (no need to play anything), export; akeytsu should show every
  animation in the character's bank and a separate `<cam>_Camera` object with
  one animation per camera. Then import back: the report lists `n of m
  animations changed` per character and for the camera.
  **Confirmed (user, 2026-10-22): the second design works** - every
  animation in the bank, the separate camera object with its animations, the
  import writes akeytsu's changes back. Then asked: cameras at their BAC start
  time in akeytsu (no padding on import) and new akeytsu animations added
  instead of skipped -> both built 2026-10-22 (XenoKit only, AktBridge
  unchanged; see `CameraBinding.Offset` and "New animations"), not seen.
  Waiting on the user: export a skill whose camera cut starts after frame 0;
  in akeytsu the camera should hold until the body reaches that frame; import
  back and check the cam.ean animation's length is unchanged. Add an
  animation (and a camera animation) in akeytsu, import: the report lists
  `Added to the EAN: [id] ...` / `added to the cam.ean: ...`.

* 2026-10-24: baked/keyframe export options added (see "Keyframe (cleaned)
  export"). XenoKit + AktBridge built, offline checked (above), not seen in
  akeytsu yet. Waiting on the user: export one skill with "Export to .akt
  (keyframes)..." and open it; the animation bank should play the same with
  about a third to a half of the keys (the progress dialog lists `N frames ->
  M keys` per animation), the scene should have no tracks for bones that stay
  at rest, and the camera should move as before. Import back is still baked by
  design.

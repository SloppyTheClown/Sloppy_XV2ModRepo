# Battle styles: props (canes, swords, spears, poles, guns)

Covers: BattleStyles - why a styled CaC showed no prop, the prop
bones in the CaC's ESK + fitted EAN, prop meshes as BCS physics objects
via a per-style BCS (`battle_styles\fit\<RACE>.<STYLE>.bcs`), where the
skeleton path comes from, runtime generation (`src\prop_fit.cpp`, builds
8-14), sizing by arm / height, back depth, the scan of every roster
moveset. Overview: battle-styles.md; EAN fit: battle-styles-fit.md.

## Props missing
**Approach proven in game with Whis's cane (tests 1-3; per-style
display = test 4).** A moveset whose character holds a prop shows no prop on the CaC (user: Whis, Demigra,
Future Trunks, Android 16, Fu, Galena, Goku Daima Mini, Gamma).
**Cause (files, `ClaudeTools\battle_styles\propbones.py CODE...`):** props are
not effects and not BCS physics objects. They are submeshes inside the
character's own base part EMDs, skinned to extra `a_x_*` bones that only
its skeleton has, posed by its EAN. A styled CaC gets neither the mesh
(its own parts are drawn) nor the bones (FitEanToRace maps tracks by bone
name to the race skeleton and drops the rest).

| Style | Prop mesh in | Prop bones (parent) | EAN tracks |
|---|---|---|---|
| WIS Whis | wis_000_rist.emd | a_x_cane_root (b_R_Hand) + front1/2, back1 | root 82/88 |
| DMG Demigra | dmg_000_face_forehead.emd | a_x_cane_root (b_R_Hand) + front1/2, back1 | root 65/87 |
| TRX Future Trunks | trx_000_face_forehead.emd | a_x_sword1 (b_C_Chest) - sword2/3 | sword3 80/82 |
| FUU Fu | face_forehead (knife), face_ear (glasses) | a_x_sword1 (b_C_Pelvis) - sword4, a_x_glasses (b_C_Head) | 88/93 |
| GAL Galena | gal_000_rist.emd | a_x_spear_root (b_R_Hand) + front1-3, back1-3 | all 89/90 |
| NTG Goku Mini | ntg_000_face_forehead.emd (all 8 bones: pole extends) | a_x_nyb_root1 (b_C_Chest) - root2, front1-3, back1-3 | 43-64/89 |
| FIO/FIT Gamma | bust (a_x_gun1), face_forehead (a_x_gun2) | a_x_gun1 (b_C_Spine1) - gun2 | 81/83 |
| G16 Android 16 | **no mesh** on its bones | a_x_leftarm/a_x_rightarm (b_C_Pelvis) | 16, 13/100 |

Every prop root's parent is a standard bone every CaC has. G16's arm
bones move 0.5-8 m and scale to 0 in G16_ATC_SHOT_*, ATC_014, ATC_021,
SPA_RKP_*, ULT_HZF - the flying fist likely follows them, but how it is
drawn is unverified (BSA type-9 entries 352 ids 6/7, 520-529/540 id 30).
WIS/DMG/G16 BACs have no character-EEPK (type 8, EepkType 2) records;
no BAC uses type 13 (part invisibility); type 18 is all function 0.
**Fix needs two things:** (1) the style's a_x bones (parent + rest
transform) added to the merged EAN skeleton - and the CaC's ESK, if
the game skins part meshes from it - with their tracks kept;
(2) the prop submeshes drawn: either merged (with EMM/EMB) into one of
the CaC's part EMDs, or a separate model on the bone (a physics object
is rigid on one BoneToAttach - fine for staffs/gun, not the spear or the
extending pole). **Unknowns:** how `<RACE>_000.esk` is loaded (the CMS
getter 0x2c5510 has no ESK field); whether part EMD bones resolve by
name against the character skeleton; whether a bone added at load can
be attached to.

**Leads (XenoKit source, unverified for the game):** the character
skeleton is `chara/<code>/<BcsPath>_000.esk` (Xv2CoreLib Xenoverse2.cs),
so swapping CMS field 0 to a fitted BCS likely swaps the ESK too; a
physics object's own ESK bones are matched to the character skeleton by
name (unmatched ones follow their parent). BCS part paths:
`chara/<part Name>/<EmdPath>.emd`, physics object ESK = EMD stem + `.esk`,
dyt = EMB + `.dyt.emb` with flag 0x2. Matrices: row vectors, ESK
AbsoluteTransform = inverse world (checked on WIS_000.esk).

**Test 1 (confirmed in game: cane in the right hand, correct
orientation, from the save slot screen on):** Whis's cane as a
rigid physics object, no DLL change (`ClaudeTools\battle_styles\prop_test1.py`,
inputs from `prop_get.py` + the XML serializer). `chara\WIS\WIS_bsprop.emd`
= the 4 cane submeshes of WIS_000_Rist (stick_circle, tueao_d, tuekin_c,
tuekuro_a) moved into hand space (`v @ Abs(b_R_Hand)`), skinned to
b_C_Base; `WIS_bsprop.esk` = Clown Nose layout. Loose `chara\HUM\HUM.bcs`:
every Bust part with a model (452) gets PhysicsObject Name WIS, flags
0x2, BoneToAttach b_R_Hand, EMM/EMB WIS_000_Rist. Original kept as
`HUM.bcs.before_proptest`. So a physics object on a Bust part (flags 0x2)
loads and draws another character's mesh/EMM/EMB, and `v @ Abs(bone)` +
b_C_Base skinning places it on that bone.

**Test 2 (confirmed in game: cane floats past the hand, same angle,
save slot screen):** can an attach bone
be added to the CaC skeleton? Loose `chara\HUM\HUM_000.esk` gets
a_x_cane_root as the last child of b_R_Hand (Whis's rotation relative
to the hand, position + 0.3 m along the hand's -X so it is visible);
the cane mesh is in a_x_cane_root's space (`v @ Abs(a_x_cane_root)`),
and the physics object attaches to a_x_cane_root
(`ClaudeTools\battle_styles\prop_test2.py`). Original kept as
`HUM_000.esk.before_proptest`. Cane ~30 cm past the hand = ESK bones
are the attach skeleton and can be added; no cane = not.

**Test 3 (DLL, confirmed in game: the WIS-styled HUM CaC's cane follows
Whis's battle movements exactly; unstyled CaCs keep test 2's floating
cane):** FitEanToRace
(body EAN) now appends the style's `a_x_*` bones whose parent the merged
skeleton has (`AppendPropBones`: new index/name/transform/extra-info
tables at the end of the file, race indices unchanged, IK section left in
place; skeleton +24 = per-bone extra info, 8 bytes) and keeps all their
tracks. Offline (`ean_fit_test WIS:HUM`): 4 bones under b_R_Hand,
a_x_cane_root tracked in 82/123 animations, Xv2CoreLib parses it; GAL
7 bones, NTG 8. So the EAN tracks drive a bone added to the CaC's ESK
(the track overrides test 2's 0.3 m rest offset), and the whole approach
works: prop mesh as a physics object on the prop bone + bone in the ESK +
bone and tracks in the fitted EAN.
**Still to do for a real fix:** per-style display (tests 1-2 edit the
global HUM.bcs/ESK, so every HUM CaC shows the cane); prop EMD/ESK and the
ESK bones generated at runtime for every race; multi-bone props (spear,
pole). (G16's fist: parked, see build 8.)

**Where the skeleton path comes from (static, 1.26.01 image):** the only
`.esk` string users are CharaResource loaders 0x298240 and 0x2987f0. Both
ask the CMS getter for **field 0 (BCS)** (wrapper calls 0x2982ea,
0x298894), load it, cut the path at its last '.', and append "_000"
(0xf74dec) + an LOD suffix (0x29ef40: table 0x1477790 = "", "_LOD01",
"_LOD02", "_LOD03", index from flag bits 2/4/8 of the request, 0x29ef60)
+ ".esk". So the CaC skeleton is `<BCS path stem>_000[_LODnn].esk`, and a
BCS of our own brings its own skeleton. Other field-0 callers: 0x19e994
(a 14-entry loop loading every battle player's BCS). Part files keep
coming from `chara/<part Name>/` (HUM.bcs parts say Name HUM), so the BCS
can live anywhere. HUM_000_LOD01..03.esk are byte-identical to HUM_000.esk.

**Test 4 (confirmed in game: cane on the Whis-styled HUM CaC in the
character select preview and Training, none on the unstyled one):**
per-style cane. In the lobby the styled CaC had it too (source "loaded
character", log) and kept it after the style was changed there - the
lobby model isn't reloaded on a style change. Lobby poses are neutral
anyway, so build 4b uses the prop BCS only inside the character select
preview or the battle player loader (t_previewSlot / t_loadingPlayer);
4b confirmed in game (cane gone after switching to a preset with
another style). Global
HUM.bcs/HUM_000.esk restored (MD5 = the originals, backups removed).
`ClaudeTools\battle_styles\prop_test4.py` writes `data\battle_styles\fit\HUM.WIS.bcs`
(test 2's BCS) and `HUM.WIS_000.esk` + `_LOD01..03` (a_x_cane_root at
Whis's rest pose, no offset). DLL: CMS field 0 of a CaC whose style has
`battle_styles/fit/<RACE>.<STYLE>.bcs` returns that path
(mod.cpp StyleBcsPath; existence cached per session; style picked by
StyleFor like the moveset fields). Expected: cane only on Whis-styled
HUM CaCs, in the hand; unstyled HUM CaCs have none. The log line
`chara 100 BCS (...) -> battle_styles/fit/HUM.WIS.bcs` says which
path (character select, battle player, loaded character) asked.

**Build 8 - props generated at runtime (installed on the PC, not seen in
game):** `src\prop_fit.cpp` FitPropsToRace replaces prop_test4.py's
files, once per (race, style), when fit_to_race and the EAN swap are on
(StyleBcsPath -> FitPropBcs -> FitOnce, logged like the other fits):
* prop bones = the style ESK's a_x_* bones the race lacks whose parent the
  race has (or an earlier prop bone) - ean_fit's rule, so the EAN tracks
  match. Appended to `<race>_000[_LOD01..03].esk` (LODs only where the
  race has them) with the style's relative transform; abs =
  inv(rel @ inv(raceAbs[parent])).
* per (style part EMD of the first part set, prop bone): a copy of the
  EMD with only the triangles whose vertices' dominant blend bone (2 of 3,
  else vertex 0) is that bone, vertices moved into the bone's space
  ([p,1] @ style abs), every list's bone names -> b_C_Base, bounds
  recomputed; + the 4-bone identity ESK. Written as
  `chara/<code>/<emd>_bsprop_<bone>.emd/.esk` (unused vertices are kept,
  so the file is the size of the original). Bones named *glasses* (Fu)
  get no mesh.
* BCS: every part set's Bust part (Model != -1 or an EMD name) gets a new
  physics array at the end of the file: its old objects, then one per
  prop mesh (flags 2, code = the part's folder, EMM/EMB = the part's).
Offline (`build\Release\prop_fit_test.exe <game> <out> STYLE:RACE ...`,
checked with the XML serializer): WIS on HUM is the hand-made test 4
cane to float precision (bounds, abs transform, physics object).
Results: WIS/DMG cane root, TRX sword2+sword3, FUU sword2+sword3, GAL
spear root (front/back bones carry no triangles), NTG all 8 pole bones
(8 rigid pieces - the extending pole may show gaps), FIO/FIT gun1 (Bust)
+ gun2 (forehead). G16: no mesh; its fist is parked (user, 2026-10-12:
leave it alone until asked).

**Build 8 in game (2026-10-12):** WIS on HUM and FRI: cane in the right
place, right on HUM but huge on FRI. WIS/DMG/TRX/FUU/GAL load and switch
in the select preview; Fu's glasses stay off. NTG: only a tiny bit of the
pole shows, away from the hand. Cause: props kept the style character's
absolute size and offsets, but skeletons differ - shoulder-to-wrist rest
length (b_R_Arm1 + b_R_Arm2 + b_R_Hand): HUM 0.692, FRI 0.494, WIS 0.713,
NTG 0.273, DMG 0.774, TRX/FUU 0.572, GAL 0.507, FIT 0.704
(`ClaudeTools\battle_styles\eskscale.py` + a loop). The FRI ESK is ~0.71x
HUM's yet a FRI CaC stands about as tall in game, so the game scales it
up and an unscaled cane grows with it. NTG's pole hangs off b_C_Chest
(root1 rest -0.059 0.113 0.096) and its tracks move it to the hand in
kid-sized offsets. Leg ratios don't fit (WIS has long legs: 0.73 would
shrink the cane that's right on HUM).

**Build 9 - props sized by arm length (installed on the PC, not seen in
game):** propScale = race arm / style arm (WIS->HUM 0.970, WIS->FRI
0.693, NTG->HUM 2.535), applied to the prop mesh vertices (after the
move into bone space), the prop bones' rest positions in the fitted ESKs
and EAN skeleton, and every prop bone position key (ean_fit plan
positionScale). Rotations/scale keys untouched. The mesh files now carry
the race (`<emd>_bsprop_<RACE>_<bone>`), since each race gets its own
size; the old `*_bsprop_a_x_*` files were deleted from the PC. Both logs
print the scale (`sized x0.693 (arm 0.713 -> 0.494)`, `prop bones added
(x0.693)`).

**Build 9 in game (2026-10-12):** the cane on FRI looks right. On the HUM
CaC, switching presets GAL -> WIS -> DMG made the character select
preview stop: every later preset switch logged only "loadout from preset
N", with no BCS/EAN request at all (the preview never finished loading
DMG). Build 8 previewed DMG fine with the same mesh, only unscaled and
named `DMG_000_Face_forehead_bsprop_a_x_cane_root` (42 chars); build 9's
`DMG_000_Face_forehead_bsprop_HUM_a_x_cane_root` is 46 chars
("data/chara/DMG/<name>.emd" = 65 chars, build 8 = 61). The other build 9
names (WIS/GAL/JCO, 37-40 chars) loaded. Likely a fixed 64-byte path
buffer somewhere in the loader - not proven in the exe.

**Build 10 - short prop names (confirmed in game 2026-10-12):** `<code>_bsprop_<RACE>_<n>`
(n = the prop mesh's index in the fit, e.g. `DMG_bsprop_HUM_0`,
`NTG_bsprop_HUM_7`), at most ~18 chars. Preset switching works again, so
the long name was the cause (path buffer theory still not proven in the
exe). WIS cane on HUM vs FRI looks perfectly scaled - arm-ratio sizing
confirmed for shrinking. NTG pole on HUM: right spot, but only the middle
piece showed (spiky ends), far too thick, and the back holder far too big.

**Why (offline, `ClaudeTools\battle_styles\emdbones.py`):** props are modelled
at real size even on a kid. NTG's pole (`skill/spa/2230_ntg_nyd/ntg_stick.emo`)
is 3.4 cm thick, 0.53 m long; the adult Power Pole the CaC skill uses
(`skill/spa/042_gkg_ny2/nyb_bas.emo`, same as `vfx/chara/hum/nyb_bas.emo`)
is 3.4 cm thick, 1.23 m long. So x2.535 made it 8.6 cm thick. The pole
is one mesh (`NTG_000_Face_forehead.emd`, "c000_Face_forehead_stick")
skinned to a_x_nyb_root2 (middle) + back1-3 / front1-3 (rest 0.067, 0.1,
0.1 out along z), and the quads between pieces are split across bones.
In NTG_BAS_STAND front2/3 and back2/3 sit 0.2-0.4 m out: the pole
telescopes. Cut per bone into rigid pieces, the pieces part (x2.5 sent
them far away) and each keeps only its own half-quads - the spikes. The
holder ("stickcase") is on root1 alone.

**Build 11 - real-size meshes + skinned pole (installed on the PC, not
seen in game):** `prop_fit.h` PropMeshScale = min(1, arm ratio): meshes
shrink on a smaller race, never grow. Bone positions still use the arm
ratio (they place the prop on the body), except bones named
`*front*`/`*back*` (IsPropShapeBone - they lengthen the prop itself), which
use the mesh scale - in the fitted ESK, the fitted EAN skeleton and the
EAN position keys alike. Prop bones joined by triangles where one is a
shape bone (PropBoneGroups) become one physics object, hung on the
group's first bone; its own ESK gets the others under b_C_Base with the
style's rest transforms (`PropMeshSkeleton(name, extra)`), and the EMD
keeps their names in the triangle lists so the segments stay skinned.
Unproven: whether the game moves bones of a physics object's ESK by name
from the character's EAN. If not, the pole stays at rest length (0.53 m,
whole), still the right look. Not grouped: TRX/FUU a_x_sword2 + sword3
share triangles but sword3 is drawn out of the sheath (tracks in 80/82
animations), so they stay one rigid object per bone. Fit log for
NTG->HUM: `placed x2.535, meshes x1.000 ...; NTG_bsprop_HUM_0 on
a_x_nyb_root2 (+6 skinned bones); NTG_bsprop_HUM_1 on a_x_nyb_root1`.
TRX/FUU/GAL props are now real size too (were x1.21/x1.36). Build 10's
NTG_bsprop_HUM_2..7 files are left over in `data\chara\NTG\` (unused,
removed by uninstall).
**Build 11 in game:** NTG pole on HUM looks great (whole, thin, holder
right). Fu's sword (FUU on HUM) looked stretched: a_x_sword3 sits 0.473 m
along a_x_sword2 (the sheath length) and was placed at the arm ratio
(x1.21) while the mesh stayed x1.0, so the blade piece (object on sword3)
stood 10 cm off the hilt piece (object on sword2) and the triangles they
share (kept rigid on sword2) stretched across the gap. **Build 12:**
`IsPropPartBone` (a_x_sword2..4) - their positions go with the mesh scale
like the shape bones, in ESK + EAN keys (a_x_sword1, the mount on the
body, still follows the arm). Kept out of IsPropShapeBone so the two
pieces stay separate objects (the blade is drawn). Offline: HUM.FUU
sword3 0.473 (was 0.572), HUM.TRX 0.676. **Confirmed in game:** Fu's and
Trunks's swords right, everything right in a Training battle too.
**Proven:** the game does move the bones of a physics object's own ESK
by name from the character's EAN - NTG's pole (one skinned object,
root2 + 6 bones) extends and retracts in attacks.
**Gamma (FIO/FIT, gun1 + gun2) confirmed in game** (2026-10-12).

**Every roster moveset scanned for props (2026-10-12):**
`propbones.py` over all 218 style codes (`stylenames.py` list; it now
skips the 21 codes with no own EAN - PR1, BUZ, NP1, ... borrow one), then
`prop_fit_test` on each hit. a_x_* bones with a mesh skinned to them,
besides the 8 known (WIS DMG TRX FUU GAL NTG FIO FIT):
| Code | Bone(s) | Tracks in the moveset EAN | Verdict |
|---|---|---|---|
| TPO Tapion | a_x_sword1-4 (sheath + blade, like Trunks), a_x_flute (ocarina at the hip) | 81/83, 72/83 | prop |
| TRG Trunks (SSJ Rage) | a_x_sword1 (whole sword one object) | 82/90 | prop |
| XBF Fu, XBL Future's Doom | a_x_sword1-4 (on b_R_Hand) | 48/72, 23/83 | prop |
| VDS Vados | a_x_cane_* (Whis's cane layout) | 84/87 | prop |
| TOW Towa | a_x_spear_root + front/back1-3 (telescoping like NTG: +4 skinned bones) | 87/89 | prop |
| JCO Jaco | a_x_gun (on the pelvis) | 84/87 | prop |
| SIM Cell Max | a_x_L/R_wings1-2 | 22/28 | giant moveset (broken anyway), kept |
| CML Chamel | a_x_cane_* | 0 (moveset is CAB's EAN) | not a prop here |
| SIG Beast Gohan | a_x_Bang1-5 (hair) | 0/89 | not a prop |
| D01 | a_x_hair* (17 strands) | 0/83 | not a prop |
| SVV Videl (Super) | a_x_belt_* | 3/91 | not a prop |
| XAC Cheelai | a_x_Pouch | 0 (moveset is G18's EAN) | not a prop |
| TE1, FIG, XBF, FUU | a_x_glasses | 0-5 (FUU 88/93) | face part, always skipped |
Build 12 drew every a_x_* mesh, so SIG/D01/SVV/XAC styles put a second
copy of the style's hair/belt/pouch on the CaC. **Build 13:**
`FitPropsToRace` gets the moveset's body EAN (CMS field 1 of the style)
and keeps only prop bones with tracks in >= 10% of its animations
(`kPropTrackShare`; real props 28 %+, the rest 0-3 %) plus the bones under
them (`AnimatedPropBones`; Fu's sword2/3 hang off the animated sword1).
Offline: the 8 known props unchanged (same triangles), the 7 new ones
made, SIG/D01/SVV/XAC/CML/TE1/FIG "prop bones, none animated by X.ean".
No non-a_x_ prop found: the other extra skinned bones are toes (FRZ family),
ArmZoom (NMC/MAM), SCD physics, pockets (HIT), claws, ears, beards.
Not done: prop EMDs keep the whole source vertex buffer (50-70 KB each;
compacting means rewriting indices across shared buffers - not worth it).
**Build 13 in game (2026-10-13):** extras gone (SIG/SVV/XAC add nothing);
TPO, TRG, FUU, VDS, JCO, WIS right. XBL (Future's Doom, not normally
playable) left as is - don't work on it. Towa's spear much too long, lower
tip in the floor, left hand off the shaft (on HUF). Back props: swords clip
slightly into a HUM back, right on FRI, float far off a HUF back; NTG pole
stands off a HUM back.

**Build 14 - back depth from Arale's Poop Stick (2026-10-13):**
At the arm ratio a back prop's depth followed the race's arm, not how deep
its back is. The game's own
per-race reference: the Poop Stick accessory `chara/<RACE>/<RACE>_897.emd`
is modelled per race against that race's back. Its deepest-but-one
vertex depth (from b_C_Chest) = where the race's back is
(`RaceBackDepth`). The style's back: its Bust EMD vertices behind the chest
bone, 90th percentile of depth (`StyleBackDepth`; the max was noisy -
Tapion's collar stands up past the back) plus 0.023 m (stick to back gap
measured on HUM). shift = race back - style back, added to the rest
depth of a back prop bone (a_x_* under b_C_Chest with z > 0,
`IsBackPropBone`) and to its z keys weighted by `BackShiftWeight` (full
within 0.15 m of rest, none from 0.35 m: the sword bone is pulled into the
hand in sword attacks, 0.4-0.7 m - `ClaudeTools\battle_styles\backkeys.py`;
NTG's pole root moves 11 cm at most). Offline shifts: TRX:HUM +0.039,
TRX:HUF -0.054, TRX:FRI -0.011, NTG:HUM +0.110, TOW:HUF +0.029.
Dead end checked: do physics objects get the race's BCS body scale
(b_C_Base on body 0: HUM 0.827, HUF/FRI 0.900, MAM 0.500, MAF 0.910, NMC
0.690; style characters 1.0)? Yes - MAM's stick sits 0.398 deep against
an unscaled MAM back of 0.349, so it's modelled in skeleton space and
scaled with the body; comparing unscaled depths is right.

**Build 14 - prop meshes sized by height (2026-10-13):** Towa's spear
(2.14 m, root on b_R_Hand, front/back1-3 telescoping) was sized x0.932 on
HUF by arm, but HUF is x0.851 of Towa's height: the arm ratio overstates
how big a short race is (relatively long arms). Floor-to-head heights in
skeleton space (`ClaudeTools\battle_styles\bodyheight.py`: b_C_Head y minus the
lowest foot/toe joint, through the rotations): TOW 1.293, HUM 1.477,
HUF 1.100, FRI 1.112, MAM 1.696, MAF 1.119, NMC 1.776, WIS/VDS 1.819,
TRX/FUU/TPO/TRG 1.380, GAL 1.293, NTG 0.601. Rule now: mesh scale =
min(1, race height / style height) (`PropPlacement::heightRatio`, measured
from both `_000.esk` in `prop_fit.cpp StandingHeight`, arm ratio when
unknown); shape and sword part bones follow the mesh, placement bones
keep the arm ratio. Effects: TOW:HUF x0.850, WIS:FRI 0.69 -> 0.61,
WIS:HUM 0.97 -> 0.81, TRX:HUF 0.80; TRX/NTG/TOW on HUM stay 1. Whis's cane
was called right at the old sizes - recheck it. The left hand still won't
sit exactly on a two-handed shaft: the CaC's arm proportions differ from
the style character's, and the hands follow the race's arms.
**In game (2026-10-13):** back swords "a lot better", NTG pole placement
better. Towa's spear on HUF (preview) still looks very long and passes
through the chest in the idle, but close to Towa's own look - accepted.
The clip is pose/body shape (Towa's thin torso), not size; the lever, if
ever needed, is a forward offset of the hand-held prop bone. Whis's cane
fine at the new (smaller) size; no visible jump when a sword is drawn
from the back. Props work considered done.

## Hand-held props without a prop bone (Kaido's club, 2026-10-14)
x2m "Kaido" by TGFB (K7H, converted to style 9O6) showed no club in game.
Two causes:
* The converter shipped only the moveset and pointed the CMS BCS at HUM's,
  so `FitPropsToRace` saw HUM's body: "NOT used - no prop bones".
* Even with Kaido's own files there is no a_x_* bone: his skeleton's extra
  bones are D_* (dragon form), and the club lives in `K7H_000_Face_ear.emd`
  (mesh name K0H_000_Face_ear) as two submeshes skinned to nothing but
  b_R_Hand (2.35 m and 0.87 m long; Kaido's arm 1.43 m).

Fix:
* DLL (`prop_fit.cpp HandPropSubmeshes`): a submesh whose triangles are
  >= 99 % owned by b_R_Hand / b_L_Hand, with its longest side > 0.5 x the
  style's arm length, is a "hand prop". It is cut with only those submeshes
  (`CutPropMesh` `onlySubmeshes`), hung rigidly on the hand bone (no bones
  of its own) and sized by `PropMeshScale` like the others (HUM x0.527).
  An empty a_x_* set no longer stops the fit. The note says "(hand prop)".
* Converter (`moveset_to_style.py x2m_prop_files`): when an x2m character's
  first part set has a part whose EMD has an a_x_* submesh or a hand-only
  submesh (`holds_prop`, by the triangle lists' bone names), it ships its
  BCS (same-length renames "K7H\0" / "K7H_" -> new code), `_000.esk` and
  only those parts' .emd/.emm/.emb/.dyt.emb (Kaido: Face_ear, ~2 MB of
  116 MB), and the CMS BCS field becomes the new code. The game never
  spawns the style character, so the other parts' missing files don't matter.
* Roster check (prop_fit_test, all 248 CMS BCS:EAN pairs on HUM): the same
  21 fits as before, no new hand prop anywhere. Kaido's converted files fit
  on HUM / FRI / MAM offline.
* Also fixed: re-converting an installed style gave it new codes (its own
  codes were in the installed CMS); `installed_style_codes` now returns the
  style XML's own earlier codes, and they are free to reuse.

**Confirmed in game (2026-10-14):** the club shows after re-converting
Kaido, size and grip fine. The D_* dragon bones stay ignored.

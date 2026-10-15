// Style props: the object a style character holds (Whis's cane, Trunks's
// sword, Gamma's guns) drawn on a CaC that uses its moveset.
//
// The prop is part of the style character's own body meshes, skinned to
// "a_x_*" bones the CaC's skeleton lacks, and the CaC never loads those
// meshes. Only a_x_* bones the moveset animates count (a held prop moves in
// most animations; still ones are hair, belts, pouches - AnimatedPropBones).
// So the mod builds, per race and style:
// * the race's BCS with every Bust part given one physics object per prop
//   bone: a rigid mesh attached to that bone (flags 2 = dyt from the EMB, as
//   the Clown Nose accessory; costumes-and-models.md);
// * the race's skeleton with the prop bones added under the same parents,
//   with the style's rest transforms - the game builds the skeleton path from
//   the BCS path ("<bcs minus .bcs>_000[_LOD01..03].esk", 0x298240,
//   0x2987f0), so the fitted BCS brings its own skeleton; the fitted body EAN
//   animates the bones (ean_fit.h);
// * per (style part, group of prop bones joined by triangles): the part's EMD
//   cut down to the triangles skinned mostly to those bones, moved into the
//   space of the group's first bone (the one the object hangs on) and skinned
//   to b_C_Base, with a skeleton of the same name: 4 identity bones plus the
//   group's other bones under b_C_Base, which keep their skinning (Goku
//   Mini's telescoping pole).
// Proven by hand first: ClaudeTools\battle_styles\prop_test1/2/4.py,
// Research_Notes\battle-styles-props.md "Props missing".
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct PropFile {
    std::string path;  // relative to <game>\data
    std::vector<uint8_t> bytes;
};

// Reads one game file (relative to data); false when it exists nowhere.
using ReadGameData = std::function<bool(const std::string& path, std::vector<uint8_t>& bytes)>;

// Builds the files above. `raceBcs` holds the CaC's BCS (read from
// `raceBcsPath`, "chara/HUM/HUM.bcs"), `styleBcsPath` is the style's
// ("chara/WIS/WIS.bcs"), `styleEanPath` the moveset's body EAN (CMS field
// 1 - not always the style character's own: Cheelai's is G18's; only prop
// bones it animates become props) and `fittedBcsPath` where the fitted BCS goes
// ("battle_styles/fit/HUM.WIS.bcs"; its skeletons go next to it).
// `files` gets the fitted BCS first, then the skeletons and prop meshes.
// False (files empty) when the style has no prop or a file isn't
// understood; `note` says what was done or why not.
bool FitPropsToRace(const ReadGameData& read, const std::vector<uint8_t>& raceBcs, const std::string& raceBcsPath,
                    const std::string& styleBcsPath, const std::string& styleEanPath, const std::string& fittedBcsPath,
                    std::vector<PropFile>& files, std::string& note);

// Prop sizes, shared by the fitted ESK (here) and the fitted EAN (ean_fit.cpp)
// so the bones and their tracks agree. `armRatio` = race arm / style arm
// (shoulder to wrist, ArmLength in both files); `sizeRatio` = race height /
// style height (PropPlacement::heightRatio), the arm ratio when unknown.
// * Meshes never grow: a prop is modelled at its real size even on a small
//   character - Goku Mini's pole is 3.4 cm thick like the Power Pole skill's
//   that adults use (skill/spa/042_gkg_ny2/nyb_bas.emo); x2.5 made it far too
//   thick (build 10 in game). They do shrink on a smaller race, by height:
//   at the arm ratio (builds 9-13) Towa's 2.14 m spear was x0.93 on a female
//   human, who is x0.85 of Towa's height, and looked much too long, its lower
//   tip in the floor (build 13 in game). Whis's cane at the arm ratio was
//   right on a FRI (x0.69; height gives x0.61) and on a HUM (x0.97; x0.81).
// * Bone positions place the prop on the body (chest -> back holder -> hand),
//   so they follow the arm ratio - except bones named *front* / *back*: they
//   lengthen the prop itself (cane and spear ends, the pole's telescoping
//   pieces) and go with the mesh. So do a sword's inner bones (IsPropPartBone):
//   Fu's a_x_sword3 sits 0.473 m along a_x_sword2 - the sheath's length -
//   and at the arm ratio (x1.21 on HUM) the blade piece stood 10 cm off the
//   hilt piece, stretching the triangles they share (build 11 in game).
inline double PropMeshScale(double sizeRatio) { return sizeRatio < 1 ? sizeRatio : 1; }
// Sword bones past the one on the body (a_x_sword1 <- pelvis/chest): hilt,
// blade and tip offsets of Fu's and Trunks's swords (propbones.py FUU TRX).
// Not shape bones: the sheath and the drawn blade stay separate objects.
inline bool IsPropPartBone(const std::string& bone)
{
    return bone.find("a_x_sword") == 0 && bone != "a_x_sword1";
}
inline bool IsPropShapeBone(const std::string& bone)
{
    return bone.find("front") != std::string::npos || bone.find("back") != std::string::npos;
}
inline double PropBonePositionScale(const std::string& bone, double armRatio, double sizeRatio)
{
    return IsPropShapeBone(bone) || IsPropPartBone(bone) ? PropMeshScale(sizeRatio) : armRatio;
}

// Props carried on the back: Trunks's, Tapion's and Trunks Rage's sheathed
// sword (a_x_sword1), Goku Mini's pole (a_x_nyb_root1) - a prop bone under
// b_C_Chest behind it (rest depth z > 0: +z is the back in chest-bone
// space). At the arm ratio their depth followed the race's arm, not its
// back: swords clipped a male human's back a little, sat right on a Frieza
// race and floated off a female's; Goku Mini's pole stood off a male human's
// back (build 13 in game). So their depth keeps the style's distance from
// its back instead, with the race's back where the game hangs Arale's Poop
// Stick accessory (prop_fit.cpp RaceBackDepth); across the back (x, y) they
// still follow the arm ratio. `backKnown` false (no stick or style body
// found): the arm ratio for all three axes, as before.
// Also the height ratio that sizes the meshes (PropMeshScale; 0 = unknown).
struct PropPlacement {
    bool backKnown = false;
    double shift = 0;                     // added to a back prop bone's depth (rest and keys)
    double raceBack = 0, styleBack = 0;  // measured, for the log
    double heightRatio = 0;              // race / style floor-to-head height (prop_fit.cpp StandingHeight)
    // The ratio meshes are sized by: height, else the arm.
    double SizeRatio(double armRatio) const { return heightRatio > 0 ? heightRatio : armRatio; }
};
// `raceBcsPath` as FitPropsToRace takes it ("chara/HUF/HUF.bcs": the stick is
// "chara/HUF/HUF_897.emd", the skeleton "chara/HUF/HUF_000.esk");
// `styleEanPath` the moveset's body EAN, whose character's body is measured
// ("chara/TRX/TRX.ean" -> TRX's Bust and skeleton). The EAN fit and the prop
// fit both call it, so the bones and their tracks agree.
PropPlacement MeasurePropPlacement(const ReadGameData& read, const std::string& raceBcsPath,
                                   const std::string& styleEanPath);
inline bool IsBackPropBone(const std::string& parent, double restDepth)
{
    return parent == "b_C_Chest" && restDepth > 0;
}
// How much of the depth shift an animation key of a back prop gets: the
// bone itself is drawn into the hand in Trunks's, Tapion's and Trunks
// Rage's sword attacks (0.4-0.7 m from its rest; Goku Mini's pole root moves
// 11 cm at most, staying on the back - ClaudeTools\battle_styles\backkeys.py), and
// there the hand's arm-ratio placement is right. Full within 15 cm of the
// rest position, none from 35 cm, linear in between (no jump mid-swing).
inline double BackShiftWeight(const float key[3], const float rest[3])
{
    const double dx = key[0] - rest[0], dy = key[1] - rest[1], dz = key[2] - rest[2];
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    return distance <= 0.15 ? 1 : distance >= 0.35 ? 0 : (0.35 - distance) / 0.2;
}
// One axis of a prop bone position (rest or animation key) placed on the race;
// `weight` = BackShiftWeight for a key (1 for the rest position).
inline double PlacePropAxis(int axis, double value, double scale, bool backBone, const PropPlacement& back,
                            double weight = 1)
{
    const double scaled = value * scale;
    if (axis != 2 || !backBone || !back.backKnown) return scaled;
    return scaled + weight * (value + back.shift - scaled);
}

// In the name of every mesh and skeleton FitPropsToRace writes into the
// style's chara folder ("chara/WIS/WIS_bsprop_FRI_0.emd" for a FRI),
// so build.bat uninstall can remove them ("chara\*\*_bsprop_*").
const char* const kPropFileTag = "_bsprop_";

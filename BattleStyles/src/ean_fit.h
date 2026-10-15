// Fitting a borrowed moveset (body EAN, camera EAN, BAC) to a created
// character's race.
#pragma once
#include <cstddef>
#include <cstdint>
#include "prop_fit.h"
#include <map>
#include <string>
#include <vector>

// Every CaC race EAN holds the same base animations at ids 0..81 (BAS_STAND ..
// BAS_STAND_01_S), then its own transformation set from 82 on: Kaioken and
// Beast on HUM, Giant on NMC (82..125, "_GNT"), Golden on FRI, Majin on MAM.
// Awoken skills such as Turn Giant play those by id from the character's own
// EAN. Measured on the 1.26.01 data (ClaudeTools\battle_styles\eantracks.py).
const int kRaceBaseAnimations = 82;
// The race camera EANs: id 0 the battle start camera, then the transformation
// cameras (HUM_CAM_KIN_000, FRI_CAM_GRD_000, *_CAM_000_MGT).
const int kRaceBaseCameras = 1;

struct EanFit {
    // Style animation ids >= the base were moved up by this much (to sit
    // after the race's own), so a BAC of the style must be shifted to match.
    int shift = 0;
};

// Builds the moveset EAN a CaC loads: the race's own file (its skeleton, so
// race-only bones like the Namekian/Majin arm-stretch chain and the tail keep
// their rest pose; and its transformation animations at their ids) with the
// style's animations put over it. Style ids below `baseAnimations` replace the
// race's; the style's higher ids are moved after the race's last id. Each
// style animation gets its bone indices remapped by name to the race skeleton.
//
// `body` (the body EAN, not a camera EAN) also fits the tracks:
// * pelvis position keys scaled by race leg length / style leg length -
//   legs get only rotations, their lengths come from the CaC's skeleton, so
//   the style's pelvis height would leave a shorter race's feet in the air;
// * no tracks on bones the race's own EAN never animates (the face: f_*
//   bones are driven by the face EAN, a style idle that sets them puts its
//   own face - teeth, mouth, eyes - over the CaC's);
// * position tracks only where the race's own animations set positions too
//   and the bone hangs off the same chain of parents in both skeletons; any
//   other one forces the style character's bone length on the CaC (Zarbon's
//   idle positions 116 bones; the Namekian's elbow and forearm hang off the
//   x_*_ArmZoom chain, Zarbon's off the arm bones); the kept ones (hands,
//   Piccolo's arm stretch) scaled by race rest length / style rest length;
// * the race tail (X_T_TAIL1..10: FRI, HUM/HUF) added to every style
//   animation that doesn't move it, resampled from the race's animation for
//   the same base id when its length is close, else from its standing loop;
// * the style's prop bones (a_x_*: Whis's cane, Fu's sword) appended to the
//   race skeleton with their tracks kept, positions sized as in prop_fit.h
//   (PropBonePositionScale), so the prop meshes prop_fit.cpp hangs on them
//   move as in the style character's hands; `back` places the ones carried
//   on the back by depth (prop_fit.h PropPlacement; default: arm ratio only).
// Returns false (out untouched) when either file is not an EAN it
// understands; `note` says what was done or why not.
bool FitEanToRace(const uint8_t* style, size_t styleSize, const uint8_t* race, size_t raceSize, bool body,
                  int baseAnimations, std::vector<uint8_t>& out, EanFit& fit, std::string& note,
                  const PropPlacement& back = {});

// Per bone name of an EAN's skeleton: in how many of its animations a track
// moves that bone; `animations` = the animations the file holds. Used to tell
// a held prop (Whis's cane: tracks in 82 of 88 animations) from a still
// a_x_* part (Beast Gohan's hair bangs, Videl's belt; prop_fit.cpp
// AnimatedPropBones). False when the file isn't an EAN it understands.
bool EanBoneUse(const uint8_t* data, size_t size, std::map<std::string, int>& animationsWithTrack, int& animations,
                std::string& note);

// Builds the damage file (BDM) a CaC loads: the race's own entries at their
// ids, then the style's. Awoken skills hit through the character's BDM by id
// (Turn Giant's 002_GOK_GNT_AFTER.bac swings use NMC_PLAYER.bdm 35..42; ZBN's
// BDM has only id 0, so those swings hit nothing). A style id the race uses
// too moves after the highest id of both; `styleIds` maps every style id to
// its id in the merged file. `out` may be null to get only the map.
bool FitBdmToRace(const uint8_t* style, size_t styleSize, const uint8_t* race, size_t raceSize,
                  std::vector<uint8_t>* out, std::map<uint16_t, uint16_t>& styleIds, std::string& note);

struct BacFit {
    int bodyShift = 0;                        // EanFit::shift of the body EAN
    int cameraShift = 0;                      // EanFit::shift of the camera EAN
    std::map<uint16_t, uint16_t> bdmIds;      // FitBdmToRace's styleIds; empty = BDM not merged
    // Sounds (type 11) on the character's own SE bank: kept as they are,
    // pointed at the merged bank's cues (acb_merge.h), or dropped.
    enum class CharacterSe { Keep, Remap, Drop };
    CharacterSe characterSe = CharacterSe::Keep;
    std::map<uint16_t, uint16_t> characterSeCues;  // Remap: style cue id -> its id in the merged bank
    // The race's own BAC (`*_PLAYER.bac`); null = none. Its entries the style
    // lacks are copied in (only with the body EAN merged: they play race
    // animation ids, which keep their ids there).
    const uint8_t* raceBac = nullptr;
    size_t raceBacSize = 0;
};

// Fits the style BAC (`*_PLAYER.bac`) to the merged files:
// * animation records (type 0) on the character EAN with an id >=
//   kRaceBaseAnimations moved up by bodyShift, camera records (type 10) on
//   the character camera EAN with an id >= kRaceBaseCameras by cameraShift;
// * hitboxes (type 1) on the character BDM pointed at the style entry's
//   merged id;
// * sounds (type 11) on the character SE bank: with Remap, their cue id
//   becomes the style cue's id in the race + style bank MergeAcb made (the
//   CaC loads its race's bank, chara_sound.cso: CAR_BTL_NMC_SE ..., whose
//   cue ids are other sounds - ZMS_PLAYER.bac has 39 such records, cues
//   0..6); a cue the merged bank lacks, or every one with Drop, is dropped.
// * entries the race BAC has and the style's lacks (past its last entry or
//   flagged empty) copied from the race BAC as they are: CaC-only actions
//   the game calls by entry number (a styled CaC's Dual Ultimate rush hit
//   nothing, its skill BAC has no hitbox of its own; NMC_PLAYER.bac has 901 entries, CLN_PLAYER.bac 723; 475-515,
//   725/726, 770-773 exist only for CaCs). Race animation, damage and
//   character SE ids keep their ids in the merged files, so they need no
//   remapping.
// Dropped records leave unused bytes at the end of their record array;
// copied entries are appended, with a new entry table when it grows.
bool FitBacToRace(const uint8_t* bac, size_t size, const BacFit& fit, std::vector<uint8_t>& out, std::string& note);

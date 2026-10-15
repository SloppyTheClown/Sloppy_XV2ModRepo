#include "ean_fit.h"
#include "binary.h"
#include "prop_fit.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

// EAN layout (XV2-Tools Xv2CoreLib Animation/EAN/Parser.cs + Deserializer.cs,
// ESK/ESK_File.cs):
//   header (32 bytes): +18 u16 animation count, +20 skeleton offset, +24
//           animation offset table, +28 animation name offset table
//   skeleton (at its offset): +0 i16 bone count, +4 bone index table (8 bytes
//           per bone, parent i16 first), +8 bone name offset table, +12
//           transforms (48 bytes per bone: position, rotation, scale); all
//           relative to the skeleton
//   animation: +2 index size (0 = u8 frame numbers, 1 = u16), +3 value size
//           (1 = 4 halves, 2 = 4 floats), +8 node count, +12 node table; all
//           offsets inside an animation are relative to it (so a whole
//           animation can be moved as one block)
//   node:  +0 i16 bone index, +2 i16 track count, +4 track table (relative
//           to the node)
//   track: +0 type (0 position, 1 rotation, 2 scale), +4 key count, +8 frame
//           numbers, +12 values (relative to the track)
//
// BAC layout (Xv2CoreLib BAC/Parser.cs, BAC_File.cs): +8 entry count, +16
//   entry table; entry 16 bytes: +0 flags (0x80000000 = empty), +4 i16 type
//   count, +8 type list; type list 16 bytes each: +0 i16 type, +2 i16 record
//   count, +8 records. Type 0 (animation) records are 36 bytes, +8 EAN type
//   (5 = the character's EAN), +10 animation id. Type 10 (camera) records are
//   76 bytes, +8 EAN type (4 = the character's camera EAN), +12 animation id.
//   Type 1 (hitbox) records are 64 bytes, +8 u16 BDM entry id, low nibble of
//   +19 the BDM (0 common, 1 the character's, 2 the skill's). Type 11
//   (sound) records are 16 bytes, +8 u16 sound bank (0 common SE, 2 the
//   character's SE, 3 its voice, 10/11 the skill's), +12 cue id.
//
// BDM layout (Xv2CoreLib BDM/Parser.cs): 16-byte header, +8 entry count, +12
//   entry offset (16); entries of 1284 bytes (1084 in the older type 1
//   files), +0 i32 id. Looked up by id: ids have gaps and need not be sorted
//   (BRL_PLAYER.bdm: 0, 116, 77, 62).
namespace {

const size_t kBdmHeader = 16;
const size_t kBdmEntryXv2 = 1284;
const size_t kBdmEntryXv2Small = 1084;
const uint8_t kBdmCharacter = 1;
const uint16_t kAcbCharacterSe = 2;

using binary::Reader;
using binary::Load;
using binary::Store;
using binary::HalfToFloat;
using binary::FloatToHalf;
using binary::Align16;
using binary::AppendOwnBytes;

struct Bone {
    std::string name;
    int parent;
    float position[3];  // rest offset from the parent
};

struct Animation {
    size_t offset = 0;  // in the file; 0 = empty slot
    size_t length = 0;  // bytes up to the end of its last referenced data
    std::string name;
};

struct Ean {
    size_t skeleton = 0, animationTable = 0;
    // skeleton tables, absolute in the file (extraInfo 0 = none): index
    // table 8 bytes per bone (parent, child, sibling, IK flag - i16 each,
    // -1 = none; Xv2CoreLib ESK_File.cs Write), transforms 48, extra info 8
    size_t indexTable = 0, transforms = 0, extraInfo = 0;
    std::vector<Bone> bones;
    std::vector<Animation> animations;  // by id
    std::map<std::string, int> boneIndex;
};

// The extent of one animation block: the end of the furthest table or key
// array it references, so the block can be copied whole.
bool AnimationLength(const Reader& file, size_t animation, size_t& length)
{
    uint8_t indexSize, valueSize;
    int32_t nodeCount, nodeTable;
    if (!file.Get(animation + 2, indexSize) || !file.Get(animation + 3, valueSize) ||
        !file.Get(animation + 8, nodeCount) || !file.Get(animation + 12, nodeTable) ||
        (valueSize != 1 && valueSize != 2) || nodeCount < 0)
        return false;
    size_t end = animation + 16;
    auto reach = [&](size_t at, size_t bytes) {
        if (!file.Has(at, bytes)) return false;
        if (at + bytes > end) end = at + bytes;
        return true;
    };
    if (nodeCount > 0 && !reach(animation + nodeTable, 4 * (size_t)nodeCount)) return false;
    for (int32_t n = 0; n < nodeCount; ++n) {
        int32_t nodeOffset, trackTable;
        int16_t trackCount;
        if (!file.Get(animation + nodeTable + 4 * (size_t)n, nodeOffset)) return false;
        size_t node = animation + nodeOffset;
        if (!file.Get(node + 2, trackCount) || !file.Get(node + 4, trackTable) || !reach(node, 8)) return false;
        if (trackCount > 0 && !reach(node + trackTable, 4 * (size_t)trackCount)) return false;
        for (int t = 0; t < trackCount; ++t) {
            int32_t trackOffset, frames, values;
            uint32_t keyCount;
            if (!file.Get(node + trackTable + 4 * (size_t)t, trackOffset)) return false;
            size_t track = node + trackOffset;
            if (!file.Get(track + 4, keyCount) || !file.Get(track + 8, frames) || !file.Get(track + 12, values) ||
                !reach(track, 16) || !reach(track + frames, (size_t)keyCount * (indexSize ? 2 : 1)) ||
                !reach(track + values, (size_t)keyCount * (valueSize == 1 ? 8 : 16)))
                return false;
        }
    }
    length = end - animation;
    return true;
}

bool ParseEan(const Reader& file, Ean& ean, std::string& note)
{
    uint16_t animationCount;
    int32_t skeleton, animationTable, nameTable, indexTable, nameOffsets, transforms;
    int16_t boneCount;
    if (!file.Get(18, animationCount) || !file.Get(20, skeleton) || !file.Get(24, animationTable) ||
        !file.Get(28, nameTable) || skeleton <= 0 || !file.Get(skeleton, boneCount) || boneCount <= 0 ||
        !file.Get(skeleton + 4, indexTable) || !file.Get(skeleton + 8, nameOffsets) ||
        !file.Get(skeleton + 12, transforms)) {
        note = "not an EAN";
        return false;
    }
    ean.skeleton = skeleton;
    ean.animationTable = animationTable;
    ean.indexTable = (size_t)skeleton + indexTable;
    ean.transforms = (size_t)skeleton + transforms;
    int32_t extraInfo = 0;  // skeleton +24 (Xv2CoreLib ESK_File.cs Read: "Unk2")
    if (file.Get(skeleton + 24, extraInfo) && extraInfo > 0 && file.Has((size_t)skeleton + extraInfo, 8 * (size_t)boneCount))
        ean.extraInfo = (size_t)skeleton + extraInfo;
    if (!file.Has(ean.indexTable, 8 * (size_t)boneCount) || !file.Has(ean.transforms, 48 * (size_t)boneCount)) {
        note = "skeleton out of bounds";
        return false;
    }
    for (int i = 0; i < boneCount; ++i) {
        Bone bone{};
        int16_t parent;
        int32_t nameAt;
        size_t transform = (size_t)skeleton + transforms + 48 * (size_t)i;
        if (!file.Get(skeleton + indexTable + 8 * (size_t)i, parent) ||
            !file.Get(skeleton + nameOffsets + 4 * (size_t)i, nameAt) || !file.Get(transform, bone.position[0]) ||
            !file.Get(transform + 4, bone.position[1]) || !file.Get(transform + 8, bone.position[2])) {
            note = "skeleton out of bounds";
            return false;
        }
        bone.parent = parent;
        bone.name = file.Name(skeleton + nameAt);
        ean.boneIndex.emplace(bone.name, i);
        ean.bones.push_back(bone);
    }
    ean.animations.resize(animationCount);
    for (size_t a = 0; a < animationCount; ++a) {
        int32_t offset, nameAt;
        if (!file.Get(animationTable + 4 * a, offset)) { note = "animation table out of bounds"; return false; }
        if (offset == 0) continue;
        Animation& animation = ean.animations[a];
        animation.offset = offset;
        if (!AnimationLength(file, offset, animation.length)) { note = "unexpected animation layout"; return false; }
        if (file.Get(nameTable + 4 * a, nameAt)) animation.name = file.Name(nameAt);
    }
    return true;
}

// Hip-to-toe-joint length of the left leg: thigh (b_L_Leg2's offset from the
// hip) + shin (b_L_Foot's from the knee) + the toe joint's drop below the
// ankle (b_L_Toe Y). Bone offsets are rotation-free, so the ratio of two
// files' lengths is how far the same pose's foot ends up below the hip.
// Measured: GOK 0.354 + 0.428 + 0.068, FRI 0.259 + 0.320 + 0.043.
float LegLength(const Ean& ean)
{
    auto rest = [&](const char* name) -> const float* {
        auto found = ean.boneIndex.find(name);
        return found == ean.boneIndex.end() ? nullptr : ean.bones[found->second].position;
    };
    const float *thigh = rest("b_L_Leg2"), *shin = rest("b_L_Foot"), *toe = rest("b_L_Toe");
    if (!thigh || !shin || !toe) return 0;
    auto length = [](const float* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
    return length(thigh) + length(shin) + std::fabs(toe[1]);
}

// Shoulder to wrist: the rest lengths of b_R_Arm1, b_R_Arm2 and b_R_Hand.
// Props are placed and sized by it (race / style, prop_fit.h
// PropBonePositionScale), as a held prop goes with the arm, not the legs:
// Whis 0.713 vs HUM 0.692 keeps his cane as it is on a HUM, FRI 0.494
// shrinks it, Goku Mini 0.273 moves his pole out to an adult's hand (leg
// ratios would shrink the cane to 0.73 on a HUM; seen in game, build 8:
// unscaled props are right on HUM, too big on FRI, off the hand for Goku
// Mini). prop_fit.cpp ArmLength measures the same on the ESK.
float ArmLength(const Ean& ean)
{
    float total = 0;
    for (const char* name : {"b_R_Arm1", "b_R_Arm2", "b_R_Hand"}) {
        auto found = ean.boneIndex.find(name);
        if (found == ean.boneIndex.end()) return 0;
        const float* v = ean.bones[found->second].position;
        total += std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    }
    return total;
}

// Bones whose position every ordinary animation sets: the root and pelvis,
// and the g_* helper bones (IK/camera targets). Other bones normally get
// rotations only (measured over all GOK animations).
bool AlwaysPositioned(const std::string& name)
{
    return name == "b_C_Base" || name == "b_C_Pelvis" || name.compare(0, 2, "g_") == 0;
}

// What the race's own EAN does with each of its bones, over all animations.
struct RaceUse {
    std::vector<bool> animated, positioned;
};

RaceUse RaceBoneUse(const Reader& file, const Ean& race)
{
    RaceUse use{std::vector<bool>(race.bones.size()), std::vector<bool>(race.bones.size())};
    for (const Animation& animation : race.animations) {
        if (!animation.offset) continue;
        int32_t nodeCount, nodeTable;
        file.Get(animation.offset + 8, nodeCount);
        file.Get(animation.offset + 12, nodeTable);
        for (int32_t n = 0; n < nodeCount; ++n) {  // bounds checked by AnimationLength
            int32_t nodeOffset, trackTable;
            int16_t bone, trackCount;
            file.Get(animation.offset + nodeTable + 4 * (size_t)n, nodeOffset);
            size_t node = animation.offset + nodeOffset;
            file.Get(node, bone);
            file.Get(node + 2, trackCount);
            file.Get(node + 4, trackTable);
            if (bone < 0 || (size_t)bone >= race.bones.size()) continue;
            for (int t = 0; t < trackCount; ++t) {
                int32_t trackOffset;
                uint8_t type;
                file.Get(node + trackTable + 4 * (size_t)t, trackOffset);
                file.Get(node + trackOffset, type);
                use.animated[bone] = true;
                if (type == 0) use.positioned[bone] = true;
            }
        }
    }
    return use;
}

// True when the bone has the same parents, by name, up to the root in both
// skeletons (the root itself is named after the character - GOK_BAS_BONE -
// so it is not compared).
bool SameParentChain(const Ean& style, int styleBone, const Ean& race, int raceBone)
{
    for (int depth = 0; depth < 256; ++depth) {
        int styleParent = style.bones[styleBone].parent, raceParent = race.bones[raceBone].parent;
        if (styleParent < 0 || raceParent < 0) return styleParent < 0 && raceParent < 0;
        if ((size_t)styleParent >= style.bones.size() || (size_t)raceParent >= race.bones.size()) return false;
        bool styleRoot = style.bones[styleParent].parent < 0, raceRoot = race.bones[raceParent].parent < 0;
        if (styleRoot || raceRoot) return styleRoot && raceRoot;
        if (style.bones[styleParent].name != race.bones[raceParent].name) return false;
        styleBone = styleParent;
        raceBone = raceParent;
    }
    return false;
}

// Per style bone: its race bone index (-1 = the race has none), and which of
// its tracks to keep.
struct BonePlan {
    int raceBone = -1;
    bool keepAll = true;        // false: drop every track (bone the race never animates)
    bool keepPosition = true;   // false: drop its position tracks
    float positionScale = 1;    // its position keys are multiplied by this
    bool backProp = false;      // a prop carried on the back: depth (z) keys get depthShift, not the scale
    float depthShift = 0;       // prop_fit.h PropPlacement
    float rest[3] = {};         // the back prop's rest position (BackShiftWeight)
};

struct FitStats {
    size_t nodesDropped = 0, tracksDropped = 0, scaledKeys = 0;
};

void RemoveTableEntry(std::vector<uint8_t>& out, size_t table, int slot, int count)
{
    memmove(out.data() + table + 4 * slot, out.data() + table + 4 * (slot + 1), 4 * (size_t)(count - slot - 1));
}

// Edits a style animation already copied to out[block...]: bone indices to the
// race skeleton, tracks dropped and position keys scaled per `plan`. Nodes and
// tracks are taken out of their tables (the data stays, unreferenced).
void FitAnimation(std::vector<uint8_t>& out, size_t block, const std::vector<BonePlan>& plan, FitStats& stats)
{
    uint8_t valueSize = out[block + 3];
    int32_t nodeCount = Load<int32_t>(out, block + 8);
    size_t nodeTable = block + Load<int32_t>(out, block + 12);
    for (int32_t n = nodeCount - 1; n >= 0; --n) {
        size_t node = block + Load<int32_t>(out, nodeTable + 4 * (size_t)n);
        int16_t bone = Load<int16_t>(out, node);
        const BonePlan* bonePlan = bone >= 0 && (size_t)bone < plan.size() ? &plan[bone] : nullptr;
        int16_t trackCount = Load<int16_t>(out, node + 2);
        size_t trackTable = node + Load<int32_t>(out, node + 4);
        if (bonePlan && bonePlan->raceBone >= 0 && bonePlan->keepAll) {
            for (int t = trackCount - 1; t >= 0; --t) {
                size_t track = node + Load<int32_t>(out, trackTable + 4 * (size_t)t);
                if (out[track] != 0) continue;  // rotation or scale: kept
                if (!bonePlan->keepPosition) {
                    RemoveTableEntry(out, trackTable, t, trackCount--);
                    ++stats.tracksDropped;
                } else if (bonePlan->positionScale != 1 || bonePlan->backProp) {
                    const float scale = bonePlan->positionScale;
                    // a back prop's depth keys near its rest get the depth shift (PlacePropAxis, BackShiftWeight)
                    PropPlacement back;
                    back.backKnown = bonePlan->backProp;
                    back.shift = bonePlan->depthShift;
                    uint32_t keyCount = Load<uint32_t>(out, track + 4);
                    size_t values = track + Load<int32_t>(out, track + 12);
                    for (uint32_t k = 0; k < keyCount; ++k) {
                        // a key's three axes: halves (valueSize 1) or floats
                        auto at = [&](int axis) {
                            return valueSize == 1 ? values + 8 * (size_t)k + 2 * axis : values + 16 * (size_t)k + 4 * axis;
                        };
                        float key[3];
                        for (int axis = 0; axis < 3; ++axis)
                            key[axis] = valueSize == 1 ? HalfToFloat(Load<uint16_t>(out, at(axis))) : Load<float>(out, at(axis));
                        const double weight = bonePlan->backProp ? BackShiftWeight(key, bonePlan->rest) : 0;
                        for (int axis = 0; axis < 3; ++axis) {
                            const float placed = (float)PlacePropAxis(axis, key[axis], scale, bonePlan->backProp, back, weight);
                            if (valueSize == 1) Store<uint16_t>(out, at(axis), FloatToHalf(placed));
                            else Store<float>(out, at(axis), placed);
                        }
                    }
                    stats.scaledKeys += keyCount;
                }
            }
            Store<int16_t>(out, node + 2, trackCount);
            if (trackCount > 0) {
                Store<int16_t>(out, node, (int16_t)bonePlan->raceBone);
                continue;
            }
        } else if (bonePlan && bonePlan->raceBone >= 0) {
            stats.tracksDropped += trackCount;
        }
        RemoveTableEntry(out, nodeTable, n, nodeCount--);  // no race bone, or nothing left
        ++stats.nodesDropped;
    }
    Store<int32_t>(out, block + 8, nodeCount);
}

// Prop bones: the style character's "a_x_*" bones (Whis's cane, Trunks's
// sword, Gamma's guns; ClaudeTools\battle_styles\propbones.py) that the race lacks.
// Each hangs off a bone every CaC has or another prop bone.
bool IsPropBone(const std::string& name) { return name.compare(0, 4, "a_x_") == 0; }

struct PropBone {
    int styleBone;  // index in the style skeleton
    int parent;     // index in the merged skeleton
};

// Rebuilds the merged skeleton's tables at the end of `out` with `props`
// appended after the race's bones (race indices unchanged, so the race
// animations and the IK section, left where it is, still match). The
// skeleton header stays put; its table offsets are relative to it. The old
// tables stay in the file, unreferenced.
void AppendPropBones(std::vector<uint8_t>& out, const Ean& race, const Reader& styleFile, const Ean& style,
                     const std::vector<PropBone>& props, float armRatio, const PropPlacement& back)
{
    size_t raceCount = race.bones.size(), count = raceCount + props.size();
    auto relative = [&](size_t at) { return (int32_t)(at - race.skeleton); };

    Align16(out);
    size_t indexTable = out.size();
    AppendOwnBytes(out, race.indexTable, 8 * raceCount);
    out.resize(indexTable + 8 * count, 0);
    auto link = [&](size_t bone, int field) { return indexTable + 8 * bone + 2 * field; };  // 0 parent, 1 child, 2 sibling, 3 IK flag
    for (size_t k = 0; k < props.size(); ++k) {
        size_t bone = raceCount + k;
        int16_t ikFlag = 0;
        styleFile.Get(style.indexTable + 8 * (size_t)props[k].styleBone + 6, ikFlag);
        Store<int16_t>(out, link(bone, 0), (int16_t)props[k].parent);
        Store<int16_t>(out, link(bone, 1), -1);
        Store<int16_t>(out, link(bone, 2), -1);
        Store<int16_t>(out, link(bone, 3), ikFlag);
        // last in its parent's child list: the parent's child, or the last sibling's sibling
        size_t slot = link(props[k].parent, 1);
        for (int guard = 0; Load<int16_t>(out, slot) >= 0 && guard < 0x8000; ++guard)
            slot = link((size_t)Load<int16_t>(out, slot), 2);
        Store<int16_t>(out, slot, (int16_t)bone);
    }

    // names: the race's entries keep pointing at its strings, new ones follow
    size_t nameTable = out.size();
    out.resize(nameTable + 4 * count, 0);
    int32_t raceNames = Load<int32_t>(out, race.skeleton + 8);
    memcpy(out.data() + nameTable, out.data() + race.skeleton + raceNames, 4 * raceCount);
    for (size_t k = 0; k < props.size(); ++k) {
        Store<int32_t>(out, nameTable + 4 * (raceCount + k), relative(out.size()));
        const std::string& name = style.bones[props[k].styleBone].name;
        out.insert(out.end(), name.begin(), name.end());
        out.push_back(0);
    }

    // rest transforms (position, rotation, scale) from the style skeleton, the
    // position sized to the race (prop_fit.h PropBonePositionScale)
    Align16(out);
    size_t transforms = out.size();
    AppendOwnBytes(out, race.transforms, 48 * raceCount);
    for (const PropBone& prop : props) {
        const uint8_t* from = styleFile.data + style.transforms + 48 * (size_t)prop.styleBone;
        const size_t at = out.size();
        out.insert(out.end(), from, from + 48);
        const Bone& bone = style.bones[prop.styleBone];
        const double scale = PropBonePositionScale(bone.name, armRatio, back.SizeRatio(armRatio));
        const bool backBone = IsBackPropBone(style.bones[bone.parent].name, bone.position[2]);
        for (int axis = 0; axis < 3; ++axis)
            Store<float>(out, at + 4 * axis, (float)PlacePropAxis(axis, Load<float>(out, at + 4 * axis), scale, backBone, back));
    }

    size_t extraInfo = 0;
    if (race.extraInfo) {
        Align16(out);
        extraInfo = out.size();
        AppendOwnBytes(out, race.extraInfo, 8 * raceCount);
        for (const PropBone& prop : props) {
            // the style's, else what every CaC bone has (0, 0, 0xffff, 0 - HUM_000.esk)
            uint8_t values[8] = {0, 0, 0, 0, 0xff, 0xff, 0, 0};
            if (style.extraInfo) memcpy(values, styleFile.data + style.extraInfo + 8 * (size_t)prop.styleBone, 8);
            out.insert(out.end(), values, values + 8);
        }
    }

    Store<int16_t>(out, race.skeleton, (int16_t)count);
    Store<int32_t>(out, race.skeleton + 4, relative(indexTable));
    Store<int32_t>(out, race.skeleton + 8, relative(nameTable));
    Store<int32_t>(out, race.skeleton + 12, relative(transforms));
    if (extraInfo) Store<int32_t>(out, race.skeleton + 24, relative(extraInfo));
}

// Race tail: the Frieza race and HUM/HUF (the Saiyan tail of SYM/SYF) have
// a 10-bone chain X_T_TAIL1..10 that the roster characters' EANs mostly lack
// (GOK.ean has none), so a borrowed animation leaves the tail at rest. Every
// base animation of those races keys all 10 bones (ClaudeTools\battle_styles\
// tailprobe.py; FRI_BAS_STAND 201 frames, HUM 141). The race's tail tracks
// are put into each style animation that has none for them.
bool IsTailBone(const std::string& name) { return name.compare(0, 8, "X_T_TAIL") == 0; }

struct TailKey {
    int frame;
    float value[4];
};

struct TailTrack {
    uint8_t header[4];  // type (0 position, 1 rotation, 2 scale) + Xv2CoreLib I_01/I_02, copied as they are
    std::vector<TailKey> keys;
};

// The tracks of one bone in one animation of `file`, keys decoded.
std::vector<TailTrack> ReadBoneTracks(const Reader& file, size_t animation, int bone)
{
    std::vector<TailTrack> tracks;
    uint8_t indexSize = file.data[animation + 2], valueSize = file.data[animation + 3];
    int32_t nodeCount, nodeTable;
    file.Get(animation + 8, nodeCount);
    file.Get(animation + 12, nodeTable);
    for (int32_t n = 0; n < nodeCount; ++n) {  // bounds checked by AnimationLength
        int32_t nodeOffset, trackTable;
        int16_t nodeBone, trackCount;
        file.Get(animation + nodeTable + 4 * (size_t)n, nodeOffset);
        size_t node = animation + nodeOffset;
        file.Get(node, nodeBone);
        if (nodeBone != bone) continue;
        file.Get(node + 2, trackCount);
        file.Get(node + 4, trackTable);
        for (int t = 0; t < trackCount; ++t) {
            int32_t trackOffset, frames, values;
            uint32_t keyCount;
            file.Get(node + trackTable + 4 * (size_t)t, trackOffset);
            size_t track = node + trackOffset;
            file.Get(track + 4, keyCount);
            file.Get(track + 8, frames);
            file.Get(track + 12, values);
            TailTrack decoded;
            memcpy(decoded.header, file.data + track, 4);
            for (uint32_t k = 0; k < keyCount; ++k) {
                TailKey key{};
                if (indexSize) { uint16_t frame; file.Get(track + frames + 2 * (size_t)k, frame); key.frame = frame; }
                else key.frame = file.data[track + frames + k];
                for (int axis = 0; axis < 4; ++axis) {
                    if (valueSize == 1) {
                        uint16_t half;
                        file.Get(track + values + 8 * (size_t)k + 2 * axis, half);
                        key.value[axis] = HalfToFloat(half);
                    } else {
                        file.Get(track + values + 16 * (size_t)k + 4 * axis, key.value[axis]);
                    }
                }
                decoded.keys.push_back(key);
            }
            if (!decoded.keys.empty()) tracks.push_back(decoded);
        }
        break;
    }
    return tracks;
}

// The track's value at a (fractional) frame: linear between the keys around
// it, rotations (quaternions) taken to the same hemisphere and normalised.
void SampleTrack(const TailTrack& track, double frame, float (&value)[4])
{
    const std::vector<TailKey>& keys = track.keys;
    size_t next = 0;
    while (next < keys.size() && keys[next].frame < frame) ++next;
    if (next == 0 || next == keys.size()) {
        memcpy(value, keys[next == 0 ? 0 : keys.size() - 1].value, sizeof(value));
        return;
    }
    const TailKey &before = keys[next - 1], &after = keys[next];
    float weight = (float)((frame - before.frame) / (after.frame - before.frame));
    bool rotation = track.header[0] == 1;
    float sign = 1;
    if (rotation) {
        float dot = 0;
        for (int axis = 0; axis < 4; ++axis) dot += before.value[axis] * after.value[axis];
        if (dot < 0) sign = -1;
    }
    float length = 0;
    for (int axis = 0; axis < 4; ++axis) {
        value[axis] = before.value[axis] + (sign * after.value[axis] - before.value[axis]) * weight;
        length += value[axis] * value[axis];
    }
    if (rotation && length > 1e-12f)
        for (float& component : value) component /= std::sqrt(length);
}

// Where the race animation donating the tail is read for each frame of the
// style animation: stretched over it (same base move, similar length) or
// looped (any other move gets the race's standing loop).
struct TailTiming {
    bool loop = false;
    double scale = 1;   // stretched: donor frame = frame * scale
    int period = 1;     // looped: donor frame = frame mod period (last key = first pose)
};

struct TailStats {
    size_t animations = 0, nodes = 0;
};

// Appends nodes for the race tail bones `donorTracks` (race bone index ->
// tracks) to a style animation already copied to out[block...] and fitted:
// one key per frame of the style animation (two for a track that doesn't
// move), in its own key sizes. Its node table is rebuilt at the end of `out`
// (the old one stays, unreferenced). The block must be the last thing in
// `out`, as all new data is relative to it.
void AppendTailNodes(std::vector<uint8_t>& out, size_t block, const std::map<int, std::vector<TailTrack>>& donorTracks,
                     const TailTiming& timing, TailStats& stats)
{
    uint8_t indexSize = out[block + 2], valueSize = out[block + 3];
    int32_t frameCount = Load<int32_t>(out, block + 4);
    int32_t nodeCount = Load<int32_t>(out, block + 8);
    size_t oldTable = block + Load<int32_t>(out, block + 12);
    if (frameCount < 1 || (!indexSize && frameCount > 256)) return;

    // bones the fitted style animation already moves keep their own tracks
    std::vector<int> missing;
    for (const auto& entry : donorTracks) {
        bool present = false;
        for (int32_t n = 0; n < nodeCount && !present; ++n)
            present = Load<int16_t>(out, block + Load<int32_t>(out, oldTable + 4 * (size_t)n)) == entry.first;
        if (!present && !entry.second.empty()) missing.push_back(entry.first);
    }
    if (missing.empty()) return;

    Align16(out);
    size_t nodeTable = out.size();
    out.resize(nodeTable + 4 * (nodeCount + missing.size()), 0);
    memcpy(out.data() + nodeTable, out.data() + oldTable, 4 * (size_t)nodeCount);
    for (size_t m = 0; m < missing.size(); ++m) {
        const std::vector<TailTrack>& tracks = donorTracks.at(missing[m]);
        Align16(out);
        size_t node = out.size();
        Store<int32_t>(out, nodeTable + 4 * (nodeCount + m), (int32_t)(node - block));
        out.resize(node + 8 + 4 * tracks.size(), 0);
        Store<int16_t>(out, node, (int16_t)missing[m]);
        Store<int16_t>(out, node + 2, (int16_t)tracks.size());
        Store<int32_t>(out, node + 4, 8);
        for (size_t t = 0; t < tracks.size(); ++t) {
            const TailTrack& donor = tracks[t];
            // a track that holds one value (the tail bones' scale) needs only
            // the first and last key
            bool still = true;
            for (const TailKey& key : donor.keys)
                still = still && memcmp(key.value, donor.keys[0].value, sizeof(key.value)) == 0;
            std::vector<int> frames;
            for (int frame = 0; frame < frameCount; ++frame)
                if (!still || frame == 0 || frame == frameCount - 1) frames.push_back(frame);

            Align16(out);
            size_t track = out.size();
            Store<int32_t>(out, node + 8 + 4 * t, (int32_t)(track - node));
            out.resize(track + 16, 0);
            memcpy(out.data() + track, donor.header, 4);
            Store<uint32_t>(out, track + 4, (uint32_t)frames.size());
            Store<int32_t>(out, track + 8, (int32_t)(out.size() - track));
            for (int frame : frames) {
                if (indexSize) { out.resize(out.size() + 2); Store<uint16_t>(out, out.size() - 2, (uint16_t)frame); }
                else out.push_back((uint8_t)frame);
            }
            // float keys are read with movaps (see FitEanToRace), so 16-aligned
            if (valueSize == 1) out.resize((out.size() + 3) & ~(size_t)3, 0);
            else Align16(out);
            Store<int32_t>(out, track + 12, (int32_t)(out.size() - track));
            for (int frame : frames) {
                double donorFrame = timing.loop ? (double)(frame % timing.period) : frame * timing.scale;
                float value[4];
                SampleTrack(donor, donorFrame, value);
                for (float component : value) {
                    if (valueSize == 1) { out.resize(out.size() + 2); Store<uint16_t>(out, out.size() - 2, FloatToHalf(component)); }
                    else { out.resize(out.size() + 4); Store<float>(out, out.size() - 4, component); }
                }
            }
        }
        ++stats.nodes;
    }
    Store<int32_t>(out, block + 8, nodeCount + (int32_t)missing.size());
    Store<int32_t>(out, block + 12, (int32_t)(nodeTable - block));
    ++stats.animations;
}

}  // namespace

bool EanBoneUse(const uint8_t* data, size_t size, std::map<std::string, int>& animationsWithTrack, int& animations,
                std::string& note)
{
    const Reader file{data, size};
    Ean ean;
    if (!ParseEan(file, ean, note)) return false;
    animations = 0;
    for (const Animation& animation : ean.animations) {
        if (!animation.offset) continue;
        ++animations;
        int32_t nodeCount, nodeTable;
        file.Get(animation.offset + 8, nodeCount);
        file.Get(animation.offset + 12, nodeTable);
        std::vector<bool> moved(ean.bones.size());
        for (int32_t n = 0; n < nodeCount; ++n) {  // bounds checked by AnimationLength
            int32_t nodeOffset;
            int16_t bone, trackCount;
            file.Get(animation.offset + nodeTable + 4 * (size_t)n, nodeOffset);
            file.Get(animation.offset + nodeOffset, bone);
            file.Get(animation.offset + nodeOffset + 2, trackCount);
            if (bone >= 0 && (size_t)bone < moved.size() && trackCount > 0) moved[bone] = true;
        }
        for (size_t bone = 0; bone < moved.size(); ++bone)
            if (moved[bone]) ++animationsWithTrack[ean.bones[bone].name];
    }
    return true;
}

bool FitEanToRace(const uint8_t* styleData, size_t styleSize, const uint8_t* raceData, size_t raceSize, bool body,
                  int baseAnimations, std::vector<uint8_t>& out, EanFit& fit, std::string& note,
                  const PropPlacement& back)
{
    Reader styleFile{styleData, styleSize}, raceFile{raceData, raceSize};
    Ean style, race;
    if (!ParseEan(styleFile, style, note)) { note = "style: " + note; return false; }
    if (!ParseEan(raceFile, race, note)) { note = "race: " + note; return false; }
    // The race file up to its animation table (header + skeleton) is copied
    // as it is: the skeleton must lie inside it.
    if (race.skeleton < 32 || race.animationTable <= race.skeleton || race.animationTable > raceSize) {
        note = "race: skeleton not before the animations";
        return false;
    }

    float pelvisScale = 1, styleLeg = 0, raceLeg = 0;
    if (body) {
        styleLeg = LegLength(style);
        raceLeg = LegLength(race);
        if (styleLeg < 0.1f || raceLeg < 0.1f) { note = "leg bones not found"; return false; }
        pelvisScale = raceLeg / styleLeg;
    }
    float armRatio = 1;  // sizes the prop bone positions (rest + keys), see ArmLength
    const float styleArm = ArmLength(style), raceArm = ArmLength(race);
    if (body && styleArm > 0.05f && raceArm > 0.05f) armRatio = raceArm / styleArm;

    // Which tracks of each style bone survive.
    RaceUse use = body ? RaceBoneUse(raceFile, race) : RaceUse{};
    std::vector<BonePlan> plan(style.bones.size());
    std::vector<PropBone> props;
    for (size_t i = 0; i < style.bones.size(); ++i) {
        auto found = race.boneIndex.find(style.bones[i].name);
        if (found == race.boneIndex.end()) {
            // a prop bone joins the merged skeleton with all its tracks; the
            // style skeleton lists parents before children
            int parent = style.bones[i].parent;
            if (body && IsPropBone(style.bones[i].name) && parent >= 0 && parent < (int)i &&
                plan[parent].raceBone >= 0 && race.bones.size() + props.size() < 0x7fff) {
                plan[i].raceBone = (int)(race.bones.size() + props.size());
                plan[i].positionScale = (float)PropBonePositionScale(style.bones[i].name, armRatio, back.SizeRatio(armRatio));
                plan[i].backProp = back.backKnown && IsBackPropBone(style.bones[parent].name, style.bones[i].position[2]);
                plan[i].depthShift = (float)back.shift;
                memcpy(plan[i].rest, style.bones[i].position, sizeof(plan[i].rest));
                props.push_back({(int)i, plan[parent].raceBone});
            }
            continue;
        }
        BonePlan& bonePlan = plan[i];
        bonePlan.raceBone = found->second;
        if (!body) continue;
        const std::string& name = style.bones[i].name;
        bool always = AlwaysPositioned(name);
        bonePlan.keepAll = always || use.animated[bonePlan.raceBone];
        bonePlan.keepPosition = always || (use.positioned[bonePlan.raceBone] &&
                                           SameParentChain(style, (int)i, race, bonePlan.raceBone));
        // The pelvis height goes with the leg length. Another kept position
        // (hands and the arm-stretch chain of Piccolo on a Namekian) is the
        // style's bone length, possibly stretched: scaled to the race's, so
        // Zarbon's idle doesn't give a Majin his shorter forearm (0.282 vs
        // 0.313, b_L_Hand rest offsets).
        if (name == "b_C_Pelvis") {
            bonePlan.positionScale = pelvisScale;
        } else if (bonePlan.keepPosition && !always) {
            auto length = [](const float* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
            float styleRest = length(style.bones[i].position), raceRest = length(race.bones[bonePlan.raceBone].position);
            if (styleRest > 1e-4f && raceRest > 1e-4f) bonePlan.positionScale = raceRest / styleRest;
        }
    }

    // Ids: the style's below the base replace the race's; its others go after
    // the race's last.
    int raceCount = (int)race.animations.size(), styleCount = (int)style.animations.size();
    fit.shift = raceCount > baseAnimations ? raceCount - baseAnimations : 0;
    auto styleTarget = [&](int id) { return id < baseAnimations ? id : id + fit.shift; };
    int total = raceCount;
    for (int id = 0; id < styleCount; ++id)
        if (style.animations[id].offset && styleTarget(id) + 1 > total) total = styleTarget(id) + 1;
    if (total > 0xffff) { note = "too many animations"; return false; }
    struct Slot { const Reader* file; const Animation* animation; bool fromStyle; };
    std::vector<Slot> slots(total, Slot{nullptr, nullptr, false});
    for (int id = 0; id < raceCount; ++id)
        if (race.animations[id].offset) slots[id] = {&raceFile, &race.animations[id], false};
    int styleAnimations = 0;
    for (int id = 0; id < styleCount; ++id)
        if (style.animations[id].offset) {
            slots[styleTarget(id)] = {&styleFile, &style.animations[id], true};
            ++styleAnimations;
        }

    // The race tail bones and, per race animation, their tracks (read when
    // first needed): donors for style animations without a tail.
    std::vector<int> tailBones;
    if (body)
        for (size_t b = 0; b < race.bones.size(); ++b)
            if (IsTailBone(race.bones[b].name)) tailBones.push_back((int)b);
    std::map<int, std::map<int, std::vector<TailTrack>>> tailDonors;
    auto donorTracks = [&](int raceId) -> const std::map<int, std::vector<TailTrack>>& {
        auto known = tailDonors.find(raceId);
        if (known != tailDonors.end()) return known->second;
        std::map<int, std::vector<TailTrack>>& tracks = tailDonors[raceId];
        if (raceId < raceCount && race.animations[raceId].offset)
            for (int bone : tailBones) {
                std::vector<TailTrack> boneTracks = ReadBoneTracks(raceFile, race.animations[raceId].offset, bone);
                if (!boneTracks.empty()) tracks[bone] = boneTracks;
            }
        return tracks;
    };
    auto frameCountOf = [&](int raceId) {
        int32_t frames = 0;
        raceFile.Get(race.animations[raceId].offset + 4, frames);
        return frames;
    };
    TailStats tailStats;

    out.assign(raceData, raceData + race.animationTable);
    Align16(out);
    size_t animationTable = out.size();
    out.resize(animationTable + 4 * (size_t)total, 0);
    FitStats stats;
    int raceKept = 0;
    for (int id = 0; id < total; ++id) {
        const Slot& slot = slots[id];
        if (!slot.animation) continue;
        // Keep the source's offset mod 16: float keys (16 bytes each) are read
        // with movaps by xgcore's AnimationSequencer::CalcNodeCurrentPosition,
        // and an animation's offsets are relative to it. The cpk files start
        // every animation 16-aligned, but loose ones need not (VDL.ean, CLN.ean:
        // some at 8 mod 16, keys aligned in the file) - realigning those to 16
        // crashed the game (ClaudeTools\battle_styles\eanalign.py).
        size_t sourceAlignment = slot.animation->offset & 15;
        out.resize(out.size() + ((sourceAlignment - out.size()) & 15), 0);
        size_t block = out.size();
        out.insert(out.end(), slot.file->data + slot.animation->offset,
                   slot.file->data + slot.animation->offset + slot.animation->length);
        Store<int32_t>(out, animationTable + 4 * (size_t)id, (int32_t)block);
        if (!slot.fromStyle) { ++raceKept; continue; }
        FitAnimation(out, block, plan, stats);
        if (tailBones.empty()) continue;
        // Tail donor: the race's own animation for the same base move when it
        // has the tail and a length within 2x (stretched over the style's, so
        // a loop stays a loop); else the race's standing loop (id 0) repeated.
        int32_t styleFrames = Load<int32_t>(out, block + 4);
        TailTiming timing;
        int donor = 0;
        if (id < baseAnimations && !donorTracks(id).empty()) {
            int32_t raceFrames = frameCountOf(id);
            double ratio = styleFrames > 1 && raceFrames > 1 ? (double)(raceFrames - 1) / (styleFrames - 1) : 0;
            if (ratio >= 0.5 && ratio <= 2) { donor = id; timing.scale = ratio; }
        }
        if (donor == 0 && id != 0) {
            if (donorTracks(0).empty()) continue;
            timing.loop = true;
            timing.period = std::max(1, frameCountOf(0) - 1);
        } else if (donor == 0) {  // the style's own stand: the race stand stretched over it, if close
            int32_t raceFrames = frameCountOf(0);
            double ratio = styleFrames > 1 && raceFrames > 1 ? (double)(raceFrames - 1) / (styleFrames - 1) : 0;
            if (ratio >= 0.5 && ratio <= 2) timing.scale = ratio;
            else { timing.loop = true; timing.period = std::max(1, raceFrames - 1); }
        }
        AppendTailNodes(out, block, donorTracks(donor), timing, tailStats);
    }
    Align16(out);
    size_t nameTable = out.size();
    out.resize(nameTable + 4 * (size_t)total, 0);
    for (int id = 0; id < total; ++id) {
        if (!slots[id].animation) continue;
        Store<int32_t>(out, nameTable + 4 * (size_t)id, (int32_t)out.size());
        const std::string& name = slots[id].animation->name;
        out.insert(out.end(), name.begin(), name.end());
        out.push_back(0);
    }
    Store<uint16_t>(out, 18, (uint16_t)total);
    Store<int32_t>(out, 24, (int32_t)animationTable);
    Store<int32_t>(out, 28, (int32_t)nameTable);
    if (!props.empty()) AppendPropBones(out, race, styleFile, style, props, armRatio, back);

    char text[400];
    if (body)
        snprintf(text, sizeof(text), "%d style + %d race animations, style ids %d+ moved up %d; leg %.3f -> %.3f "
                 "(x%.3f), %zu position keys scaled; %zu tracks + %zu nodes dropped; %zu prop bones added (arm x%.3f); "
                 "race tail (%zu bones) added to %zu animations (%zu nodes)",
                 styleAnimations, raceKept, baseAnimations, fit.shift, styleLeg, raceLeg, pelvisScale,
                 stats.scaledKeys, stats.tracksDropped, stats.nodesDropped, props.size(), armRatio,
                 tailBones.size(), tailStats.animations, tailStats.nodes);
    else
        snprintf(text, sizeof(text), "%d style + %d race animations, style ids %d+ moved up %d; %zu nodes dropped",
                 styleAnimations, raceKept, baseAnimations, fit.shift, stats.nodesDropped);
    note = text;
    return true;
}

namespace {

// The entry ids of a BDM, in file order; false when it is not one.
bool ParseBdm(const Reader& file, std::vector<uint16_t>& ids, size_t& entrySize, std::string& note)
{
    uint32_t magic, count;
    int32_t entries;
    if (!file.Get(0, magic) || magic != 0x4d444223 /* "#BDM" */ || !file.Get(8, count) || !file.Get(12, entries) ||
        entries != kBdmHeader) {
        note = "not a BDM";
        return false;
    }
    entrySize = count ? (file.size - kBdmHeader) / count : kBdmEntryXv2;
    if (entrySize != kBdmEntryXv2 && entrySize != kBdmEntryXv2Small) {
        note = "unknown BDM entry size";
        return false;
    }
    ids.clear();
    for (uint32_t i = 0; i < count; ++i) {
        int32_t id;
        file.Get(kBdmHeader + entrySize * i, id);
        if (id < 0 || id >= 0xffff) { note = "BDM id out of range"; return false; }
        ids.push_back((uint16_t)id);
    }
    return true;
}

// BAC layout (XV2-Tools Xv2CoreLib BAC/Parser.cs): entry count u32 +8,
// entry table i32 +16; entry 16 bytes: flags +0 (0x80000000 = empty), type
// count i16 +4, type list i32 +8; type list entry 16 bytes: type +0, record
// count +2, records i32 +8. Record sizes aren't stored: like Xv2CoreLib, a
// record array runs to the next thing in the file.
const size_t kBacEntrySize = 16, kBacTypeSize = 16;
const uint32_t kBacEmpty = 0x80000000u;

// Copies every entry `race` has and `out` (the fitted style BAC) lacks into
// `out` (ean_fit.h FitBacToRace). `added` = how many.
bool AddRaceEntries(std::vector<uint8_t>& out, const Reader& race, size_t& added, std::string& note)
{
    uint32_t raceCount, styleCount;
    int32_t raceTable, styleTable;
    Reader style{out.data(), out.size()};
    if (!race.Get(8, raceCount) || !race.Get(16, raceTable) || !race.Has((size_t)raceTable, kBacEntrySize * raceCount) ||
        !style.Get(8, styleCount) || !style.Get(16, styleTable) ||
        !style.Has((size_t)styleTable, kBacEntrySize * styleCount)) {
        note = "race BAC: entry table out of bounds";
        return false;
    }
    // Where each record array ends: the next type list, record array or
    // entry table start after it, or the end of the file.
    std::vector<size_t> starts{(size_t)raceTable, race.size};
    for (uint32_t e = 0; e < raceCount; ++e) {
        size_t entry = (size_t)raceTable + kBacEntrySize * e;
        int16_t typeCount = 0;
        int32_t typeList = 0;
        race.Get(entry + 4, typeCount);
        race.Get(entry + 8, typeList);
        if (typeCount <= 0 || !race.Has((size_t)typeList, kBacTypeSize * (size_t)typeCount)) continue;
        starts.push_back((size_t)typeList);
        for (int t = 0; t < typeCount; ++t) {
            int32_t records = 0;
            race.Get((size_t)typeList + kBacTypeSize * t + 8, records);
            if (records > 0) starts.push_back((size_t)records);
        }
    }
    std::sort(starts.begin(), starts.end());
    auto endOf = [&](size_t start) { return *std::upper_bound(starts.begin(), starts.end(), start); };

    uint32_t total = (std::max)(raceCount, styleCount);
    std::vector<uint8_t> table(kBacEntrySize * total, 0);
    memcpy(table.data(), out.data() + styleTable, kBacEntrySize * styleCount);
    for (uint32_t e = styleCount; e < total; ++e) Store<uint32_t>(table, kBacEntrySize * e, kBacEmpty);
    added = 0;
    for (uint32_t e = 0; e < raceCount; ++e) {
        size_t entry = (size_t)raceTable + kBacEntrySize * e;
        uint32_t raceFlags = 0;
        int16_t typeCount = 0;
        int32_t typeList = 0;
        race.Get(entry, raceFlags);
        race.Get(entry + 4, typeCount);
        race.Get(entry + 8, typeList);
        bool styleHas = !(Load<uint32_t>(table, kBacEntrySize * e) & kBacEmpty);
        if (styleHas || (raceFlags & kBacEmpty)) continue;
        // An entry without types that isn't flagged empty does nothing (NMC
        // 0, 1, 69, 72): copied as it is.
        if (typeCount <= 0 || !race.Has((size_t)typeList, kBacTypeSize * (size_t)typeCount)) {
            memcpy(table.data() + kBacEntrySize * e, race.data + entry, kBacEntrySize);
            Store<int16_t>(table, kBacEntrySize * e + 4, 0);
            Store<int32_t>(table, kBacEntrySize * e + 8, 0);
            ++added;
            continue;
        }
        Align16(out);
        size_t newTypeList = out.size();
        out.insert(out.end(), race.data + typeList, race.data + typeList + kBacTypeSize * typeCount);
        for (int t = 0; t < typeCount; ++t) {
            size_t typeAt = newTypeList + kBacTypeSize * t;
            int32_t records = Load<int32_t>(out, typeAt + 8);
            if (records <= 0 || (size_t)records >= race.size) {
                Store<int32_t>(out, typeAt + 8, 0);
                continue;
            }
            // keep the source's offset mod 16 (aligned reads, as with EAN keys)
            out.resize(out.size() + (((size_t)records - out.size()) & 15), 0);
            Store<int32_t>(out, typeAt + 8, (int32_t)out.size());
            out.insert(out.end(), race.data + records, race.data + endOf((size_t)records));
        }
        memcpy(table.data() + kBacEntrySize * e, race.data + entry, kBacEntrySize);
        Store<int32_t>(table, kBacEntrySize * e + 8, (int32_t)newTypeList);
        ++added;
    }
    if (total == styleCount) {
        memcpy(out.data() + styleTable, table.data(), table.size());
    } else {
        Align16(out);
        Store<int32_t>(out, 16, (int32_t)out.size());
        out.insert(out.end(), table.begin(), table.end());
        Store<uint32_t>(out, 8, total);
    }
    return true;
}

}  // namespace

bool FitBdmToRace(const uint8_t* styleData, size_t styleSize, const uint8_t* raceData, size_t raceSize,
                  std::vector<uint8_t>* out, std::map<uint16_t, uint16_t>& styleIds, std::string& note)
{
    Reader style{styleData, styleSize}, race{raceData, raceSize};
    std::vector<uint16_t> styleList, raceList;
    size_t styleEntry, raceEntry;
    if (!ParseBdm(style, styleList, styleEntry, note) || !ParseBdm(race, raceList, raceEntry, note)) return false;
    if (styleList.size() && raceList.size() && styleEntry != raceEntry) {
        note = "BDM entry sizes differ";
        return false;
    }
    size_t entrySize = raceList.empty() ? styleEntry : raceEntry;
    // A style id the race uses too moves after the highest id of both.
    uint16_t nextFree = 0;
    for (uint16_t id : styleList) nextFree = (std::max)(nextFree, (uint16_t)(id + 1));
    for (uint16_t id : raceList) nextFree = (std::max)(nextFree, (uint16_t)(id + 1));
    styleIds.clear();
    size_t moved = 0;
    for (uint16_t id : styleList) {
        bool taken = std::find(raceList.begin(), raceList.end(), id) != raceList.end();
        if (taken && nextFree >= 0xffff) { note = "BDM id overflow"; return false; }
        styleIds[id] = taken ? nextFree++ : id;
        moved += taken;
    }
    if (out) {
        // race header + entries, then the style's entries with their new ids
        out->assign(raceData, raceData + kBdmHeader + entrySize * raceList.size());
        for (size_t i = 0; i < styleList.size(); ++i) {
            size_t at = out->size();
            out->insert(out->end(), styleData + kBdmHeader + styleEntry * i,
                        styleData + kBdmHeader + styleEntry * (i + 1));
            Store<int32_t>(*out, at, styleIds[styleList[i]]);
        }
        Store<uint32_t>(*out, 8, (uint32_t)(raceList.size() + styleList.size()));
    }
    char text[120];
    snprintf(text, sizeof(text), "%zu style + %zu race damage entries, %zu style ids moved", styleList.size(),
             raceList.size(), moved);
    note = text;
    return true;
}

bool FitBacToRace(const uint8_t* bac, size_t size, const BacFit& fit, std::vector<uint8_t>& out, std::string& note)
{
    Reader file{bac, size};
    uint32_t magic, entryCount;
    int32_t entryTable;
    if (!file.Get(0, magic) || magic != 0x43414223 /* "#BAC" */ || !file.Get(8, entryCount) ||
        !file.Get(16, entryTable)) {
        note = "not a BAC";
        return false;
    }
    out.assign(bac, bac + size);
    size_t animations = 0, cameras = 0, hitboxes = 0, soundsMoved = 0, soundsDropped = 0;
    for (uint32_t e = 0; e < entryCount; ++e) {
        size_t entry = (size_t)entryTable + 16 * (size_t)e;
        uint32_t flags;
        int16_t typeCount;
        int32_t typeList;
        if (!file.Get(entry, flags) || !file.Get(entry + 4, typeCount) || !file.Get(entry + 8, typeList)) {
            note = "entry table out of bounds";
            return false;
        }
        if (flags & 0x80000000u) continue;  // empty entry
        for (int t = 0; t < typeCount; ++t) {
            size_t list = (size_t)typeList + 16 * (size_t)t;
            int16_t type, recordCount;
            int32_t records;
            if (!file.Get(list, type) || !file.Get(list + 2, recordCount) || !file.Get(list + 8, records)) {
                note = "type list out of bounds";
                return false;
            }
            if (records == 0) continue;
            size_t recordSize = type == 0 ? 36 : type == 1 ? 64 : type == 10 ? 76 : type == 11 ? 16 : 0;
            if (recordSize == 0) continue;
            if (!file.Has((size_t)records, recordSize * (size_t)(recordCount > 0 ? recordCount : 0))) {
                note = "record out of bounds";
                return false;
            }
            int16_t kept = 0;
            for (int r = 0; r < recordCount; ++r) {
                size_t record = (size_t)records + recordSize * (size_t)r;
                size_t target = (size_t)records + recordSize * (size_t)kept;
                if (target != record) memcpy(out.data() + target, bac + record, recordSize);
                if (type == 0 || type == 10) {
                    // animation / camera: style ids past the race's base set moved
                    size_t idAt = type == 0 ? 10 : 12;
                    uint16_t characterEan = type == 0 ? 5 : 4, eanType = Load<uint16_t>(out, target + 8),
                             id = Load<uint16_t>(out, target + idAt);
                    int base = type == 0 ? kRaceBaseAnimations : kRaceBaseCameras;
                    int shift = type == 0 ? fit.bodyShift : fit.cameraShift;
                    if (shift && eanType == characterEan && id >= base && id != 0xffff) {
                        if (id + shift >= 0xffff) { note = "animation id overflow"; return false; }
                        Store<uint16_t>(out, target + idAt, (uint16_t)(id + shift));
                        ++(type == 0 ? animations : cameras);
                    }
                } else if (type == 1) {
                    // hitbox on the character BDM: the style's entry under its merged id
                    uint8_t bdmFile = out[target + 19] & 0xf;
                    uint16_t id = Load<uint16_t>(out, target + 8);
                    auto mapped = fit.bdmIds.find(id);
                    if (bdmFile == kBdmCharacter && mapped != fit.bdmIds.end() && mapped->second != id) {
                        Store<uint16_t>(out, target + 8, mapped->second);
                        ++hitboxes;
                    }
                } else if (type == 11 && fit.characterSe != BacFit::CharacterSe::Keep &&
                           Load<uint16_t>(out, target + 8) == kAcbCharacterSe) {
                    // the style's cue under its id in the merged bank (cue id at +12)
                    auto mapped = fit.characterSeCues.find(Load<uint16_t>(out, target + 12));
                    if (fit.characterSe == BacFit::CharacterSe::Drop || mapped == fit.characterSeCues.end()) {
                        ++soundsDropped;
                        continue;  // dropped: the CaC's own sound bank has other cues under these ids
                    }
                    Store<uint16_t>(out, target + 12, mapped->second);
                    ++soundsMoved;
                }
                ++kept;
            }
            if (kept != recordCount) Store<int16_t>(out, list + 2, kept);
        }
    }
    size_t raceEntries = 0;
    if (fit.raceBac && !AddRaceEntries(out, Reader{fit.raceBac, fit.raceBacSize}, raceEntries, note)) return false;
    char text[300];
    snprintf(text, sizeof(text), "%zu animation refs moved up %d, %zu camera refs moved up %d, %zu hitboxes moved to "
             "their merged damage entry, %zu character sounds moved to their merged cue, %zu dropped; %zu race "
             "entries added", animations, fit.bodyShift, cameras, fit.cameraShift, hitboxes, soundsMoved,
             soundsDropped, raceEntries);
    note = text;
    return true;
}

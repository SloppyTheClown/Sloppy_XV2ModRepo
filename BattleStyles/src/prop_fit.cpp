#include "prop_fit.h"
#include "binary.h"
#include "ean_fit.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

// ESK layout (Xv2CoreLib ESK/ESK_File.cs; checked against the serializer's
// WIS_bsprop.esk, which works in game):
//   header (32 bytes): "#ESK" FE FF 1C 00, +8 version 0x9284, +16 skeleton offset
//   skeleton: +0 i16 bone count, +2 i16 flag, +4 index table (8 bytes per
//           bone: parent, child, sibling, IK flag - i16, -1 = none), +8 name
//           offset table, +12 relative transforms (48 bytes: position,
//           rotation quaternion xyzw, scale), +16 absolute transforms (64
//           bytes: the inverse of the bone's rest world matrix, row vectors),
//           +20 IK section, +24 extra info (8 bytes per bone), +28 u64
//           skeleton id; every offset relative to the skeleton
//
// EMD layout (Xv2CoreLib EMD/Parser.cs, EMD_File.cs):
//   header: "#EMD", +18 u16 model count, +20 model table, +24 model name
//           table (both absolute; table entries absolute, 0 = none)
//   model:  +2 u16 mesh count, +4 mesh table (relative to the model, and so
//           are its entries)
//   mesh:   +0 bounds (48 bytes: center, min, max - xyzw each), +48 name,
//           +54 u16 submesh count, +56 submesh table (all relative to the mesh)
//   submesh: +0 bounds, +48 vertex flags, +52 vertex size, +56 vertex count,
//           +60 vertices, +70 u16 triangle list count, +76 triangle list
//           table (all relative to the submesh)
//   triangle list: +0 index count (u32 indices when over 65535, else u16),
//           +4 bone name count, +8 indices (0 = right after the 16-byte
//           header), +12 bone name table (all relative to the list); a
//           vertex's blend indices pick from its list's bone names
//   vertex: position (3 floats), then by flag: normal, UV, UV2, tangent,
//           color, blend (4 index bytes + 3 weights, the 4th is 1 - sum);
//           flag 0x8000 stores normal, UVs, tangent and weights as halves
//
// BCS layout (Xv2CoreLib BCS/Parser.cs):
//   header: +12 u16 part set count, +24 part set table (absolute, 0 = none)
//   part set: +20 part count (10), +24 part table (relative to the part set;
//           its entries too, 0 = none) in the order of kPartNames
//   part:   +0 i16 model (-1 = none), +2 i16 model2 (the EMB's number), +52
//           chara code (4 bytes), +56/60/64 EMD/EMM/EMB name (relative to
//           the part, 0 = by number), +74 i16 physics object count, +76
//           physics object array (relative to the part)
//   physics object (72 bytes): +0 i16 model1, +2 i16 model2, +4 i16 texture,
//           +24 flags, +36 chara code (4 bytes, its files' folder), +40..+60
//           EMD, EMM, EMB, EAN, bone to attach, SCD names (relative to the
//           object, 0 = none)
namespace {

using binary::AppendOwnBytes;
using binary::AppendString;
using binary::Align16;
using binary::FloatToHalf;
using binary::HalfToFloat;
using binary::Load;
using binary::Reader;
using binary::Store;

const uint32_t kEmdSignature = 0x444d4523;  // "#EMD"
const uint32_t kEskSignature = 0x4b534523;  // "#ESK"

// ------------------------------------------------------------- matrices ---
// Row vectors, as the ESK stores them: p' = [p, 1] * M, translation in row 3.
struct Matrix {
    double m[4][4];
};

Matrix Identity()
{
    Matrix result{};
    for (int i = 0; i < 4; ++i) result.m[i][i] = 1;
    return result;
}

// `a` then `b`: [p] * a * b.
Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) result.m[i][j] += a.m[i][k] * b.m[k][j];
    return result;
}

// Gauss-Jordan with partial pivoting; false for a singular matrix.
bool Invert(const Matrix& in, Matrix& out)
{
    Matrix work = in;
    out = Identity();
    for (int column = 0; column < 4; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 4; ++row)
            if (std::fabs(work.m[row][column]) > std::fabs(work.m[pivot][column])) pivot = row;
        if (std::fabs(work.m[pivot][column]) < 1e-12) return false;
        std::swap(work.m[pivot], work.m[column]);
        std::swap(out.m[pivot], out.m[column]);
        const double scale = 1 / work.m[column][column];
        for (int k = 0; k < 4; ++k) { work.m[column][k] *= scale; out.m[column][k] *= scale; }
        for (int row = 0; row < 4; ++row) {
            if (row == column) continue;
            const double factor = work.m[row][column];
            for (int k = 0; k < 4; ++k) {
                work.m[row][k] -= factor * work.m[column][k];
                out.m[row][k] -= factor * out.m[column][k];
            }
        }
    }
    return true;
}

// 16 floats, row by row (an ESK absolute transform).
Matrix ReadMatrix(const uint8_t* floats)
{
    Matrix result;
    for (int i = 0; i < 16; ++i) {
        float value;
        memcpy(&value, floats + 4 * i, 4);
        result.m[i / 4][i % 4] = value;
    }
    return result;
}

void AppendMatrix(std::vector<uint8_t>& out, const Matrix& matrix)
{
    for (int i = 0; i < 16; ++i) {
        float value = (float)matrix.m[i / 4][i % 4];
        const uint8_t* bytes = (const uint8_t*)&value;
        out.insert(out.end(), bytes, bytes + 4);
    }
}

// An ESK relative transform (position, quaternion xyzw, scale; 48 bytes) as
// a matrix: scale * rotation, then the position
// (ClaudeTools\battle_styles\esk.py rel), its position times `positionScale`.
Matrix RelativeMatrix(const uint8_t* transform, double positionScale = 1)
{
    float values[12];
    memcpy(values, transform, sizeof(values));
    for (int j = 0; j < 3; ++j) values[j] = (float)(values[j] * positionScale);
    const double x = values[4], y = values[5], z = values[6], w = values[7];
    const double rotation[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w)},
                                   {2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w)},
                                   {2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)}};
    Matrix result{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) result.m[i][j] = values[8 + i] * rotation[i][j];
    for (int j = 0; j < 3; ++j) result.m[3][j] = values[j];
    result.m[3][3] = 1;
    return result;
}

// ------------------------------------------------------------ skeletons ---
struct Skeleton {
    size_t base = 0;  // skeleton header, absolute; the rest absolute too (0 = none)
    size_t indexTable = 0, nameTable = 0, relative = 0, absolute = 0, extraInfo = 0;
    std::vector<std::string> names;
    std::vector<int> parents;
    std::map<std::string, int> index;
};

bool ParseEsk(const Reader& file, Skeleton& skeleton)
{
    uint32_t signature;
    int32_t base, indexTable, nameTable, relative, absolute = 0, extraInfo = 0;
    int16_t count;
    if (!file.Get(0, signature) || signature != kEskSignature || !file.Get(16, base) || base <= 0 ||
        !file.Get(base, count) || count <= 0 || !file.Get(base + 4, indexTable) || !file.Get(base + 8, nameTable) ||
        !file.Get(base + 12, relative) || !file.Get(base + 16, absolute) || !file.Get(base + 24, extraInfo))
        return false;
    skeleton.base = base;
    skeleton.indexTable = (size_t)base + indexTable;
    skeleton.nameTable = (size_t)base + nameTable;
    skeleton.relative = (size_t)base + relative;
    if (absolute > 0) skeleton.absolute = (size_t)base + absolute;
    if (extraInfo > 0) skeleton.extraInfo = (size_t)base + extraInfo;
    if (!file.Has(skeleton.indexTable, 8 * (size_t)count) || !file.Has(skeleton.nameTable, 4 * (size_t)count) ||
        !file.Has(skeleton.relative, 48 * (size_t)count) ||
        (skeleton.absolute && !file.Has(skeleton.absolute, 64 * (size_t)count)) ||
        (skeleton.extraInfo && !file.Has(skeleton.extraInfo, 8 * (size_t)count)))
        return false;
    for (int i = 0; i < count; ++i) {
        int16_t parent;
        int32_t nameAt;
        file.Get(skeleton.indexTable + 8 * (size_t)i, parent);
        file.Get(skeleton.nameTable + 4 * (size_t)i, nameAt);
        skeleton.parents.push_back(parent);
        skeleton.names.push_back(file.Name((size_t)base + nameAt));
        skeleton.index.emplace(skeleton.names.back(), i);
    }
    return true;
}

// Shoulder to wrist: the rest lengths of b_R_Arm1, b_R_Arm2 and b_R_Hand; 0
// when one is missing. Props are sized by race / style (ean_fit.cpp
// ArmLength, which measures the same on the EAN, says why).
double ArmLength(const Reader& file, const Skeleton& skeleton)
{
    double total = 0;
    for (const char* name : {"b_R_Arm1", "b_R_Arm2", "b_R_Hand"}) {
        auto found = skeleton.index.find(name);
        float position[3];
        if (found == skeleton.index.end() || !file.Has(skeleton.relative + 48 * (size_t)found->second, 12)) return 0;
        memcpy(position, file.data + skeleton.relative + 48 * (size_t)found->second, sizeof(position));
        total += std::sqrt(position[0] * position[0] + position[1] * position[1] + position[2] * position[2]);
    }
    return total;
}

// Prop bones: the style character's "a_x_*" bones the race lacks, each
// under a bone the race has or an earlier prop bone - the same choice as the
// fitted EAN's (ean_fit.cpp IsPropBone), so its tracks find them by name.
bool IsPropBone(const std::string& name) { return name.compare(0, 4, "a_x_") == 0; }

struct PropBone {
    int styleBone;  // index in the style skeleton
    int parent;     // index in the fitted skeleton
};

std::vector<PropBone> ChoosePropBones(const Skeleton& race, const Skeleton& style)
{
    std::vector<PropBone> props;
    std::vector<int> fitted(style.names.size(), -1);  // style bone -> fitted bone
    for (size_t i = 0; i < style.names.size(); ++i) {
        auto known = race.index.find(style.names[i]);
        if (known != race.index.end()) {
            fitted[i] = known->second;
            continue;
        }
        const int parent = style.parents[i];
        if (!IsPropBone(style.names[i]) || parent < 0 || parent >= (int)i || fitted[parent] < 0 ||
            race.names.size() + props.size() >= 0x7fff)
            continue;
        fitted[i] = (int)(race.names.size() + props.size());
        props.push_back({(int)i, fitted[parent]});
    }
    return props;
}

// Rebuilds the race skeleton's tables at the end of `out` (a copy of the race
// ESK) with `props` after the race's bones (race indices unchanged, the IK
// section stays where it is); the skeleton header's offsets are relative to
// it. A prop bone keeps the style's rest transform under its parent, its
// position sized by PropBonePositionScale (prop_fit.h), and its absolute
// transform is the inverse of rest * parent's rest world.
bool AppendEskBones(std::vector<uint8_t>& out, const Skeleton& race, const Reader& styleFile, const Skeleton& style,
                    const std::vector<PropBone>& props, double armRatio, const PropPlacement& back)
{
    const size_t raceCount = race.names.size(), count = raceCount + props.size();
    auto relative = [&](size_t at) { return (int32_t)(at - race.base); };

    Align16(out);
    const size_t indexTable = out.size();
    AppendOwnBytes(out, race.indexTable, 8 * raceCount);
    out.resize(indexTable + 8 * count, 0);
    auto link = [&](size_t bone, int field) { return indexTable + 8 * bone + 2 * field; };  // 0 parent, 1 child, 2 sibling, 3 IK flag
    for (size_t k = 0; k < props.size(); ++k) {
        const size_t bone = raceCount + k;
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
    const size_t nameTable = out.size();
    out.resize(nameTable + 4 * count, 0);
    memcpy(out.data() + nameTable, out.data() + race.nameTable, 4 * raceCount);
    for (size_t k = 0; k < props.size(); ++k)
        Store<int32_t>(out, nameTable + 4 * (raceCount + k),
                       relative(AppendString(out, style.names[props[k].styleBone])));

    Align16(out);
    const size_t transforms = out.size();
    AppendOwnBytes(out, race.relative, 48 * raceCount);
    std::vector<size_t> placed;  // where each prop bone's placed rest transform is in `out`
    for (const PropBone& prop : props) {
        const uint8_t* from = styleFile.data + style.relative + 48 * (size_t)prop.styleBone;
        const size_t at = out.size();
        out.insert(out.end(), from, from + 48);
        const double scale = PropBonePositionScale(style.names[prop.styleBone], armRatio, back.SizeRatio(armRatio));
        const int styleParent = style.parents[prop.styleBone];
        const bool backBone = IsBackPropBone(style.names[styleParent], Load<float>(out, at + 8));
        for (int axis = 0; axis < 3; ++axis)
            Store<float>(out, at + 4 * axis, (float)PlacePropAxis(axis, Load<float>(out, at + 4 * axis), scale, backBone, back));
        placed.push_back(at);
    }

    size_t absolute = 0;
    if (race.absolute) {
        std::vector<Matrix> propWorlds;  // rest world matrices of the prop bones
        for (size_t k = 0; k < props.size(); ++k) {
            const PropBone& prop = props[k];
            Matrix parentWorld;
            if (prop.parent < (int)raceCount) {
                if (!Invert(ReadMatrix(out.data() + race.absolute + 64 * (size_t)prop.parent), parentWorld))
                    return false;
            } else {
                parentWorld = propWorlds[prop.parent - raceCount];
            }
            propWorlds.push_back(Multiply(RelativeMatrix(out.data() + placed[k]), parentWorld));
        }
        Align16(out);
        absolute = out.size();
        AppendOwnBytes(out, race.absolute, 64 * raceCount);
        for (const Matrix& world : propWorlds) {
            Matrix inverse;
            if (!Invert(world, inverse)) return false;
            AppendMatrix(out, inverse);
        }
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

    Store<int16_t>(out, race.base, (int16_t)count);
    Store<int32_t>(out, race.base + 4, relative(indexTable));
    Store<int32_t>(out, race.base + 8, relative(nameTable));
    Store<int32_t>(out, race.base + 12, relative(transforms));
    if (absolute) Store<int32_t>(out, race.base + 16, relative(absolute));
    if (extraInfo) Store<int32_t>(out, race.base + 24, relative(extraInfo));
    return true;
}

// A bone a prop mesh skeleton adds under b_C_Base: one of a multi-bone
// prop's bones besides the one it is attached to (Goku Mini's pole pieces).
struct PropMeshBone {
    std::string name;
    int parent;           // index in the prop mesh skeleton (1 = b_C_Base)
    float relative[12];   // rest transform under it: position, quaternion xyzw, scale
};

// The skeleton next to a prop mesh: EXP_<name> > {b_C_Base, x_x_SkinRoot >
// c_<name>}, all at rest at the origin - the layout of the serializer's
// WIS_bsprop.esk (ClaudeTools\battle_styles\prop_test1.py prop_esk, after the
// Clown Nose's nose_esk), which works in game - then `extra` (index 4 on),
// absolute transform = inverse of its rest world under b_C_Base. b_C_Base
// sits on the bone the object is attached to; the extra bones carry the
// character skeleton's names, so its animation can move them (an unproven
// guess - without that they stay at rest and the mesh is rigid).
std::vector<uint8_t> PropMeshSkeleton(const std::string& name, const std::vector<PropMeshBone>& extra)
{
    const size_t base = 0x20, header = 0x24;  // skeleton header: count, flag, six offsets, u64 id
    std::vector<std::string> names = {"EXP_" + name, "b_C_Base", "x_x_SkinRoot", "c_" + name};
    std::vector<int> parents = {-1, 0, 0, 2};
    const float rest[12] = {0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1};  // position, rotation, scale
    std::vector<const float*> relatives(names.size(), rest);
    std::vector<Matrix> worlds(names.size(), Identity());
    for (const PropMeshBone& bone : extra) {
        names.push_back(bone.name);
        parents.push_back(bone.parent);
        relatives.push_back(bone.relative);
        worlds.push_back(Multiply(RelativeMatrix((const uint8_t*)bone.relative), worlds[bone.parent]));
    }
    const int count = (int)names.size();

    std::vector<uint8_t> out(base + header, 0);
    memcpy(out.data(), "#ESK\xfe\xff\x1c\x00", 8);
    Store<uint32_t>(out, 8, 0x9284);  // version
    Store<int32_t>(out, 16, (int32_t)base);
    Store<int16_t>(out, base, (int16_t)count);
    Store<int16_t>(out, base + 2, 1);               // flag
    Store<uint64_t>(out, base + 28, 0x5b5b0001);    // skeleton id

    // index table: parent, first child, next sibling, IK flag
    const size_t indexTable = out.size();
    Store<int32_t>(out, base + 4, (int32_t)(indexTable - base));
    out.resize(indexTable + 8 * (size_t)count, 0);
    for (int i = 0; i < count; ++i) {
        int16_t child = -1, sibling = -1;
        for (int j = 0; j < count && child < 0; ++j)
            if (parents[j] == i) child = (int16_t)j;
        for (int j = i + 1; j < count && sibling < 0; ++j)
            if (parents[j] == parents[i] && parents[i] >= 0) sibling = (int16_t)j;
        const int16_t links[4] = {(int16_t)parents[i], child, sibling, 0};
        memcpy(out.data() + indexTable + 8 * (size_t)i, links, sizeof(links));
    }
    const size_t nameTable = out.size();
    Store<int32_t>(out, base + 8, (int32_t)(nameTable - base));
    out.resize(nameTable + 4 * (size_t)count, 0);
    for (int i = 0; i < count; ++i)
        Store<int32_t>(out, nameTable + 4 * (size_t)i, (int32_t)(AppendString(out, names[i]) - base));

    Align16(out);
    Store<int32_t>(out, base + 12, (int32_t)(out.size() - base));
    for (const float* relative : relatives)
        out.insert(out.end(), (const uint8_t*)relative, (const uint8_t*)relative + 48);
    Store<int32_t>(out, base + 16, (int32_t)(out.size() - base));
    for (const Matrix& world : worlds) {
        Matrix inverse;
        if (!Invert(world, inverse)) inverse = Identity();
        AppendMatrix(out, inverse);
    }
    Store<int32_t>(out, base + 24, (int32_t)(out.size() - base));
    const uint8_t extraInfo[8] = {0, 0, 0, 0, 0xff, 0xff, 0, 0};
    for (int i = 0; i < count; ++i) out.insert(out.end(), extraInfo, extraInfo + 8);
    return out;
}

// --------------------------------------------------------------- meshes ---
const size_t kNone = (size_t)-1;

struct VertexLayout {
    size_t size = 0, normal = kNone, tangent = kNone, blend = kNone;
    bool compressed = false;
};

// Field offsets in one vertex (Xv2CoreLib EMD_Vertex.ReadVertices); false
// without positions or blend weights.
bool LayoutOf(uint32_t flags, VertexLayout& layout)
{
    const bool compressed = (flags & 0x8000) != 0;
    if (!(flags & 0x1)) return false;  // position
    size_t at = 12;
    if (flags & 0x2) { layout.normal = at; at += compressed ? 8 : 12; }
    if (flags & 0x4) at += compressed ? 4 : 8;  // UV
    if (flags & 0x8) at += compressed ? 4 : 8;  // UV2
    if (flags & 0x80) { layout.tangent = at; at += compressed ? 8 : 12; }
    if (flags & 0x40) at += 4;  // color
    if (flags & 0x200) { layout.blend = at; at += 4 + (compressed ? 8 : 12); }
    layout.size = at;
    layout.compressed = compressed;
    return layout.blend != kNone;
}

// The blend slot (index into its triangle list's bone names) with the most
// weight on the vertex at `vertex`.
int DominantSlot(const uint8_t* vertex, const VertexLayout& layout)
{
    const uint8_t* blend = vertex + layout.blend;
    float weights[4];
    for (int i = 0; i < 3; ++i) {
        if (layout.compressed) {
            uint16_t half;
            memcpy(&half, blend + 4 + 2 * i, 2);
            weights[i] = HalfToFloat(half);
        } else {
            memcpy(&weights[i], blend + 4 + 4 * i, 4);
        }
    }
    weights[3] = 1 - (weights[0] + weights[1] + weights[2]);
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (weights[i] > weights[best]) best = i;
    return blend[best];
}

// Three floats or halves at `at`.
void ReadVector(const uint8_t* at, bool compressed, double (&value)[3])
{
    for (int i = 0; i < 3; ++i) {
        if (compressed) {
            uint16_t half;
            memcpy(&half, at + 2 * i, 2);
            value[i] = HalfToFloat(half);
        } else {
            float single;
            memcpy(&single, at + 4 * i, 4);
            value[i] = single;
        }
    }
}

void WriteVector(uint8_t* at, bool compressed, const double (&value)[3])
{
    for (int i = 0; i < 3; ++i) {
        if (compressed) {
            uint16_t half = FloatToHalf((float)value[i]);
            memcpy(at + 2 * i, &half, 2);
        } else {
            float single = (float)value[i];
            memcpy(at + 4 * i, &single, 4);
        }
    }
}

// Moves `count` vertices from model space into a bone's space: positions by
// `toBone` (the bone's absolute transform in the style skeleton) and then
// times `meshScale` (PropMeshScale), normals and tangents by its rotation,
// renormalised.
void MoveVertices(std::vector<uint8_t>& out, size_t vertices, size_t count, const VertexLayout& layout,
                  const Matrix& toBone, double meshScale)
{
    for (size_t v = 0; v < count; ++v) {
        uint8_t* vertex = out.data() + vertices + v * layout.size;
        double position[3], moved[3];
        ReadVector(vertex, false, position);
        for (int j = 0; j < 3; ++j)
            moved[j] = (position[0] * toBone.m[0][j] + position[1] * toBone.m[1][j] + position[2] * toBone.m[2][j] +
                        toBone.m[3][j]) * meshScale;
        WriteVector(vertex, false, moved);
        for (size_t field : {layout.normal, layout.tangent}) {
            if (field == kNone) continue;
            double direction[3], turned[3];
            ReadVector(vertex + field, layout.compressed, direction);
            for (int j = 0; j < 3; ++j)
                turned[j] = direction[0] * toBone.m[0][j] + direction[1] * toBone.m[1][j] + direction[2] * toBone.m[2][j];
            const double length = std::sqrt(turned[0] * turned[0] + turned[1] * turned[1] + turned[2] * turned[2]);
            if (length > 0)
                for (double& component : turned) component /= length;
            WriteVector(vertex + field, layout.compressed, turned);
        }
    }
}

struct Bounds {
    double low[3] = {INFINITY, INFINITY, INFINITY}, high[3] = {-INFINITY, -INFINITY, -INFINITY};
    bool Empty() const { return low[0] > high[0]; }
    void Add(const double (&point)[3])
    {
        for (int i = 0; i < 3; ++i) { low[i] = std::min(low[i], point[i]); high[i] = std::max(high[i], point[i]); }
    }
    void Add(const Bounds& other)
    {
        if (other.Empty()) return;
        Add(other.low);
        Add(other.high);
    }
};

// An EMD bounds block (48 bytes at `at`): center (w = half the diagonal),
// min (w = 0), max (w = the diagonal), as prop_test1.py set_aabb wrote them.
void WriteBounds(std::vector<uint8_t>& out, size_t at, const Bounds& bounds)
{
    if (bounds.Empty()) return;
    double diagonal = 0;
    for (int i = 0; i < 3; ++i) diagonal += (bounds.high[i] - bounds.low[i]) * (bounds.high[i] - bounds.low[i]);
    diagonal = std::sqrt(diagonal);
    float values[12];
    for (int i = 0; i < 3; ++i) {
        values[i] = (float)((bounds.low[i] + bounds.high[i]) / 2);
        values[4 + i] = (float)bounds.low[i];
        values[8 + i] = (float)bounds.high[i];
    }
    values[3] = (float)(diagonal / 2);
    values[7] = 0;
    values[11] = (float)diagonal;
    memcpy(out.data() + at, values, sizeof(values));
}

// The bone a triangle belongs to: the dominant blend bone (DominantSlot) of
// two of its three corners, else of the first.
int TriangleOwner(const int (&owner)[3]) { return owner[1] == owner[2] ? owner[1] : owner[0]; }

// One triangle for ForEachTriangle: its triangle list's bone names, the
// dominant blend slot (DominantSlot) of each corner, the corners' vertices
// (position first: 3 floats) and which submesh of the file it is in (its
// number, and its offset in the file as CutPropMesh sees it).
struct TriangleCorners {
    const std::vector<std::string>* boneNames;
    int owner[3];
    const uint8_t* vertex[3];
    size_t submesh;
    size_t submeshAt;
};

// Calls `visit` for every triangle of every triangle list in `emd` with
// blend weights.
void ForEachTriangle(const Reader& file, const std::function<void(const TriangleCorners&)>& visit)
{
    size_t submeshNumber = 0;
    uint16_t modelCount;
    int32_t modelTable;
    if (!file.Get(18, modelCount) || !file.Get(20, modelTable) || !file.Has(modelTable, 4 * (size_t)modelCount))
        return;
    for (size_t m = 0; m < modelCount; ++m) {
        int32_t model = 0, meshTable;
        uint16_t meshCount;
        file.Get(modelTable + 4 * m, model);
        if (model <= 0 || !file.Get(model + 2, meshCount) || !file.Get(model + 4, meshTable) ||
            !file.Has((size_t)model + meshTable, 4 * (size_t)meshCount))
            continue;
        for (size_t a = 0; a < meshCount; ++a) {
            int32_t meshAt, submeshTable;
            uint16_t submeshCount;
            file.Get(model + meshTable + 4 * a, meshAt);
            const size_t mesh = (size_t)model + meshAt;
            if (!file.Has(mesh, 60) || !file.Get(mesh + 54, submeshCount) || !file.Get(mesh + 56, submeshTable) ||
                !file.Has(mesh + submeshTable, 4 * (size_t)submeshCount))
                continue;
            for (size_t s = 0; s < submeshCount; ++s) {
                int32_t submeshAt, vertexSize, vertexCount, vertexAt, listTable;
                uint32_t flags;
                uint16_t listCount;
                file.Get(mesh + submeshTable + 4 * s, submeshAt);
                const size_t submesh = mesh + submeshAt;
                VertexLayout layout;
                if (!file.Has(submesh, 80) || !file.Get(submesh + 48, flags) || !file.Get(submesh + 52, vertexSize) ||
                    !file.Get(submesh + 56, vertexCount) || !file.Get(submesh + 60, vertexAt) ||
                    !file.Get(submesh + 70, listCount) || !file.Get(submesh + 76, listTable) ||
                    !LayoutOf(flags, layout) || (int32_t)layout.size != vertexSize || vertexCount <= 0 ||
                    !file.Has(submesh + vertexAt, layout.size * (size_t)vertexCount) ||
                    !file.Has(submesh + listTable, 4 * (size_t)listCount))
                    continue;
                const uint8_t* vertices = file.data + submesh + vertexAt;
                ++submeshNumber;
                for (size_t t = 0; t < listCount; ++t) {
                    int32_t listAt, indexCount, boneCount, indicesAt, boneTable;
                    file.Get(submesh + listTable + 4 * t, listAt);
                    const size_t list = submesh + listAt;
                    if (!file.Get(list, indexCount) || !file.Get(list + 4, boneCount) ||
                        !file.Get(list + 8, indicesAt) || !file.Get(list + 12, boneTable) || indexCount <= 0 ||
                        boneCount <= 0 || !file.Has(list + boneTable, 4 * (size_t)boneCount))
                        continue;
                    const size_t indices = indicesAt ? list + indicesAt : list + 16;
                    const bool wide = indexCount > 0xffff;
                    if (!file.Has(indices, (size_t)indexCount * (wide ? 4 : 2))) continue;
                    std::vector<std::string> boneNames;
                    for (int b = 0; b < boneCount; ++b) {
                        int32_t nameAt;
                        file.Get(list + boneTable + 4 * (size_t)b, nameAt);
                        boneNames.push_back(file.Name(list + nameAt));
                    }
                    TriangleCorners triangle{&boneNames, {}, {}, submeshNumber, submesh};
                    for (int32_t f = 0; f + 2 < indexCount; f += 3) {
                        int(&owner)[3] = triangle.owner;
                        bool valid = true;
                        for (int c = 0; c < 3 && valid; ++c) {
                            uint32_t corner;
                            if (wide) {
                                file.Get(indices + 4 * (size_t)(f + c), corner);
                            } else {
                                uint16_t narrow;
                                file.Get(indices + 2 * (size_t)(f + c), narrow);
                                corner = narrow;
                            }
                            valid = corner < (uint32_t)vertexCount;
                            if (!valid) break;
                            triangle.vertex[c] = vertices + corner * layout.size;
                            owner[c] = DominantSlot(triangle.vertex[c], layout);
                            valid = owner[c] < boneCount;
                        }
                        if (valid) visit(triangle);
                    }
                }
            }
        }
    }
}

// The prop bones of `emd` grouped by the triangles that join them: a
// triangle whose corners belong to a shape bone (IsPropShapeBone) and
// another prop bone puts both in one group. Only shape bones: Trunks's
// a_x_sword3 shares triangles with a_x_sword2 too but is drawn out of the
// sheath (tracks in 80 of 82 TRX animations), so it stays an object of its
// own that follows its bone - proven in game, unlike a skinned bone of a
// physics object's skeleton.
// Goku Mini's pole is one mesh skinned to seven bones that slide apart
// (a_x_nyb_root2 and front1-3 / back1-3; in NTG_BAS_STAND front2/3 sit
// 0.2-0.4 m out, rest 0.1): cut per bone, each piece was rigid and the pole
// fell apart into a middle piece with spikes (build 10 in game). A group is
// one physics object, skinned to all its bones (PropMeshSkeleton).
std::vector<std::set<std::string>> PropBoneGroups(const std::vector<uint8_t>& emd, const std::set<std::string>& propBones)
{
    std::map<std::string, std::string> leader;  // union-find over bone names
    std::function<std::string(const std::string&)> find = [&](const std::string& bone) {
        auto found = leader.find(bone);
        if (found == leader.end() || found->second == bone) return bone;
        return found->second = find(found->second);
    };
    std::set<std::string> used;
    ForEachTriangle(Reader{emd.data(), emd.size()}, [&](const TriangleCorners& triangle) {
        const std::vector<std::string>& names = *triangle.boneNames;
        const int(&owner)[3] = triangle.owner;
        if (!propBones.count(names[TriangleOwner(owner)])) return;
        for (int c = 0; c < 3; ++c)
            if (propBones.count(names[owner[c]])) used.insert(names[owner[c]]);
        for (int c = 0; c < 3; ++c)
            for (int d = c + 1; d < 3; ++d) {
                const std::string &one = names[owner[c]], &other = names[owner[d]];
                if (one != other && propBones.count(one) && propBones.count(other) &&
                    (IsPropShapeBone(one) || IsPropShapeBone(other)))
                    leader[find(one)] = find(other);
            }
    });
    std::map<std::string, std::set<std::string>> groups;
    for (const std::string& bone : used) groups[find(bone)].insert(bone);
    std::vector<std::set<std::string>> result;
    for (auto& group : groups) result.push_back(std::move(group.second));
    return result;
}

// Hand-held props without a prop bone: a submesh skinned to nothing but one
// hand bone and longer than kHandPropLength of the style's arm. Kaido's
// club (x2m "Kaido" by TGFB, K7H_000_Face_ear: two "Club" submeshes, 2.35 m
// and 0.87 m against a 1.3 m arm) is skinned straight to b_R_Hand - his
// skeleton has no a_x_* bone - so ChoosePropBones finds nothing. A hand or
// glove has finger bones and is far shorter than half an arm.
const double kHandPropLength = 0.5;
const double kHandPropShare = 0.99;  // of the submesh's triangles owned by the hand bone

bool IsHandBone(const std::string& bone) { return bone == "b_R_Hand" || bone == "b_L_Hand"; }

// The hand-held prop submeshes of `emd` (offsets, as CutPropMesh sees them)
// by the hand bone they hang on.
std::map<std::string, std::set<size_t>> HandPropSubmeshes(const std::vector<uint8_t>& emd, double styleArm)
{
    struct Submesh {
        std::map<std::string, size_t> trianglesOf;  // owner bone -> triangles
        size_t triangles = 0;
        Bounds bounds;
    };
    std::map<size_t, Submesh> submeshes;
    ForEachTriangle(Reader{emd.data(), emd.size()}, [&](const TriangleCorners& triangle) {
        Submesh& submesh = submeshes[triangle.submeshAt];
        ++submesh.triangles;
        ++submesh.trianglesOf[(*triangle.boneNames)[TriangleOwner(triangle.owner)]];
        for (const uint8_t* vertex : triangle.vertex) {
            float position[3];
            memcpy(position, vertex, sizeof(position));
            const double point[3] = {position[0], position[1], position[2]};
            submesh.bounds.Add(point);
        }
    });
    std::map<std::string, std::set<size_t>> found;
    if (styleArm <= 0.05) return found;
    for (const auto& [at, submesh] : submeshes) {
        double longest = 0;
        for (int axis = 0; axis < 3; ++axis) longest = std::max(longest, submesh.bounds.high[axis] - submesh.bounds.low[axis]);
        for (const auto& [bone, triangles] : submesh.trianglesOf)
            if (IsHandBone(bone) && triangles >= kHandPropShare * (double)submesh.triangles &&
                longest > kHandPropLength * styleArm)
                found[bone].insert(at);
    }
    return found;
}

// Cuts `emd` down to the triangles that belong (TriangleOwner) to a bone in
// `group` (only in the submeshes at `onlySubmeshes` when given), moved into the space of the bone the object hangs on (`toBone`,
// sized by `meshScale`). Blend bones in `skinned` (the prop mesh skeleton's
// own bones) keep their names, every other one becomes b_C_Base, the bone
// the object hangs on. Edits a copy in place: tables and counts are
// compacted, kept indices rewritten from the start of their array; names are
// added at the end. False (out unusable) when no triangle is left.
bool CutPropMesh(const std::vector<uint8_t>& emd, const std::set<std::string>& group,
                 const std::set<std::string>& skinned, const Matrix& toBone, double meshScale,
                 const std::string& name, std::vector<uint8_t>& out, size_t& triangles,
                 const std::set<size_t>* onlySubmeshes = nullptr)
{
    const Reader file{emd.data(), emd.size()};
    uint32_t signature;
    uint16_t modelCount;
    int32_t modelTable, modelNames;
    triangles = 0;
    if (!file.Get(0, signature) || signature != kEmdSignature || !file.Get(18, modelCount) ||
        !file.Get(20, modelTable) || !file.Get(24, modelNames) || !file.Has(modelTable, 4 * (size_t)modelCount) ||
        (modelNames && !file.Has(modelNames, 4 * (size_t)modelCount)))
        return false;
    out = emd;
    const size_t baseBoneName = AppendString(out, "b_C_Base");
    const size_t ownName = AppendString(out, name);
    std::set<size_t> movedBuffers;  // vertex buffers already in bone space
    uint16_t keptModels = 0;
    for (size_t m = 0; m < modelCount; ++m) {
        int32_t model = 0, meshTable;
        uint16_t meshCount;
        file.Get(modelTable + 4 * m, model);
        if (model <= 0 || !file.Get(model + 2, meshCount) || !file.Get(model + 4, meshTable) ||
            !file.Has((size_t)model + meshTable, 4 * (size_t)meshCount))
            continue;
        uint16_t keptMeshes = 0;
        for (size_t a = 0; a < meshCount; ++a) {
            int32_t meshAt, submeshTable;
            uint16_t submeshCount;
            file.Get(model + meshTable + 4 * a, meshAt);
            const size_t mesh = (size_t)model + meshAt;
            if (!file.Has(mesh, 60) || !file.Get(mesh + 54, submeshCount) || !file.Get(mesh + 56, submeshTable) ||
                !file.Has(mesh + submeshTable, 4 * (size_t)submeshCount))
                continue;
            Bounds meshBounds;
            uint16_t keptSubmeshes = 0;
            for (size_t s = 0; s < submeshCount; ++s) {
                int32_t submeshAt, vertexSize, vertexCount, vertexAt, listTable;
                uint32_t flags;
                uint16_t listCount;
                file.Get(mesh + submeshTable + 4 * s, submeshAt);
                const size_t submesh = mesh + submeshAt;
                VertexLayout layout;
                if ((onlySubmeshes && !onlySubmeshes->count(submesh)) || !file.Has(submesh, 80) ||
                    !file.Get(submesh + 48, flags) || !file.Get(submesh + 52, vertexSize) ||
                    !file.Get(submesh + 56, vertexCount) || !file.Get(submesh + 60, vertexAt) ||
                    !file.Get(submesh + 70, listCount) || !file.Get(submesh + 76, listTable) ||
                    !LayoutOf(flags, layout) || (int32_t)layout.size != vertexSize || vertexCount <= 0)
                    continue;
                const size_t vertices = submesh + vertexAt;
                if (!file.Has(vertices, layout.size * (size_t)vertexCount) ||
                    !file.Has(submesh + listTable, 4 * (size_t)listCount))
                    continue;
                std::vector<bool> used(vertexCount, false);
                uint16_t keptLists = 0;
                for (size_t t = 0; t < listCount; ++t) {
                    int32_t listAt, indexCount, boneCount, indicesAt, boneTable;
                    file.Get(submesh + listTable + 4 * t, listAt);
                    const size_t list = submesh + listAt;
                    if (!file.Get(list, indexCount) || !file.Get(list + 4, boneCount) ||
                        !file.Get(list + 8, indicesAt) || !file.Get(list + 12, boneTable) || indexCount <= 0 ||
                        boneCount <= 0 || !file.Has(list + boneTable, 4 * (size_t)boneCount))
                        continue;
                    const size_t indices = indicesAt ? list + indicesAt : list + 16;
                    const bool wide = indexCount > 0xffff;
                    if (!file.Has(indices, (size_t)indexCount * (wide ? 4 : 2))) continue;
                    std::vector<bool> inGroup(boneCount), keepName(boneCount);
                    for (int b = 0; b < boneCount; ++b) {
                        int32_t nameAt;
                        file.Get(list + boneTable + 4 * (size_t)b, nameAt);
                        const std::string boneName = file.Name(list + nameAt);
                        inGroup[b] = group.count(boneName) > 0;
                        keepName[b] = skinned.count(boneName) > 0;
                    }
                    if (std::find(inGroup.begin(), inGroup.end(), true) == inGroup.end()) continue;
                    std::vector<uint32_t> kept;
                    for (int32_t f = 0; f + 2 < indexCount; f += 3) {
                        uint32_t corner[3];
                        int owner[3];
                        bool valid = true;
                        for (int c = 0; c < 3; ++c) {
                            if (wide) {
                                file.Get(indices + 4 * (size_t)(f + c), corner[c]);
                            } else {
                                uint16_t narrow;
                                file.Get(indices + 2 * (size_t)(f + c), narrow);
                                corner[c] = narrow;
                            }
                            valid = valid && corner[c] < (uint32_t)vertexCount;
                            if (valid) owner[c] = DominantSlot(emd.data() + vertices + corner[c] * layout.size, layout);
                        }
                        if (!valid) continue;
                        const int winner = TriangleOwner(owner);
                        if (winner < boneCount && inGroup[winner]) kept.insert(kept.end(), corner, corner + 3);
                    }
                    if (kept.empty()) continue;
                    // the reader picks the index size from the count (Parser.cs is32Bit)
                    const bool keptWide = kept.size() > 0xffff;
                    if (!keptWide && *std::max_element(kept.begin(), kept.end()) > 0xffff) continue;
                    for (size_t k = 0; k < kept.size(); ++k) {
                        if (keptWide) Store<uint32_t>(out, indices + 4 * k, kept[k]);
                        else Store<uint16_t>(out, indices + 2 * k, (uint16_t)kept[k]);
                    }
                    Store<int32_t>(out, list, (int32_t)kept.size());
                    for (int b = 0; b < boneCount; ++b)
                        if (!keepName[b])
                            Store<int32_t>(out, list + boneTable + 4 * (size_t)b, (int32_t)(baseBoneName - list));
                    Store<int32_t>(out, submesh + listTable + 4 * (size_t)keptLists++, listAt);
                    for (uint32_t corner : kept) used[corner] = true;
                    triangles += kept.size() / 3;
                }
                Store<uint16_t>(out, submesh + 70, keptLists);
                if (!keptLists) continue;
                if (movedBuffers.insert(vertices).second)
                    MoveVertices(out, vertices, (size_t)vertexCount, layout, toBone, meshScale);
                Bounds submeshBounds;
                for (int32_t v = 0; v < vertexCount; ++v) {
                    if (!used[v]) continue;
                    double position[3];
                    ReadVector(out.data() + vertices + v * layout.size, false, position);
                    submeshBounds.Add(position);
                }
                WriteBounds(out, submesh, submeshBounds);
                meshBounds.Add(submeshBounds);
                Store<int32_t>(out, mesh + submeshTable + 4 * (size_t)keptSubmeshes++, submeshAt);
            }
            Store<uint16_t>(out, mesh + 54, keptSubmeshes);
            if (!keptSubmeshes) continue;
            WriteBounds(out, mesh, meshBounds);
            Store<int32_t>(out, mesh + 48, (int32_t)(ownName - mesh));
            Store<int32_t>(out, model + meshTable + 4 * (size_t)keptMeshes++, meshAt);
        }
        Store<uint16_t>(out, model + 2, keptMeshes);
        if (!keptMeshes) continue;
        Store<int32_t>(out, modelTable + 4 * (size_t)keptModels, model);
        if (modelNames) Store<int32_t>(out, modelNames + 4 * (size_t)keptModels, (int32_t)ownName);
        ++keptModels;
    }
    Store<uint16_t>(out, 18, keptModels);
    return keptModels > 0;
}

// ------------------------------------------------------------------- BCS ---
const char* const kPartNames[10] = {"Face_base", "Face_forehead", "Face_eye", "Face_nose", "Face_ear",
                                    "Hair",      "Bust",          "Pants",    "Rist",      "Boots"};
const size_t kBustSlot = 6;
const size_t kPhysicsObjectSize = 72;
const uint32_t kDytFromTextureEmb = 2;  // physics object flag: dyt from the EMB's name (Xv2CoreLib PartFlags)

// One BCS part's files; names relative to chara/<code>/, no extension.
struct PartFiles {
    std::string code, emd, emm, emb;
};

// The part at `part`, slot `slot` of its part set. A name the part gives
// wins; else the numbered file "<code>_<model:000>_<part>" (Xv2CoreLib
// Part.GetModelPath / GetEmbPath: model for the EMD and EMM, model2 for the
// EMB). False when it has no mesh.
bool FilesOfPart(const Reader& bcs, size_t part, size_t slot, const std::string& ownerCode, PartFiles& files)
{
    int16_t model, model2;
    if (!bcs.Has(part, 80) || !bcs.Get(part, model) || !bcs.Get(part + 2, model2)) return false;
    files.code = std::string((const char*)bcs.data + part + 52, strnlen((const char*)bcs.data + part + 52, 4));
    if (files.code.empty()) files.code = ownerCode;
    auto text = [&](size_t field) {
        int32_t at = 0;
        return bcs.Get(part + field, at) && at ? bcs.Name(part + at) : std::string();
    };
    auto numbered = [&](int number) {
        char name[64];
        snprintf(name, sizeof(name), "%s_%03d_%s", files.code.c_str(), number, kPartNames[slot]);
        return std::string(name);
    };
    files.emd = text(56);
    if (files.emd.empty() && model != -1) files.emd = numbered(model);
    files.emm = text(60);
    if (files.emm.empty()) files.emm = model != -1 ? numbered(model) : files.emd;
    files.emb = text(64);
    if (files.emb.empty()) files.emb = model2 != -1 ? numbered(model2) : files.emd;
    return !files.emd.empty();
}

// "chara/<code>/<name>.emd", following a leading "../XXX/" to another
// character's folder.
std::string EmdPathOf(const PartFiles& files)
{
    std::string code = files.code, name = files.emd;
    while (name.compare(0, 3, "../") == 0) {
        const size_t slash = name.find('/', 3);
        if (slash == std::string::npos) break;
        code = name.substr(3, slash - 3);
        name = name.substr(slash + 1);
    }
    return "chara/" + code + "/" + name + ".emd";
}

// The offsets of a BCS's part sets (absolute).
std::vector<size_t> PartSets(const Reader& bcs)
{
    std::vector<size_t> sets;
    uint16_t count;
    int32_t table;
    if (!bcs.Get(12, count) || !bcs.Get(24, table) || !bcs.Has(table, 4 * (size_t)count)) return sets;
    for (size_t i = 0; i < count; ++i) {
        int32_t at;
        bcs.Get(table + 4 * i, at);
        int32_t partCount, partTable;
        if (at > 0 && bcs.Get(at + 20, partCount) && partCount == 10 && bcs.Get(at + 24, partTable) &&
            bcs.Has((size_t)at + partTable, 40))
            sets.push_back(at);
    }
    return sets;
}

// The part in slot `slot` of the part set at `set` (absolute), or 0.
size_t PartAt(const Reader& bcs, size_t set, size_t slot)
{
    int32_t partTable, at;
    if (!bcs.Get(set + 24, partTable) || !bcs.Get(set + partTable + 4 * slot, at) || at <= 0) return 0;
    return bcs.Has(set + at, 80) ? set + at : 0;
}

struct PropObject {
    std::string code;                         // the folder of its files
    std::string emd, emm, emb, bone;          // names (relative to chara/<code>/) and the bone to attach to
};

// Gives every Bust part with a mesh, in every part set of `out` (a copy of
// the race BCS), the prop objects after its own physics objects: a new
// array at the end of the file, each object's names after it. Returns the
// number of parts changed.
size_t AddPhysicsObjects(std::vector<uint8_t>& out, const std::vector<PropObject>& objects)
{
    const std::vector<uint8_t> original = out;
    const Reader file{original.data(), original.size()};
    std::set<size_t> done;  // a part shared by two part sets gets its objects once
    size_t changed = 0;
    for (size_t set : PartSets(file)) {
        const size_t part = PartAt(file, set, kBustSlot);
        int16_t model, oldCount;
        int32_t emdName, oldArrayAt;
        if (!part || !file.Get(part, model) || !file.Get(part + 56, emdName) || (model == -1 && emdName == 0) ||
            !file.Get(part + 74, oldCount) || !file.Get(part + 76, oldArrayAt) || oldCount < 0 ||
            oldCount + objects.size() > 0x7fff || !done.insert(part).second)
            continue;
        const size_t oldArray = part + oldArrayAt;
        if (oldCount && !file.Has(oldArray, kPhysicsObjectSize * oldCount)) continue;

        Align16(out);
        const size_t array = out.size();
        out.insert(out.end(), original.begin() + oldArray, original.begin() + oldArray + kPhysicsObjectSize * oldCount);
        for (const PropObject& object : objects) {
            const size_t at = out.size();
            out.resize(at + kPhysicsObjectSize, 0);
            Store<int16_t>(out, at + 0, -1);  // model1: the mesh comes from the EMD name
            Store<int16_t>(out, at + 2, -1);  // model2
            Store<uint32_t>(out, at + 24, kDytFromTextureEmb);
            memcpy(out.data() + at + 36, object.code.data(), std::min<size_t>(object.code.size(), 4));
        }
        // names: EMD, EMM, EMB, EAN, bone to attach, SCD (object +40..+60)
        const size_t total = oldCount + objects.size();
        for (size_t k = 0; k < total; ++k) {
            const size_t object = array + kPhysicsObjectSize * k;
            for (size_t field = 0; field < 6; ++field) {
                std::string text;
                bool present;
                if (k < (size_t)oldCount) {
                    int32_t at = 0;
                    file.Get(oldArray + kPhysicsObjectSize * k + 40 + 4 * field, at);
                    present = at != 0;
                    if (present) text = file.Name(oldArray + kPhysicsObjectSize * k + at);
                } else {
                    const PropObject& prop = objects[k - oldCount];
                    const std::string* names[6] = {&prop.emd, &prop.emm, &prop.emb, nullptr, &prop.bone, nullptr};
                    present = names[field] != nullptr;
                    if (present) text = *names[field];
                }
                Store<int32_t>(out, object + 40 + 4 * field, present ? (int32_t)(AppendString(out, text) - object) : 0);
            }
        }
        Store<int16_t>(out, part + 74, (int16_t)total);
        Store<int32_t>(out, part + 76, (int32_t)(array - part));
        ++changed;
    }
    return changed;
}

std::string WithoutExtension(const std::string& path)
{
    const size_t dot = path.find_last_of('.'), slash = path.find_last_of('/');
    return dot == std::string::npos || (slash != std::string::npos && dot < slash) ? path : path.substr(0, dot);
}

// After the last slash; some race paths come with a backslash ("chara/HUF\HUF.bcs" in the log).
std::string FileName(const std::string& path) { return path.substr(path.find_last_of("/\\") + 1); }

// A held prop moves through the moveset: its top bone has tracks in most of
// the moveset EAN's animations (propbones.py over all 218 roster movesets:
// Fu's Future's Doom sword 23/83 is the lowest, Whis's cane 82/88). Other
// a_x_* parts are still or nearly so: Beast Gohan's hair bangs and D01's hair
// 0, Videl's belt 3/91, Cheelai's pouch (G18's moveset) 0 - drawn as props
// they'd stick a second copy of the style's hair or clothes on the CaC.
const double kPropTrackShare = 0.10;

// The prop bones whose meshes become props: those the moveset animates (in at
// least kPropTrackShare of its animations), and the bones under them (Fu's
// a_x_sword2/3 hang off the animated a_x_sword1). Fu's glasses hang off
// a_x_glasses: part of his face, not a prop.
std::set<std::string> AnimatedPropBones(const Skeleton& style, const std::vector<PropBone>& props,
                                        const std::map<std::string, int>& animationsWithTrack, int animations)
{
    std::set<std::string> kept;
    for (const PropBone& prop : props) {  // parents come before their children
        const std::string& bone = style.names[prop.styleBone];
        if (bone.find("glasses") != std::string::npos) continue;
        auto tracks = animationsWithTrack.find(bone);
        const bool animated = tracks != animationsWithTrack.end() && tracks->second >= kPropTrackShare * animations;
        const int parent = style.parents[prop.styleBone];
        if (animated || (parent >= 0 && kept.count(style.names[parent]))) kept.insert(bone);
    }
    return kept;
}

// ---------------------------------------------------------- back depth ---
// The race's back: the near face (least depth) of Arale's Poop Stick, the
// accessory carried on the back (accessory_item.idb id 61, BCS part set 897):
// "<race>_897.emd", a physics object on b_C_Chest whose mesh is in chest-bone
// space. Its stick is the longest submesh (0.97 m tall, across the back like
// a sheathed sword); the others are the poop on top. The game's own
// placement on every race, a little off the bare body for the clothes:
// HUM 0.185, HUF 0.093, FRI 0.135, NMC 0.215, MAM 0.398, MAF 0.093 against
// back meshes of 0.156 / 0.064 / 0.114 / 0.189 / 0.349 / 0.070 at most
// (ClaudeTools\battle_styles\backdepth.py). False when the race has none.
bool RaceBackDepth(const ReadGameData& read, const std::string& raceStem, double& depth)
{
    std::vector<uint8_t> emd;
    if (!read(raceStem + "_897.emd", emd)) return false;
    std::map<size_t, Bounds> submeshes;
    ForEachTriangle(Reader{emd.data(), emd.size()}, [&](const TriangleCorners& triangle) {
        for (const uint8_t* vertex : triangle.vertex) {
            float position[3];
            memcpy(position, vertex, sizeof(position));
            const double point[3] = {position[0], position[1], position[2]};
            submeshes[triangle.submesh].Add(point);
        }
    });
    const Bounds* stick = nullptr;
    for (const auto& submesh : submeshes)
        if (!stick || submesh.second.high[1] - submesh.second.low[1] > stick->high[1] - stick->low[1])
            stick = &submesh.second;
    if (!stick || stick->high[1] - stick->low[1] < 0.3) return false;
    depth = stick->low[2];
    return true;
}

// Measured on the style character's own body (its first part set's Bust,
// in chest-bone space): the back's depth as the 90th percentile of the
// vertices skinned mostly to b_C_Chest within 10 cm of the centre line,
// from 10 cm below the chest bone to 15 cm above (below the collar). The
// maximum would follow a collar or a cape's clasp: Tapion's 0.165 against
// Trunks's 0.147 with the same sword placement; the percentile gives
// 0.126 / 0.125 (Trunks Rage 0.115, Goku Mini 0.055; backdepth.py).
// The stick sits off the race's bare back for clothes the style's body
// already has, so kStyleClothesGap moves the style's back out to match: 2.3 cm, from
// the swords in build 13 - on a Frieza race right at 0.124 (stick 0.135,
// Trunks 0.125: 2.9 cm), clipping a dressed male human a little at 0.173
// (stick 0.185: under 3.0 cm).
const double kStyleClothesGap = 0.023;
bool StyleBackDepth(const ReadGameData& read, const std::string& folder, const std::string& code, double& depth)
{
    std::vector<uint8_t> bcsBytes, eskBytes, emd;
    Skeleton skeleton;
    if (!read(folder + code + ".bcs", bcsBytes) || !read(folder + code + "_000.esk", eskBytes) ||
        !ParseEsk(Reader{eskBytes.data(), eskBytes.size()}, skeleton) || !skeleton.absolute)
        return false;
    const auto chest = skeleton.index.find("b_C_Chest");
    if (chest == skeleton.index.end()) return false;
    const Matrix toChest = ReadMatrix(eskBytes.data() + skeleton.absolute + 64 * (size_t)chest->second);
    const Reader bcs{bcsBytes.data(), bcsBytes.size()};
    const std::vector<size_t> sets = PartSets(bcs);
    PartFiles files;
    const size_t part = sets.empty() ? 0 : PartAt(bcs, sets[0], kBustSlot);
    if (!part || !FilesOfPart(bcs, part, kBustSlot, code, files) || !read(EmdPathOf(files), emd)) return false;
    std::map<const uint8_t*, double> depths;  // per vertex (triangles share them)
    ForEachTriangle(Reader{emd.data(), emd.size()}, [&](const TriangleCorners& triangle) {
        for (int c = 0; c < 3; ++c) {
            if ((*triangle.boneNames)[triangle.owner[c]] != "b_C_Chest") continue;
            float position[3];
            memcpy(position, triangle.vertex[c], sizeof(position));
            double inChest[3];
            for (int axis = 0; axis < 3; ++axis)
                inChest[axis] = position[0] * toChest.m[0][axis] + position[1] * toChest.m[1][axis] +
                                position[2] * toChest.m[2][axis] + toChest.m[3][axis];
            if (std::fabs(inChest[0]) < 0.1 && inChest[1] >= -0.1 && inChest[1] <= 0.15)
                depths[triangle.vertex[c]] = inChest[2];
        }
    });
    if (depths.size() < 20) return false;
    std::vector<double> sorted;
    for (const auto& vertex : depths) sorted.push_back(vertex.second);
    std::sort(sorted.begin(), sorted.end());
    depth = sorted[(size_t)(0.9 * (double)(sorted.size() - 1))] + kStyleClothesGap;
    return true;
}

// Floor to head: b_C_Head's rest height above the lowest foot or toe joint,
// through the relative transforms (skeleton space; 0 when a bone is missing).
// The race's BCS body scale (HUF 0.90, HUM 0.83, MAM 0.50) is left out: the
// game applies it to the body and its physics objects alike - Arale's Poop
// Stick is modelled at MAM's unscaled back (0.398 deep, back mesh 0.349).
// Measured (ClaudeTools\battle_styles\bodyheight.py): TOW 1.293, HUF 1.100,
// HUM 1.477, FRI 1.112, WIS 1.819, TRX 1.380, NTG 0.601.
double StandingHeight(const Reader& file, const Skeleton& skeleton)
{
    std::vector<Matrix> worlds;
    double head = 0, floor = 0;
    bool headFound = false, floorFound = false;
    for (size_t i = 0; i < skeleton.names.size(); ++i) {
        if (!file.Has(skeleton.relative + 48 * i, 48)) return 0;
        const Matrix local = RelativeMatrix(file.data + skeleton.relative + 48 * i);
        const int parent = skeleton.parents[i];
        if (parent >= (int)i) return 0;  // parents come first in every skeleton read so far
        worlds.push_back(parent >= 0 ? Multiply(local, worlds[(size_t)parent]) : local);
        const std::string& name = skeleton.names[i];
        const double height = worlds.back().m[3][1];
        if (name == "b_C_Head") {
            head = height;
            headFound = true;
        } else if (name.compare(0, 8, "b_L_Foot") == 0 || name.compare(0, 8, "b_R_Foot") == 0 ||
                   name.compare(0, 7, "b_L_Toe") == 0 || name.compare(0, 7, "b_R_Toe") == 0) {
            if (!floorFound || height < floor) floor = height;
            floorFound = true;
        }
    }
    return headFound && floorFound && head > floor ? head - floor : 0;
}

// StandingHeight of "<stem>_000.esk"; 0 when missing.
double StandingHeightOf(const ReadGameData& read, const std::string& stem)
{
    std::vector<uint8_t> bytes;
    Skeleton skeleton;
    if (!read(stem + "_000.esk", bytes) || !ParseEsk(Reader{bytes.data(), bytes.size()}, skeleton)) return 0;
    return StandingHeight(Reader{bytes.data(), bytes.size()}, skeleton);
}

}  // namespace

PropPlacement MeasurePropPlacement(const ReadGameData& read, const std::string& raceBcsPath,
                                   const std::string& styleEanPath)
{
    PropPlacement back;
    // the moveset's owner ("chara/TRX/TRX.ean" -> TRX): its skeleton holds the
    // prop bones' rest positions and its animations were made for its body
    const size_t slash = styleEanPath.find_last_of("/\\");
    if (slash == std::string::npos) return back;
    const std::string folder = styleEanPath.substr(0, slash + 1), code = FileName(styleEanPath.substr(0, slash));
    if (RaceBackDepth(read, WithoutExtension(raceBcsPath), back.raceBack) &&
        StyleBackDepth(read, folder, code, back.styleBack)) {
        back.backKnown = true;
        back.shift = back.raceBack - back.styleBack;
    }
    // 0.3 m: no adult or Goku Mini (0.601) is shorter; below it a bone is misread
    const double raceHeight = StandingHeightOf(read, WithoutExtension(raceBcsPath)),
                 styleHeight = StandingHeightOf(read, folder + code);
    if (raceHeight > 0.3 && styleHeight > 0.3) back.heightRatio = raceHeight / styleHeight;
    return back;
}

bool FitPropsToRace(const ReadGameData& read, const std::vector<uint8_t>& raceBcs, const std::string& raceBcsPath,
                    const std::string& styleBcsPath, const std::string& styleEanPath, const std::string& fittedBcsPath,
                    std::vector<PropFile>& files, std::string& note)
{
    files.clear();
    const std::string raceStem = WithoutExtension(raceBcsPath), styleStem = WithoutExtension(styleBcsPath),
                      fittedStem = WithoutExtension(fittedBcsPath);

    std::vector<uint8_t> styleEskBytes, raceEskBytes, styleBcsBytes;
    Skeleton styleSkeleton, raceSkeleton;
    if (!read(styleStem + "_000.esk", styleEskBytes) ||
        !ParseEsk(Reader{styleEskBytes.data(), styleEskBytes.size()}, styleSkeleton) || !styleSkeleton.absolute) {
        note = "style skeleton " + styleStem + "_000.esk not found or not understood";
        return false;
    }
    const Reader styleEsk{styleEskBytes.data(), styleEskBytes.size()};
    if (!read(raceStem + "_000.esk", raceEskBytes) ||
        !ParseEsk(Reader{raceEskBytes.data(), raceEskBytes.size()}, raceSkeleton)) {
        note = "race skeleton " + raceStem + "_000.esk not found or not understood";
        return false;
    }
    const std::vector<PropBone> props = ChoosePropBones(raceSkeleton, styleSkeleton);
    const double styleArm = ArmLength(styleEsk, styleSkeleton),
                 raceArm = ArmLength(Reader{raceEskBytes.data(), raceEskBytes.size()}, raceSkeleton);
    const double armRatio = styleArm > 0.05 && raceArm > 0.05 ? raceArm / styleArm : 1;
    const PropPlacement back = MeasurePropPlacement(read, raceBcsPath, styleEanPath);
    const double meshScale = PropMeshScale(back.SizeRatio(armRatio));
    // no a_x_* prop bone (or none animated) still leaves hand-held props
    // (HandPropSubmeshes); "no prop bones" only once neither kind is found
    std::set<std::string> propBones;
    std::string withoutBones = "no prop bones";
    if (!props.empty()) {
        std::vector<uint8_t> styleEanBytes;
        std::map<std::string, int> animationsWithTrack;
        int animations = 0;
        std::string eanNote;
        if (!read(styleEanPath, styleEanBytes) ||
            !EanBoneUse(styleEanBytes.data(), styleEanBytes.size(), animationsWithTrack, animations, eanNote)) {
            note = "style EAN " + styleEanPath + " not found or not understood";
            return false;
        }
        propBones = AnimatedPropBones(styleSkeleton, props, animationsWithTrack, animations);
        if (propBones.empty()) withoutBones = "prop bones, none animated by " + FileName(styleEanPath);
    }

    // the prop meshes: the style's first part set, every part, every prop bone
    if (!read(styleBcsPath, styleBcsBytes)) {
        note = propBones.empty() ? withoutBones : "style BCS not found";
        return false;
    }
    const Reader styleBcs{styleBcsBytes.data(), styleBcsBytes.size()};
    const std::vector<size_t> styleSets = PartSets(styleBcs);
    if (styleSets.empty()) {
        note = "style BCS has no part set";
        return false;
    }
    const std::string styleCode = FileName(styleStem), raceCode = FileName(raceStem);
    std::vector<PropObject> objects;
    std::vector<PropFile> meshes;
    std::set<std::string> cut;  // EMDs already cut (two parts may share one)
    std::vector<size_t> skinnedCounts;  // per object: bones of its own skeleton besides b_C_Base
    std::vector<size_t> handProps;  // objects that are hand-held props
    size_t triangles = 0;
    for (size_t slot = 0; slot < 10; ++slot) {
        const size_t part = PartAt(styleBcs, styleSets[0], slot);
        PartFiles partFiles;
        if (!part || !FilesOfPart(styleBcs, part, slot, styleCode, partFiles)) continue;
        const std::string emdPath = EmdPathOf(partFiles);
        std::vector<uint8_t> emd;
        if (!cut.insert(emdPath).second || !read(emdPath, emd)) continue;
        for (const std::set<std::string>& group : PropBoneGroups(emd, propBones)) {
            // the object hangs on the group's first bone in the skeleton (the
            // others are its children: a_x_nyb_root2 > front1 > front2...);
            // the others become the prop mesh skeleton's own bones, under the
            // same parents, positioned as in the fitted skeleton
            int top = -1;
            for (const std::string& bone : group) {
                const int index = styleSkeleton.index.at(bone);
                if (top < 0 || index < top) top = index;
            }
            const std::string& bone = styleSkeleton.names[top];
            std::vector<PropMeshBone> extra;
            std::map<int, int> meshBone = {{top, 1}};  // style bone -> prop mesh skeleton bone (1 = b_C_Base)
            std::set<std::string> skinned;
            for (size_t i = (size_t)top + 1; i < styleSkeleton.names.size(); ++i) {
                auto parent = meshBone.find(styleSkeleton.parents[i]);
                // a member whose parent isn't in the group stays rigid with the top (b_C_Base)
                if (!group.count(styleSkeleton.names[i]) || parent == meshBone.end()) continue;
                PropMeshBone added{styleSkeleton.names[i], parent->second, {}};
                memcpy(added.relative, styleEskBytes.data() + styleSkeleton.relative + 48 * i, sizeof(added.relative));
                const double scale = PropBonePositionScale(added.name, armRatio, back.SizeRatio(armRatio));
                for (int axis = 0; axis < 3; ++axis) added.relative[axis] = (float)(added.relative[axis] * scale);
                meshBone[(int)i] = 4 + (int)extra.size();
                extra.push_back(added);
                skinned.insert(added.name);
            }
            const Matrix toBone = ReadMatrix(styleEskBytes.data() + styleSkeleton.absolute + 64 * (size_t)top);
            // the race in the name: the mesh is sized to it (meshScale), and
            // every race's fit of this style writes into the same folder. Kept
            // short (code + race + number): the 46-character
            // "DMG_000_Face_forehead_bsprop_HUM_a_x_cane_root" hung the
            // character select preview, the 42-character build 8 name didn't
            // (battle-styles-props.md "Build 9 in game").
            const std::string name =
                partFiles.code + kPropFileTag + raceCode + "_" + std::to_string(objects.size());
            std::vector<uint8_t> mesh;
            size_t kept = 0;
            if (!CutPropMesh(emd, group, skinned, toBone, meshScale, name, mesh, kept)) continue;
            const std::string folder = "chara/" + partFiles.code + "/";
            meshes.push_back({folder + name + ".emd", std::move(mesh)});
            meshes.push_back({folder + name + ".esk", PropMeshSkeleton(name, extra)});
            objects.push_back({partFiles.code, name, partFiles.emm, partFiles.emb, bone});
            skinnedCounts.push_back(extra.size());
            triangles += kept;
        }
        // hand-held props: rigid on the hand bone, no bones of their own
        for (const auto& [bone, submeshes] : HandPropSubmeshes(emd, styleArm)) {
            const auto hand = styleSkeleton.index.find(bone);
            if (hand == styleSkeleton.index.end()) continue;
            const Matrix toBone = ReadMatrix(styleEskBytes.data() + styleSkeleton.absolute + 64 * (size_t)hand->second);
            const std::string name =
                partFiles.code + kPropFileTag + raceCode + "_" + std::to_string(objects.size());
            std::vector<uint8_t> mesh;
            size_t kept = 0;
            if (!CutPropMesh(emd, {bone}, {}, toBone, meshScale, name, mesh, kept, &submeshes)) continue;
            const std::string folder = "chara/" + partFiles.code + "/";
            meshes.push_back({folder + name + ".emd", std::move(mesh)});
            meshes.push_back({folder + name + ".esk", PropMeshSkeleton(name, {})});
            objects.push_back({partFiles.code, name, partFiles.emm, partFiles.emb, bone});
            skinnedCounts.push_back(0);
            handProps.push_back(objects.size() - 1);
            triangles += kept;
        }
    }
    if (objects.empty()) {
        note = propBones.empty() ? withoutBones : "prop bones but no mesh skinned to them";
        return false;
    }

    std::vector<uint8_t> fittedBcs = raceBcs;
    const size_t parts = AddPhysicsObjects(fittedBcs, objects);
    if (!parts) {
        note = "race BCS has no Bust part with a mesh";
        return false;
    }
    files.push_back({fittedBcsPath, std::move(fittedBcs)});

    // the race skeletons with the prop bones (the LOD ones only where the race has them)
    for (const char* suffix : {"_000", "_000_LOD01", "_000_LOD02", "_000_LOD03"}) {
        std::vector<uint8_t> bytes;
        Skeleton skeleton;
        const bool main = strcmp(suffix, "_000") == 0;
        if (main) bytes = raceEskBytes;
        else if (!read(raceStem + suffix + ".esk", bytes)) continue;
        if (!ParseEsk(Reader{bytes.data(), bytes.size()}, skeleton)) {
            note = raceStem + suffix + ".esk not understood";
            files.clear();
            return false;
        }
        std::vector<uint8_t> out = bytes;
        if (!AppendEskBones(out, skeleton, styleEsk, styleSkeleton, ChoosePropBones(skeleton, styleSkeleton), armRatio,
                            back)) {
            note = raceStem + suffix + ".esk: a bone's rest matrix is singular";
            files.clear();
            return false;
        }
        files.push_back({fittedStem + suffix + ".esk", std::move(out)});
    }
    files.insert(files.end(), std::make_move_iterator(meshes.begin()), std::make_move_iterator(meshes.end()));

    char summary[200];
    snprintf(summary, sizeof(summary),
             "%zu prop bone(s), %zu prop mesh(es) of %zu triangle(s) on %zu Bust part(s), placed x%.3f, "
             "meshes x%.3f (arm %.3f -> %.3f, height x%.3f)",
             props.size(), objects.size(), triangles, parts, armRatio, meshScale, styleArm, raceArm, back.heightRatio);
    note = summary;
    if (back.backKnown) {
        snprintf(summary, sizeof(summary), ", back props %+.3f deep (back %.3f -> %.3f)", back.shift, back.styleBack,
                 back.raceBack);
        note += summary;
    }
    for (size_t k = 0; k < objects.size(); ++k) {
        note += "; " + objects[k].emd + " on " + objects[k].bone;
        if (skinnedCounts[k]) note += " (+" + std::to_string(skinnedCounts[k]) + " skinned bones)";
        if (std::count(handProps.begin(), handProps.end(), k)) note += " (hand prop)";
    }
    return true;
}

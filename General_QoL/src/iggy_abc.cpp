// ---------------------------------------------------------------------------
//  Menu ActionScript swaps in .iggy files.
//
//  Iggy layout (Research_Notes\ui-iggy-and-textures.md): subfile count u32
//  at 0x1C, entries {id, size, size, offset} from 0x20. The menus patched
//  here have two: id 1 = the movie at 0x40, id 0 = its pointer index right
//  after it. The movie header's field at +0x98 is a self-relative u64 to the
//  AS3 block {u64 1, u32 ABC size, ABC}. The patch appends a new block at the
//  end of the movie, points that field at it and adds the index commands that
//  walk it - nothing else moves, so no other offset needs fixing; the old
//  block stays as unused bytes. Same layout as ClaudeTools\iggy\lobyhud_abc.py --iggy.
// ---------------------------------------------------------------------------
#include "iggy_abc.h"
#include "avatar_faculty_abc.h"
#include <cstring>
#include <iterator>

namespace {

uint32_t Rd32(const uint8_t* bytes, size_t at) { uint32_t v; memcpy(&v, bytes + at, 4); return v; }
uint64_t Rd64(const uint8_t* bytes, size_t at) { uint64_t v; memcpy(&v, bytes + at, 8); return v; }
void Wr32(uint8_t* bytes, size_t at, uint32_t v) { memcpy(bytes + at, &v, 4); }
void Wr64(uint8_t* bytes, size_t at, uint64_t v) { memcpy(bytes + at, &v, 8); }

uint64_t Fnv64(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
    return hash;
}

const size_t kSubfileCount = 0x1C, kSubfileTable = 0x20, kSubfileEntry = 16;
const size_t kAbcPointer = 0x98;     // movie header: self-relative u64 to the AS3 block
const size_t kAbcBlockHeader = 12;   // {u64 1, u32 ABC size}

// Index byte code (decoder iggy_w64.dll 0x59860..0x59bb1; the full list is in
// ClaudeTools\iggy\lobyhud_abc.py): fe n = skip n + 1 bytes, ff u32 = skip u32
// bytes, fd s k (offset, kind)... = s bytes with k fields. Vanilla walks the
// AS3 block as fd 0f 01 00 02 (15 bytes, relative pointer at +0) + ff <rest>.
const uint8_t kIndexSkipSmall = 0xfe, kIndexSkipLarge = 0xff;
const uint8_t kIndexAbcBlockHeader[] = {0xfd, 0x0f, 0x01, 0x00, 0x02};
const size_t kIndexAbcBlockHeaderWalk = 0x0f;

}  // namespace

bool PatchIggyAbc(const uint8_t* data, size_t size, uint32_t vanillaSize, uint64_t vanillaHash,
                  const uint8_t* abc, size_t abcSize, std::vector<uint8_t>& out, std::string& note)
{
    // Two subfiles: the movie (id 1) first, its index (id 0) right after it.
    if (size < kSubfileTable + 2 * kSubfileEntry || Rd32(data, kSubfileCount) != 2) {
        note = "unexpected layout";
        return false;
    }
    const size_t movieEntry = kSubfileTable, indexEntry = kSubfileTable + kSubfileEntry;
    uint32_t movieSize = Rd32(data, movieEntry + 4), movieOffset = Rd32(data, movieEntry + 12);
    uint32_t indexSize = Rd32(data, indexEntry + 4), indexOffset = Rd32(data, indexEntry + 12);
    if (Rd32(data, movieEntry) != 1 || Rd32(data, indexEntry) != 0 ||
        movieOffset != kSubfileTable + 2 * kSubfileEntry || indexOffset != (uint64_t)movieOffset + movieSize ||
        (uint64_t)indexOffset + indexSize > size || movieSize < kAbcPointer + 8) {
        note = "unexpected layout";
        return false;
    }
    // The AS3 block must be the vanilla one (another mod's menu stays as it is).
    const size_t field = movieOffset + kAbcPointer;
    uint64_t block = field + Rd64(data, field);
    if (block < movieOffset || block + kAbcBlockHeader > (uint64_t)movieOffset + movieSize ||
        Rd64(data, (size_t)block) != 1) {
        note = "unexpected layout";
        return false;
    }
    uint32_t oldAbcSize = Rd32(data, (size_t)block + 8);
    if (oldAbcSize != vanillaSize || block + kAbcBlockHeader + oldAbcSize > (uint64_t)movieOffset + movieSize ||
        Fnv64(data + block + kAbcBlockHeader, oldAbcSize) != vanillaHash) {
        note = "menu script is not the vanilla one (another mod's?)";
        return false;
    }

    out.assign(data, data + indexOffset);  // header + movie
    out.resize((out.size() + 15) & ~(size_t)15);
    const size_t newBlock = out.size();
    out.resize(newBlock + kAbcBlockHeader);
    Wr64(out.data(), newBlock, 1);
    Wr32(out.data(), newBlock + 8, (uint32_t)abcSize);
    out.insert(out.end(), abc, abc + abcSize);
    out.resize((out.size() + 7) & ~(size_t)7);
    const uint32_t newMovieSize = (uint32_t)(out.size() - movieOffset), newIndexOffset = (uint32_t)out.size();
    out.insert(out.end(), data + indexOffset, data + indexOffset + indexSize);
    // The index walks the movie and fixes up only the pointers it passes,
    // so it must walk the new block too (its +0 "1" becomes null).
    const size_t gap = newBlock - indexOffset;  // alignment bytes after the old movie end
    if (gap) out.insert(out.end(), {kIndexSkipSmall, (uint8_t)(gap - 1)});
    out.insert(out.end(), std::begin(kIndexAbcBlockHeader), std::end(kIndexAbcBlockHeader));
    const uint32_t abcRest = newIndexOffset - (uint32_t)(newBlock + kIndexAbcBlockHeaderWalk);
    out.push_back(kIndexSkipLarge);
    out.resize(out.size() + 4);
    Wr32(out.data(), out.size() - 4, abcRest);
    const uint32_t newIndexSize = (uint32_t)(out.size() - newIndexOffset);
    Wr32(out.data(), movieEntry + 4, newMovieSize);
    Wr32(out.data(), movieEntry + 8, newMovieSize);
    Wr32(out.data(), indexEntry + 4, newIndexSize);
    Wr32(out.data(), indexEntry + 8, newIndexSize);
    Wr32(out.data(), indexEntry + 12, newIndexOffset);
    Wr64(out.data(), field, newBlock - field);
    return true;
}

// Where the movie's active AS3 block's ABC is (after any earlier patch).
static bool FindActiveAbc(const uint8_t* data, size_t size, size_t& abcOffset, uint32_t& abcSize)
{
    if (size < kSubfileTable + 2 * kSubfileEntry || Rd32(data, kSubfileCount) != 2) return false;
    const uint32_t movieSize = Rd32(data, kSubfileTable + 4), movieOffset = Rd32(data, kSubfileTable + 12);
    if ((uint64_t)movieOffset + movieSize > size || movieSize < kAbcPointer + 8) return false;
    const size_t field = movieOffset + kAbcPointer;
    const uint64_t block = field + Rd64(data, field);
    if (block < movieOffset || block + kAbcBlockHeader > (uint64_t)movieOffset + movieSize ||
        Rd64(data, (size_t)block) != 1)
        return false;
    abcSize = Rd32(data, (size_t)block + 8);
    abcOffset = (size_t)block + kAbcBlockHeader;
    return abcOffset + abcSize <= (uint64_t)movieOffset + movieSize;
}

// The shared AvatarStatus marks which mods' parts are on with the string
// "XV2FACULTY:" + one character per mod (as3\AvatarStatus.as FeatureFlags);
// the characters are rewritten in place, so the ABC's layout never changes.
static const char kFacultyMarker[] = "XV2FACULTY:";
static const size_t kFacultyFreePointsFlag = 0;  // General_QoL's: '1' = free points

// The free points flag's offset in `data`'s active ABC, or 0 when that ABC
// is not the shared script.
static size_t FindFreePointsFlag(const uint8_t* data, size_t size)
{
    size_t abcOffset; uint32_t abcSize;
    if (!FindActiveAbc(data, size, abcOffset, abcSize)) return 0;
    const size_t markerSize = sizeof(kFacultyMarker) - 1;
    for (size_t at = abcOffset; at + markerSize + 2 <= abcOffset + abcSize; ++at)
        if (memcmp(data + at, kFacultyMarker, markerSize) == 0) return at + markerSize + kFacultyFreePointsFlag;
    return 0;
}

bool PatchAvatarFaculty(const uint8_t* data, size_t size, std::vector<uint8_t>& out, std::string& note)
{
    // Vanilla: the shared script with the free points flag set. Another mod
    // already serving the shared script (Combined Supers' Supers box): only
    // the flag is set in its copy, so both parts show.
    if (PatchIggyAbc(data, size, kVanillaAvatarFacultyAbcSize, kVanillaAvatarFacultyAbcHash, kAvatarFacultyAbc,
                     sizeof(kAvatarFacultyAbc), out, note)) {
        const size_t flag = FindFreePointsFlag(out.data(), out.size());
        if (!flag) {
            note = "shared attribute script has no feature flags (rebuild avatar_faculty_abc.h)";
            return false;
        }
        out[flag] = '1';
        note = "attribute script with free points (" + std::to_string(out.size()) + " bytes)";
        return true;
    }
    const size_t flag = FindFreePointsFlag(data, size);
    if (!flag) return false;  // `note` from PatchIggyAbc: someone else's script
    if (data[flag] == '1') {
        note = "free points already on in the shared attribute script";
        return false;
    }
    out.assign(data, data + size);
    out[flag] = '1';
    note = "free points switched on in the shared attribute script another mod serves";
    return true;
}

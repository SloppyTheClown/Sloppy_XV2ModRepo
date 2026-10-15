// ---------------------------------------------------------------------------
//  Replacing an .iggy menu's ActionScript (AS3). Copied unchanged from
//  BattleStyles\src\iggy_abc.cpp; same method as General_QoL's LOBYHUD
//  edit (map_names.cpp) and ClaudeTools\iggy\lobyhud_abc.py --iggy.
//
//  Iggy layout (Research_Notes\ui-iggy-and-textures.md): subfile count u32
//  at 0x1C, entries {id, size, size, offset} from 0x20. These menus have two:
//  id 1 = the movie at 0x40, id 0 = its pointer index right after it. The
//  movie header's field at +0x98 is a self-relative u64 to the AS3 block
//  {u64 1, u32 ABC size, ABC}. The new block goes at the end of the movie,
//  that field points at it and the index gets the commands that walk it -
//  nothing else moves, so no other offset needs fixing; the old block stays
//  as unused bytes.
// ---------------------------------------------------------------------------
#include "iggy_abc.h"
#include <cstring>
#include <iterator>

namespace {

// Little-endian u32 / u64 read and write at byte offset `at` (unaligned).
uint32_t Rd32(const uint8_t* bytes, size_t at) { uint32_t v; memcpy(&v, bytes + at, 4); return v; }
uint64_t Rd64(const uint8_t* bytes, size_t at) { uint64_t v; memcpy(&v, bytes + at, 8); return v; }
void Wr32(uint8_t* bytes, size_t at, uint32_t v) { memcpy(bytes + at, &v, 4); }
void Wr64(uint8_t* bytes, size_t at, uint64_t v) { memcpy(bytes + at, &v, 8); }

const size_t kSubfileCount = 0x1C, kSubfileTable = 0x20, kSubfileEntry = 16;
const size_t kAbcPointer = 0x98;     // movie header: self-relative u64 to the AS3 block
const size_t kAbcBlockHeader = 12;   // {u64 1, u32 ABC size}

// Index byte code (decoder iggy_w64.dll 0x59860..0x59bb1; the full list is in
// ClaudeTools\iggy\lobyhud_abc.py): fe n = skip n + 1 bytes, ff u32 = skip u32
// bytes, fd s k (offset, kind)... = s bytes with k fields. Vanilla walks the
// AS3 block as fd 0f 01 00 02 (15 bytes, relative pointer at +0) + ff <rest>.
// Without these commands for the new block its +0 keeps the raw 1 and Iggy
// crashes following it as a pointer (tried on LOBYHUD).
const uint8_t kIndexSkipSmall = 0xfe, kIndexSkipLarge = 0xff;
const uint8_t kIndexAbcBlockHeader[] = {0xfd, 0x0f, 0x01, 0x00, 0x02};
const size_t kIndexAbcBlockHeaderWalk = 0x0f;

}  // namespace

// FNV-1a 64 of `size` bytes (offset basis 0xcbf29ce484222325, prime
// 0x100000001b3) - the hash ClaudeTools\iggy\*_abc.py print for a vanilla ABC.
uint64_t Fnv64(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
    return hash;
}

// See iggy_abc.h.
bool ReplaceIggyAbc(const uint8_t* data, size_t size, uint32_t expectedAbcSize, uint64_t expectedAbcHash,
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
    const size_t field = movieOffset + kAbcPointer;
    uint64_t block = field + Rd64(data, field);
    if (block < movieOffset || block + kAbcBlockHeader > (uint64_t)movieOffset + movieSize ||
        Rd64(data, (size_t)block) != 1) {
        note = "unexpected layout";
        return false;
    }
    uint32_t oldAbcSize = Rd32(data, (size_t)block + 8);
    if (oldAbcSize != expectedAbcSize || block + kAbcBlockHeader + oldAbcSize > (uint64_t)movieOffset + movieSize ||
        Fnv64(data + block + kAbcBlockHeader, oldAbcSize) != expectedAbcHash) {
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
    note = "script replaced (" + std::to_string(out.size()) + " bytes)";
    return true;
}

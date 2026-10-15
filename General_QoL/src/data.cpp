// Edits to quest, item and TP shop files for game 1.26.01. QXD and IDB
// layouts from XV2-Tools (Xv2CoreLib); EST read from the game's own files.
#include "data.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

namespace {

int32_t rd32(const uint8_t* d, size_t o) { int32_t v; memcpy(&v, d + o, 4); return v; }
uint16_t rd16(const uint8_t* d, size_t o) { uint16_t v; memcpy(&v, d + o, 2); return v; }

// tpq_data.qxd: index 50 = CTP_00_01 (the prologue); the main story and the
// DLC stories (CTP_*) come after it.
const int32_t kPrologue = 50;
const int32_t kStoryType = 0, kParallelType = 1, kExpertType = 4, kNone = -1;

// Item AVAILABILITY (Eternity's name): 0x7fff = in the zeni shop from the
// start (custom skills installed as X2M use it and show at once), 0x7ffe =
// TP medal shop; 30..46 = from some point of the story.
const uint16_t kTpShop = 0x7ffe, kAlways = 0x7fff;

}  // namespace

int QxdClearRequirements(const uint8_t* src, size_t n, bool story, bool parallel, bool expert,
                         std::vector<uint8_t>& out)
{
    if (n < 16 || memcmp(src, "#QXD", 4)) return -1;
    uint32_t count = (uint32_t)rd32(src, 8), first = (uint32_t)rd32(src, 12);
    if ((uint64_t)first + (uint64_t)count * 292 > n) return -1;
    out.assign(src, src + n);
    int changed = 0;
    for (uint32_t i = 0; i < count; ++i) {
        size_t q = first + (size_t)i * 292;
        for (int r = 0; r < 4; ++r) {
            size_t type = q + 48 + r * 4, index = q + 64 + r * 4;
            int32_t t = rd32(src, type), v = rd32(src, index);
            if ((story && t == kStoryType && v >= kPrologue) || (parallel && t == kParallelType && v >= 0) ||
                (expert && t == kExpertType && v >= 0)) {
                memcpy(out.data() + type, &kNone, 4);
                memcpy(out.data() + index, &kNone, 4);
                ++changed;
            }
        }
    }
    return changed;
}

int QxdUnlockFromStart(std::vector<uint8_t>& out)
{
    // Unlock kind at record +0x50, read by the game 1.26.01 at:
    //   0x83e160 (unlock check): 0 = unlock at once, 1 = all requirements
    //            cleared, 2 = a flag, 4 = level; 3 is not handled there
    //   0x83e050 (shown at the counter): kind 0, or the parent (shorts +0x28
    //            type / +0x2a index) unlocked, or the quest itself cleared
    //   0x59b910 (list filter): kind 3 is listed only once cleared
    // Found by reverse engineering the game.
    const size_t kUnlockKind = 80;
    if (out.size() < 16 || memcmp(out.data(), "#QXD", 4)) return -1;
    uint32_t count = (uint32_t)rd32(out.data(), 8), first = (uint32_t)rd32(out.data(), 12);
    if ((uint64_t)first + (uint64_t)count * 292 > out.size()) return -1;
    const int32_t unlockedFromStart = 0;
    int changed = 0;
    for (uint32_t i = 0; i < count; ++i) {
        size_t kind = first + (size_t)i * 292 + kUnlockKind;
        if (rd32(out.data(), kind) == unlockedFromStart) continue;
        memcpy(out.data() + kind, &unlockedFromStart, 4);
        ++changed;
    }
    return changed;
}

int IdbClearStoryPoint(const uint8_t* src, size_t n, std::vector<uint8_t>& out)
{
    const size_t entry = 64 + 3 * 240;
    if (n < 16 || memcmp(src, "#IDB", 4)) return -1;
    uint32_t count = (uint32_t)rd32(src, 8), first = (uint32_t)rd32(src, 12);
    if (first != 16 || 16 + (uint64_t)count * entry != n) return -1;
    out.assign(src, src + n);
    int changed = 0;
    for (uint32_t i = 0; i < count; ++i) {
        size_t at = first + i * entry + 20;
        uint16_t v = rd16(src, at);
        if (v != 0 && v < kTpShop) {
            memcpy(out.data() + at, &kAlways, 2);
            ++changed;
        }
    }
    return changed;
}

int EstAllSeasons(const uint8_t* src, size_t n, std::vector<uint8_t>& out)
{
    const int kSeasons = 4;
    if (n < 16 || memcmp(src, "#EST", 4)) return -1;
    int32_t count = rd32(src, 8);
    if (count < 0 || 16 + (uint64_t)count * 12 != n) return -1;
    // Every distinct (type, ID), in first-seen order.
    std::vector<std::pair<int32_t, int32_t>> items;
    std::set<std::pair<int32_t, int32_t>> seen;
    for (int32_t i = 0; i < count; ++i) {
        int32_t season = rd32(src, 16 + i * 12 + 8);
        if (season < 0 || season >= kSeasons) return -1;
        std::pair<int32_t, int32_t> it{rd32(src, 16 + i * 12), rd32(src, 16 + i * 12 + 4)};
        if (seen.insert(it).second) items.push_back(it);
    }
    // Group by kind as the shop's own seasons do: skills, the four costume
    // parts (a set's parts stay together), accessories, super souls, others.
    auto group = [](int32_t type) {
        if (type >= 10 && type <= 12) return 0;
        if (type >= 0 && type <= 3) return 1;
        return type + 2;
    };
    std::stable_sort(items.begin(), items.end(),
                     [&](const auto& a, const auto& b) { return group(a.first) < group(b.first); });
    int32_t total = (int32_t)items.size() * kSeasons;
    out.assign(src, src + 16);
    memcpy(out.data() + 8, &total, 4);
    out.reserve(16 + (size_t)total * 12);
    for (int32_t s = 0; s < kSeasons; ++s)
        for (const auto& it : items) {
            int32_t e[3] = {it.first, it.second, s};
            out.insert(out.end(), (const uint8_t*)e, (const uint8_t*)e + 12);
        }
    return (int32_t)items.size();
}

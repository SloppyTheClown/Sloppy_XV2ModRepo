// ---------------------------------------------------------------------------
//  The SaveSync merge: a C++ port of offline\savesync.py (its rules were
//  confirmed in game on 2026-10-11), on the game's decrypted save image. The
//  offsets are DBXV2.sav file offsets (Xv2CoreLib SAV_File.cs Offsets,
//  measured on a 1.26.01 save - Research_Notes\save-flags-and-unlocks.md
//  "SaveSync"). Keep this file and the script in step.
//
//  * SYSFLAGs: OR, except the few multi-bit counters in kKeepOwnFlags.
//  * Quest records: matched by (type, quest id) - the order differs per
//    slot; per quest the furthest state, best rank, best score and the
//    cleared-once flag win; a quest only another slot has goes in a free
//    record.
//  * Tokipedia (Infinite History pages): OR.
//  * Mentor progress: highest friendship and dual-attack gauge; flags OR
//    except LICENSED / DISMISSAL (enrolment, kept per slot).
//  * Shenron "more characters" wishes made: highest (the game unlocks that
//    many wish characters).
// ---------------------------------------------------------------------------
#include "merge.h"
#include <algorithm>
#include <cstring>
#include <set>

namespace {

// Per-character blocks: the main one and the DLC one (Xv2CoreLib
// Offsets.CAC_SIZE / CAC_DLC_SIZE).
const size_t kCharacterSize = 50888, kDlcCharacterSize = 25392;
const size_t kCharacterName = 124976 + 68, kCharacterNameSize = 64;  // ASCII

const size_t kSysFlags = 95280, kSysFlagBytes = 1024;  // 8192 bits; the game sets up to 7347
// Flag groups holding a counter or a "which line next" choice in several
// bits (lobby_event_manage.tsd names), so OR-ing them would make up a value:
// EH_NIL_BAQ_COUNT 3607, EH_NIL_BAQ_AGAIN_01..05 3609-3613,
// EH_NIL_SECURITY_TALK_3_0..3 3653-3656. Each slot keeps its own.
const int kKeepOwnFlags[] = {3607, 3609, 3610, 3611, 3612, 3613, 3653, 3654, 3655, 3656};

// 24-byte quest records {i32 type, quest id, state (0 none, 1 open, 2
// played, 3 cleared), rank (6 best), cleared-once flag, best score};
// unused = type -1. The 888 bytes before TPQ (96952..97840) are not quest
// records.
struct QuestArray {
    const char* name;
    int32_t     recordType;
    size_t      offset;
    int         count;
    size_t      stride;
};
const QuestArray kQuestArrays[] = {
    {"TPQ story", 0, 97840, 128, kCharacterSize},       {"TMQ parallel", 1, 100912, 192, kCharacterSize},
    {"BAQ rift", 2, 105520, 128, kCharacterSize},       {"TCQ mentor lessons", 3, 108592, 256, kCharacterSize},
    {"HLQ expert", 4, 114736, 96, kCharacterSize},      {"RBQ raid", 5, 117040, 96, kCharacterSize},
    {"CHQ Elder Kai", 6, 119344, 64, kCharacterSize},   {"LEQ Frieza siege", 7, 120880, 128, kCharacterSize},
    {"OSQ Infinite History", 11, 524984, 39, kDlcCharacterSize}, {"PRB", 12, 525944, 39, kDlcCharacterSize},
    {"TPQ expansion", 0, 527144, 30, kDlcCharacterSize}, {"TMQ expansion", 1, 530120, 31, kDlcCharacterSize},
};
const size_t kRecordSize = 24;
enum RecordField { kType, kQuestId, kState, kRank, kClearedOnce, kScore, kRecordFields };
const int32_t kUnused = -1;

const size_t kTokipedia = 532280, kTokipediaBytes = 12 * 8;  // Xv2CoreLib Offsets.TOKIPEDIA (u64 flags each)

// 8-byte entries {u16 friendship 0..1000, u16 dual-attack gauge 0..4000,
// u32 flags}; flag bits = SET_MASTER_FLAG names (save-flags-and-unlocks.md).
const size_t kMentorProgress = 125956, kMentorEntrySize = 8;
const int kMentorCount = 34;
const uint32_t kMentorFlagLicensed = 1u << 1, kMentorFlagDismissal = 1u << 2;
const uint32_t kMentorKeepOwn = kMentorFlagLicensed | kMentorFlagDismissal;

// Shenron wish counts: 32 bytes in PlayData (file 126276 + 168), one per wish
// type of wish_script.dbw. Wish characters are no flag: the character
// select's check_unlock (0x5e2560) unlocks the first N wish characters, N =
// byte 7 (read by 0x84ad70 -> 0x83f1f0). Only that byte is merged; the other
// wishes give items, so raising their counts would skip the reward.
const size_t kWishCounts = 126276 + 168, kWishCharacters = 7;

struct Record { int32_t field[kRecordFields]; };

Record ReadRecord(const std::vector<uint8_t>& image, size_t at)
{
    Record record;
    memcpy(record.field, image.data() + at, sizeof(record.field));
    return record;
}

int PopCount(uint8_t value)
{
    int bits = 0;
    for (; value; value &= value - 1) ++bits;
    return bits;
}

void MergeSysFlags(std::vector<uint8_t>& image, const std::vector<int>& slots, std::map<int, MergeReport>& report)
{
    uint8_t merged[kSysFlagBytes] = {};
    for (int slot : slots) {
        const uint8_t* own = image.data() + kSysFlags + slot * kCharacterSize;
        for (size_t i = 0; i < kSysFlagBytes; ++i) merged[i] |= own[i];
    }
    for (int slot : slots) {
        uint8_t* own = image.data() + kSysFlags + slot * kCharacterSize;
        uint8_t result[kSysFlagBytes];
        memcpy(result, merged, sizeof(result));
        for (int flag : kKeepOwnFlags) {
            uint8_t bit = (uint8_t)(1 << (flag % 8));
            result[flag / 8] = (uint8_t)((result[flag / 8] & ~bit) | (own[flag / 8] & bit));
        }
        for (size_t i = 0; i < kSysFlagBytes; ++i) report[slot].flags += PopCount((uint8_t)(result[i] & ~own[i]));
        memcpy(own, result, sizeof(result));
    }
}

// Whether an array looks as measured: only its own record type, each quest
// once, unused records only at the end (the game might stop reading at the
// first unused one).
bool CheckArray(const std::vector<uint8_t>& image, size_t start, const QuestArray& array, int slot, std::string& error)
{
    std::set<std::pair<int32_t, int32_t>> seen;
    bool freeSeen = false;
    for (int i = 0; i < array.count; ++i) {
        Record record = ReadRecord(image, start + i * kRecordSize);
        if (record.field[kType] == kUnused) { freeSeen = true; continue; }
        const char* problem = record.field[kType] != array.recordType ? "a record of another type"
                              : !seen.insert({record.field[kType], record.field[kQuestId]}).second ? "a quest listed twice"
                              : freeSeen ? "an unused record before a used one" : nullptr;
        if (problem) {
            error = "slot " + std::to_string(slot) + " " + array.name + ": " + problem;
            return false;
        }
    }
    return true;
}

bool MergeQuestArray(std::vector<uint8_t>& image, const std::vector<int>& slots, const QuestArray& array,
                     std::map<int, MergeReport>& report, std::string& error)
{
    std::map<std::pair<int32_t, int32_t>, Record> best;
    for (int slot : slots) {
        size_t start = array.offset + slot * array.stride;
        if (!CheckArray(image, start, array, slot, error)) return false;
        for (int i = 0; i < array.count; ++i) {
            Record record = ReadRecord(image, start + i * kRecordSize);
            if (record.field[kType] == kUnused) continue;
            auto key = std::make_pair(record.field[kType], record.field[kQuestId]);
            auto found = best.find(key);
            if (found == best.end()) { best[key] = record; continue; }
            for (int field : {kState, kRank, kClearedOnce, kScore})
                found->second.field[field] = std::max(found->second.field[field], record.field[field]);
        }
    }
    for (int slot : slots) {
        size_t start = array.offset + slot * array.stride;
        std::map<std::pair<int32_t, int32_t>, int> indexByKey;
        std::vector<int> freeRecords;
        for (int i = 0; i < array.count; ++i) {
            Record record = ReadRecord(image, start + i * kRecordSize);
            if (record.field[kType] == kUnused) freeRecords.push_back(i);
            else indexByKey[{record.field[kType], record.field[kQuestId]}] = i;
        }
        size_t nextFree = 0;
        for (const auto& [key, merged] : best) {
            int index;
            auto own = indexByKey.find(key);
            if (own != indexByKey.end()) {
                index = own->second;
                if (memcmp(image.data() + start + index * kRecordSize, merged.field, kRecordSize) == 0) continue;
            } else if (nextFree < freeRecords.size()) {
                index = freeRecords[nextFree++];
            } else {
                error = "slot " + std::to_string(slot) + " " + array.name + ": no free record for quest " +
                        std::to_string(key.second);
                return false;
            }
            memcpy(image.data() + start + index * kRecordSize, merged.field, kRecordSize);
            ++report[slot].questRecords;
        }
    }
    return true;
}

void MergeTokipedia(std::vector<uint8_t>& image, const std::vector<int>& slots, std::map<int, MergeReport>& report)
{
    uint8_t merged[kTokipediaBytes] = {};
    for (int slot : slots) {
        const uint8_t* own = image.data() + kTokipedia + slot * kDlcCharacterSize;
        for (size_t i = 0; i < kTokipediaBytes; ++i) merged[i] |= own[i];
    }
    for (int slot : slots) {
        uint8_t* own = image.data() + kTokipedia + slot * kDlcCharacterSize;
        for (size_t i = 0; i < kTokipediaBytes; ++i) report[slot].tokipedia += PopCount((uint8_t)(merged[i] & ~own[i]));
        memcpy(own, merged, sizeof(merged));
    }
}

struct MentorEntry {
    uint16_t friendship;
    uint16_t gauge;
    uint32_t flags;
};

void MergeMentors(std::vector<uint8_t>& image, const std::vector<int>& slots, std::map<int, MergeReport>& report)
{
    for (int mentor = 0; mentor < kMentorCount; ++mentor) {
        uint16_t friendship = 0, gauge = 0;
        uint32_t sharedFlags = 0;
        for (int slot : slots) {
            MentorEntry entry;
            memcpy(&entry, image.data() + kMentorProgress + slot * kCharacterSize + mentor * kMentorEntrySize, sizeof(entry));
            friendship = std::max(friendship, entry.friendship);
            gauge = std::max(gauge, entry.gauge);
            sharedFlags |= entry.flags & ~kMentorKeepOwn;
        }
        for (int slot : slots) {
            uint8_t* at = image.data() + kMentorProgress + slot * kCharacterSize + mentor * kMentorEntrySize;
            MentorEntry own;
            memcpy(&own, at, sizeof(own));
            MentorEntry merged{friendship, gauge, sharedFlags | (own.flags & kMentorKeepOwn)};
            if (memcmp(&merged, &own, sizeof(merged)) == 0) continue;
            memcpy(at, &merged, sizeof(merged));
            ++report[slot].mentors;
        }
    }
}

// Highest "more characters" wish count of the slots, for all of them.
void MergeWishCharacters(std::vector<uint8_t>& image, const std::vector<int>& slots, std::map<int, MergeReport>& report)
{
    uint8_t highest = 0;
    for (int slot : slots) highest = std::max(highest, image[kWishCounts + slot * kCharacterSize + kWishCharacters]);
    for (int slot : slots) {
        uint8_t& own = image[kWishCounts + slot * kCharacterSize + kWishCharacters];
        report[slot].wishCharacters = highest - own;
        own = highest;
    }
}

}  // namespace

std::string CharacterName(const uint8_t* image, int slot)
{
    const char* name = (const char*)image + slot * kCharacterSize + kCharacterName;
    return std::string(name, strnlen(name, kCharacterNameSize));
}

bool MergeSlots(std::vector<uint8_t>& image, const std::vector<int>& slots, std::map<int, MergeReport>& report,
                std::string& error)
{
    if (image.size() < kMergedImageSize) { error = "image too small"; return false; }
    std::vector<uint8_t> merged = image;  // all or nothing: a layout surprise leaves `image` as it was
    report.clear();
    for (int slot : slots) report[slot] = MergeReport();
    MergeSysFlags(merged, slots, report);
    for (const QuestArray& array : kQuestArrays)
        if (!MergeQuestArray(merged, slots, array, report, error)) return false;
    MergeTokipedia(merged, slots, report);
    MergeMentors(merged, slots, report);
    MergeWishCharacters(merged, slots, report);
    image.swap(merged);
    return true;
}

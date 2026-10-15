// The SaveSync merge on a decrypted save image - see merge.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Bytes of the image the merge reads and writes: everything before the
// save's fourth section (file offset 0xdf1f8, where the serializer's fourth
// pointer goes - SaveData +0xdf260 - 0x68). Every per-CaC area merged lies
// below it.
static const size_t kMergedImageSize = 0xdf1f8;

static const int kSlotCount = 8;

struct MergeReport {
    int flags = 0;         // SYSFLAG bits gained
    int questRecords = 0;  // quest records changed or added
    int tokipedia = 0;     // Tokipedia bits gained
    int mentors = 0;       // mentor entries changed
    int wishCharacters = 0;  // Shenron wish characters gained
};

// The CaC name of `slot` (ASCII), empty for an empty slot. `image` holds at
// least kMergedImageSize bytes.
std::string CharacterName(const uint8_t* image, int slot);

// Gives every slot in `slots` the union of their unlocks and the best value
// per quest, in place. False, with the reason in `error` and `image`
// unchanged, when a quest array is not laid out as measured.
bool MergeSlots(std::vector<uint8_t>& image, const std::vector<int>& slots, std::map<int, MergeReport>& report,
                std::string& error);

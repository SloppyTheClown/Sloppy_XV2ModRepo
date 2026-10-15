// Replacing an .iggy menu's ActionScript (AS3) - see iggy_abc.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Builds a copy of the .iggy file `data` (size bytes) with its AS3 block
// replaced by `abc` (abcSize bytes), if its current ABC is the one expected
// (expectedAbcSize bytes with FNV-1a 64 hash expectedAbcHash).
// Returns true with the new file in `out` and "script replaced (N bytes)" in
// `note`; false, with the reason in `note` ("unexpected layout", "menu script
// is not the vanilla one ..."), when the file is laid out differently or
// another mod already changed its script. `data` is never changed.
bool ReplaceIggyAbc(const uint8_t* data, size_t size, uint32_t expectedAbcSize, uint64_t expectedAbcHash,
                    const uint8_t* abc, size_t abcSize, std::vector<uint8_t>& out, std::string& note);

// FNV-1a 64 hash of `size` bytes (how vanilla ABCs are identified).
uint64_t Fnv64(const uint8_t* bytes, size_t size);

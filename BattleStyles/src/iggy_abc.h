// Replacing an .iggy menu's ActionScript (AS3) - see iggy_abc.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// `data` with its AS3 block replaced by `abc`, if its current block is the
// one expected (size and FNV-1a hash of the vanilla ABC). False, with the
// reason in `note`, when the file is laid out differently or another mod
// already changed its script.
bool ReplaceIggyAbc(const uint8_t* data, size_t size, uint32_t expectedAbcSize, uint64_t expectedAbcHash,
                    const uint8_t* abc, size_t abcSize, std::vector<uint8_t>& out, std::string& note);

uint64_t Fnv64(const uint8_t* bytes, size_t size);

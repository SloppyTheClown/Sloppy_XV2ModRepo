// Swapping a menu's ActionScript (ABC) in an .iggy file as it loads.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Puts `abc` in place of the movie's AS3 block when that block is the vanilla
// one (size + FNV-1a hash, from the generated *_abc.h headers); another mod's
// script stays as it is. `note` says what went wrong. Returns whether `out`
// holds the patched file.
bool PatchIggyAbc(const uint8_t* data, size_t size, uint32_t vanillaSize, uint64_t vanillaHash,
                  const uint8_t* abc, size_t abcSize, std::vector<uint8_t>& out, std::string& note);

// AVATAR_FACULTY.iggy (attribute points) with the shared AvatarStatus
// (as3\AvatarStatus.as, built into avatar_faculty_abc.h) and its free points
// flag set: points confirmed earlier can be taken back out, down to 0. When
// another mod (Combined Supers) already serves the shared script, only the
// flag is set in that copy.
bool PatchAvatarFaculty(const uint8_t* data, size_t size, std::vector<uint8_t>& out, std::string& note);

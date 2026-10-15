#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Lobby scripts (.tsr): in every IF whose "else" label is `label` and whose
// condition contains `from`, replace `from` with `to` (same length, so the
// file keeps its layout). `out` gets the patched copy; returns the number of
// conditions changed, or -1 if the file does not have the expected layout.
int TsrReplaceInIf(const uint8_t* src, size_t n, const char* label, const char* from, const char* to,
                   std::vector<uint8_t>& out);

// Any file with plain-text strings (.tsd): if `needle` occurs exactly once,
// `out` gets a copy with `from` inside that occurrence replaced by `to` (same
// length). Returns the number of occurrences found (1 = done), or -1 if
// `from` is not in `needle` or the lengths differ.
int ReplaceInUnique(const uint8_t* src, size_t n, const char* needle, const char* from, const char* to,
                    std::vector<uint8_t>& out);

// Lobby NPC IDs first..last.
struct NpcRange {
    int first, last;
};
using NpcRanges = std::vector<NpcRange>;

// Lobby event table (.tsd): in the conditions of the talk triggers (type 1)
// of the lobby NPCs in `npcs`, every `from` becomes `to` (same length).
// `out` gets the patched copy; returns the number of triggers changed, or -1
// if the file does not have the expected layout.
int TsdReplaceForNpcs(const uint8_t* src, size_t n, const NpcRanges& npcs, const char* from, const char* to,
                      std::vector<uint8_t>& out);

// Same, for every story quest requirement "QUEST(TPQ,STATE,CTP_NN_NN)==3":
// each becomes kAlwaysEvent.
int TsdStoryAlways(const uint8_t* src, size_t n, const NpcRanges& npcs, std::vector<uint8_t>& out);

// Conditions true for every character (level >= 1), each as long as the
// condition it replaces. Event table conditions may carry spaces - the game's
// own "( AVATAR(LEVEL)>=1)" does; the lobby scripts never do, so the script
// one is padded with a second, valid term instead.
static const char kAlwaysScript[] = "AVATAR(LEVEL)>=1||QUEST(TMQ,STATE,TMQ_1400)==3";  // QUEST(TPQ,STATE,CTP_04_04)==CONST(QUEST_CLEAR)
static const char kAlwaysEvent[] = "             AVATAR(LEVEL)>=1";                   // QUEST(TPQ,STATE,CTP_NN_NN)==3
// Never true (no quest state is 9), for triggers that must not fire.
static const char kNeverPrologue[] = "QUEST(TPQ,STATE,CTP_00_01)==9";                  // QUEST(TPQ,STATE,CTP_00_01)!=3

#pragma once
#include <cstdint>
#include <vector>

// Quest files (.qxd, 292 bytes per quest): each quest has up to 4
// requirements, types at +48 and quest indices at +64; type and index -1 =
// no requirement. Type 0 = the TPQ list (the story from index 50, CTP_00_01,
// the prologue), type 1 = a parallel quest (TMQ), type 4 = an expert mission
// (HLQ).
// Clears every requirement on a story quest (`story`), a parallel quest
// (`parallel`) and/or an expert mission (`expert`). `out` gets the patched
// copy; returns the number of requirements cleared, or -1 if the file does
// not have the expected layout.
int QxdClearRequirements(const uint8_t* src, size_t n, bool story, bool parallel, bool expert,
                         std::vector<uint8_t>& out);

// Parallel quests (tmq_data.qxd): sets every record's unlock kind (+80) to 0
// = unlocked from the start. Clearing the requirements is not enough: the
// quest counter lists a parallel quest only when its parent quest (+40/+42)
// is unlocked or its kind is 0, and kind 3 quests (TMQ_0301, TMQ_0401, ...)
// are only opened by story events. Edits `out` in place (a copy made by
// QxdClearRequirements); returns the number of records changed, or -1 if the
// file does not have the expected layout.
int QxdUnlockFromStart(std::vector<uint8_t>& out);

// Item files (.idb, 64 + 3x240 bytes per entry): the u16 at +20
// (AVAILABILITY) is the story point from which the shop sells the item, or
// 0x7fff = from the start, 0x7ffe = TP medal shop. Every story point becomes
// 0x7fff. Returns the number of items changed, or -1 if the file does not
// have the expected layout.
int IdbClearStoryPoint(const uint8_t* src, size_t n, std::vector<uint8_t>& out);

// TP medal shop table (EventShopTbl*.est): "#EST", u16 0xfffe, u16, i32
// count, i32, then count x (i32 item type, i32 item ID, i32 season 0..3); the
// shop lists the current season's entries. `out` lists every item in every
// season, grouped by item type in the order the file first has them.
// Returns the number of items per season, or -1 if the layout is unexpected.
int EstAllSeasons(const uint8_t* src, size_t n, std::vector<uint8_t>& out);

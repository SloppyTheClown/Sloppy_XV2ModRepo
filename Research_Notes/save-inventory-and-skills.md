# Inventory and skills in memory

Covers: the save manager's inventory/skill lists in memory, giving items (0x616b80) and unlocking skills (0x84cbd0) - the debug menu's item getter.
Split out of `save-flags-and-unlocks.md` (2026-10-22).

## Inventory and skills in memory; giving items (2026-10-12)
Used by DebugMenu's item getter. Static analysis of a decrypted 1.26.01
image (`ClaudeTools\exe` da.py / xref.py); confirmed in game (item getter works).
* **Inventory = mgr + 0x20e8** (1.26: 0x20d8) (getter 0x828bb0; writer 0x82d390 to image
  +0x1410 = file 5136, reader 0x82dbb0). 10 std::lists of 0x10 bytes
  (+0 head sentinel, +8 count), one per EquipmentType (Xv2CoreLib
  `SAV_File.cs`): 0 top, 1 bottom, 2 gloves, 3 shoes, 4 accessory,
  5 super soul, 6 material, 7 important (extra), 8 capsule (battle),
  9 QQ bang. List = 0x82d2f0(inv, type) = inv + type*0x10.
  Node: +0 next, +8 prev, +0x10 type, +0x14 id, +0x18 count, +0x1c max,
  +0x20 flags.
* **Add primitive 0x83af40(list, type, id, count, flags)**: fails for a
  negative type/id; capacity 0x400 nodes (0x100 for type 9); max 99, type 7
  always count 1 / max 1; adding to an existing id **fails** if the sum passes
  the max (no clamp) - so give only `max - have`. Wrapper 0x82cda0 also posts
  events (0x863cf0, queue `[0x1554438]`).
* **Give 0x854e80(type, id, count, silent)**: types 0-9 set the
  acquired/new flag (mgr+0x2218 object, 0x82f140), call 0x4c0f40, then
  0x82cda0; type 7 items with IDB +0xc == 0x42 also set partner key flags
  (0x824490). Types 0x10-0x13 go to other flag functions (not mapped).
  **0x616b80(type, id, count, silent) is the safe entry**: it first checks the
  item exists in the loaded IDB (type 9 skipped), returns 1 / 0.
  0x61aa00(type, id) = the loaded IDB entry (+4 Name_MsgID).
  The give does NOT check DLC ownership.
* **DLC ownership of an item** (static, decrypted image; 2026-10-12): IDB
  +0xe DLC_FLAG (i16, XV2DataEdit "I_10") = a content pack id, not a
  bitmask. 0x616c10(flag) maps -1 (0xffff) / 0xff to 0x44 = none, else the
  flag itself; 0x2b9d50(pack, mode) = ownership: pack >= 0x43 -> 1; mode 1
  returns "owned", mode 0 also accepts a second state (unknown). Callers
  e.g. 0x667880 (LoadBGShop, 3 flags), 0x63d0aa / 0x641f73 (via 0x852270 /
  0x852a80 shop tables). Quests use a bitmask instead (0x59c0c0 / 0x617b20:
  bit 0..21 -> packs 5,6,7,8,21,22,23,30,31,35,36,42,44,46,48,50,-,55,57,59,
  61,64). 0x2b9d50 calls 0x2ba520, hooked by XV2Patcher. Skills: their DLC
  flag is in skill_item.idb, (Type = CUS category index 0/1/2/5, ID = ID2);
  840 of 843 records match a CUS skill. Debug menu gate: not seen in game.
* **Loaded IDB in memory** (static, 2026-10-12): `0x61aa00(type, id)` =
  `0x85f180(0x86aa60([0x1568308], table_0x10c85a0[type]), id)`. Table
  0x10d6f80 (give type -> IDB index): 0..8 -> 2,3,4,5,6,7,1,8,0; 9 -> -1;
  10..15 -> 9 (skill_item.idb); 16..19 -> 10..13. 0x86aa60 jumps to
  resource slot index+0xb and returns the raw IDB file (+8 count, +0xc
  first record, 0x310-byte records). 0x85f180 matches the id (+0) only -
  ambiguous for skill_item.idb, whose ids repeat per type, so the debug
  menu walks it matching +0xc type too. 0x616c10 = `flag+1 & ~0x100 ? flag
  : 0x44`.
* **Skills = mgr + 0x40a8** (1.26: 0x4098) (getter 0x828c50; writer 0x83e6a0 to image
  +0xb430 = file 46128, reader 0x83ea30). 6 lists in CUS order (super,
  ultimate, evasive, unknown, blast, awoken); every CUS skill has a node.
  Record at node+0x10: +0 unlocked (int), +4 ID1, +0xc type, +0x10 ID2.
  Find: 0x84c880(skills, category, ID2) -> record or 0 (compares node+0x20).
  0xba720(record) = `[record]` (callers test == 1).
  **Unlock: 0x84cbd0(record, 1)** - writes record +0 and posts event 6
  (callers at 0x3ad6fa, 0x422018 do find -> check -> unlock).
* Item and skill names offline: XV2DataEdit `msg.Names`
  (data-files-and-xv2dataedit.md). In game: Debug Menu's `debug_menu.lua`
  reads the same IDB/CUS/msg layouts in Lua through the loader's
  `files.ReadGameFile`.

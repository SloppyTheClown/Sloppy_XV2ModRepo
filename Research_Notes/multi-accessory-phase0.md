# Multi accessory - phase 0 findings

Split out of multi-accessory.md (index there).

## Phase 0 findings (static, 1.26.01)
* **Call chain.** Model builder (RTTI-less object used by EditChara,
  EditMenu, CharaBg, lobby...) `0x65cf60(builder, desc, ...)` = set
  appearance, under the builder's Monitor lock at +0x218. `desc` is 0xb0
  bytes: +0x00..+0x27 ten part set ids, **+0x28 accessory part set id**
  (= BCS part set index, e.g. 846 glasses, 835 hat, 854 scouter),
  +0x2c..+0xab 32 dwords (colours), +0xac. Previous descs are kept at
  builder +0x168 / +0x244. The accessory loader is called from **one site,
  0x65d400**, only when +0x28 changed or parts changed.
* **Loader `0x65c6c0(builder, partSet)`** needs builder +0x108 (load state)
  == 0 and +0x18 (the race's chara resource / BCS) set. It **flips
  +0xb0 (buffer 0/1)** and fills that buffer; partSet < 0 = unequip
  (flip, state 2, return 1). Per buffer b, three std::vectors (24 bytes):
  * +0x80 + 24b: part holders (0x60, ctor 0x29ce40, one per call, made when
    a part has flags & 4; parts loaded via 0x29d620) - hair replacers.
  * +0x20 + 24b: physics objects (0x220, ctor 0x29aab0, loaded by 0x29aef0
    with the holder part's dyt (vt+0x30) as external dyt; from each part's
    physics list (+0x4a count, +0x4c offset, 0x48 stride, flags & 4) and the
    part set's own list (+0x16 count, +0x1c offset)).
  * +0x50 + 24b: the part index of each physics object.
  Ends with state 2; returns 1 if the buffer holds anything.
* **Ready check** `0x657e70` and **apply** `0x65e290` walk the whole
  vectors (any count). Apply frees the *other* buffer (holders: restore the
  base part per slot via Chara vt+0x368, then 0x29ce90 + HeapFree;
  physics: vt[0](1)), clears it, then attaches the current one: holder
  parts -> `Chara vt+0x368(slot, part)`, physics object i ->
  `vt+0x390(i, obj)` + `vt+0x398(i, partIndex)`.
* **Hard cap: 4 accessory physics objects per character.** The Chara
  (`Chara@Common`, vtable 0xf9bb00, builder +0xb8) allocates 0x22 slot
  objects (0x1b0, ctor 0x28be40) at +0x3c0 in its ctor (0x27d6e0 loop):
  30 regular physics slots +0x3c0..+0x4af, then **4 accessory slots
  +0x4b0..+0x4cf** with part indices at +0x4d0 (4 x i32). vt+0x390
  (0x27e460) ignores i >= 4 (no crash, just not drawn); vt+0x3a0
  (0x282b60) clears all 4.
* **Verdict: go.** Storage is a list, so loading a second part set into
  the same buffer should attach and free with the first. Limits: total
  physics objects across all accessories <= 4 (glasses/nose = 1 each;
  hats are holders, which use no slots); two holders on the same part slot
  -> the last attached wins.
* **Second path: CharaResourceBase** (`CharaResourceBase@Common` /
  `LobbyCharaResourceBase@Lobby`, vtables 0xf8fa28 / 0x107fff0, slot [5] =
  0x297ac0 -> 0x299ac0). Used by the lobby and save-slot screen (they never
  call 0x65c6c0), probably battle. The accessory part set is
  **record +0x20 (i16, <0 none)**, record = `0x27a330(*0x1568240,
  resource+0xc8)` (0xe0-byte records, id at +0x90). 0x299ac0 reads it 3x:
  * part replacement: 0x29a700(res, entry, slot 0..9): the accessory's part
    for that slot replaces the base part when flags & 4 (hats). Also called
    from 0x297c3b (second branch of 0x297ac0, not traced).
  * part-level physics: 0x29a4d0(res, &vector<0x28 entries>, holder, res+0xd8,
    res+0xdc), one call at 0x299c46; push_backs flags-4 physics of each
    accessory part, using the holder part's dyt (0x29d2a0 + vt+0x30).
  * part set physics: loop at 0x299cfb over BCS part set +0x16/+0x1c
    -> 0x29d470(holder, phys, -1, d8, 0, 0, dc).
  Objects go into a std::list at holder +0x50 (0x29d470).
* **Live probe** `ClaudeTools\multiacc_probe\` (MultiAccProbe.dll, loader
  mod), ids from `extra_accessories.txt` (default 846), log
  `multiacc_probe.log`. Install/remove: `Build_Install.bat` /
  `Build_Uninstall.bat`.
  * Builder hook (call at 0x65d400): after the game's load, un-flip +0xb0,
    +0x108 = 0, call the loader again per id. **Works in game
    (2026-10-25)**: equip screen preview shows glasses 846 together with the
    equipped accessory (hats: holders 1 + physics 1; physics accessories:
    physics 2). Lobby and save-slot screen showed nothing (other path).
  * Resource hook (call at 0x299c46): after 0x29a4d0, per extra id swap
    record +0x20, call 0x29a4d0 again, restore, then load the part set's
    own physics via 0x29d470. Extras' hair replacement (0x29a700) not
    covered. **Works in game (2026-10-25)**: glasses 846 + the equipped hat
    1406 show in save select, Conton City, character select and battle.
  * **The resource hook applies to every resource, not just ours.** Record
    ids seen in the log (`resource+0xc8`, = record +0x90):
    * 0 = the current CaC (lobby, character select; logged twice per load);
    * 1 = also accessory 1406, seen in battle - probably the player's battle
      copy (not proven; could be a team slot);
    * 4096..4102 = save-slot screen, one per filled slot (4096 + slot index;
      4096 later shows 1406 = the loaded slot);
    * 128..284 = lobby NPC avatars, many with their own accessory (836, 840,
      855, 907...) - the probe gave all of them glasses too;
    * 47, 48 = unknown (in the lobby NPC run; story/vendor NPCs?).
    So the real mod must pick the target by record id (0, 4096 + current
    slot, battle id) and, online, by the other player's data. Log of that
    run: `ClaudeTools\multiacc_probe\logs\2026-10-25_resource_hook_all_chars.log`.
  * **Filter (ids 0, 1, >= 4096 only) works in game (2026-10-25)**: no
    NPC avatar got glasses; every save slot and every CaC in character
    select did; in training with a second CaC as partner only the player had
    them. Corrected id meanings from that log
    (`logs\2026-10-25_record_id_filter.log`):
    * 0 = character select preview of **each** hovered CaC (accessories
      1406/2005/834/2035/843 all under id 0) and the player in battle;
    * 1 = the player entering Conton City (right before the NPC avatars);
    * 3 = the second CaC as training partner (1406 too), so another slot's
      CaC in battle has its own id;
    * 409 shows up right after id 1 (1406 twice, 855 once) - unknown.
    Ids alone don't say which CaC it is (0 is every CaC in character
    select), so per-CaC extras need the CaC's identity from the record.
  * **Record dump (2026-10-25, `logs\2026-10-25_record_dump.log`)**: the
    0xe0 bytes of every record with id < 16 or >= 4096, once per distinct
    content. In game: glasses on every CaC in save select and character
    select and on the player in Conton City and training; training partner
    (id 3, filtered out) without. This run, save select hovers came in as
    **id 0** and character select hovers as **id 4096** (each hovered CaC
    reuses the one id), the player in Conton as id 1. The 7 records
    4096..4102 at boot are placeholders (accessory -1, +0x00 = 100..106 in
    order, +0xc0 = 0).
    * **No save slot index, but the CaC's name**: +0xb0 is an MSVC
      `std::string` (inline buffer or heap pointer at +0xb0, size +0xc0,
      capacity +0xc8, pointer when capacity >= 16) - the "per-CaC value" at
      +0xc0 was each CaC's name length. Other fields: +0x00 i32 race chara
      id (100 HUM .. 107 MAF); +0x04 i32 = save CaC +28, +0x08 i32 = save
      CaC +24 (appearance values); +0x0c..+0x1f part set ids; +0x20
      accessory part set; +0x90 record id; +0x94..+0xa5 u16 skill ids (CUS);
      +0xd0..+0xdf copied, zero so far.
    * **Who fills the record (static)**: 0x653830(CaC object, record, -1 or
      id) copies a CaC into a record: name from 0x107410(CaC) = CaC +0xc,
      race from 0x28ff40(0x81b1c0(0x43140(CaC))), +0x04 from 0xb9190,
      +0x08 from 0x92950, parts from the CaC +0x7c getters. 12 callers
      (0x38bdde, 0x38db8e, 0x38dc43, 0x38dccf, 0x38ddc8, 0x38ff4b, 0x3a5bd0,
      0x3a8d1a, 0x3a9971, 0x3a99f0, 0x3b019d, 0x3f3f96). Its entry is
      already a `jmp` in the running image (XV2Patcher hooks it) - don't
      hook the entry. Pattern (0x4df090): 0x204030 init a local record,
      0x653830(cac, rec, -1), 0x279da0(table) alloc, id 0x1000,
      0x27a5a0(table, rec, id) copies it field by field (name by string
      assign). Boot placeholders 4096..4103 come from 0x4fdcd0 -> 0x6168d0
      (static default table 0x10c8240, stride 0x5c), not from the save.
    * **CaC slots live**: 0x828b40(manager, slot) = `*0x1568310` + 0x120d0 +
      slot * 0xe880 (slot -1 = current, 0x433e0(manager + 8)); name at +0xc.
      So record -> slot = compare the record name with the 8 slot names
      (probe build 2026-10-25, below). Same-named CaCs can't be told apart
      that way (race/+0x04/+0x08 could break ties); online players with one
      of our names would match too.
    * **Mapping by content works for this save**: save CaC block =
      124976 + slot * 50888 (Xv2CoreLib Offsets.CAC / CAC_SIZE): +20 race
      index 0..7 (= record +0x00 - 100), +24 = record +0x08, +28 = record
      +0x04. Those three picked one slot for each of the 8 CaCs (three
      Namekians told apart by +24/+28); the loaded CaC (1406) = slot 5.
      Fragile for look-alike CaCs; the save's part/colour fields are not
      mapped yet. Better: find who fills the record (table `*0x1568240`,
      0x27a330) - the save select / character select code knows the slot.
      Matching script: `ClaudeTools\multiacc_probe\match_slots.py`.
    * Probe bug: two threads dumped at once in training and the lines
      interleaved/dropped (partner id 3 + player id 0) - fixed with a log lock.
  * **Probe build "slot by name" works in game (2026-10-25,
    `logs\2026-10-25_slot_by_name.log`)**: the record-id filter is gone;
    each load logs `name=".." slot=N` and gets the extras of that slot
    (config lines `<slot> <ids>`, `*` = all); the equip screen uses the
    current slot. Test config glasses on slot 0 only: save select and
    character select showed them on slot 0's CaC only, Conton City player
    (slot 5, id 1) none, training: slot 0 as partner (id 3) had them, the
    player (slot 5, id 0) didn't. Every local CaC load resolved to the right
    slot.
    * **Online lobby players come through the same path**: ids ~410..430,
      record = their CaC (name, accessory 855/893/...). So phase 5 has a
      hook point - the record is built from the other player's data; what
      would carry their extras is still open.
    * **False matches seen**: two online players named "PATROLLER" (the
      default name) matched our slot 7. The real mod must only name-match
      local ids (< 16 or >= 4096; lobby players are 410..430, NPCs 144..347
      with names `name_<msg id>`) and break same-name ties by race/+0x04/+0x08.
  * **Part replacement 0x29a700 (static, 2026-10-25)**: `(res, PartEntry
    *out, slot 0..9, flags, arg5)`, called in a 10-slot loop from 0x299b1d
    (0x299ac0) and 0x297c3b (0x297ac0). out (0x10 bytes) = +0 part, +8
    flags (| 0x40 when flags & 8 and slot 1..4), +0xc arg5. Base part = BCS
    part set `record +0x0c + slot*2` (or `res +0xd4` when `res +0xf8` != 0 -
    an override part set, maybe transformations); then the record accessory's
    part for the slot (`0x298200(partSet, slot)`, = part set +0x14 count,
    +0x18 offsets) replaces it when part +0x18 & 4. Entries are then loaded
    per slot by 0x29d620 / 0x29d6d0.
  * **Probe build "hair replacers" (2026-10-25, confirmed in game)**: both
    0x29a700 calls wrapped; after the game's pick, each extra's flags-4 part
    for the slot replaces it (last extra wins, logs `res extra partSet=..
    replaces part slot ..`). The name match is limited to local ids again
    (< 16 or >= 4096). Config `* 835` (hat) + `5 846` (glasses). Result (log
    `ClaudeTools\multiacc_probe\logs\2026-10-25_hair_replacer.log`): hat
    835 only ever replaces part slot 5 (Hair); ids seen 0, 1, 3, 4096.
    * Save select, equip screen and cutscenes show the extra hat. It stacks
      with an equipped attached object (nose, glasses) and with an equipped
      holder accessory (Arale hat + hat 835 both drawn).
    * An equipped hair replacer is overridden (last wins): Hercule hair
      gone; a FRI CaC's helmet replaced and the **head went missing**. Not
      the hat itself: 835 equipped normally works on FRI (user, many
      times). Likely the stacking - two replacers on one part slot (the
      builder holds two holders for slot 5; restoring/freeing them is
      written for one per slot). Not traced; phase 1 avoids it by never
      stacking replacers on a slot.
    * Transformations: SSJ/SSJ2 and other forms keep the hat; SSJ3's hair
      draws over it (user is fine with that). Likely the res+0xf8 override
      path picking the form's hair part.

  * **Why Arale hat + hat stacked fine (BCS dump):** on MAM/MAF, 835 and
    843 slot 5 are both flags-4 parts with model 0 + EMB
    `MAM_000_Face_base` (same part) plus one flags-5 physics object (the
    hat model `MAM_835` / `MAM_843`). Swapping one identical part for the
    other is a no-op, so both physics objects hang. FRI 835 instead is a
    model -1, flags 0 part + flags-4 physics; helmet 2035 replaces Hair
    with model 2035 - different part, head vanished.


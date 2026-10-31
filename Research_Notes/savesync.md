# SaveSync: per-CaC data and merging slots

Covers: what in the save is per CaC, how SaveSync merges slots offline, and the live save image in game.
Split out of `save-flags-and-unlocks.md` (2026-10-22).

## SaveSync: what is per CaC and how slots merge (2026-10-11)
* Per CaC in the decrypted file (Xv2CoreLib Offsets): SYSFLAGs 95280 (1024
  bytes), quest arrays, mentor progress 125956, play data, appearance,
  presets; DLC block 519816 + slot*25392 (OSQ/PRB/expanded TPQ+TMQ arrays,
  Tokipedia 532280, Hero Colosseum TTQ 532664 / TFB 533176, mentor
  customisation). Inventory and skills are save-wide (no slot stride), so
  costumes are already shared by all CaCs.
* Quest arrays (24-byte records, Xv2CoreLib): TPQ 97840 x128, TMQ 100912
  x192, BAQ 105520 x128, TCQ 108592 x256, HLQ 114736 x96, RBQ 117040 x96,
  CHQ 119344 x64, LEQ 120880 x128; DLC block OSQ 524984 x39 (type 11), PRB
  525944 x39 (12), TPQ+ 527144 x30, TMQ+ 530120 x31. On the PC save every
  array holds only its own type, no duplicates, unused records (-1) only at
  the end - but **the record order differs per slot**. **DLC block,
  measured 2026-10-25** (Xv2CoreLib's OSQ 39 / PRB 39 / TPQ+ 30 / TMQ+ 31
  are too small and it misses arrays): each array runs to the next one,
  filled with -1 records: OSQ 524984 x40, PRB 525944 x40, type 13 526904
  x10, TPQ+ 527144 x32 (ids 128-159), type 14 527912 x40, type 15 528872
  x10, type 16 529112 x10, **TPQ+2 529352 x32 (ids 160-191, type 0 again)**,
  TMQ+ 530120 x48 (ids 192-239), type 18 531272 x42, then Tokipedia. Types
  13-18 have ids from 0, all state 0 on the PC save (meaning unknown).
  Up to 2026-10-25 SaveSync left out TPQ ids 158-191, so newer DLC story
  missions did not sync (seen: 3 slots missing 20 records), so merge by
  (type, id). A wipe of quest progress (e.g. a qxd with more quests than
  the save's records) empties the DLC-block arrays too - a restore has to
  cover them, not only 97840..123952
  (`ClaudeTools\save\restore_dlc_progress.py` does the DLC block). **96952..97840 is not quest records** (ClaudeTools\save scripts used
  96952 x165 for TPQ until 2026-10-11; a merge there wrote junk into
  CTP_00_01 on a test copy).
* SYSFLAGs set in some slots but not others (PC save, 258 bits) are all
  one-way progress: badges, *_FIRST, first talks, UNLOCK_*_XULTIMATE,
  GIMMICK_EMO_UNLOCK_*, notices, USED_RECEIPT_*, OSQ_* - OR is right. Except
  multi-bit counters: EH_NIL_BAQ_COUNT 3607, EH_NIL_BAQ_AGAIN_01..05
  3609-3613, EH_NIL_SECURITY_TALK_3_0..3 3653-3656 (kept per slot). Names:
  constants section (4) of lobby_event_manage.tsd, `{i32 len, name, i32
  type, i32 len, value}`.
* Mentor flags on finished mentors: 0x11 / 0x19 (+0x700/0xf00 TALKED_COS);
  LICENSED / DISMISSAL never seen set on a finished one -> treated as
  enrolment state, kept per slot; other bits OR, friendship/gauge max.
* Reference implementation: `SaveSync\offline\savesync.py`. **Confirmed
  in game** (2026-10-11, slot 2 -> slot 7, fresh CaC
  gained 714 flags / 626 quest records / 33 mentors): story, quests + scores,
  mentors, roster characters, roster costume variants and stages all
  followed. So roster/costume/stage unlocks are derived from flags + quest
  clears (no separate per-CaC unlock table); the unknown regions
  (96304..97840, rest of the DLC block, ~0xb4154) need no merging.

## SaveSync in game: the live save image (2026-10-11)
Implemented in `SaveSync\src\savesync.cpp`. Seen in game 2026-10-11:
the merge at the slot screen and on toggling (self-check passed every time,
synced slots played fine afterwards, Tokipedia gained 0 bits so its reader
is still unproven). The save hook below is confirmed too: a new CaC made
in a toggled slot plays the intro, then has everything unlocked.
* Save data object SD = `[0x15681a8]`. Serialize the whole save to the file
  image: `0x869120(SD+0x68, SD+0x7b3f8, SD+0xb0880, SD+0xdf260)`; the image
  at SD+0x68 starts with "#SAV". Parse = `0x869ce0(SD)`.
* Parse's per-slot loop (0x869e35): r14 = SD+slot*0xc6c8, r13 =
  SD+slot*0x6330, r15 = SD+slot*0x4c84, rbp = SD+slot*0x1830. It calls
  `0x828ed0(mgr, slot)` (set current slot), then per reader:
  `0x84f580` on object `0x828ca0(mgr,-1)` (args r14+0x17498, r13+0x7ef00,
  r15+0xb8e40: SYSFLAGs + quests), `0x849d00` on `0x828c10(mgr,-1)` (r14+0x17e98,
  r13+0x80320, rbp+0xe8640: quests + DLC block), `0x83df60` on
  `0x828bd0(mgr)` (r14+0x1ec6c: mentors); 0x81bd00/0x828b40,
  0x84dd00/0x828c70, 0x8433f0, 0x845930 are not needed for the merge. At
  the end it restores the slot from `0x828c60`. File offsets check out:
  SYSFLAGs 95280 = image+0x17498, mentors 125956 = +0x1ec6c, OSQ 524984 =
  +0x80320. Tokipedia 532280 (+0x81fa0) is assumed to be inside the DLC
  block reader; the DLL's self-check (serialize again, diff against the
  merged image) logs any byte that did not stick.
* SaveSync's merge: serialize, MergeSlots on a copy, write it back, per
  toggled slot SetSlot + the three readers, restore the playing slot,
  serialize again to self-check, then save-now only if nothing differs.
* Save now = CAccount vtable+0x68 = `0x2d9300`: needs [SD+0x254458] != 0;
  if CAccount+0x6c (save thread running) it serializes into SD+0x12a260 and
  sets +0x94 = 1 (queued), else serializes into SD+0x68, sets +0x6c = 1 and
  starts the save thread. SaveSync treats +0x6c / +0x94 set as "busy, retry".
* **Dead end, seen in game:** hooking `0x398a70` to catch "character
  creation ended" synced nothing (a CaC remade in a toggled slot played the
  whole intro, nothing logged). It is not a leave step: 12 call sites in
  Event::EditCharacter use it as a generic "fade, then go to the state at
  +0xc0" transition (it sets +0xb0 = 0x39a700, which moves +0xc0 into +0xb0).
* Instead SaveSync hooks **save now** (CAccount vtable 0xfa8778 slot 13 =
  0xf9a760, .rdata). Nothing calls 0x2d9300 directly; the serializer
  0x869120's other caller is 0x2d8090 (the account's own load/create step).
  The game saves a new CaC after creation (the remade CaC was in
  DBXV2.sav by the prologue), so the merge runs inside that save when the
  playing slot is toggled. Whether that is early enough to skip the intro
  is not known yet.
* Save slot menu: AVATAR_SLOT.iggy, class AvatarSlot; its AS callback is
  vtable 0x10afc18 slot 17 (vanilla 0x50a640). SaveSync's patched script
  (`as3\AvatarSlot.as`, built by `ClaudeTools\iggy\menu_abc.py ss_avatarslot`)
  calls back with 9910 (screen opened), 9911 (toggle slot), 9912 (log),
  9913 (slot picked, sent right before CallbackDecide - build 4 merges
  only there, plus once at the first save of a CaC made in a toggled
  empty slot); the
  DLL answers through user data int 2 = 0x10000 | toggle mask, polled by
  the script on ENTER_FRAME (same channel as Battle Styles/Chara Search).
* Method: `ClaudeTools\exe` da.py on the parse loop from 0x869ce0 and on the
  CAccount vtable. Dead end: hooking xv2patcher's slot setter instead of the
  game's own readers (it only covers what xv2patcher touches).

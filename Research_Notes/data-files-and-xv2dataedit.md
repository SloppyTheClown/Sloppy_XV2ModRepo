# Game data archives and file formats (XV2DataEdit)

Covers: reading the .cpk archives, which archive wins, the table formats
XV2DataEdit edits (QXD, CUS, IDB, BDM, BCM), name lookup for skills and
items, and XV2DataEdit's design and verification state. Lobby/quest field
meanings: lobby-quests-and-shops.md. PSC/PUP: stats-and-combined-supers.md.

## Archives
* `XV2DataEdit\xv2data\cpk.py` reads the archives (@UTF TOC, CRILAYLA). Search
  order = XV2-Tools' CPK_Reader, first hit wins: data_dNN_dlc newest first,
  ..., data1 before data; data_as / _develop skipped. Keep the original path
  case - loose folders are upper-case and the Deck is case-sensitive.
* Loose files under `<game>\data` override the archives (XV2Patcher).
* cpk.py cannot read ACBs (use `ClaudeTools\gamefiles\acb_utf.py`).

## Formats (from the local XV2-Tools checkout `ClaudeTools\XV2-Tools`,
Xv2CoreLib, 1.25+)
* QXD: quest 292 B, chara 132 B, rewards. CUS v3 (92 B). IDB v3 (64 + 3x240 B
  = 784). BDM XV2_0 (1284 B; 2 BDMs with other layouts are skipped:
  671_JCO_SEC 1164 B, one XV1 756 B). BCM 112 B, flat.
* Vanilla BCMs reach 537,852 entries (ULT/2130_SVE_UE1, 60 MB); skill_input
  is 1.24M rows - over Excel's 1,048,575 limit (export refuses; CSV streams).
* `msg.py` reads `data/msg/*_title_<lang>.msg` for quest titles.
* Names: skills by entry KEY `spe_skill_/ult_/avoid_skill_/met_skill_` + ID2
  (4 digits) - skill_item.idb IDs are CUS ID2s (GKD = Spirit Bomb, GCS =
  Genocide Shell, RSR = Rage Saucer); skill items' Name_MsgID just repeats
  the ID (wrong names). Other items: entry ID = Name_MsgID (accessory 40 ->
  msg 62; "One-Star Dragon Ball" = ID 1 / msg 0). radio_item has no name file.
* Match table files by exact name and check signatures: `custom_skill.cus*`
  once matched the user's loose custom_skill.cus.xml (GBs of garbage).

## XV2DataEdit design
* Rules (.json) or exported .xlsx/.csv edit quest/skill/item values; output
  goes to `<game>\data` as loose files; manifest + backups of other mods'
  loose files in `<game>\XV2DataEdit`.
* Edits are IN PLACE; offsets/counts/string pointers are never fields, so
  files never change size. Every apply starts from the ORIGINAL (loose mod
  file, backup, or cpk) -> idempotent; files no longer changed are reverted.
  Applying two workbooks one at a time: the second undoes the first (by design).
* .xlsx via stdlib `xlsx.py` (frozen header, autofilter); CSVs with a BOM.
  `xv2edit.bat` pauses when started from Explorer (%cmdcmdline% check); bare
  file args mean `apply` (drag and drop).
* LazyBone's XML serializer would not build (WPF markup compile can't load
  YAXLib in LB_Common) - hence pure Python.

## State
* Verified offline (scratch game folder): 612 files, every differing byte in a
  logged field, re-apply identical, revert and restore byte-identical; real
  Excel-saved workbooks applied exactly the edited cells.
* NOT verified in game; nothing written to the real install yet. First check:
  a visible value (TPQ Zeni x3, a skill's KiCost). Deck untested
  (`python3 xv2edit.py --game <path>` should run there).

## Boot crash: duplicate quest IDs in a QXD (2026-10-05)
* Signature: access violation at `DBXV2.exe+0x5a74d7` (`mov rax,[rcx]`, rcx=0)
  about 1 s after the item IDBs load, before the title iggytex. The loop walks
  the quest objects of one QXD (r12 = quest count, rdi = index, r14 -> the
  type string, e.g. "HLQ") and calls vt+0x60 (name) on a null entry.
* Cause: an expert-mission pack's install + uninstall left `HLQ\hlq_data.qxd`
  at 101 quests whose padding slots 94-100 were all `empty_Q94` / id 94. The
  game indexes quests by id, so slot 95 was never created. Stock 1.26 HLQ: 30
  quests (20 real + `empty_Q` padding); Revamp's: 38 (+ HLQ_RVP_001-008).
* Check: every QXD quest's id (+16, 292-byte records from offset [12]) must be
  unique; for padding it equals its index. Fix used: rename/renumber the
  duplicate `empty_Q` slots to `empty_Q<index>` / id = index (backup kept as
  `hlq_data.qxd.broken_backup`). Confirmed in game: boots again. Read the crash dump with
  `cdb -z <LOCALAPPDATA>\CrashDumps\DBXV2.exe.<pid>.dmp -c ".ecxr;r;kn"`.
* **Side effect (2026-10-08) - wiped quest progress, see "Real damage" below:** that fix
  left HLQ at 101 quests (ids 0-100, 73 `empty_Q` slots that still carry
  data). Since then mentors look reset on every save: Play Data shows "?"
  instead of lesson ranks and 0% completion, and mentors offer their intro
  lesson again, while the save still holds every cleared lesson with its rank
  and score. The save's HLQ record array has 96 slots (114736-117040, 24-byte
  records). But vanilla TMQ (197) and TPQ (176) also outnumber the record
  runs I measured, so "more quests than slots" is not proven to be the
  mechanism. Fix to test: set the count back to Revamp's 38
  (`ClaudeTools\gamefiles\qxd_set_quest_count.py hlq_data.qxd 38`, which keeps a
  `.count101` backup; `--restore` undoes it). The extra records stay in the
  file unread. Lesson learned: when removing padding, renumber it *and* drop
  it back to the stock count; don't leave the uninstall's count.
  **Result:** count 38 did not help (scores still blank). The file was then
  rebuilt as a clean Revamp install leaves it with
  `ClaudeTools\gamefiles\qxd_clean_revamp.cs`: the vanilla cpk HLQ plus the
  installed HLQ_RVP_001-008 and Revamp's 26 Characters1 entries (ids 0,
  90-114), with vanilla Characters2 and 1 collection. The pack had left 10
  overwritten vanilla `empty_Q` slots and 782 collections. The result is
  38 quests, 156+31 characters and 53184 bytes; a copy is
  `ClaudeTools\gamefiles\hlq_data_clean_revamp.qxd`. The backups `.count101` and
  `.count38` are in the game's `quest\HLQ`. Not yet seen in game.
  **Real damage (found the same day): the quest progress was really
  wiped, not just shown blank.** In the PC save (written 19:16), every
  character had TPQ 1/165 cleared and 0 for TMQ, BAQ, TCQ, HLQ and RBQ
  (`ClaudeTools\save\quest_states.py`); the in-memory vectors agreed (all states
  0). Steam's dated backups
  (`Steam\userdata\<account>\323470\remote\DBXV21\DBXV2_<date>.sav`) still
  had everything on 2026-10-04 (slot 0: TPQ 128, TMQ 192, TCQ 130, HLQ 30
  of 38). So the wipe happened between Oct 4 21:58 and Oct 8, which fits
  the Oct 5 pack install/uninstall. Mentor friendship, items and skills
  survived; only the quest records were reset, so the game then saved the
  reset. The trigger is not proven: the 101-quest HLQ is the likeliest, but
  memory TPQ (178) and TMQ (202) also exceed their save runs (165/192)
  without harm. Fix: `ClaudeTools\save\restore_quest_progress.py <current>
  <backup> <out>` merges the backup's quest records back in, per record
  where the further state wins, only in slots with matching names, and
  re-encrypts. The md5 at +0x10 covers the section; re-encrypting the
  unchanged save gives the same file byte for byte.
  **Corrupt save (first try):** the game said "save data is corrupt" and left
  the file alone. The decrypted 0x80 header holds byte-sum checksums
  (Xv2CoreLib `Encryption.cs` EncryptSaveFile; all genuine saves match):
  0x1A = sum of every 32nd byte of the plain body, 0x1B = the same over the
  encrypted body, 0x14 = header[5] + sum(0x15..0x1B), all mod 256 (0x15-0x19
  sum the key/iv bytes, unchanged by an edit). The md5 alone isn't enough, and
  a CTR roundtrip check can't catch this (or a wrong keystream). The tool
  now rewrites them; rebuilt and installed 2026-10-08 (old file kept as
  `DBXV2.sav.corrupt`, the pre-restore one as `DBXV2.sav.wiped`, in both
  save folders). Any tool that edits the decrypted save must do the same.
  **Confirmed in game:** the save loads, and mentor scores and completion are
  back; this survives a save and a full restart. On the first load the Oolong
  "monster" lobby cutscene played again, once. That is probably a lobby event
  whose trigger is a quest record we restored, while its seen-flag stayed
  as the wipe left it. Harmless; not investigated.
  Xv2CoreLib's quest offsets: TPQ starts at 97840 (128 records), and TPQ/TMQ
  have 30/31 more records at 527144/530120 (stride 25392). The restore leaves
  those as they are.
  * In-memory quest data: mgr+0x11ae0+slot*0xe880 holds 19 std::vectors
    (begin/end/cap) of 0x48-byte objects, one per quest type in the order
    TPQ TMQ BAQ TCQ HLQ RBQ CHQ LEQ TTQ TFB TNB OSQ PRB PRD RBD RBS GBB EVT
    CBF. Each object: vtable +0, type +8, id +0xc, state +0x10, name
    std::string +0x20 (SSO; size +0x30, capacity +0x38).

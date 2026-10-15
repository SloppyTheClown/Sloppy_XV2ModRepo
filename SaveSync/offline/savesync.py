r"""SaveSync phase 1: merges the unlock progress of chosen CaC slots in a
DBXV2.sav file, so every chosen slot ends up with everything any of them has.
The in-game mod (phase 2) will do the same merge; this script proves the
rules in game first.

Usage (game closed):
    python savesync.py --slots 0,1,2            report only, writes nothing
    python savesync.py --slots 0,1,2 --write    backs up DBXV2.sav, then merges
    python savesync.py --undo                   puts the newest backup back
Slots are 0-7 as listed in the game's save slot screen (top = 0).
--save <path> picks a save; default: the newest DBXV2.sav under
<Steam>\userdata\<account>\323470\remote\DBXV21 (the one the game loads; the
copy under %LOCALAPPDATA%\BANDAI NAMCO Entertainment is not read).

What is merged, per CaC (layouts: Xv2CoreLib SAV_File.cs, measured on a
1.26.01 save 2026-10-11; Research_Notes\save-flags-and-unlocks.md "SaveSync"):
* SYSFLAGs (story, shops, badges, X-ultimate, emotes, notices...): OR,
  except the few multi-bit counters in KEEP_OWN_FLAGS.
* Quest records (story TPQ, parallel TMQ, rifts, mentor lessons, expert,
  raids, Elder Kai, Frieza siege, Infinite History, PRB, the 1.25 expansions):
  matched by (type, quest id) since the order differs per slot; per quest the
  furthest state, best rank, best score and the cleared-once flag win; a quest
  only another slot has is added to a free record.
* Tokipedia (Infinite History pages): OR.
* Mentor progress: highest friendship and dual-attack gauge; flags OR except
  LICENSED / DISMISSAL (enrolment state, kept per slot).
* Shenron "more characters" wishes made: highest (the game unlocks that many
  wish characters).
Inventory, skills and Zeni are already shared by all CaCs in the save file;
appearance, level, presets and play statistics stay per CaC.
"""
import argparse, glob, os, shutil, struct, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sav_crypto import decrypt, encrypt_like         # noqa: E402

STEAM_APP_ID = "323470"                             # Dragon Ball Xenoverse 2
BACKUP_SUFFIX = ".savesync-backup-"
SLOT_COUNT = 8

# Per-character blocks: the main one (stride 50888) and the DLC one added
# with the first DLC packs (stride 25392) - Xv2CoreLib Offsets.CAC_SIZE /
# CAC_DLC_SIZE.
CHARACTER_SIZE, DLC_CHARACTER_SIZE = 50888, 25392
CHARACTER_NAME = 124976 + 68                        # ASCII, 64 bytes

SYSFLAGS, SYSFLAG_BYTES = 95280, 1024               # 8192 bits; the game sets up to 7347
# Flag groups that hold a counter or a "which line next" choice in several
# bits (lobby_event_manage.tsd names), so OR-ing them would make up a value:
# EH_NIL_BAQ_COUNT 3607, EH_NIL_BAQ_AGAIN_01..05 3609-3613,
# EH_NIL_SECURITY_TALK_3_0..3 3653-3656. Each slot keeps its own.
KEEP_OWN_FLAGS = {3607, *range(3609, 3614), *range(3653, 3657)}

# 24-byte quest records {i32 type, quest id, state (0 none, 1 open, 2 played,
# 3 cleared), rank (6 best), cleared-once flag, best score}; unused = type -1.
# (name, record type, offset, record count, block stride): Xv2CoreLib
# Offsets.QUESTS_*; each array checked on a 1.26.01 save to hold only its own
# type, no duplicate quest and no unused record before a used one. The 888
# bytes before TPQ (96952..97840) are not quest records.
QUEST_ARRAYS = [
    ("TPQ story", 0, 97840, 128, CHARACTER_SIZE), ("TMQ parallel", 1, 100912, 192, CHARACTER_SIZE),
    ("BAQ rift", 2, 105520, 128, CHARACTER_SIZE), ("TCQ mentor lessons", 3, 108592, 256, CHARACTER_SIZE),
    ("HLQ expert", 4, 114736, 96, CHARACTER_SIZE), ("RBQ raid", 5, 117040, 96, CHARACTER_SIZE),
    ("CHQ Elder Kai", 6, 119344, 64, CHARACTER_SIZE), ("LEQ Frieza siege", 7, 120880, 128, CHARACTER_SIZE),
    ("OSQ Infinite History", 11, 524984, 39, DLC_CHARACTER_SIZE), ("PRB", 12, 525944, 39, DLC_CHARACTER_SIZE),
    ("TPQ expansion", 0, 527144, 30, DLC_CHARACTER_SIZE), ("TMQ expansion", 1, 530120, 31, DLC_CHARACTER_SIZE),
]
RECORD_SIZE = 24
STATE, RANK, CLEARED_ONCE, SCORE = 2, 3, 4, 5      # record field indexes

TOKIPEDIA, TOKIPEDIA_BYTES = 532280, 12 * 8         # Xv2CoreLib Offsets.TOKIPEDIA (u64 flags each)

# 8-byte entries {u16 friendship 0..1000, u16 dual-attack gauge 0..4000,
# u32 flags}; flag bits = SET_MASTER_FLAG names (save-flags-and-unlocks.md).
MENTOR_PROGRESS, MENTOR_COUNT = 125956, 34
MENTOR_FLAG_LICENSED, MENTOR_FLAG_DISMISSAL = 1 << 1, 1 << 2
MENTOR_KEEP_OWN = MENTOR_FLAG_LICENSED | MENTOR_FLAG_DISMISSAL

# Shenron wish counts: 32 bytes in PlayData (file 126276 + 168), one per wish
# type of wish_script.dbw. Wish characters are no flag: the character select's
# check_unlock (0x5e2560) unlocks the first N of the 16 wish characters, N =
# byte 7 (read by 0x84ad70 -> 0x83f1f0). Only that one is merged (highest
# wins); the other wishes give items, so raising their counts would skip them.
WISH_COUNTS, WISH_CHARACTERS = 126276 + 168, 7


def character_name(save, slot):
    at = slot * CHARACTER_SIZE + CHARACTER_NAME
    return save[at:at + 64].split(b"\0")[0].decode(errors="replace")


def merge_sysflags(save, slots):
    """OR of every slot's SYSFLAG bits, written to each slot. Returns {slot: bits gained}."""
    merged = bytearray(SYSFLAG_BYTES)
    for slot in slots:
        start = SYSFLAGS + slot * CHARACTER_SIZE
        for i, value in enumerate(save[start:start + SYSFLAG_BYTES]):
            merged[i] |= value
    gained = {}
    for slot in slots:
        start = SYSFLAGS + slot * CHARACTER_SIZE
        own = save[start:start + SYSFLAG_BYTES]
        result = bytearray(merged)
        for flag in KEEP_OWN_FLAGS:
            byte, bit = divmod(flag, 8)
            result[byte] = (result[byte] & ~(1 << bit)) | (own[byte] & (1 << bit))
        gained[slot] = sum(bin(new & ~old).count("1") for new, old in zip(result, own))
        save[start:start + SYSFLAG_BYTES] = result
    return gained


def read_records(save, start, count):
    """Quest records of one array: [(index, [6 ints])]."""
    return [(i, list(struct.unpack_from("<6i", save, start + i * RECORD_SIZE))) for i in range(count)]


def check_array(records, record_type, slot, name):
    """Stops unless the array looks as measured: only its own record type, each
    quest once, unused records only at the end (the game might stop reading
    at the first unused one)."""
    used = [i for i, r in records if r[0] != -1]
    free = [i for i, r in records if r[0] == -1]
    if any(r[0] not in (-1, record_type) for _, r in records):
        sys.exit(f"slot {slot} {name}: a record of another type; layout differs, stopping")
    if len({(r[0], r[1]) for _, r in records if r[0] != -1}) != len(used):
        sys.exit(f"slot {slot} {name}: a quest listed twice; stopping")
    if used and free and free[0] < used[-1]:
        sys.exit(f"slot {slot} {name}: unused record {free[0]} before used record {used[-1]}; stopping")


def merge_quest_array(save, slots, record_type, offset, count, stride, name):
    """Best value per quest across the slots, written back to each slot's own
    record of that quest (or a free record). Returns {slot: records changed}."""
    best = {}
    for slot in slots:
        records = read_records(save, offset + slot * stride, count)
        check_array(records, record_type, slot, name)
        for _, record in records:
            if record[0] == -1:
                continue
            key = (record[0], record[1])
            if key not in best:
                best[key] = list(record)
            else:
                merged = best[key]
                for field in (STATE, RANK, CLEARED_ONCE, SCORE):
                    merged[field] = max(merged[field], record[field])
    changed = {}
    for slot in slots:
        start = offset + slot * stride
        records = read_records(save, start, count)
        by_key = {(r[0], r[1]): i for i, r in records if r[0] != -1}
        free = [i for i, r in records if r[0] == -1]
        count_changed = 0
        for key, merged in best.items():
            if key in by_key:
                index = by_key[key]
                if records[index][1] == merged:
                    continue
            elif free:
                index = free.pop(0)
            else:
                sys.exit(f"slot {slot} {name}: no free record for quest {key}; stopping")
            struct.pack_into("<6i", save, start + index * RECORD_SIZE, *merged)
            count_changed += 1
        changed[slot] = count_changed
    return changed


def merge_tokipedia(save, slots):
    merged = bytearray(TOKIPEDIA_BYTES)
    for slot in slots:
        start = TOKIPEDIA + slot * DLC_CHARACTER_SIZE
        for i, value in enumerate(save[start:start + TOKIPEDIA_BYTES]):
            merged[i] |= value
    gained = {}
    for slot in slots:
        start = TOKIPEDIA + slot * DLC_CHARACTER_SIZE
        gained[slot] = sum(bin(new & ~old).count("1") for new, old in zip(merged, save[start:start + TOKIPEDIA_BYTES]))
        save[start:start + TOKIPEDIA_BYTES] = merged
    return gained


def merge_mentors(save, slots):
    changed = {slot: 0 for slot in slots}
    for mentor in range(MENTOR_COUNT):
        entries = {slot: struct.unpack_from("<HHI", save, MENTOR_PROGRESS + slot * CHARACTER_SIZE + 8 * mentor)
                   for slot in slots}
        friendship = max(e[0] for e in entries.values())
        gauge = max(e[1] for e in entries.values())
        shared_flags = 0
        for e in entries.values():
            shared_flags |= e[2] & ~MENTOR_KEEP_OWN
        for slot, (_, _, own_flags) in entries.items():
            merged = (friendship, gauge, shared_flags | (own_flags & MENTOR_KEEP_OWN))
            if merged != entries[slot]:
                struct.pack_into("<HHI", save, MENTOR_PROGRESS + slot * CHARACTER_SIZE + 8 * mentor, *merged)
                changed[slot] += 1
    return changed


def merge_wish_characters(save, slots):
    offsets = {slot: WISH_COUNTS + slot * CHARACTER_SIZE + WISH_CHARACTERS for slot in slots}
    highest = max(save[at] for at in offsets.values())
    gained = {}
    for slot, at in offsets.items():
        gained[slot] = highest - save[at]
        save[at] = highest
    return gained


def steam_folder():
    """Steam's install folder from the registry (HKCU Software\\Valve\\Steam SteamPath)."""
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:
            return winreg.QueryValueEx(key, "SteamPath")[0]
    except OSError:
        return r"C:\Program Files (x86)\Steam"


def newest_save():
    """The save the game reads: Steam Cloud's userdata\\<account>\\323470\\remote\\DBXV21.
    The game also writes a copy under %LOCALAPPDATA%, which it does not load
    (a merge written there was overwritten on the next save, 2026-10-11)."""
    pattern = os.path.join(steam_folder(), "userdata", "*", STEAM_APP_ID, "remote", "DBXV21", "DBXV2.sav")
    saves = glob.glob(pattern)
    if not saves:
        sys.exit(f"no save matches {pattern}; pass --save")
    return max(saves, key=os.path.getmtime)


def game_running():
    return "DBXV2.exe" in os.popen('tasklist /FI "IMAGENAME eq DBXV2.exe" /NH').read()


def undo(save_path):
    backups = sorted(glob.glob(save_path + BACKUP_SUFFIX + "*"))
    if not backups:
        sys.exit("no SaveSync backup next to " + save_path)
    shutil.copyfile(backups[-1], save_path)
    print("restored", backups[-1])


def main():
    parser = argparse.ArgumentParser(description="Merge unlock progress between CaC slots of DBXV2.sav")
    parser.add_argument("--slots", help="comma-separated slots 0-7 to sync, at least two")
    parser.add_argument("--save", help="DBXV2.sav path (default: newest in Steam userdata .../323470/remote/DBXV21)")
    parser.add_argument("--write", action="store_true", help="write the merged save (backs it up first)")
    parser.add_argument("--undo", action="store_true", help="restore the newest SaveSync backup")
    args = parser.parse_args()
    save_path = args.save or newest_save()
    if (args.write or args.undo) and game_running():
        sys.exit("close the game first (it rewrites DBXV2.sav)")
    if args.undo:
        return undo(save_path)
    slots = sorted({int(s) for s in (args.slots or "").split(",") if s.strip()})
    if len(slots) < 2 or not all(0 <= s < SLOT_COUNT for s in slots):
        sys.exit("--slots needs at least two of 0-7")

    raw = open(save_path, "rb").read()
    save = bytearray(decrypt(raw))
    if encrypt_like(raw, bytes(save)) != raw:
        sys.exit("re-encrypting the unchanged save does not give the same file; not touching it")
    for slot in slots:
        if not character_name(save, slot):
            sys.exit(f"slot {slot} is empty")
    print(save_path)
    print("syncing:", ", ".join(f"{slot} {character_name(save, slot)}" for slot in slots))

    report = {slot: [] for slot in slots}
    for slot, gained in merge_sysflags(save, slots).items():
        report[slot].append(f"{gained} flags")
    quest_totals = {slot: 0 for slot in slots}
    for name, record_type, offset, count, stride in QUEST_ARRAYS:
        for slot, changed in merge_quest_array(save, slots, record_type, offset, count, stride, name).items():
            quest_totals[slot] += changed
    for slot, total in quest_totals.items():
        report[slot].append(f"{total} quest records")
    for slot, gained in merge_tokipedia(save, slots).items():
        report[slot].append(f"{gained} Tokipedia bits")
    for slot, changed in merge_mentors(save, slots).items():
        report[slot].append(f"{changed} mentors")
    for slot, gained in merge_wish_characters(save, slots).items():
        report[slot].append(f"{gained} wish characters")
    for slot in slots:
        print(f"  slot {slot} {character_name(save, slot)}: gains " + ", ".join(report[slot]))

    if not args.write:
        print("report only; add --write to save it")
        return
    backup = save_path + BACKUP_SUFFIX + time.strftime("%Y%m%d-%H%M%S")
    shutil.copyfile(save_path, backup)
    open(save_path, "wb").write(encrypt_like(raw, bytes(save)))
    print("backup:", backup)
    print("wrote", save_path)


if __name__ == "__main__":
    main()

"""Put vanilla lobby flight speeds back in Revamp's lobby action files.

    python lbyflight.py <game folder> vanilla|undo

Revamp 5.1.1's CMN.<race>.lby.bac scale the Movement (type 2) items of
entries 10/11/12 (take-off, boost loop, stop) by x1.4; suspected to cause the
boost-flight stall every ~4 s (revamp-on-1.26.md "Lobby boost-flight
stutter"). "vanilla" copies those type-2 blocks from the cpk copy in place
(same size, only floats differ) after saving <file>.revamp.bak; "undo" puts
the backups back.
"""
import os, shutil, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'Tools', 'XV2DataEdit'))
from xv2data.cpk import CpkSet

FLIGHT_ENTRIES = (10, 11, 12)
MOVEMENT_TYPE = 2
MOVEMENT_ITEM_SIZE = 36   # 8-byte BAC item header + Xv2CoreLib BAC_Type2 fields


def movement_block(data, entry):
    """(offset, size) of an entry's Movement items. BAC layout: lbybacdiff.py."""
    table = struct.unpack_from('<I', data, 0x10)[0]
    _, type_count, _, type_table = struct.unpack_from('<IHHQ', data, table + 16 * entry)
    for j in range(type_count):
        type_id, items, _, at = struct.unpack_from('<HHIQ', data, type_table + 16 * j)
        if type_id == MOVEMENT_TYPE:
            return at, items * MOVEMENT_ITEM_SIZE
    return None


def main():
    game, action = sys.argv[1], sys.argv[2]
    folder = os.path.join(game, 'data', 'chara', 'CMN')
    cpks = CpkSet(os.path.join(game, 'cpk'))
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.lby.bac'):
            continue
        path = os.path.join(folder, name)
        backup = path + '.revamp.bak'
        if action == 'undo':
            if os.path.exists(backup):
                shutil.copyfile(backup, path); os.remove(backup)
                print(f'{name}: Revamp version back')
            continue
        key = 'data/chara/CMN/' + name
        if not cpks.find(key):
            continue
        vanilla, loose = cpks.read(key), bytearray(open(path, 'rb').read())
        changed = []
        for entry in FLIGHT_ENTRIES:
            mine, theirs = movement_block(loose, entry), movement_block(vanilla, entry)
            if not mine or not theirs or mine[1] != theirs[1]:
                print(f'{name}: entry {entry} laid out differently - left alone'); continue
            new = vanilla[theirs[0]:theirs[0] + theirs[1]]
            if loose[mine[0]:mine[0] + mine[1]] != new:
                loose[mine[0]:mine[0] + mine[1]] = new
                changed.append(entry)
        if not changed:
            print(f'{name}: flight already vanilla'); continue
        if not os.path.exists(backup):
            shutil.copyfile(path, backup)
        open(path, 'wb').write(loose)
        print(f'{name}: entries {changed} flight speed set to vanilla (backup {os.path.basename(backup)})')


main()

"""Compare the loose lobby action files (data\\chara\\CMN\\CMN.*.lby.bac) with
the game's own copies in the cpks, entry by entry.

    python lbybacdiff.py <game folder> [race code, e.g. HUM] [entry to dump]

BAC layout (PostInstall.cs "Bac"): "#BAC", u32 entry count at 8, entry table
at [0x10]; entry 16 bytes {u32 flags, u16 type count, u16, u64 type table};
type header 16 bytes {u16 type, u16 item count, u32, u64 items offset}.
"""
import os, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'Tools', 'XV2DataEdit'))
from xv2data.cpk import CpkSet


def entries(data):
    """{index: {type: raw item bytes}} for every non-empty entry."""
    count, table = struct.unpack_from('<I', data, 8)[0], struct.unpack_from('<I', data, 0x10)[0]
    # every offset the file points at, to size each item block
    offsets, raw = {len(data)}, []
    for i in range(count):
        flags, type_count, _, type_table = struct.unpack_from('<IHHQ', data, table + 16 * i)
        types = [struct.unpack_from('<HHIQ', data, type_table + 16 * j) for j in range(type_count)]
        if type_count:
            offsets.add(type_table)
        offsets.update(t[3] for t in types)
        raw.append((flags, types))
    ordered = sorted(offsets)
    result = {}
    for i, (flags, types) in enumerate(raw):
        if not types:
            continue
        blocks = {}
        for type_id, items, _, at in types:
            size = ordered[ordered.index(at) + 1] - at
            blocks[type_id] = (items, data[at:at + size])
        result[i] = (flags, blocks)
    return count, result


def main():
    game = sys.argv[1]
    only = sys.argv[2].upper() if len(sys.argv) > 2 else None
    dump = int(sys.argv[3]) if len(sys.argv) > 3 else None
    cpks = CpkSet(os.path.join(game, 'cpk'))
    folder = os.path.join(game, 'data', 'chara', 'CMN')
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.lby.bac') or (only and f'.{only}.' not in name):
            continue
        key = 'data/chara/CMN/' + name
        if not cpks.find(key):
            print(f'{name}: no vanilla copy'); continue
        loose = open(os.path.join(folder, name), 'rb').read()
        vcount, vanilla = entries(cpks.read(key))
        lcount, modded = entries(loose)
        changed = []
        for i in sorted(set(vanilla) | set(modded)):
            v, m = vanilla.get(i), modded.get(i)
            if v == m:
                continue
            if not v or not m:
                changed.append(f'{i}:{"only loose" if m else "only vanilla"}')
                continue
            types = sorted(t for t in set(v[1]) | set(m[1]) if v[1].get(t) != m[1].get(t))
            changed.append(f'{i}:types{types}' + (' flags' if v[0] != m[0] else ''))
        print(f'{name}: {lcount} entries (vanilla {vcount}), {len(changed)} differ')
        for line in changed:
            print('   ', line)
        if dump is not None:
            for label, side in (('vanilla', vanilla), ('loose', modded)):
                flags, blocks = side.get(dump, (None, {}))
                print(f'  entry {dump} {label}: flags {flags}')
                for type_id, (items, block) in sorted(blocks.items()):
                    print(f'    type {type_id} x{items}: {block.hex(" ", 4)}')


main()

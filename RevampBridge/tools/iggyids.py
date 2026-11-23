"""Which game version's texture numbering a (modded) .iggy uses.

    python iggyids.py <old game> <new game> [movie ...]

An .iggy holds a table of its named characters: 24-byte records {u64 name
offset (increasing), u64 character id (decreasing), u64 0}. Texture ids are
character ids, so when an update renumbers a movie's textures this table
changes with them. Compares the table of each loose .iggy (default: every
loose one) with the vanilla movie of both versions.
"""
import os, sys
from changedvanilla import index
from iggypairs import get, idtable


def numbering(mod, vo, vn):
    t, to, tn = idtable(mod), idtable(vo), idtable(vn)
    if to == tn:
        return 'same in both versions', t, to, tn
    return ('old' if t == to else 'new' if t == tn else 'neither'), t, to, tn


if __name__ == '__main__':
    old, new = index(sys.argv[1]), index(sys.argv[2])
    d = os.path.join(sys.argv[2], 'data', 'ui', 'iggy')
    movies = [m.lower() for m in sys.argv[3:]] or [f[:-5].lower() for f in os.listdir(d) if f.lower().endswith('.iggy')]
    for m in movies:
        k = 'data/ui/iggy/%s.iggy' % m
        vo, vn = get(old, k), get(new, k)
        mod = open(os.path.join(d, m + '.iggy'), 'rb').read()
        if vo is None or vn is None:
            print('%-20s not vanilla in both versions' % m); continue
        r, t, to, tn = numbering(mod, vo, vn)
        print('%-20s uses %s numbering  (table %d ids)' % (m, r, len(t)))
        if r == 'neither':
            print('    mod', t); print('    old', to); print('    new', tn)

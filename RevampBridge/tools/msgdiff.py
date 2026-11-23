"""Compare a mod's loose .msg (text) files with the game's vanilla ones.

    python msgdiff.py <game> [name filter ...]

Per file: entries vanilla has that the loose file lacks (by name), entries
whose id differs, and entries whose text differs from vanilla (the mod's
edits, or text the update changed). Optional <old game> via --old for
telling the two apart.

MSG: "#MSG", u32 count at 8, name table at [0x0C] ({u32 offset, u32
chars, u32 bytes, u32 0}; UTF-16 when bytes = 2 x chars, else 8-bit), id table at [0x10]
(u32), line table at [0x14] ({u32 line count, u32 offset of that many
string records}); string record = {u32 offset, u32 chars, u32 bytes, u32 parameter count},
UTF-16. With a parameter count of 0 the game shows "%ls" as is.
"""
import os, struct, sys
from changedvanilla import index
from iggypairs import get


def msg(b):
    assert b[:4] == b'#MSG', b[:4]
    n, names, ids, lines = struct.unpack_from('<IIII', b, 8)
    out = {}
    for i in range(n):
        o, ch, by, _ = struct.unpack_from('<IIII', b, names + 16 * i)
        nm = b[o:o + by].decode('utf-16-le' if by >= 2 * ch and by % 2 == 0 else 'latin-1').rstrip('\0')
        idv, = struct.unpack_from('<I', b, ids + 4 * i)
        lc, lo = struct.unpack_from('<II', b, lines + 8 * i)
        txt = []
        for j in range(lc):
            so, sc, sb, _ = struct.unpack_from('<IIII', b, lo + 16 * j)
            txt.append(b[so:so + sb].decode('utf-16-le', 'replace').rstrip('\0'))
        out[nm] = (idv, txt)
    return out


if __name__ == '__main__':
    args = sys.argv[1:]
    old = None
    if '--old' in args:
        i = args.index('--old'); old = index(args[i + 1]); del args[i:i + 2]
    game, flt = args[0], [f.lower() for f in args[1:]]
    new = index(game)
    d = os.path.join(game, 'data', 'msg')
    for f in sorted(os.listdir(d)):
        k = 'data/msg/' + f.lower()
        if flt and not any(x in k for x in flt):
            continue
        van = get(new, k)
        if van is None:
            continue
        m, v = msg(open(os.path.join(d, f), 'rb').read()), msg(van)
        o = msg(get(old, k)) if old and get(old, k) else None
        missing = [nm for nm in v if nm not in m]
        idchg = [nm for nm in v if nm in m and m[nm][0] != v[nm][0]]
        txt = [nm for nm in v if nm in m and m[nm][1] != v[nm][1]]
        stale = [nm for nm in txt if o and nm in o and m[nm][1] == o[nm][1]]
        if missing or idchg or stale:
            print('%s  vanilla %d loose %d' % (f, len(v), len(m)))
            if missing: print('   missing %d: %s' % (len(missing), ' '.join(missing[:12])))
            if idchg: print('   id differs %d: %s' % (len(idchg), ' '.join('%s(%d->%d)' % (x, v[x][0], m[x][0]) for x in idchg[:8])))
            if stale: print('   1.25 text where 1.26 changed it %d: %s' % (len(stale), ' '.join(stale[:12])))

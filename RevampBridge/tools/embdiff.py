"""Compare a mod's loose .emb (texture container) with the vanilla one of two
game versions: which entries the update added/changed and whether the mod's
file still has them.

    python embdiff.py <old game> <new game> [path filter ...]

Without a filter: every loose data\\ui .emb whose vanilla changed.
EMB: "#EMB", u32 count at 0x0C, entry table offset at 0x18 ({u32 data offset
relative to the entry, u32 size}), name table offset at 0x1C (u32 absolute
string offsets, 0 = no names).
"""
import os, struct, sys
from changedvanilla import index
from iggypairs import get


def emb(b):
    assert b[:4] == b'#EMB', b[:4]
    n, = struct.unpack_from('<I', b, 0x0C)
    tab, names = struct.unpack_from('<II', b, 0x18)
    out = []
    for i in range(n):
        off, size = struct.unpack_from('<II', b, tab + 8 * i)
        nm = ''
        if names:
            no, = struct.unpack_from('<I', b, names + 4 * i)
            nm = b[no:b.index(b'\0', no)].decode('ascii', 'replace')
        out.append((nm, b[tab + 8 * i + off: tab + 8 * i + off + size]))
    return out


def describe(o, n, m):
    """per vanilla texture name: what the update did (and its index old->new),
    and what the mod has under that name (and its index)"""
    oi = {nm: (i, d) for i, (nm, d) in enumerate(o)}
    mi = {nm: (i, d) for i, (nm, d) in enumerate(m)}
    rows = []
    for i, (nm, d) in enumerate(n):
        oe, me = oi.get(nm), mi.get(nm)
        upd = 'added' if oe is None else 'same' if oe[1] == d else 'CHANGED'
        if oe is not None and oe[0] != i:
            upd += ' %d->%d' % (oe[0], i)
        if me is None:
            mod = 'MISSING'
        else:
            mod = '=new' if me[1] == d else '=old' if oe is not None and me[1] == oe[1] else 'mod art'
            if me[0] != i:
                mod += ' at %d' % me[0]
        rows.append((i, upd, mod, nm))
    for nm in oi:
        if nm not in {x[0] for x in n}:
            rows.append((-1, 'removed', 'in mod' if nm in mi else '-', nm))
    return rows


if __name__ == '__main__':
    old, new = index(sys.argv[1]), index(sys.argv[2])
    flt = [f.lower() for f in sys.argv[3:]]
    data = os.path.join(sys.argv[2], 'data')
    for d, _, fs in os.walk(os.path.join(data, 'ui')):
        for f in fs:
            if not f.lower().endswith('.emb'):
                continue
            k = 'data/' + os.path.relpath(os.path.join(d, f), data).replace('\\', '/').lower()
            if flt and not any(x in k for x in flt):
                continue
            vo, vn = get(old, k), get(new, k)
            if vo is None or vn is None or vo == vn:
                continue
            o, n, m = emb(vo), emb(vn), emb(open(os.path.join(d, f), 'rb').read())
            rows = describe(o, n, m)
            interesting = [r for r in rows if r[1] != 'same' or r[2] not in ('=new', 'mod art')]
            print('%s  old %d new %d mod %d' % (k[5:], len(o), len(n), len(m)))
            for i, upd, mod, nm in interesting[:12]:
                print('    %3d %-8s mod %-8s %s' % (i, upd, mod, nm))
            if len(interesting) > 12:
                print('    ... %d more' % (len(interesting) - 12))

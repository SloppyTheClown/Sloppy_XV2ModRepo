"""List loose data\\ files whose vanilla (cpk) version differs between two
game versions - candidates for a mod replacement built against the old one.

    python changedvanilla.py <old game> <new game> [subdir filter]
"""
import os, sys
from cpkls import listing

ORDER = ['data2.cpk', 'data1.cpk', 'data0.cpk', 'data.cpk']


def index(game):
    cpks = sorted(os.listdir(os.path.join(game, 'cpk')))
    cpks = [c for c in ORDER if c in cpks] + [c for c in cpks if c not in ORDER and c.startswith('data') and 'develop' not in c]
    idx = {}
    for c in cpks:
        path = os.path.join(game, 'cpk', c)
        for name, off, st, ex, _ in listing(path)[1]:
            idx.setdefault(name.lower(), (path, off, st, ex))
    return idx


def stored(e):
    with open(e[0], 'rb') as f:
        f.seek(e[1])
        return f.read(e[2])


if __name__ == '__main__':
    old, new = index(sys.argv[1]), index(sys.argv[2])
    flt = sys.argv[3].lower() if len(sys.argv) > 3 else ''
    root = os.path.join(sys.argv[2], 'data')
    for d, _, fs in os.walk(root):
        for f in fs:
            rel = 'data/' + os.path.relpath(os.path.join(d, f), root).replace('\\', '/')
            k = rel.lower()
            if flt not in k:
                continue
            o, n = old.get(k), new.get(k)
            if n is None:
                continue
            if o is None:
                print('NEW IN 1.26 ', rel)
            elif o[2:] != n[2:] or stored(o) != stored(n):
                print('CHANGED     ', rel)

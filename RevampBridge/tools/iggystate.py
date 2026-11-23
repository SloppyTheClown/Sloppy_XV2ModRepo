"""Classify every loose Iggy file (data\\ui\\iggy, data\\system\\iggy) of a game
against the vanilla file of two game versions.

    python iggystate.py <old game> <new game> [name filter]

Per file: loose==old vanilla, loose==new vanilla, old==new (did the update
change it), or 'MOD' (the mod's own content).
"""
import os, sys
from changedvanilla import index
from iggypairs import get

if __name__ == '__main__':
    old, new = index(sys.argv[1]), index(sys.argv[2])
    flt = sys.argv[3].lower() if len(sys.argv) > 3 else ''
    data = os.path.join(sys.argv[2], 'data')
    for sub in ('ui/iggy', 'system/iggy'):
        for f in sorted(os.listdir(os.path.join(data, sub))):
            k = ('data/%s/%s' % (sub, f)).lower()
            if flt not in k:
                continue
            d = open(os.path.join(data, sub, f), 'rb').read()
            vo, vn = get(old, k), get(new, k)
            if vo is None and vn is None:
                tag = 'not vanilla'
            else:
                tag = ' '.join(t for t, c in (('==old', d == vo), ('==new', d == vn)) if c) or 'MOD'
                tag += '   update: ' + ('none' if vo == vn else 'NEW FILE' if vo is None else 'CHANGED')
            print('%-50s %9d  %s' % (k[5:], len(d), tag))

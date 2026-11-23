"""Find Iggy texture files (.iggytex/.iggyted) that no longer match the .iggy
the game pairs them with, after a game update.

    python iggypairs.py <old game> <new game> [--maps out.txt] [--json out.json]

For every Iggy movie with a loose file in <new game>\\data (ui/iggy,
system/iggy), each texture file is either loose (the mod's, built for the old
version) or the game's (new version); the .iggy likewise, except that a loose
.iggy whose id table equals the new vanilla one (e.g. xv2ins's) counts as the
game's. Files listed in data\\XV2_RevampBridge.txt are ignored. Where the two sides
differ and the game's texture layout changed between the versions, prints
what must be converted and the texture id map (old id -> new id, by content).
"""
import json, os, re, sys
from changedvanilla import index, stored
from iggytexdiff import crilayla

LANGS = 'ca de en es fr it ja kr pl pt ru tw zh'.split()


def get(idx, k):
    e = idx.get(k)
    if not e:
        return None
    d = stored(e)
    return d if e[2] == e[3] else crilayla(d)


def tex(b):
    import struct
    assert b[:8] == b'oggytex\0'
    n = struct.unpack_from('<I', b, 8)[0]
    out = []
    for i in range(n):
        idx, size, off, _ = struct.unpack_from('<IIII', b, 16 + 16 * i)
        if size & 0x7FFFFFFF == 0:
            # reference to a texture held elsewhere: the high half names it
            out.append((idx & 0xFFFF, b'REF %d' % (idx >> 16)))
        else:
            out.append((idx & 0xFFFF, b[off & 0x7FFFFFFF:(off & 0x7FFFFFFF) + (size & 0x7FFFFFFF)]))
    return out


def ddshead(d):
    return d[12:20] + d[80:92] if d[:4] == b'DDS ' else d  # size, pixel format


def idmap(old, new):
    """old id -> new id for textures with identical content (ties by order);
    then a texture the update redrew: same id, same size and format."""
    used, m = set(), {}
    for oi, od in old:
        c = [ni for ni, nd in new if nd == od and ni not in used]
        if c:
            used.add(c[0]); m[oi] = c[0]
    newd = dict(new)
    for oi, od in old:
        if oi not in m and oi in newd and oi not in used and ddshead(newd[oi]) == ddshead(od):
            used.add(oi); m[oi] = oi
    return m


def idtable(b):
    """An .iggy's table of named characters: 24-byte records {u64 name offset
    (increasing), u64 character id (decreasing), u64 0}. Texture ids are
    character ids, so a renumbering update changes this table. Returns the
    ids of the longest such run."""
    import struct
    best = []
    for start in range(0, 24, 8):
        run, prev = [], None
        for p in range(start, len(b) - 24, 24):
            a, i, z = struct.unpack_from('<QQQ', b, p)
            rec = z == 0 and 0 < a < len(b) and i < 0x10000
            if rec and (prev is None or (a > prev[0] and i < prev[1])):
                run.append(i); prev = (a, i)
            else:
                if len(run) > len(best):
                    best = run
                run, prev = ([i], (a, i)) if rec else ([], None)
        if len(run) > len(best):
            best = run
    return best


def swf_for(k, idx):
    """data/.../X_<lang>.iggyte? -> the .iggy the game pairs it with."""
    base = re.sub(r'\.iggyte[xd]$', '', k)
    if base + '.iggy' in idx:
        return base + '.iggy'
    m = re.match(r'(.*)_(%s)$' % '|'.join(LANGS), base)
    if m and m.group(1) + '.iggy' in idx:
        return m.group(1) + '.iggy'
    return None


if __name__ == '__main__':
    oldg, newg = sys.argv[1], sys.argv[2]
    old, new = index(oldg), index(newg)
    data = os.path.join(newg, 'data')
    # files the bridge created are not the mod's
    state = os.path.join(data, 'XV2_RevampBridge.txt')
    ours = {l.strip().replace('\\', '/').lower() for l in open(state)} if os.path.exists(state) else set()
    loose = {}
    for sub in ('ui/iggy', 'system/iggy'):
        for f in os.listdir(os.path.join(data, sub)):
            k = ('data/%s/%s' % (sub, f)).lower()
            if k not in ours:
                loose[k] = os.path.join(data, sub, f)
    # every texture file of every movie that has something loose
    movies = {}
    for k in list(new.keys()) + list(loose.keys()):
        if re.search(r'\.iggyte[xd]$', k) and ('/ui/iggy/' in k or '/system/iggy/' in k):
            s = swf_for(k, new)
            if s:
                movies.setdefault(s, set()).add(k)
    report = []
    for s, texs in sorted(movies.items()):
        # a loose .iggy numbered like the game's (e.g. xv2ins's, rebuilt for
        # the new version) counts as the game's side
        swf_loose = s in loose and idtable(open(loose[s], 'rb').read()) != idtable(get(new, s))
        if s in loose and not swf_loose:
            print('%s: loose, numbered like the game\'s' % s)
        if not swf_loose and not any(t in loose for t in texs):
            continue
        for t in sorted(texs):
            t_loose = t in loose
            if t_loose == swf_loose:
                continue  # same side: consistent
            vo, vn = get(old, t), get(new, t)
            if vo is None or vn is None:
                print('?? no vanilla in both versions:', t); continue
            to, tn = tex(vo), tex(vn)
            if [(i, len(d)) for i, d in to] == [(i, len(d)) for i, d in tn] and to == tn:
                continue  # layout and content unchanged
            m = idmap(to, tn)
            if t_loose:
                mod = tex(open(loose[t], 'rb').read())
                need = 'loose texture (old layout) under the game\'s new .iggy: convert to new layout'
                ok = [i for i, _ in mod] == [i for i, _ in to]
                changed_unmapped = [i for (i, d), (_, od) in zip(mod, to) if i not in m and d != od] if ok else None
            else:
                need = 'game texture (new layout) under a loose old .iggy: write a loose old-layout copy'
                ok = True
                changed_unmapped = [i for i, _ in to if i not in m]
            print('%-45s %s\n    swf %s (%s)  ids old %d new %d  mod ids==old: %s  unmapped: %s' % (
                t, need, s, 'loose' if swf_loose else 'game', len(to), len(tn), ok, changed_unmapped))
            report.append({'tex': t, 'swf': s, 'to_new': t_loose, 'map': m, 'old_ids': [i for i, _ in to],
                           'new_ids': [i for i, _ in tn], 'mod_layout_ok': ok, 'unmapped': changed_unmapped})
    if '--maps' in sys.argv:
        # one line per texture file: path|its .iggy|old ids|new ids|old:new,...
        with open(sys.argv[sys.argv.index('--maps') + 1], 'w', newline='\n') as f:
            f.write('# Generated by tools/iggypairs.py (1.25.02 -> the game it was given): Iggy texture id maps\n')
            for e in sorted(report, key=lambda e: e['tex']):
                f.write('%s|%s|%s|%s|%s\n' % (e['tex'], e['swf'],','.join(map(str, e['old_ids'])), ','.join(map(str, e['new_ids'])),
                                            ','.join('%d:%d' % kv for kv in sorted(e['map'].items()))))
    if '--json' in sys.argv:
        json.dump(report, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=0)

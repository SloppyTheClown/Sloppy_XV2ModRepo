"""Offline check of the mod's file edits (run by tests\\run.bat).

  check.py extract <game> <out.tsd>
      write the game's lobby_event_manage.tsd
  check.py compare <orig.tsd> <patched.tsd>
      decode both; exactly the intended trigger conditions may differ.
  check.py data <game> <datatest.exe> <work dir>
      run datatest on every quest file but tpq and every shop item file; only
      story and parallel quest requirements (-> none) and shop story points
      (-> 0x7fff, sold from the start) may differ; the TP shop tables list
      every item in every season.
  check.py map <game> <maptest.exe> <work dir>
      run maptest on the game's LOBYHUD.iggy; the result must equal
      tools\\iggy\\lobyhud_abc.py's patch with the ABC in src\\lobyhud_abc.h.
"""
import os, re, struct, subprocess, sys

TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')
sys.path.insert(0, os.path.join(TOOLS, 'Tools', 'XV2DataEdit'))
TSD = 'data/event/lobby/lobby_event_manage.tsd'

if sys.argv[1] == 'extract':
    from xv2data.game import Game
    g = Game(sys.argv[2])
    open(sys.argv[3], 'wb').write(g.original(TSD))
    sys.exit(0)

if sys.argv[1] == 'map':
    sys.path.insert(0, os.path.join(TOOLS, 'ClaudeTools', 'iggy'))
    from lobyhud_abc import append_abc, find_abc, index_walk
    from xv2data.cpk import CpkSet
    exe, work = sys.argv[3], sys.argv[4]
    vanilla = CpkSet(os.path.join(sys.argv[2], 'cpk')).read('data/ui/iggy/LOBYHUD.iggy')
    walked, size, _ = index_walk(vanilla)
    if walked != size:
        sys.exit('index walk of the vanilla file ends at %#x, movie is %#x - decoder model wrong' % (walked, size))
    src, dst = os.path.join(work, 'LOBYHUD.iggy'), os.path.join(work, 'LOBYHUD_patched.iggy')
    open(src, 'wb').write(vanilla)
    if subprocess.run([exe, src, dst]).returncode != 0:
        sys.exit('maptest failed')
    patched = open(dst, 'rb').read()
    block, size = find_abc(patched)
    expected = append_abc(vanilla, patched[block + 12:block + 12 + size])
    walked, size, pointers = index_walk(patched)
    walk_ok = walked == size and block - 0x40 in pointers  # movie at 0x40; new block's "1" is fixed up
    print('LOBYHUD.iggy: %d -> %d bytes, %s, index walk %s' % (len(vanilla), len(patched),
          'matches lobyhud_abc.py' if patched == expected else 'DIFFERS from lobyhud_abc.py',
          'covers the new block' if walk_ok else 'ends at %#x of %#x' % (walked, size)))
    sys.exit(0 if patched == expected and walk_ok else 1)

if sys.argv[1] == 'data':
    from xv2data.game import Game
    g = Game(sys.argv[2])
    exe, work = sys.argv[3], sys.argv[4]
    tpq = g.original('data/quest/tpq/tpq_data.qxd')
    n, off = struct.unpack_from('<ii', tpq, 8)
    story = {}  # TPQ index -> name
    for i in range(n):
        o = off + i * 292
        story[struct.unpack_from('<i', tpq, o + 16)[0]] = tpq[o:o + 16].split(b'\0')[0].decode()
    ok, total = True, 0

    def run(kind, key):
        src = g.original(key)
        i, o = os.path.join(work, 'in.bin'), os.path.join(work, 'out.bin')
        open(i, 'wb').write(src)
        r = subprocess.run([exe, kind, i, o], capture_output=True, text=True)
        return src, open(o, 'rb').read(), int(r.stdout)

    for key in g.cpk.list('data/quest/', '_data.qxd'):
        if key.lower().endswith('tpq_data.qxd'):
            continue
        parallel = key.lower().endswith('tmq_data.qxd')
        src, out, count = run('tmq' if parallel else 'qxd', key)
        n, off = struct.unpack_from('<ii', src, 8)
        want = bytearray(src)
        names = []
        if parallel:  # unlock kind (+80) -> 0 = unlocked from the start (QxdUnlockFromStart)
            for q in range(n):
                o = off + q * 292
                if struct.unpack_from('<i', src, o + 80)[0] != 0:
                    struct.pack_into('<i', want, o + 80, 0)
                    names.append(src[o:o + 16].split(b'\0')[0].decode() + ':kind0')
        for q in range(n):
            o = off + q * 292
            for r in range(4):
                t, v = struct.unpack_from('<ii', src, o + 48 + 4 * r)[0], struct.unpack_from('<i', src, o + 64 + 4 * r)[0]
                if ((t == 0 and story.get(v, '').startswith('CTP_') and story[v] != 'CTP_00_00')
                        or (t in (1, 4) and v >= 0)):
                    struct.pack_into('<i', want, o + 48 + 4 * r, -1)
                    struct.pack_into('<i', want, o + 64 + 4 * r, -1)
                    names.append(src[o:o + 16].split(b'\0')[0].decode() + ':' +
                                 (story[v] if t == 0 else f'TMQ#{v}' if t == 1 else f'HLQ#{v}'))
        good = bytes(want) == out and count == len(names)
        ok &= good
        total += count
        if count or not good:
            print(f"{key}: {count} {'OK' if good else 'FAILED'}  {' '.join(names[:12])}{' ...' if len(names) > 12 else ''}")
    for item in ('skill', 'talisman', 'accessory', 'costume_top', 'costume_bottom', 'costume_gloves', 'costume_shoes'):
        key = f'data/system/item/{item}_item.idb'
        src, out, count = run('idb', key)
        n = struct.unpack_from('<i', src, 8)[0]
        want = bytearray(src)
        points = []
        for k in range(n):
            v = struct.unpack_from('<H', src, 16 + k * 784 + 20)[0]
            if v not in (0, 32766, 32767, 65535):
                struct.pack_into('<H', want, 16 + k * 784 + 20, 0x7fff)
                points.append(v)
        good = bytes(want) == out and count == len(points)
        ok &= good
        total += count
        print(f"{key}: {count} {'OK' if good else 'FAILED'}  story points {sorted(set(points))}")
    # TP shop: same header but the count, every season lists exactly the set
    # of items the original lists in any season, each once, in one order.
    for key in ('data/system/EventShopTbl.est', 'data/system/EventShopTbl_freemium.est'):
        src, out, count = run('est', key)
        n, m = struct.unpack_from('<i', src, 8)[0], struct.unpack_from('<i', out, 8)[0]
        a = [struct.unpack_from('<3i', src, 16 + 12 * i) for i in range(n)]
        b = [struct.unpack_from('<3i', out, 16 + 12 * i) for i in range(m)]
        items = {(t, i) for t, i, s in a}
        seasons = [[(t, i) for t, i, s in b if s == k] for k in range(4)]
        good = (src[:8] == out[:8] and src[12:16] == out[12:16] and len(out) == 16 + 12 * m
                and m == 4 * len(items) and count == len(items) and [s for _, _, s in b] == sorted(s for _, _, s in b)
                and all(len(x) == len(items) and set(x) == items and x == seasons[0] for x in seasons))
        ok &= good
        print(f"{key}: {count} items {'OK' if good else 'FAILED'}  per season before: "
              f"{[sum(1 for e in a if e[2] == k) for k in range(4)]}")
    print('changed:', total)
    print('OK' if ok else 'FAILED')
    sys.exit(0 if ok else 1)


def load(name, stop):
    """The definitions of a tools\\gamefiles script, without its main part."""
    path = os.path.join(TOOLS, 'ClaudeTools', 'gamefiles', name)
    src = open(path, encoding='utf-8').read()
    ns = {'__file__': path}
    exec(src[:src.index(stop)], ns)
    return ns


# Same as src\tsr.h.
ALWAYS_EVENT = '             AVATAR(LEVEL)>=1'

# Event table: same size, only trigger conditions differ: the mentors' and
# desks' story quest requirements are always true; the transfer robots' and the
# radio's "locked while the prologue is not cleared" is never true. Copy of
# src\lobby_npcs.h:
STORY_NPCS = [(3, 36), (81, 81), (94, 97), (148, 148), (208, 273), (976, 976), (982, 982), (1007, 1046),
              (1309, 1309)]
PQ_TUTORIAL_NPCS = [(976, 976), (1046, 1046)]
PQ_TUTORIAL, ALWAYS_PQ = 'QUEST(TMQ,STATE,TMQ_1400)==3', '            AVATAR(LEVEL)>=1'
LOCKED_NPCS = [(65, 80), (94, 94)]
LOCKED, NEVER = 'QUEST(TPQ,STATE,CTP_00_01)!=3', 'QUEST(TPQ,STATE,CTP_00_01)==9'
# Left to the story, the tutorial and the DLC flags: none of these triggers may change.
UNTOUCHED = {85, 86, 87, 285, 286, 287, 288, 289, 291, 292, 308, 981, 987, 1006, 1294}


def within(npc, ranges):
    return any(a <= npc <= b for a, b in ranges)


def story_always(c):
    return re.sub(r'QUEST\(TPQ,STATE,CTP_\d\d_\d\d\)==3', ALWAYS_EVENT, c)


parse = load('tsd.py', '\ng = Game(')['parse']
ra, rb = open(sys.argv[2], 'rb').read(), open(sys.argv[3], 'rb').read()
ta, ea = parse(ra)
tb, eb = parse(rb)
ok = len(ra) == len(rb) and len(ta) == len(tb) and ea == eb
mentors = desks = locked = 0
for x, y in zip(ta, tb):
    ok = ok and all(x[k] == y[k] for k in x if k != 'Condition')
    npc = x['TNL_ID']
    want = x['Condition']
    talk = x['Type'] == 1  # type 2 (chase gimmicks) is never edited
    if talk and within(npc, STORY_NPCS):
        want = story_always(want)
    if talk and within(npc, PQ_TUTORIAL_NPCS):
        want = want.replace(PQ_TUTORIAL, ALWAYS_PQ)
    if talk and within(npc, LOCKED_NPCS):
        locked += LOCKED in want
        want = want.replace(LOCKED, NEVER)
    ok = ok and y['Condition'] == want and not (npc in UNTOUCHED and x != y)
    if x != y:
        if npc <= 36:
            mentors += 1
        else:
            desks += 1
    if x != y and npc > 36:
        print(f"trigger {x['ID']} (NPC {npc}, event {x['Event_ID']}):\n  - {x['Condition']}\n  + {y['Condition']}")
print('changed: mentor triggers', mentors, ' desk/robot triggers', desks, ' locked triggers', locked)
ok = ok and mentors > 0 and locked == 17
print('OK' if ok else 'FAILED')
sys.exit(0 if ok else 1)

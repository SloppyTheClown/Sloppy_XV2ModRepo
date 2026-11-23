"""Read-only: map the lobby_event_manage.tsd event / trigger / lobby NPC ids
that Revamp 5.1.1 writes as fixed numbers (its XMLs were made on game
1.25.02) to the ids the same vanilla entries have in 1.26.01, and print the
C# tables PostInstall's LobbyTsd uses.

Method: Revamp's XMLs (in the .installinfo) name each fixed id; the vanilla
1.25.02 file (Refs copy) says which entry that id was; the same entry is
looked up by content in vanilla 1.26.01 (lobby NPC ids inside it translated
first, 1.26 renumbered those too).

    python tsdmap.py --installinfo "<Revamp>\\Installer\\Revamp Xenoverse 2 Installer.installinfo"
                     --ref125 "<1.25.02 game copy>\\cpk" --lib <folder> [--lib <folder> ...]

--lib: folders holding the tnl / tsd parsers (tnl.g = the 1.26 game) and the
xv2data package (CpkSet); they are not part of this repo.
"""
import argparse
import contextlib
import io
import re
import sys
import zipfile

args = argparse.ArgumentParser(description=__doc__.split('\n')[0])
args.add_argument('--installinfo', required=True, help="Revamp 5.1.1's .installinfo")
args.add_argument('--ref125', required=True, help='cpk folder of a vanilla 1.25.02 game copy')
args.add_argument('--lib', action='append', default=[], help='folder with tnl.py / tsd.py / xv2data (repeatable)')
opts = args.parse_args()
for lib in opts.lib:
    sys.path.insert(0, lib)
with contextlib.redirect_stdout(io.StringIO()):
    import tnl                                   # TNL parser; tnl.g = the 1.26 game
    import tsd                                   # TSD parser; tsd.trig / tsd.events = vanilla 1.26
from xv2data.cpk import CpkSet

INSTALLINFO = opts.installinfo
REF_125 = opts.ref125
AUTO_ID_FIRST = 10000   # Revamp's new triggers: ID="{AutoID=(10000; 20000)}"

installinfo = zipfile.ZipFile(INSTALLINFO)
events_xml = installinfo.read('data/main/event/lobby_event_events.tsd.xml').decode('utf-8-sig')
triggers_xml = installinfo.read('data/main/event/lobby_event_triggers.tsd.xml').decode('utf-8-sig')
trigger_blocks = re.findall(r'<Trigger ID="([^"]*)">(.*?)</Trigger>', triggers_xml, re.S)
fixed_events = sorted(int(i) for i in re.findall(r'<Event ID="(\d+)"', events_xml))
fixed_triggers = sorted(int(i) for i, _ in trigger_blocks if i.isdigit())
fixed_event_refs = sorted({int(i) for i in re.findall(r'<Event_ID value="(\d+)"', triggers_xml)})
fixed_npc_refs = sorted({int(i) for i in re.findall(r'<TNL_ID value="(\d+)"', triggers_xml)})
new_trigger_event_refs = sorted({int(e) for i, body in trigger_blocks if not i.isdigit()
                                 for e in re.findall(r'<Event_ID value="(\d+)"', body)})
fixed_event_npcs = sorted({int(n) for v in re.findall(r'<TNL_IDS values="([^"]*)"', events_xml)
                           for n in v.split(',') if n.strip().isdigit()})

ref = CpkSet(REF_125)
old_trig_list, old_event_list = tsd.parse(ref.read('data/event/lobby/lobby_event_manage.tsd'))
old_events = {e['ID']: e for e in old_event_list}
old_triggers = {t['ID']: t for t in old_trig_list}
new_events = tsd.events
new_triggers = {t['ID']: t for t in tsd.trig}

# ---- lobby NPCs: by every field but the id; then by model + name (robots
# whose position/action changed in 1.26, e.g. Dastar 1276 -> 1294)
old_tnl = tnl.parse_tnl(ref.read('data/event/lobby/lobby_npc_list.tnl'))
new_tnl = tnl.parse_tnl(tnl.g.original('data/event/lobby/lobby_npc_list.tnl'))


def npc_map_by(key):
    found = {}
    for c in new_tnl['chara'] + new_tnl['object']:
        found.setdefault(key(c), set()).add(c['ID'])
    result = {}
    for c in old_tnl['chara'] + old_tnl['object']:
        ids = found.get(key(c), set())
        if len(ids) == 1:
            result.setdefault(c['ID'], next(iter(ids)))
    return result


npc_map = npc_map_by(lambda c: (c.get('CMS'), c['LobbyName']))
npc_map.update(npc_map_by(lambda c: tuple(v for k, v in sorted(c.items()) if k != 'ID')))


def new_npc(npc_id, old):
    return npc_map.get(npc_id, npc_id) if old else npc_id


def event_key(e, old=False):
    return (e['Name'], e['Path'], e['Script'], e['Function'], e['Args'],
            tuple(new_npc(i, old) for i in e['TNL_IDs']))


def trigger_key(t, events, old=False):
    e = events.get(t['Event_ID'])
    return (t['DLC_Flag'], t['Icon'], t['Type'], new_npc(t['TNL_ID'], old), t['TNL_SubID'], t['I_28'],
            t['Condition'], event_key(e, old) if e else None)


def nearest(hits, near):
    return min(hits, key=lambda i: abs(i - near)) if hits else None


event_map = {e: nearest([i for i, x in new_events.items() if event_key(x) == event_key(old_events[e], True)], e)
             for e in sorted(set(fixed_events) | set(fixed_event_refs))}
trigger_map = {t: nearest([i for i, x in new_triggers.items()
                           if trigger_key(x, new_events) == trigger_key(old_triggers[t], old_events, True)], t)
               for t in fixed_triggers}
npc_ref_map = {n: npc_map.get(n) for n in fixed_npc_refs}

# ---- checks the C# relies on
problems = [f'{name} {k} unmatched' for name, m in (('event', event_map), ('trigger', trigger_map),
                                                     ('npc', npc_ref_map)) for k, v in m.items() if v is None]
problems += [f'event TNL_IDS {n} moved' for n in fixed_event_npcs if npc_map.get(n, n) != n]
problems += [f'new triggers use event {e}, which is also moved onto' for e in new_trigger_event_refs
             if event_map[e] != e and e in event_map.values()]
problems += [f'new triggers use lobby NPC {n}, which moved' for i, body in trigger_blocks if not i.isdigit()
             for n in map(int, re.findall(r'<TNL_ID value="(\d+)"', body)) if npc_map.get(n, n) != n]
if problems:
    sys.exit('\n'.join(problems))


def pairs(m):
    moved = [(k, v) for k, v in m.items() if v != k]
    lines, row = [], []
    for k, v in moved:
        row.append(f'{{ {k}, {v} }}')
        if len(row) == 8:
            lines.append(', '.join(row)); row = []
    if row:
        lines.append(', '.join(row))
    return len(moved), ',\n        '.join(lines)


fixed_trigger_event_refs = sorted({int(e) for i, body in trigger_blocks if i.isdigit()
                                   for e in re.findall(r'<Event_ID value="(\d+)"', body)})
for name, m in (('EventMoves', {e: event_map[e] for e in fixed_events}),
                ('TriggerMoves', trigger_map),
                ('EventRefMoves', {e: event_map[e] for e in fixed_trigger_event_refs}),
                ('NpcMoves', npc_ref_map),
                ('NewTriggerEventMoves', {e: event_map[e] for e in new_trigger_event_refs})):
    count, body = pairs(m)
    print(f'    // {count} pairs\n    static readonly int[,] {name} =\n    {{\n        {body}\n    }};')

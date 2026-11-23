"""Which lobby NPC offers a quest, and where it stands (installed files).

    python lobbyquests.py <game> <pattern> [pattern ...]

Parses data\\event\\lobby\\lobby_event_manage.tsd (triggers: condition ->
NPC TNL id -> event) and lobby_npc_list.tnl (NPC: model, costume, name key,
position, zone), using tools\\gamefiles's parsers. Prints every trigger whose
condition or event mentions a pattern (e.g. OSQ_TMT, BAQ_RVS, CTP_RVP).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
DLC = os.path.join(HERE, '..', '..', '..', 'ClaudeTools', 'gamefiles')


def load(name, stop):
    ns = {'__file__': os.path.join(DLC, name)}
    exec(open(os.path.join(DLC, name)).read().split(stop)[0], ns)
    return ns


if __name__ == '__main__':
    game, pats = sys.argv[1], sys.argv[2:]
    tsd = load('tsd.py', '\ng = Game(')['parse']
    tnl = load('tnl.py', '\ng = Game(')['parse_tnl']
    lob = os.path.join(game, 'data', 'event', 'lobby')
    trig, ev = tsd(open(os.path.join(lob, 'lobby_event_manage.tsd'), 'rb').read())
    npcs = tnl(open(os.path.join(lob, 'lobby_npc_list.tnl'), 'rb').read())
    by = {}
    for c in npcs['chara'] + npcs['teacher']:
        by.setdefault(c['ID'], c)
    events = {e['ID']: e for e in ev}
    for t in trig:
        e = events.get(t['Event_ID'], {})
        text = t['Condition'] + ' ' + ' '.join(str(e.get(k, '')) for k in ('Name', 'Script', 'Function', 'Args'))
        if not any(p in text for p in pats):
            continue
        n = by.get(t['TNL_ID'], {})
        print('NPC %s %s/%s costume %s name %s at %s zone %s' % (t['TNL_ID'], n.get('CMS'), t['TNL_SubID'], n.get('Costume'),
              n.get('LobbyName'), n.get('Position'), n.get('Zone')))
        print('    if   ', t['Condition'])
        print('    event', e.get('Name'), e.get('Function'), e.get('Args'))

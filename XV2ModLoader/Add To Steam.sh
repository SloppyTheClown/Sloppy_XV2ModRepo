#!/bin/bash
# Steam Deck / Linux version of "Add To Steam.bat": adds the game to Steam as
# two non-Steam shortcuts running bin/DBXV2.exe under Proton Experimental,
# with the game's own library art.
#   Modded:   WINEDLLOVERRIDES="xinput1_3,d3dx11_43,winmm=n,b"  (mod loader,
#             XV2Patcher, Microsoft's D3DX 11 synced into bin: Proton's own
#             cannot load the BC7 textures some mods' menus use)
#   Unmodded: WINEDLLOVERRIDES="xinput1_3,winmm=b"    (Wine's own DLLs, no mods)
# The stock Steam entry starts EasyAntiCheat, which does not run on Linux.
#   bash "Add To Steam.sh"          add them (or update ones already there)
#   bash "Add To Steam.sh" remove   take them out of Steam again
# Sits in <game>/XV2ModLoader. Run it from Desktop Mode; Steam is closed while
# its shortcuts are changed and started again afterwards.

# started from the file manager: show the output in a terminal window
if [ ! -t 1 ] && [ -z "$XV2_IN_TERM" ] && [ -n "$DISPLAY$WAYLAND_DISPLAY" ] && command -v konsole >/dev/null; then
    XV2_IN_TERM=1 exec konsole -e bash "$0" "$@"
fi

python3 - "$0" "$@" <<'PY'
import os, re, shutil, struct, subprocess, sys, time, zlib

here = os.path.dirname(os.path.abspath(sys.argv[1]))
remove = len(sys.argv) > 2 and sys.argv[2].lower() == 'remove'
GAME_ID = 454650
DLL_MODDED = 'WINEDLLOVERRIDES="xinput1_3,d3dx11_43,winmm=n,b"'
DLL_UNMODDED = 'WINEDLLOVERRIDES="xinput1_3,winmm=b"'
PROTON = 'proton_experimental'
LAUNCHERS = [
    # "(Deck)": the PC's shortcuts show up here too, for Remote Play
    dict(name='DRAGON BALL XENOVERSE 2 (Modded) (Deck)', old='DRAGON BALL XENOVERSE 2 (Modded)',
         options=DLL_MODDED + ' %command%', modded=True),
    dict(name='DRAGON BALL XENOVERSE 2 (Unmodded) (Deck)', old='DRAGON BALL XENOVERSE 2 (Unmodded)',
         options=DLL_UNMODDED + ' %command%', modded=False),
]
# grid file suffix -> library cache name(s) of the game's own art
ART = [('p', ['library_600x900.jpg']), ('', ['header.jpg', 'library_header.jpg']),
       ('_hero', ['library_hero.jpg']), ('_logo', ['logo.png'])]
IMAGE_EXTS = ('.png', '.jpg', '.jpeg')

def fail(msg):
    print('\033[31m' + msg + '\033[0m')
    sys.exit(1)

game = here
if not os.path.isfile(os.path.join(game, 'bin', 'DBXV2.exe')):
    game = os.path.dirname(here)
exe_path = os.path.join(game, 'bin', 'DBXV2.exe')
if not os.path.isfile(exe_path):
    fail('The game was not found. This file belongs in the XV2ModLoader folder inside the game folder.')
start_dir = os.path.join(game, 'bin') + '/'

# --- Steam and its accounts -------------------------------------------------

steam = os.environ.get('XV2_STEAM')
if not steam:
    for d in ('~/.local/share/Steam', '~/.steam/steam', '~/.var/app/com.valvesoftware.Steam/.local/share/Steam'):
        d = os.path.realpath(os.path.expanduser(d))
        if os.path.isdir(os.path.join(d, 'userdata')):
            steam = d
            break
if not steam or not os.path.isdir(os.path.join(steam, 'userdata')):
    fail('Could not find Steam.')

users = [d for d in sorted(os.listdir(os.path.join(steam, 'userdata')))
         if d.isdigit() and d != '0' and os.path.isdir(os.path.join(steam, 'userdata', d))]
if not users:
    fail('No Steam accounts found. Start Steam and log in once first.')
names = {}
try:
    text = open(os.path.join(steam, 'config', 'loginusers.vdf'), encoding='utf-8', errors='replace').read()
    for sid, acct in re.findall(r'"(7656\d{13})"\s*\{[^}]*?"AccountName"\s*"([^"]*)"', text):
        names[str(int(sid) - 76561197960265728)] = acct
except OSError:
    pass

# --- binary VDF (shortcuts.vdf): lists of [type, key, value], in file order --

def read_vdf(b):
    pos = 0
    def cstr():
        nonlocal pos
        e = b.index(b'\0', pos)
        s = b[pos:e].decode('utf-8', 'surrogateescape')
        pos = e + 1
        return s
    def rmap(top):
        nonlocal pos
        out = []
        while True:
            if pos >= len(b):
                if top: return out
                raise ValueError('the file ends early')
            t = b[pos]; pos += 1
            if t == 8: return out
            k = cstr()
            if t == 0: v = rmap(False)
            elif t == 1: v = cstr()
            elif t == 2: v = struct.unpack_from('<i', b, pos)[0]; pos += 4
            elif t == 3: v = b[pos:pos + 4]; pos += 4
            elif t in (7, 10): v = b[pos:pos + 8]; pos += 8
            else: raise ValueError('unknown entry type %d' % t)
            out.append([t, k, v])
    return rmap(True)

def write_vdf(m):
    out = bytearray()
    def cstr(s): out.extend(s.encode('utf-8', 'surrogateescape') + b'\0')
    def wmap(m):
        for t, k, v in m:
            out.append(t); cstr(k)
            if t == 0: wmap(v)
            elif t == 1: cstr(v)
            elif t == 2: out.extend(struct.pack('<i', v))
            else: out.extend(v)
        out.append(8)
    wmap(m)
    return bytes(out)

def field(m, key):
    for e in m:
        if e[1].lower() == key.lower(): return e
    return None

def get(m, key, default=''):
    e = field(m, key)
    return e[2] if e else default

def set_field(m, t, key, val):
    e = field(m, key)
    if e: e[2] = val
    else: m.append([t, key, val])

def shortcut_id(exe, name):
    # Steam's id for a shortcut: crc32(exe + name) with the top bit set, stored signed
    u = zlib.crc32((exe + name).encode('utf-8')) | 0x80000000
    return u - 0x100000000

def exe_of(m):
    e = get(m, 'exe').strip()
    if e.startswith('"') and e.find('"', 1) > 0: e = e[1:e.find('"', 1)]
    return os.path.realpath(e) if e else ''

def which_launcher(m):
    # an entry that runs this game's DBXV2.exe: by name, else by its DLL overrides
    if exe_of(m) != os.path.realpath(exe_path): return None
    for l in LAUNCHERS:
        if get(m, 'appname') in (l['name'], l['old']): return l
    opts = get(m, 'LaunchOptions').replace(' ', '')
    if 'winmm=n' in opts: return LAUNCHERS[0]
    if 'winmm=b' in opts: return LAUNCHERS[1]
    return LAUNCHERS[0]   # a plain shortcut to the exe: the loader is what it was made for

# --- the game's art ---------------------------------------------------------

def find_art(suffix, cache_names):
    for u in users:
        for ext in IMAGE_EXTS:
            f = os.path.join(steam, 'userdata', u, 'config', 'grid', '%d%s%s' % (GAME_ID, suffix, ext))
            if os.path.isfile(f): return f
    cache = os.path.join(steam, 'appcache', 'librarycache')
    for n in cache_names:
        for f in (os.path.join(cache, str(GAME_ID), n), os.path.join(cache, '%d_%s' % (GAME_ID, n))):
            if os.path.isfile(f): return f
        for root, _, files in os.walk(os.path.join(cache, str(GAME_ID))):
            if n in files: return os.path.join(root, n)
    return None

def clear_art(grid, uid):
    for suffix, _ in ART:
        for ext in IMAGE_EXTS:
            f = os.path.join(grid, '%d%s%s' % (uid, suffix, ext))
            if os.path.isfile(f): os.remove(f)

art = {}
if not remove:
    for suffix, cache_names in ART:
        f = find_art(suffix, cache_names)
        if f: art[suffix] = f
    if not art:
        print("\033[33mNo library art for the game was found - open its page in Steam once so Steam caches it.\033[0m")

# --- Proton for the shortcuts (config/config.vdf, text VDF) ------------------

def set_compat(text, ids, add):
    # CompatToolMapping holds one block per app id; shortcut ids are unsigned
    def section():
        m = re.search(r'"CompatToolMapping"\s*\{', text)
        if not m: return None
        depth, i = 1, m.end()
        while depth:
            if i >= len(text): raise ValueError('config.vdf ends early')
            depth += {'{': 1, '}': -1}.get(text[i], 0)
            i += 1
        return m.end(), i - 1   # inside the braces
    if not section():
        if not add: return text
        s = re.search(r'"Steam"\s*\{', text)
        if not s: raise ValueError('config.vdf has no "Steam" section')
        text = text[:s.end()] + '\n\t\t\t\t"CompatToolMapping"\n\t\t\t\t{\n\t\t\t\t}' + text[s.end():]
    for i in ids:
        a, b = section()
        found = re.search(r'\n[ \t]*"%d"\s*\{[^{}]*\}' % i, text[a:b])
        if add and not found:
            text = text[:a] + ('\n\t\t\t\t\t"%d"\n\t\t\t\t\t{\n\t\t\t\t\t\t"name"\t\t"%s"\n'
                               '\t\t\t\t\t\t"config"\t\t""\n\t\t\t\t\t\t"priority"\t\t"250"\n\t\t\t\t\t}'
                               % (i, PROTON)) + text[a:]
        elif not add and found:
            text = text[:a + found.start()] + text[a + found.end():]
    return text

# --- Steam must be closed: it rewrites shortcuts.vdf and config.vdf on exit ---

def steam_running():
    return subprocess.run(['pgrep', '-x', 'steam'], stdout=subprocess.DEVNULL).returncode == 0

# XV2_STEAM points at a copy of a Steam folder (testing): leave Steam alone
restart = False
if not os.environ.get('XV2_STEAM') and steam_running():
    print('Steam has to be closed while its shortcuts are changed. It is started again afterwards.')
    try:
        with open('/dev/tty') as tty:
            print('Close Steam now? [Y/N] ', end='', flush=True)
            answer = tty.readline()
    except OSError:
        answer = ''
    if not answer.strip().lower().startswith('y'):
        fail('Nothing was changed.')
    subprocess.Popen(['steam', '-shutdown'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.time() + 60
    while steam_running():
        if time.time() > deadline:
            fail('Steam did not close within a minute. Close it yourself and run this again.')
        time.sleep(0.5)
    time.sleep(1)
    restart = True

# --- per account --------------------------------------------------------------

failed = False
compat_ids = set()
removed_ids = set()
for u in users:
    who = names.get(u, u)
    config = os.path.join(steam, 'userdata', u, 'config')
    path = os.path.join(config, 'shortcuts.vdf')
    grid = os.path.join(config, 'grid')
    try:
        root = read_vdf(open(path, 'rb').read()) if os.path.isfile(path) else []
        sc = field(root, 'shortcuts')
        if not sc:
            sc = [0, 'shortcuts', []]
            root.append(sc)
        lst = sc[2]
        done = []

        for l in LAUNCHERS:
            entry = None
            for e in list(lst):
                if e[0] != 0 or which_launcher(e[2]) is not l: continue
                if remove:
                    uid = get(e[2], 'appid', 0) & 0xffffffff
                    if uid and os.path.isdir(grid): clear_art(grid, uid)
                    if uid: removed_ids.add(uid)
                    lst.remove(e)
                    done.append('removed ' + get(e[2], 'appname'))
                elif entry is None:
                    entry = e[2]
            if remove: continue

            exe = '"' + exe_path + '"'
            if entry is not None:
                what = 'updated'
                if get(entry, 'appname') != l['name']:
                    what = 'renamed %s to' % get(entry, 'appname')
                    set_field(entry, 1, 'appname', l['name'])   # its appid stays, so art and Proton stay
                set_field(entry, 1, 'exe', exe)
                set_field(entry, 1, 'StartDir', start_dir)
                opts = get(entry, 'LaunchOptions')
                want = DLL_MODDED if l['modded'] else DLL_UNMODDED
                if want not in opts:
                    # keep extras such as PROTON_LOG=1, replace any other DLL override
                    opts = re.sub(r'WINEDLLOVERRIDES=("[^"]*"|\S*)\s*', '', opts).strip()
                    if '%command%' not in opts: opts = (opts + ' %command%').strip()
                    set_field(entry, 1, 'LaunchOptions', want + ' ' + opts)
            else:
                what = 'added'
                entry = [[2, 'appid', shortcut_id(exe, l['name'])], [1, 'appname', l['name']], [1, 'exe', exe],
                         [1, 'StartDir', start_dir], [1, 'icon', ''], [1, 'ShortcutPath', ''],
                         [1, 'LaunchOptions', l['options']], [2, 'IsHidden', 0], [2, 'AllowDesktopConfig', 1],
                         [2, 'AllowOverlay', 1], [2, 'OpenVR', 0], [2, 'Devkit', 0], [1, 'DevkitGameID', ''],
                         [2, 'DevkitOverrideAppID', 0], [2, 'LastPlayTime', 0], [1, 'FlatpakAppID', ''],
                         [1, 'sortas', ''], [0, 'tags', []]]
                lst.append([0, '', entry])
            name = get(entry, 'appname')
            if not field(entry, 'appid'): entry.insert(0, [2, 'appid', shortcut_id(exe, name)])
            uid = get(entry, 'appid') & 0xffffffff
            compat_ids.add(uid)
            if art:
                os.makedirs(grid, exist_ok=True)
                clear_art(grid, uid)
                for suffix, src in art.items():
                    shutil.copyfile(src, os.path.join(grid, '%d%s%s' % (uid, suffix, os.path.splitext(src)[1].lower())))
            done.append('%s %s' % (what, name))

        for i, e in enumerate(lst): e[1] = str(i)   # entries are keyed 0, 1, 2 ...
        os.makedirs(config, exist_ok=True)
        if os.path.isfile(path): shutil.copyfile(path, path + '.bak')
        data = write_vdf(root)
        with open(path + '.tmp', 'wb') as f: f.write(data)
        os.replace(path + '.tmp', path)
        print('%s: %s' % (who, ', '.join(done) or 'nothing to remove'))
    except Exception as ex:
        print('\033[31m%s: FAILED, left unchanged - %s\033[0m' % (who, ex))
        failed = True

# Proton Experimental for every shortcut that was added or updated
cfg = os.path.join(steam, 'config', 'config.vdf')
ids = removed_ids if remove else compat_ids
if ids and os.path.isfile(cfg):
    try:
        text = open(cfg, encoding='utf-8', errors='surrogateescape').read()
        new = set_compat(text, sorted(ids), not remove)
        if new != text:
            shutil.copyfile(cfg, cfg + '.bak')
            with open(cfg + '.tmp', 'w', encoding='utf-8', errors='surrogateescape') as f: f.write(new)
            os.replace(cfg + '.tmp', cfg)
        if not remove: print('Proton: set to Proton Experimental where none was chosen.')
    except Exception as ex:
        print('\033[31mProton setting FAILED, left unchanged - %s\033[0m' % ex)
        print('Pick Proton yourself: shortcut > Properties > Compatibility.')
        failed = True

# Game Mode starts Steam again by itself; Desktop Mode needs it started
if restart:
    time.sleep(3)
    if not steam_running():
        subprocess.Popen(['setsid', 'steam'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         stdin=subprocess.DEVNULL, start_new_session=True)
if failed: sys.exit(1)
if not remove: print('Done. Library art: %d of %d images found.' % (len(art), len(ART)))
PY
rc=$?
if [ -n "$XV2_IN_TERM" ]; then read -r -p 'Press Enter to close.'; fi
exit $rc

"""Writes the release's patch set, so the release carries no file of Revamp's.

The bridge's lib\\ (Revamp's Xv2CoreLib rebuilt + the libraries the Revamp
installer embeds) and epatches\\ (Revamp's exe patches, ported) are made from
Revamp's files. For each of them this lists where it comes from in the
player's Revamp download and, where the bridge's file differs, writes a bsdiff
patch from Revamp's file to it. src\\Release.cs applies them.

usage: make_patches.py <exe_res> <corelib_res> <Revamp .installinfo> <build> <out>

  exe_res      the Costura resources of Revamp's installer exe, unpacked
  corelib_res  the Costura resources of that installer's xv2corelib.dll, unpacked
  build        the bridge's build folder (lib\\, epatches\\, iggymaps_*.txt)
  out          written: patches\\manifest.txt, patches\\*.rbpatch, files\\*

manifest.txt, one line per file the release rebuilds:
  <target under the work folder>|<source>|<patch resource or ->|<SHA-256 of the result>
  source: exe:<name> / corelib:<name> (Costura resource), installinfo:<entry>,
          bridge:<resource> (the bridge's own file, embedded as it is)

Needs bsdiff4 (pip install bsdiff4).
"""
import hashlib
import os
import shutil
import struct
import sys
import zipfile
import zlib

import bsdiff4.core


def sha(b):
    return hashlib.sha256(b).hexdigest()


def write_patch(old, new, path):
    """bsdiff with our own container (raw deflate, so .NET can read it):
    "RBPATCH1", old/new length, SHA-256 old/new, control/diff/extra lengths,
    then the three blocks deflated together."""
    ctrl, diff, extra = bsdiff4.core.diff(old, new)
    comp = zlib.compressobj(9, zlib.DEFLATED, -15)
    body = comp.compress(ctrl + diff + extra) + comp.flush()
    with open(path, 'wb') as f:
        f.write(b'RBPATCH1' + struct.pack('<qq', len(old), len(new)))
        f.write(hashlib.sha256(old).digest() + hashlib.sha256(new).digest())
        f.write(struct.pack('<qqq', len(ctrl), len(diff), len(extra)) + body)
    return os.path.getsize(path)


def main():
    exe_res, corelib_res, installinfo, build, out = sys.argv[1:6]
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, 'patches'))
    os.makedirs(os.path.join(out, 'files'))
    lines = ['# target|source|patch|sha256 - see src\\make_patches.py']

    def add(target, source, new, old=None):
        patch = '-'
        if old is not None and old != new:
            name = target.replace('\\', '_') + '.rbpatch'
            size = write_patch(old, new, os.path.join(out, 'patches', name))
            patch = 'patches/' + name
            print(f'  {target}: patch {size // 1024} KB from {source}')
        lines.append(f'{target}|{source}|{patch}|{sha(new)}')

    # lib\: every file must come from Revamp's installer (as it is, or patched).
    # Both the exe and its xv2corelib embed some libraries, in different
    # versions: an identical copy wins over patching the other one.
    for name in sorted(os.listdir(os.path.join(build, 'lib'))):
        new = open(os.path.join(build, 'lib', name), 'rb').read()
        found = [(f'{kind}:{name}', open(os.path.join(folder, name), 'rb').read())
                 for kind, folder in (('exe', exe_res), ('corelib', corelib_res))
                 if os.path.exists(os.path.join(folder, name))]
        if not found:
            raise SystemExit(f'lib\\{name} is not in the Revamp installer - the release cannot rebuild it')
        source, old = next((f for f in found if f[1] == new), found[0])
        add('lib\\' + name, source, new, old)

    # epatches\<build>\: ported from the .installinfo's Epatches\.
    z = zipfile.ZipFile(installinfo)
    root = os.path.join(build, 'epatches')
    for game in sorted(os.listdir(root)):
        for name in sorted(os.listdir(os.path.join(root, game))):
            new = open(os.path.join(root, game, name), 'rb').read()
            add(f'epatches\\{game}\\{name}', 'installinfo:Epatches/' + name, new, z.read('Epatches/' + name))

    # The texture id maps are the bridge's own data.
    for name in sorted(os.listdir(build)):
        if name.startswith('iggymaps_') and name.endswith('.txt'):
            shutil.copyfile(os.path.join(build, name), os.path.join(out, 'files', name))
            add(name, 'bridge:files/' + name, open(os.path.join(build, name), 'rb').read())

    open(os.path.join(out, 'patches', 'manifest.txt'), 'w', newline='\n').write('\n'.join(lines) + '\n')
    print(f'{len(lines) - 1} files in the manifest')


if __name__ == '__main__':
    main()

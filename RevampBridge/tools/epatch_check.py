"""Check Eternity patch files (XV2Patcher Epatches) against a DBXV2.exe.

For every <Patch>: where its instruction pattern is found (file offset and
RVA), whether that is unique, and - for patches whose Write contains
`E9 xx xx xx xx` (jmp rel32) - where that jump would land in this exe, so
hard-coded code-cave jumps can be compared with where the cave patch lands.

usage: epatch_check.py <DBXV2.exe> <patch.xml> [...]
"""
import re
import struct
import sys
import xml.etree.ElementTree as ET


def sections(exe):
    pe = struct.unpack_from('<I', exe, 0x3C)[0]
    n = struct.unpack_from('<H', exe, pe + 6)[0]
    opt = struct.unpack_from('<H', exe, pe + 20)[0]
    base = struct.unpack_from('<Q', exe, pe + 24 + 24)[0]
    out = []
    for i in range(n):
        s = pe + 24 + opt + i * 40
        name = exe[s:s + 8].rstrip(b'\0').decode()
        vsize, va, rsize, raw = struct.unpack_from('<IIII', exe, s + 8)
        out.append((name, va, vsize, raw, rsize))
    return base, out


def rva_to_off(secs, rva):
    for name, va, vs, raw, rs in secs:
        if va <= rva < va + max(vs, rs):
            return raw + (rva - va)
    return None


def off_to_rva(secs, off):
    for name, va, vs, raw, rs in secs:
        if raw <= off < raw + rs:
            return va + (off - raw), name
    return None, None


def pattern(code):
    """Instruction hex (XX = wildcard) -> regex over bytes."""
    toks = re.sub(r'\s+', '', code)
    rx = b''
    for i in range(0, len(toks), 2):
        t = toks[i:i + 2]
        rx += b'.' if t.upper() == 'XX' else re.escape(bytes([int(t, 16)]))
    return re.compile(rx, re.S)


def main():
    exe = open(sys.argv[1], 'rb').read()
    base, secs = sections(exe)
    for path in sys.argv[2:]:
        # XV2Patcher's parser accepts a raw '<' inside attribute values
        # (Revamp has comment="... <=-1 ..."); ElementTree does not.
        text = open(path, encoding='utf-8-sig').read()
        text = re.sub(r'="([^"]*)"', lambda m: '="' + m.group(1).replace('&', '&amp;').replace('<', '&lt;') + '"', text)
        root = ET.fromstring(text)
        print(f'== {path}: {root.get("name")}')
        for p in root.iter('Patch'):
            insts = [i.get('code') for i in p.findall('Instruction')]
            rx = pattern(''.join(insts))
            start_rva = int(p.get('search_start', '0'), 16)
            start = rva_to_off(secs, start_rva) or 0
            hits = [m.start() for m in rx.finditer(exe, start)]
            all_hits = [m.start() for m in rx.finditer(exe)]
            name = p.get('name')
            if not hits:
                print(f'  {name:24} NOT FOUND after {start_rva:#x} (anywhere: {len(all_hits)})')
                continue
            rva, sec = off_to_rva(secs, hits[0])
            note = f'{len(all_hits)} matches in exe' if len(all_hits) > 1 else 'unique'
            print(f'  {name:24} at {rva:#x} ({sec}), search_start {start_rva:#x}, {note}')
            w = p.find('Write')
            if w is None:
                continue
            wv = re.sub(r'\s+', '', w.get('value'))
            idx = int(w.get('inst_index', '0'))
            woff = sum(len(re.sub(r'\s+', '', c)) // 2 for c in insts[:idx])
            # jmp rel32 at the start of the written bytes
            m = re.match(r'E9([0-9A-Fa-f]{8})', wv)
            if m:
                rel = struct.unpack('<i', bytes.fromhex(m.group(1)))[0]
                src = rva + woff + 5
                print(f'  {"":24}   writes jmp -> {src + rel:#x}')


if __name__ == '__main__':
    main()

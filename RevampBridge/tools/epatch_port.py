"""Analyse (and later port) Eternity patches between two decrypted DBXV2 images.

Images are flat module dumps (offset == RVA), e.g. from dumpimage.py.

usage: epatch_port.py analyse <old.bin> <new.bin> <patch.xml> [...]
"""
import re
import sys
import xml.etree.ElementTree as ET

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

cs = Cs(CS_ARCH_X86, CS_MODE_64)
cs.detail = True


def load_patches(path):
    text = open(path, encoding='utf-8-sig').read()
    text = re.sub(r'="([^"]*)"', lambda m: '="' + m.group(1).replace('&', '&amp;').replace('<', '&lt;') + '"', text)
    return ET.fromstring(text)


def hexbytes(s):
    return re.sub(r'\s+', '', s)


def pattern(code):
    toks = hexbytes(code)
    rx = b''
    for i in range(0, len(toks), 2):
        t = toks[i:i + 2]
        rx += b'.' if t.upper() == 'XX' else re.escape(bytes([int(t, 16)]))
    return re.compile(rx, re.S)


def find(img, insts, start):
    rx = pattern(''.join(insts))
    return [m.start() for m in rx.finditer(img, start)], [m.start() for m in rx.finditer(img)]


def refs(code, addr):
    """Every absolute target in a code blob: (insn addr, mnemonic, kind, target)."""
    out = []
    for i in cs.disasm(code, addr):
        for op in i.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                out.append((i.address, f'{i.mnemonic} {i.op_str}', 'rip', i.address + i.size + op.mem.disp))
            elif op.type == X86_OP_IMM and i.mnemonic in ('jmp', 'call') or (op.type == X86_OP_IMM and i.mnemonic.startswith('j')):
                out.append((i.address, f'{i.mnemonic} {i.op_str}', 'branch', op.imm))
    return out


TEXT_END = 0x1_000_000  # both images' .text end below this


def signature(img, addr, n):
    """Regex for n instructions from addr, relative operands wildcarded."""
    rx, size = b'', 0
    insns = list(cs.disasm(img[addr:addr + 16 * n], addr, n))
    for i in insns:
        b = bytearray(i.bytes)
        wild = set()
        if i.disp_size and any(op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP for op in i.operands):
            wild |= set(range(i.disp_offset, i.disp_offset + i.disp_size))
        if i.imm_size and (i.mnemonic in ('call', 'jmp') or i.mnemonic.startswith('j')):
            wild |= set(range(i.imm_offset, i.imm_offset + i.imm_size))
        for k, byte in enumerate(b):
            rx += b'.' if k in wild else re.escape(bytes([byte]))
        size += i.size
    return re.compile(rx, re.S), size, insns


def map_site(old, new, addr, lo=0x1000, hi=TEXT_END, before=0):
    """Find addr's counterpart in new: shortest unique forward signature.
    `before` instructions are not supported directly; callers pass an earlier
    boundary and add the offset back."""
    for n in (4, 6, 8, 12, 16, 24, 32):
        rx, size, _ = signature(old, addr, n)
        hits = [m.start() for m in rx.finditer(new, lo, hi)]
        if len(hits) == 1:
            return hits[0], n
        if not hits:
            return None, n
    return None, n


def xrefs(img, target, lo=0x1000, hi=TEXT_END):
    """Code addresses whose rel32 at +k resolves to target (rip-relative)."""
    import array
    out = []
    view = memoryview(img)
    for i in range(lo, hi - 4):
        if int.from_bytes(view[i:i + 4], 'little', signed=True) == target - (i + 4):
            out.append(i)
    return out


def analyse(old, new, paths):
    for path in paths:
        root = load_patches(path)
        print(f'\n===== {root.get("name")}')
        for p in root.iter('Patch'):
            name = p.get('name')
            insts = [i.get('code') for i in p.findall('Instruction')]
            ss = int(p.get('search_start', '0'), 16)
            o_hits, o_all = find(old, insts, ss)
            n_hits, n_all = find(new, insts, 0)
            w = p.find('Write')
            wv = hexbytes(w.get('value')) if w is not None else ''
            idx = int(w.get('inst_index', '0')) if w is not None else 0
            woff = sum(len(hexbytes(c)) // 2 for c in insts[:idx])
            o_at = o_hits[0] if o_hits else None
            print(f'-- {name}: old at {o_at and hex(o_at)} ({len(o_all)} in image)  new: {[hex(x) for x in n_all[:4]]}{"..." if len(n_all) > 4 else ""}')
            if o_at is None or 'X' in wv.upper():
                if 'X' in wv.upper():
                    print('   write has wildcards; bytes kept from the game')
                continue
            wb = bytes.fromhex(wv)
            for a, m, kind, t in refs(wb, o_at + woff):
                where = 'cave' if t >= 0x1B3F000 else 'game'
                print(f'   {a:#x}: {m:40} {kind:6} -> {t:#x} ({where})')


if __name__ == '__main__':
    if sys.argv[1] == 'analyse':
        old = open(sys.argv[2], 'rb').read()
        new = open(sys.argv[3], 'rb').read()
        analyse(old, new, sys.argv[4:])

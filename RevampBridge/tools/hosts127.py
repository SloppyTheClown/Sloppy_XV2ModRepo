"""Compare the functions Revamp's Epatches hook between 1.26.01 and 1.27.

ClaudeTools\\exe\\img127.bin was dumped while XV2Patcher had applied the
1.26.01 ports to 1.27 (JMP_Part1..4B, Edit_Part5 hit their patterns). Those
bytes are put back from the patterns first and the result saved as
img127_clean.bin next to it.

Then, for each hook site (1.26.01 -> 1.27), the .pdata function holding it is
disassembled in both images and compared instruction by instruction with
rip-relative displacements and branch targets masked. Any difference in
registers, stack slots or struct offsets is printed - the cave code reads
those, so they must match (or the cave be rewritten).
usage: hosts127.py <img126.bin> <img127.bin>
"""
import bisect
import os
import re
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

import codematch as cm

# 1.27 address: (bytes XV2Patcher wrote there with the 1.26.01 ports, original bytes)
RESTORE = {
    0x299EBC: ('E9 ?? ?? ?? ?? 90 90', '46 8B 04 B1 45 85 C0'),         # JMP_Part1
    0x29D39D: ('E9 ?? ?? ?? ??', '8B 43 18 85 C0'),                     # JMP_Part2
    0xCA734: ('E9 ?? ?? ?? ?? 90 90', '44 8B 04 99 45 85 C0'),          # JMP_Part3
    0x16C22F: ('E9 ?? ?? ?? ?? 90 90 90', '0F BF 58 40 0F BF 78 3C'),   # JMP_Part4A_Alt2
    0xFD4D1: ('90 90 90 90', '0F BF 40 40'),                            # JMP_Part4B_Alt2 (1st half)
    0xFD4DA: ('E9 ?? ?? ?? ?? 90 90 90 90 90 90 90', 'F3 0F 10 46 0C 44 03 CB 49 8B 4D 10'),  # (2nd half)
    0xFD4F8: ('0F 44 D0', '0F 4E D0'),                                  # Edit_Part5
    # AddRevampCPK (RevampExtra's first patch; ChangeCueID1 after it failed
    # on 1.27 - r12 instead of r15 - and XV2Patcher skipped the rest of the file)
    0xFA8BE8: ('2E 2E 2F 64 61 74 61 2F 72 76 70 2E 63 70 6B 00',
               '64 61 74 61 32 2E 63 70 6B 00 00 00 00 00 00 00'),          # "data2.cpk"
}

# 1.26.01 hook site -> 1.27 hook site
SITES = {
    0x299DFC: 0x299EBC,  # Part1
    0x29D2DD: 0x29D39D,  # Part2 / Part6 host
    0xCA684: 0xCA734,    # Part3
    0x16C16F: 0x16C22F,  # Part4A
    0xFD411: 0xFD4D1,    # Part4B, Part5, CusAura1
    0xFE389: 0xFE449,    # body change
    0xE7990: 0xE7A50,    # CusAura2
    0xC8866: 0xC8866,    # CusAura3
    0x7F2184: 0x8000F4,  # BAQ vox
    0x60721D: 0x60A498,  # title BGM 1
    0x58751B: 0x58806B,  # title BGM 2
    0x83213E: 0x84024E,  # partner colour fix
}

cs = Cs(CS_ARCH_X86, CS_MODE_64)
REL = re.compile(r'rip [+-] 0x[0-9a-f]+|0x[0-9a-f]{4,}')


def restore(img):
    img = bytearray(img)
    for addr, (patched, orig) in RESTORE.items():
        pat = patched.split()
        cur = img[addr:addr + len(pat)]
        ok = all(p == '??' or int(p, 16) == c for p, c in zip(pat, cur))
        if not ok:
            print(f'restore {addr:#x}: bytes are {cur.hex()} - not the 1.26.01 write, left alone')
            continue
        img[addr:addr + len(pat)] = bytes.fromhex(orig)
    return bytes(img)


def func(funcs, addr):
    k = bisect.bisect_right(funcs, (addr, 1 << 40)) - 1
    b, e = funcs[k]
    assert b <= addr < e, hex(addr)
    return b, e


def listing(img, b, e):
    return [(i.address, f'{i.mnemonic} {REL.sub("*", i.op_str)}') for i in cs.disasm(img[b:e], b)]


def main():
    old = open(sys.argv[1], 'rb').read()
    new = restore(open(sys.argv[2], 'rb').read())
    clean = os.path.join(os.path.dirname(sys.argv[2]), 'img127_clean.bin')
    open(clean, 'wb').write(new)
    fo, fn = cm.functions(old), cm.functions(new)
    for a, b in SITES.items():
        ob, oe = func(fo, a)
        nb, ne = func(fn, b)
        lo, ln = listing(old, ob, oe), listing(new, nb, ne)
        diffs = [(x, y) for x, y in zip(lo, ln) if x[1] != y[1]]
        site_off = (a - ob, b - nb)
        print(f'{a:#x} -> {b:#x}: fn {ob:#x}..{oe:#x} -> {nb:#x}..{ne:#x}, site at +{site_off[0]:#x} / +{site_off[1]:#x}, '
              f'{len(lo)} / {len(ln)} insns, {len(diffs)} differ')
        for x, y in diffs[:12]:
            print(f'    {x[0]:#9x} {x[1]:<40} | {y[0]:#9x} {y[1]}')


if __name__ == '__main__':
    main()

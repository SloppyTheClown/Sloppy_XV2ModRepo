"""Locate the 1.26.01 code addresses port_epatches.py uses in the 1.27 image.

For each address: the 1.27 match (codematch votes), and whether the bytes
around it are identical; if not, both disassemblies are printed so register /
stack changes at a hook can be seen.
usage: sites127.py <img126.bin> <img127.bin>
"""
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

import codematch as cm

# 1.26.01 address -> what it is (port_epatches.py)
SITES = {
    0x299DFC: 'Part1 site', 0x299E01: 'Part1 return',
    0x29D2DD: 'Part2 site', 0x29D2E2: 'Part2 return',
    0xCA684: 'Part3 site', 0xCA689: 'Part3 return',
    0x16C16F: 'Part4A site', 0x16C177: 'Part4A return',
    0xFD411: 'Part4B site', 0xFD415: 'CusAura1 site', 0xFD41A: 'Part4B jmp',
    0xFD426: 'Part4B return / Part5', 0xFD438: 'Part5 cmovle',
    0xFE389: 'body change site', 0xFE38E: 'body change return',
    0xE7990: 'CusAura2 site', 0xC8866: 'CusAura3 site',
    0x7F2184: 'BAQ vox site', 0x7F218B: 'BAQ vox return',
    0x60721D: 'title BGM 1', 0x58751B: 'title BGM 2',
    0x83213E: 'partner colour fix',
}

cs = Cs(CS_ARCH_X86, CS_MODE_64)


def dis(img, addr, n=8):
    out = []
    for ins in cs.disasm(img[addr:addr + 64], addr):
        out.append(f'      {ins.address:#9x}  {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}')
        if len(out) == n:
            break
    return '\n'.join(out)


def main():
    old_img, new_img = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
    old, new = cm.Index(sys.argv[1]), cm.Index(sys.argv[2])
    for a, what in SITES.items():
        b, v, r = cm.locate(old, new, a)
        if b is None:
            print(f'{a:#9x} {what}: NOT LOCATED')
            continue
        same = old_img[a - 16:a + 32] == new_img[b - 16:b + 32]
        print(f'{a:#9x} -> {b:#9x} ({b - a:+#x}) votes {v}/{r} {"same bytes" if same else "BYTES DIFFER"}  {what}')
        if not same:
            print('    1.26.01:\n' + dis(old_img, a) + '\n    1.27:\n' + dis(new_img, b))


if __name__ == '__main__':
    main()

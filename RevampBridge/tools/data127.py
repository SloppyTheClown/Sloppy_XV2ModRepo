"""Where the data Revamp's Epatches use sits in 1.26.01 and 1.27.

Prints: every match of the AddRevampCPK / AddCharCodes / AddUnusedPartnerID30
patterns (from the 1.26.01 port), the "CABA_ALL" string, the partner rows'
string pointers, and the last section (code-cave slack) of each image.
usage: data127.py <img126.bin> <img127_clean.bin> <RevampExtra.xml (1.26.01 port)>
"""
import re
import struct
import sys

BASE = 0x140000000


def pattern(instrs):
    rx = b''
    for c in instrs:
        h = re.sub(r'\s+', '', c)
        for k in range(0, len(h), 2):
            rx += b'.' if h[k:k + 2].upper() == 'XX' else re.escape(bytes.fromhex(h[k:k + 2]))
    return re.compile(rx, re.S)


def patch(xml, name):
    m = re.search(r'<Patch name="' + name + r'"[^>]*>(.*?)</Patch>', xml, re.S)
    return re.findall(r'code="([^"]*)"', m.group(1))


def last_section(img):
    pe = struct.unpack_from('<I', img, 0x3C)[0]
    n = struct.unpack_from('<H', img, pe + 6)[0]
    sec = pe + 24 + struct.unpack_from('<H', img, pe + 20)[0]
    out = []
    for i in range(n):
        name = img[sec + 40 * i:sec + 40 * i + 8].rstrip(b'\0').decode()
        vsize, va = struct.unpack_from('<II', img, sec + 40 * i + 8)
        out.append((name, va, vsize))
    return out


def main():
    imgs = [open(p, 'rb').read() for p in sys.argv[1:3]]
    xml = open(sys.argv[3], encoding='utf-8-sig').read()
    for label, img in zip(('1.26.01', '1.27'), imgs):
        print(f'== {label}')
        for name in ('AddRevampCPK', 'AddCharCodes', 'AddUnusedPartnerID30'):
            hits = [m.start() for m in pattern(patch(xml, name)).finditer(img)]
            print(f'  {name}: {[hex(h) for h in hits[:6]]}')
        print('  CABA_ALL:', [hex(m.start()) for m in re.finditer(rb'CABA_ALL\0', img)][:4])
        print('  RSV\\0:', [hex(m.start()) for m in re.finditer(rb'(?<=\0)RSV\0', img)][:6])
        rows = [m.start() for m in pattern(patch(xml, 'AddUnusedPartnerID30')).finditer(img)]
        if rows:
            r = rows[0]
            ptrs = {struct.unpack_from('<Q', img, r + 24 * k)[0] - BASE for k in range(10)}
            print('  partner row string pointers:', sorted(hex(p) for p in ptrs),
                  'string:', [img[p:p + 4] for p in ptrs])
        for name, va, vsize in last_section(img)[-3:]:
            end = va + vsize
            z = end
            while z > va and img[z - 1] == 0:
                z -= 1
            print(f'  section {name}: {va:#x}..{end:#x}, zeros from {z:#x}')


if __name__ == '__main__':
    main()

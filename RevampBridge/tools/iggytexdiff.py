"""Compare a mod's .iggytex against the vanilla one of two game versions.

    python iggytexdiff.py <old.cpk> <new.cpk> <cpk path> <mod.iggytex>

Prints, per texture: DDS size/format in old, new and mod, and whether the
mod's texture equals old vanilla (unchanged) or not (the mod's art).
"""
import struct, sys
from cpkls import listing


def crilayla(inp):
    usize, hdr_off = struct.unpack_from('<ii', inp, 8)
    res = bytearray(usize + 0x100)
    res[0:0x100] = inp[hdr_off + 0x10: hdr_off + 0x110]
    in_off = len(inp) - 0x100 - 1
    out_end = 0x100 + usize - 1
    pool = left = done = 0

    def get(n):
        nonlocal pool, left, in_off
        v = 0
        while n:
            if left == 0:
                pool = inp[in_off]; left = 8; in_off -= 1
            t = min(left, n)
            v = (v << t) | ((pool >> (left - t)) & ((1 << t) - 1))
            left -= t; n -= t
        return v
    while done < usize:
        if get(1):
            back = out_end - done + get(13) + 3
            ln = 3
            for bits in (2, 3, 5, 8):
                v = get(bits); ln += v
                if v != (1 << bits) - 1:
                    break
            else:
                while True:
                    v = get(8); ln += v
                    if v != 255:
                        break
            for _ in range(ln):
                res[out_end - done] = res[back]; back -= 1; done += 1
        else:
            res[out_end - done] = get(8); done += 1
    return bytes(res)


def cpk_file(cpk, name):
    _, files = listing(cpk)
    for n, off, st, ex, _ in files:
        if n == name:
            with open(cpk, 'rb') as f:
                f.seek(off)
                d = f.read(st)
            return d if st == ex else crilayla(d)
    raise KeyError(name)


def textures(b):
    assert b[:8] == b'oggytex\0', b[:8]
    n = struct.unpack_from('<I', b, 8)[0]
    out = []
    for i in range(n):
        idx, size, off, _ = struct.unpack_from('<IIII', b, 16 + 16 * i)
        out.append((idx, b[off & 0x7FFFFFFF:(off & 0x7FFFFFFF) + (size & 0x7FFFFFFF)]))
    return out


def dds(t):
    if t[:4] != b'DDS ':
        return 'not dds %r' % t[:4]
    h, w = struct.unpack_from('<II', t, 12)
    four = t[84:88]
    return '%dx%d %s' % (w, h, four.decode('ascii', 'replace') if four.strip(b'\0') else 'rgb')


if __name__ == '__main__':
    old = textures(cpk_file(sys.argv[1], sys.argv[3]))
    new = textures(cpk_file(sys.argv[2], sys.argv[3]))
    mod = textures(open(sys.argv[4], 'rb').read())
    print('counts old %d new %d mod %d' % (len(old), len(new), len(mod)))
    for i in range(max(len(old), len(new), len(mod))):
        o = old[i] if i < len(old) else None
        n = new[i] if i < len(new) else None
        m = mod[i] if i < len(mod) else None
        tag = ''
        if m and o:
            tag = 'mod==old' if m[1] == o[1] else 'MOD CHANGED'
        if o and n:
            tag += '  old==new' if o[1] == n[1] else '  NEW CHANGED'
        print('%3d  old %-22s new %-22s mod %-22s %s' % (
            i, o and ('%d:' % o[0] + dds(o[1])), n and ('%d:' % n[0] + dds(n[1])), m and ('%d:' % m[0] + dds(m[1])), tag))

"""DDS (DXT1/DXT5/uncompressed BGRA) -> PNG, stdlib only. Also a contact
sheet of several images side by side.

    python ddspng.py <file.dds> <out.png>

from Python: decode(dds bytes) -> (w, h, rgba bytes); png(path, w, h, rgba);
sheet(path, [(w, h, rgba), ...]) puts images in a row on a checkerboard.
"""
import struct, sys, zlib


def _c565(v):
    return ((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31


def _colors(b, o, dxt1):
    c0, c1, bits = struct.unpack_from('<HHI', b, o)
    p0, p1 = _c565(c0), _c565(c1)
    if c0 > c1 or not dxt1:
        p2 = tuple((2 * a + c) // 3 for a, c in zip(p0, p1)); p3 = tuple((a + 2 * c) // 3 for a, c in zip(p0, p1)); a3 = 255
    else:
        p2 = tuple((a + c) // 2 for a, c in zip(p0, p1)); p3 = (0, 0, 0); a3 = 0
    pal = [p0 + (255,), p1 + (255,), p2 + (255,), p3 + (a3,)]
    return [pal[(bits >> (2 * i)) & 3] for i in range(16)]


def _alpha5(b, o):
    a0, a1 = b[o], b[o + 1]
    bits = int.from_bytes(b[o + 2:o + 8], 'little')
    if a0 > a1:
        pal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        pal = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
    return [pal[(bits >> (3 * i)) & 7] for i in range(16)]


def decode(d):
    assert d[:4] == b'DDS ', d[:4]
    h, w = struct.unpack_from('<II', d, 12)
    four = d[84:88]
    px = bytearray(w * h * 4)
    data = d[128:]
    if four in (b'DXT1', b'DXT5'):
        bs = 8 if four == b'DXT1' else 16
        o = 0
        for by in range(0, h, 4):
            for bx in range(0, w, 4):
                if four == b'DXT1':
                    cols = _colors(data, o, True); al = None
                else:
                    al = _alpha5(data, o); cols = _colors(data, o + 8, False)
                for i in range(16):
                    x, y = bx + i % 4, by + i // 4
                    if x < w and y < h:
                        r, g, b_, a = cols[i]
                        px[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes((r, g, b_, al[i] if al else a))
                o += bs
    else:  # 32-bit BGRA
        for i in range(w * h):
            b_, g, r, a = data[4 * i:4 * i + 4]
            px[4 * i:4 * i + 4] = bytes((r, g, b_, a))
    return w, h, bytes(px)


def png(path, w, h, rgba):
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    chunk = lambda t, c: struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def sheet(path, imgs, gap=8):
    """images in a row, composited (premultiplied, as Iggy draws) on a checkerboard"""
    W = sum(w for w, _, _ in imgs) + gap * (len(imgs) + 1)
    H = max(h for _, h, _ in imgs) + 2 * gap
    out = bytearray()
    for y in range(H):
        for x in range(W):
            c = 96 if ((x // 8) + (y // 8)) % 2 else 64
            out += bytes((c, c, c, 255))
    x0 = gap
    for w, h, px in imgs:
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[(y * w + x) * 4:(y * w + x) * 4 + 4]
                o = ((y + gap) * W + x0 + x) * 4
                bg = out[o]
                out[o:o + 3] = bytes(min(255, v + bg * (255 - a) // 255) for v in (r, g, b))
        x0 += w + gap
    png(path, W, H, bytes(out))


if __name__ == '__main__':
    w, h, px = decode(open(sys.argv[1], 'rb').read())
    png(sys.argv[2], w, h, px)

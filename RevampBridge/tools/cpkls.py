"""List a CPK's files: name, offset, stored size, extract size, first bytes.

    python cpkls.py <file.cpk> [filter]
"""
import struct, sys


def unmask(b):
    if b[:4] == b'@UTF':
        return b
    out, m = bytearray(len(b)), 0x655F
    for i, c in enumerate(b):
        out[i] = c ^ (m & 0xFF)
        m = (m * 0x4115) & 0xFFFFFFFF
    return bytes(out)


def packet(f, off):
    f.seek(off)
    head = f.read(16)
    size = struct.unpack_from('<Q', head, 8)[0]
    return unmask(f.read(size))


def utf(b):
    assert b[:4] == b'@UTF', b[:4]
    tsize, _, rows_off, str_off, data_off, name_off, ncols, row_len, nrows = struct.unpack_from('>IHHIIIHHI', b, 4)
    base = 8
    strs = b[base + str_off: base + data_off]

    def s(o):
        return strs[o:strs.index(b'\0', o)].decode('utf-8', 'replace')
    cols, p = [], base + 24
    for _ in range(ncols):
        flags = b[p]; nm = struct.unpack_from('>I', b, p + 1)[0]; p += 5
        storage, typ = flags & 0xF0, flags & 0x0F
        const = None
        if storage == 0x30:
            const, p = val(b, p, typ, s, base + data_off)
        cols.append((s(nm), storage, typ, const))
    rows = []
    for r in range(nrows):
        p = base + rows_off + r * row_len
        row = {}
        for nm, storage, typ, const in cols:
            if storage == 0x50:
                row[nm], p = val(b, p, typ, s, base + data_off)
            else:
                row[nm] = const
        rows.append(row)
    return rows


SZ = {0: 'B', 1: 'b', 2: 'H', 3: 'h', 4: 'I', 5: 'i', 6: 'Q', 7: 'q', 8: 'f'}


def val(b, p, typ, s, data):
    if typ in SZ:
        fmt = '>' + SZ[typ]
        return struct.unpack_from(fmt, b, p)[0], p + struct.calcsize(fmt)
    if typ == 0xA:
        return s(struct.unpack_from('>I', b, p)[0]), p + 4
    if typ == 0xB:
        o, n = struct.unpack_from('>II', b, p)
        return (o, n), p + 8
    raise ValueError(typ)


def listing(path):
    with open(path, 'rb') as f:
        hdr = utf(packet(f, 0))[0]
        toc = utf(packet(f, hdr['TocOffset']))
        content = hdr['ContentOffset']
        base = min(content, hdr['TocOffset'])
        out = []
        for r in toc:
            name = (r['DirName'] + '/' if r['DirName'] else '') + r['FileName']
            off = r['FileOffset'] + base
            f.seek(off)
            out.append((name, off, r['FileSize'], r['ExtractSize'], f.read(16)))
        return hdr, out


if __name__ == '__main__':
    hdr, files = listing(sys.argv[1])
    flt = sys.argv[2] if len(sys.argv) > 2 else ''
    print({k: v for k, v in hdr.items() if 'Offset' in k or k in ('Align', 'Files')})
    for name, off, st, ex, head in files:
        if flt in name:
            print(f'{name:60} {off:12} {st:10} {ex:10} {head[:16].hex()}')

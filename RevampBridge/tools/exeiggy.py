"""Extract the Iggy movies (.iggy) embedded in a program (e.g. xv2ins.exe's
default CHARASELE/STAGESELE/... built for XV2Patcher).

    python exeiggy.py <exe> <out dir> [compare dir ...]

An Iggy file starts with 0xED0A6749; its size is the end of the last subfile
in the header's table. Prints the XV2Patcher AS3 tag of each and which .iggy
in the compare dirs it equals.
"""
import os, re, sys

MAGIC = b'\x49\x67\x0a\xed'

if __name__ == '__main__':
    exe = open(sys.argv[1], 'rb').read()
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    cmp = {}
    for d in sys.argv[3:]:
        for f in os.listdir(d):
            if f.lower().endswith('.iggy'):
                cmp[os.path.join(d, f)] = open(os.path.join(d, f), 'rb').read()
    starts = [m.start() for m in re.finditer(re.escape(MAGIC), exe)]
    for n, s in enumerate(starts):
        end = starts[n + 1] if n + 1 < len(starts) else len(exe)
        blob = exe[s:end]
        tag = re.search(rb'XV2_PATCHER_AS3_V\d+', blob)
        tag = tag.group().decode() if tag else '-'
        # header: u32 subfile count at 0x1C, then {id, size, size, offset} each
        import struct
        nsub = struct.unpack_from('<I', blob, 0x1C)[0]
        size = 0
        if 0 < nsub < 16:
            for i in range(nsub):
                _, sz, _, off = struct.unpack_from('<IIII', blob, 0x20 + 16 * i)
                size = max(size, off + sz)
        size = size if 0 < size <= len(blob) else len(blob)
        blob = blob[:size]
        same = [p for p, d in cmp.items() if d == blob]
        name = os.path.join(out, 'embedded_%d_%s.iggy' % (n, tag))
        open(name, 'wb').write(blob)
        print('0x%08X  %8d  %-22s %s' % (s, size, tag, ', '.join(same) or '(no equal file)'))

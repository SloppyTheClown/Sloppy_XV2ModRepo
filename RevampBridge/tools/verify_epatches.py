"""Apply Eternity patch files to a decrypted image the way XV2Patcher does,
and check the result.

For each patch: first pattern match at or after search_start (and how many
matches exist from there), the write applied (XX keeps the byte). Then every
written byte range is disassembled and every branch/call/rip reference that
leaves it is reported with where it lands - a code target must be the start
of an instruction of the function it lands in (per .pdata) or a cave.

usage: verify_epatches.py <image.bin> <patch.xml> [...]   (files applied in order)
"""
import re
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from codematch import functions  # noqa: E402

cs = Cs(CS_ARCH_X86, CS_MODE_64)
cs.detail = True


def load(path):
    text = open(path, encoding='utf-8-sig').read()
    text = re.sub(r'<!--.*?-->', '', text, flags=re.S)
    for m in re.finditer(r'<Patch\s[^>]*name="([^"]+)"[^>]*search_start="([^"]+)"[^>]*>(.*?)</Patch>', text, re.S):
        instrs = re.findall(r'<Instruction\s+code="([^"]*)"', m.group(3))
        w = re.search(r'<Write\b([^>]*)value="([^"]*)"', m.group(3))
        idx = re.search(r'inst_index="(\d+)"', w.group(1))
        yield m.group(1), int(m.group(2), 16), instrs, w.group(2), int(idx.group(1)) if idx else 0


def rx_of(hexstr):
    h = re.sub(r'\s+', '', hexstr)
    return b''.join(b'.' if h[i:i + 2].upper() == 'XX' else re.escape(bytes([int(h[i:i + 2], 16)])) for i in range(0, len(h), 2))


def main():
    img = bytearray(open(sys.argv[1], 'rb').read())
    fns = functions(bytes(img))
    starts = {}

    def insn_starts(fn):
        if fn not in starts:
            b, e = fn
            starts[fn] = {i.address for i in cs.disasm(bytes(img[b:e]), b)}
        return starts[fn]

    written = []
    problems = 0
    for path in sys.argv[2:]:
        print(f'== {path}')
        for name, ss, instrs, write, idx in load(path):
            rx = re.compile(b''.join(rx_of(c) for c in instrs), re.S)
            hits = [m.start() for m in rx.finditer(img, ss)]
            if not hits:
                print(f'  !! {name}: pattern NOT FOUND after {ss:#x}')
                problems += 1
                continue
            at = hits[0]
            off = sum(len(re.sub(r'\s+', '', c)) // 2 for c in instrs[:idx])
            w = re.sub(r'\s+', '', write)
            wb = [None if w[i:i + 2].upper() == 'XX' else int(w[i:i + 2], 16) for i in range(0, len(w), 2)]
            for k, b in enumerate(wb):
                if b is not None:
                    img[at + off + k] = b
            written.append((name, at + off, len(wb)))
            flag = '' if at == ss or len(hits) == 1 or at - ss < 0x100 else '   <- first of several matches, check'
            print(f'  {name:24} match {at:#x} ({len(hits)} from start) write {at + off:#x}+{len(wb)}{flag}')

    caves = [(a, a + n) for name, a, n in written if a >= 0x1A1E248]
    print('== references leaving written code')
    for name, a, n in written:
        if a >= 0xF4A05C + 0x1000 and a < 0x1A1E248:
            continue  # data writes
        for ins in cs.disasm(bytes(img[a:a + n]), a):
            if ins.mnemonic == 'int3':
                continue
            for op in ins.operands:
                t = None
                if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                    t, kind = ins.address + ins.size + op.mem.disp, 'data'
                elif op.type == X86_OP_IMM and (ins.mnemonic in ('jmp', 'call') or ins.mnemonic.startswith('j')):
                    t, kind = op.imm, 'code'
                if t is None or a <= t < a + n:
                    continue
                if kind == 'code' and any(c0 <= t < c1 for c0, c1 in caves):
                    where = 'cave'
                elif kind == 'code' and any(w0 <= t < w0 + wn for _, w0, wn in written):
                    where = 'patched code (' + next(nm for nm, w0, wn in written if w0 <= t < w0 + wn) + ')'
                elif kind == 'code':
                    fn = next(((b, e) for b, e in fns if b <= t < e), None)
                    ok = fn and t in insn_starts(fn)
                    where = f'fn {fn[0]:#x} ' + ('ok' if ok else 'NOT AN INSTRUCTION START') if fn else 'NO FUNCTION'
                    if not ok:
                        problems += 1
                else:
                    where = 'data'
                print(f'  {name:24} {ins.address:#x} {ins.mnemonic} {ins.op_str:28} -> {t:#x} {where}')
    print(f'problems: {problems}')
    open(sys.argv[1] + '.patched', 'wb').write(img)


if __name__ == '__main__':
    main()

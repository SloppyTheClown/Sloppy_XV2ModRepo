"""Print old/new disassembly around a site pair.  usage: sidebyside.py old.bin new.bin oldaddr newaddr [before after]"""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from codematch import functions

cs = Cs(CS_ARCH_X86, CS_MODE_64)


def window(img, fns, addr, before, after):
    f = next(((b, e) for b, e in fns if b <= addr < e), None)
    start = f[0] if f else addr - 0x80
    ins = list(cs.disasm(img[start:(f[1] if f else addr + 0x100)], start))
    k = next(i for i, x in enumerate(ins) if x.address >= addr)
    return ins[max(0, k - before):k + after], f


def main():
    old, new = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
    oa, na = int(sys.argv[3], 16), int(sys.argv[4], 16)
    b = int(sys.argv[5]) if len(sys.argv) > 5 else 10
    a = int(sys.argv[6]) if len(sys.argv) > 6 else 14
    ow, of = window(old, functions(old), oa, b, a)
    nw, nf = window(new, functions(new), na, b, a)
    print(f'old fn {of and hex(of[0])}  new fn {nf and hex(nf[0])}')
    for i in range(max(len(ow), len(nw))):
        l = r = ''
        if i < len(ow):
            x = ow[i]; l = f"{'>' if x.address == oa else ' '}{x.address:7x} {x.bytes.hex():22.22} {x.mnemonic} {x.op_str}"
        if i < len(nw):
            x = nw[i]; r = f"{'>' if x.address == na else ' '}{x.address:7x} {x.bytes.hex():22.22} {x.mnemonic} {x.op_str}"
        print(f'{l:78.78} | {r}')


if __name__ == '__main__':
    main()

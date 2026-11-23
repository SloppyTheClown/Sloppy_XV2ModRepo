"""Map a data address from the old image to the new one via the code that
references it: each rip-relative xref site is located in the new image and
its operand decoded there.  usage: globalmap.py old.bin new.bin addr..."""
import collections
import re
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP
import codematch as cm

cs = Cs(CS_ARCH_X86, CS_MODE_64)
cs.detail = True


def xref_insns(img, idx, target):
    """Instruction addresses (from the index) whose rip operand hits target."""
    out = []
    data = bytes(img)
    for m in re.finditer(re.escape(b''), b''):
        pass
    for k, a in enumerate(idx.addrs):
        if '[G]' not in idx.toks[k]:
            continue
        i = next(cs.disasm(data[a:a + 16], a, 1))
        for op in i.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP and a + i.size + op.mem.disp == target:
                out.append(a)
    return out


def rip_target(img, a):
    i = next(cs.disasm(img[a:a + 16], a, 1))
    for op in i.operands:
        if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
            return a + i.size + op.mem.disp
    return None


def main():
    oimg, nimg = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
    old, new = cm.Index(sys.argv[1]), cm.Index(sys.argv[2])
    for t in sys.argv[3:]:
        t = int(t, 16)
        xs = xref_insns(oimg, old, t)
        votes = collections.Counter()
        for a in xs:
            na, v, r = cm.locate(old, new, a)
            if na and v > r:
                nt = rip_target(nimg, na)
                if nt:
                    votes[nt] += 1
        print(f'{t:#x}: {len(xs)} xrefs -> {[(hex(k), v) for k, v in votes.most_common(3)]}')


if __name__ == '__main__':
    main()

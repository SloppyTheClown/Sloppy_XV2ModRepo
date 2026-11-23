"""Write the disassembly of the function containing each address.  usage: dumpfn.py img.bin outdir addr..."""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from codematch import functions
cs = Cs(CS_ARCH_X86, CS_MODE_64)
img = open(sys.argv[1], 'rb').read()
fns = functions(img)
for a in sys.argv[3:]:
    a = int(a, 16)
    b, e = next((b, e) for b, e in fns if b <= a < e)
    with open(f'{sys.argv[2]}/{b:x}.asm', 'w') as f:
        for i in cs.disasm(img[b:e], b):
            f.write(f"{'>' if i.address == a else ' '}{i.address:8x}  {i.mnemonic} {i.op_str}\n")
    print(f'{a:#x} in {b:#x}-{e:#x}')

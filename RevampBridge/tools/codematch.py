"""Fuzzy code matching between two decrypted DBXV2 images.

Every function (from .pdata) is disassembled and each instruction normalised:
registers masked, relative targets dropped, immediates and struct offsets
kept. A site in the old image is located in the new one by voting with 4-grams
of normalised instructions around it.

Disassembly is cached next to the image as <image>.norm.pickle.
"""
import collections
import os
import pickle
import re
import struct

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

_cs = Cs(CS_ARCH_X86, CS_MODE_64)
_cs.skipdata = True

REG = re.compile(r'\b(?:[re]?[abcd]x|[abcd][lh]|[re]?[sd]il?|[re]?[sb]pl?|r\d+[dwb]?|xmm\d+|ymm\d+)\b')
HEX = re.compile(r'0x[0-9a-f]+')


def functions(img):
    pe = struct.unpack_from('<I', img, 0x3C)[0]
    opt = pe + 24
    rva, size = struct.unpack_from('<II', img, opt + 112 + 3 * 8)  # exception directory
    out = []
    for i in range(0, size, 12):
        b, e, u = struct.unpack_from('<III', img, rva + i)
        if b and e > b:
            out.append((b, e))
    return sorted(set(out))


def norm(insn):
    if insn.mnemonic in ('call', 'jmp') or insn.mnemonic.startswith('j'):
        return insn.mnemonic
    ops = insn.op_str
    if 'rip' in ops:
        ops = re.sub(r'\[rip [+-] 0x[0-9a-f]+\]', '[G]', ops)
    return insn.mnemonic + ' ' + REG.sub('R', ops)


def disasm(path):
    cache = path + '.norm.pickle'
    if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(path):
        return pickle.load(open(cache, 'rb'))
    img = open(path, 'rb').read()
    addrs, toks = [], []
    for b, e in functions(img):
        for i in _cs.disasm(img[b:e], b):
            addrs.append(i.address)
            toks.append(norm(i))
    data = (addrs, toks)
    pickle.dump(data, open(cache, 'wb'))
    return data


class Index:
    def __init__(self, path, n=4):
        self.addrs, self.toks = disasm(path)
        self.n = n
        self.pos = {a: k for k, a in enumerate(self.addrs)}
        self.grams = collections.defaultdict(list)
        for k in range(len(self.toks) - n):
            self.grams[tuple(self.toks[k:k + n])].append(k)

    def at(self, addr):
        return self.pos.get(addr)


def locate(old, new, addr, before=16, after=16):
    """Best new address for old addr: every 4-gram near the site votes for
    the offset it implies. Returns (addr, votes, runner-up votes)."""
    k = old.at(addr)
    if k is None:
        return None, 0, 0
    votes = collections.Counter()
    for j in range(max(0, k - before), min(len(old.toks) - old.n, k + after)):
        g = tuple(old.toks[j:j + old.n])
        hits = new.grams.get(g, ())
        if 0 < len(hits) <= 8:
            for h in hits:
                votes[h + (k - j)] += 1
    if not votes:
        return None, 0, 0
    (best, v), *rest = votes.most_common(2) + [(None, 0)]
    runner = rest[0][1] if rest else 0
    return new.addrs[best] if 0 <= best < len(new.addrs) else None, v, runner


def show(img_path, idx, addr, before=8, after=12):
    k = idx.at(addr)
    if k is None:
        return f'  (no instruction boundary at {addr:#x})'
    lines = []
    for j in range(max(0, k - before), min(len(idx.toks), k + after)):
        mark = '>>' if j == k else '  '
        lines.append(f'{mark} {idx.addrs[j]:#9x}  {idx.toks[j]}')
    return '\n'.join(lines)

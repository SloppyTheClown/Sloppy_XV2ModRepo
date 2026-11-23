"""Port Revamp 5.1.1's exe patches (Eternity Epatches) from 1.25.02 to the
game builds the bridge supports (1.26.01, 1.27).

Reads RevampDynamicPartsets.xml and RevampExtra.xml out of the Revamp
.installinfo (read only) and writes one version of both per build to
<outdir>\\<build>\\ (PostInstall picks the folder by the game exe).

Every change below was worked out by comparing decrypted images of the game
versions (see README "How the exe patches were ported"):

  * code caves: Revamp puts its code in the zero slack after the Steam stub's
    .bind section - 0x1B77448.. in 1.25.02, 0x1A1E248.. in 1.26.01,
    0x1A32248.. in 1.27. Moved as one block so jumps between caves stay valid.
  * hook sites, return addresses and the two game data addresses Revamp
    references are mapped to their counterparts (BUILDS).
  * where the compiler changed registers or stack slots around a hook, the
    cave instructions that read them are rewritten to match (listed per patch;
    the comments name the 1.26.01 addresses). 1.27 has the same code at every
    site, only moved, except title BGM 1 (r15 -> r12 in the stores).

usage: port_epatches.py <Revamp .installinfo> <outdir>
"""
import os
import re
import struct
import sys
import zipfile

OLD_CAVE, CAVE_END_OLD = 0x1B77500, 0x1B78000   # Revamp's cave block in 1.25.02

# Per game build: where everything Revamp's patches touch sits. 'addr' maps
# 1.25.02 -> that build for everything a cave jumps to or reads outside the
# cave block; 'site' holds hook sites and pattern search starts.
BUILDS = {
    '1.26.01': {
        'cave': 0x1A1E500,
        'addr': {
            0x1681C08: 0x1554448,  # game/save singleton pointer (24 of 24 xrefs agree)
            0x11BD518: 0x1101CE0,  # "CABA_ALL" (same string)
            0x2B1505: 0x299E01,    # Part1 return
            0x2B4A22: 0x29D2E2,    # Part2 return
            0xC5BB0: 0xCA689,      # Part3 return
            0x1703DC: 0x16C177,    # Part4A return
            0xFCA84: 0xFD426,      # Part4B return
            0xFD7AC: 0xFE38E,      # body-change (New_Code) return
            0x853EE1: 0x7F218B,    # BAQ vox return
        },
        'site': {
            'part1': 0x299DFC, 'part2': 0x29D2DD, 'part3': 0xCA684, 'part4a': 0x16C16F,
            'part4b': 0xFD411, 'part4b_jmp': 0xFD41A, 'part5': 0xFD426,
            'cpk_search': 0xF9A000, 'bgm1': 0x60721D, 'bgm2': 0x58751B, 'color_search': 0x832000,
            'codes_search': 0x1098000, 'baq_search': 0x7F2000, 'baq': 0x7F2184,
            'body_search': 0xFE000, 'body': 0xFE389, 'aura1_search': 0xFD000, 'aura1': 0xFD415,
            'aura2_search': 0xE7000, 'aura2': 0xE7990, 'aura3_search': 0xC8000, 'aura3': 0xC8866,
            'partner_search': 0x14A7000,
        },
        'bgm1_reg': '7C',            # ModRM byte of the stores around title BGM 1 (r15)
        'partner_low': 0x85D0,       # low 16 bits of the 20 new partner code slots
    },
    # Located with tools\sites127.py / hosts127.py / data127.py (codematch +
    # instruction diff of every host function, rip/targets masked).
    '1.27': {
        'cave': 0x1A32500,           # .bind ends at 0x1A32248, zeros to 0x1A33000
        'addr': {
            0x1681C08: 0x15681A8,  # singleton pointer (28 of 30 xrefs agree)
            0x11BD518: 0x1111E80,  # "CABA_ALL"
            0x2B1505: 0x299EC1,    # Part1 return
            0x2B4A22: 0x29D3A2,    # Part2 return
            0xC5BB0: 0xCA739,      # Part3 return
            0x1703DC: 0x16C237,    # Part4A return
            0xFCA84: 0xFD4E6,      # Part4B return
            0xFD7AC: 0xFE44E,      # body-change return
            0x853EE1: 0x8000FB,    # BAQ vox return
        },
        'site': {
            'part1': 0x299EBC, 'part2': 0x29D39D, 'part3': 0xCA734, 'part4a': 0x16C22F,
            'part4b': 0xFD4D1, 'part4b_jmp': 0xFD4DA, 'part5': 0xFD4E6,
            'cpk_search': 0xFA8000, 'bgm1': 0x60A498, 'bgm2': 0x58806B, 'color_search': 0x840000,
            'codes_search': 0x10A6000, 'baq_search': 0x800000, 'baq': 0x8000F4,
            'body_search': 0xFE000, 'body': 0xFE449, 'aura1_search': 0xFD000, 'aura1': 0xFD4D5,
            'aura2_search': 0xE7000, 'aura2': 0xE7A50, 'aura3_search': 0xC8000, 'aura3': 0xC8866,
            'partner_search': 0x14BA000,
        },
        'bgm1_reg': '64',            # r12
        'partner_low': 0x6770,       # zeros 0x10A6770.., RSV string 0x10A6344 (same 64 KB window)
    },
}

T = None        # the build being written (main sets it)
ADDR = None     # T['addr']


def cave(old):
    return old - OLD_CAVE + T['cave']


# ---------------------------------------------------------------- helpers

def hexs(s):
    return re.sub(r'\s+', '', s).upper()


def spaced(b):
    return ' '.join(f'{x:02X}' for x in b)


def rel32(src_next, dst):
    return struct.pack('<i', dst - src_next)


def asm(items, org):
    """Tiny assembler: items are bytes, ('label', name), ('jcc8', opcode, name),
    ('jmp32', target_addr), ('call32', target_addr), ('rip', prefix_bytes,
    target_addr, suffix_bytes). Two passes for label offsets."""
    labels = {}
    for pass_ in range(2):
        out = bytearray()
        for it in items:
            here = org + len(out)
            if isinstance(it, (bytes, bytearray)):
                out += it
            elif it[0] == 'label':
                labels[it[1]] = here
            elif it[0] == 'jcc8':
                d = labels.get(it[2], here + 2) - (here + 2)
                if pass_ and not -128 <= d <= 127:
                    raise ValueError(f'short jump out of range to {it[2]}')
                out += bytes([it[1], d & 0xFF])
            elif it[0] == 'jmp32':
                out += b'\xE9' + rel32(here + 5, it[1])
            elif it[0] == 'call32':
                out += b'\xE8' + rel32(here + 5, it[1])
            elif it[0] == 'rip':
                pre, tgt, suf = it[1], it[2], it[3]
                n = len(pre) + 4 + len(suf)
                out += pre + rel32(here + n, tgt) + suf
    return bytes(out)


def relocate_cave(code, old_org, edits=()):
    """Move a 1.25.02 cave to its slot in build T: apply byte edits (same
    length), then re-point every rel32 that leaves the cave block."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_REG_RIP
    code = bytearray(code)
    for old_bytes, new_bytes in edits:
        o, n = bytes.fromhex(hexs(old_bytes)), bytes.fromhex(hexs(new_bytes))
        assert len(o) == len(n), (old_bytes, new_bytes)
        i = code.find(o)
        assert i >= 0 and code.find(o, i + 1) < 0, f'edit not unique in cave at {old_org:#x}: {old_bytes}'
        code[i:i + len(o)] = n
    cs = Cs(CS_ARCH_X86, CS_MODE_64)
    cs.detail = True
    new_org = cave(old_org)
    for ins in cs.disasm(bytes(code), old_org):
        if ins.mnemonic == 'int3':
            continue
        off = ins.address - old_org
        for op in ins.operands:
            tgt, field = None, None
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                tgt, field = ins.address + ins.size + op.mem.disp, ins.disp_offset
            elif op.type == X86_OP_IMM and (ins.mnemonic in ('jmp', 'call') or ins.mnemonic.startswith('j')) and ins.imm_size == 4:
                tgt, field = op.imm, ins.imm_offset
            if tgt is None:
                continue
            if OLD_CAVE <= tgt < CAVE_END_OLD:
                continue  # inside the block: shift-invariant
            if tgt not in ADDR:
                raise ValueError(f'unmapped target {tgt:#x} in cave {old_org:#x}')
            new_next = new_org + off + ins.size
            struct.pack_into('<i', code, off + field, ADDR[tgt] - new_next)
    return bytes(code)


# ---------------------------------------------------------------- patches

def zero_pattern():
    return ['00000000 00000000 00000000 00000000']


def jmp_write(site, target, total):
    """jmp rel32 at site to target, NOP-padded to total bytes."""
    return spaced(b'\xE9' + rel32(site + 5, target) + b'\x90' * (total - 5))


def call_write(site, target, total):
    return spaced(b'\xE8' + rel32(site + 5, target) + b'\x90' * (total - 5))


def port(orig):
    """orig: {patch name: (instruction codes, write value)} from Revamp's XML.
    Returns {name: (search_start, [instruction codes], write, inst_index)}."""
    P = {}
    S, R = T['site'], T['bgm1_reg']

    def old_write(name):
        return bytes.fromhex(hexs(orig[name][1]))

    # --- Dynamic Transformation Engine ----------------------------------
    # Part1: 0x2B1500 -> 0x299DFC. Loop index r12 -> r14; the character
    # pointer the cave reads moved from [rsp+0x68] to [rsp+0x70] (+0x18 for
    # the cave's three pushes: 0x80 -> 0x88).
    P['JMP_Part1'] = (S['part1'],
                      ['46 8B 04 B1', '45 85 C0', '74 71', '45 8B F8', '4C 03 FA', '74 69'],
                      jmp_write(S['part1'], cave(0x1B77530), 7), None)
    P['NewCode_Part1'] = (cave(0x1B77530), zero_pattern(), spaced(relocate_cave(old_write('NewCode_Part1'), 0x1B77530, [
        ('4C 8B 8C 24 80 00 00 00', '4C 8B 8C 24 88 00 00 00'),  # mov r9,[rsp+0x88]
        ('41 81 FC 10 27 00 00', '41 81 FE 10 27 00 00'),        # cmp r14d,10000
        ('45 89 60 04', '45 89 70 04'),                          # mov [r8+4],r14d
        ('46 8B 04 A1', '46 8B 04 B1'),                          # mov r8d,[rcx+r14*4]
    ])), None)

    # Part2: host function unchanged (0x2B49D0 -> 0x29D290). Its caller's
    # index is now r14, which the host saves at [rsp+0x48] (was r12 at
    # [rsp+0x50]); the caller's character pointer is 8 bytes further up.
    P['JMP_Part2'] = (S['part2'], ['8B 43 18', '85 C0', '74 05', '48 03 C3', 'EB 02'],
                      jmp_write(S['part2'], cave(0x1B775C0), 5), None)
    P['NewCode_Part2'] = (cave(0x1B775C0), zero_pattern(), spaced(relocate_cave(old_write('NewCode_Part2'), 0x1B775C0, [
        ('81 7C 24 50 10 27 00 00', '81 7C 24 48 10 27 00 00'),
        ('81 7C 24 50 FE FF 00 00', '81 7C 24 48 FE FF 00 00'),
        ('81 7C 24 50 40 9C 00 00', '81 7C 24 48 40 9C 00 00'),
        ('4C 8B 8C 24 E8 00 00 00', '4C 8B 8C 24 F0 00 00 00'),
    ])), None)

    # Part3: 0xC5BAB -> 0xCA684, index rdi -> rbx.
    P['JMP_Part3'] = (S['part3'], ['44 8B 04 99', '45 85 C0', '0F84 4D010000', '41 8B C0', '48 03 C2', '0F84 41010000'],
                      jmp_write(S['part3'], cave(0x1B776E0), 7), None)
    P['NewCode_Part3'] = (cave(0x1B776E0), zero_pattern(), spaced(relocate_cave(old_write('NewCode_Part3'), 0x1B776E0, [
        ('48 81 FF 10 27 00 00', '48 81 FB 10 27 00 00'),  # cmp rbx,10000
        ('44 8B 04 B9', '44 8B 04 99'),                    # mov r8d,[rcx+rbx*4]
    ])), None)

    # Part4A: 0x1703D3 -> 0x16C16F. Now movsx ebx,[rax+40] / movsx edi,[rax+3C]
    # (was edi / r15d), 8 bytes instead of 9.
    p4a = cave(0x1B77700)
    part4a = asm([
        bytes.fromhex('0FBF5840'),        # movsx ebx, word [rax+0x40]
        bytes.fromhex('83FBFF'),          # cmp ebx, -1
        ('jcc8', 0x74, 'keep'),
        bytes.fromhex('0FB75840'),        # movzx ebx, word [rax+0x40]
        ('label', 'keep'),
        bytes.fromhex('0FBF783C'),        # movsx edi, word [rax+0x3c]
        ('jmp32', ADDR[0x1703DC]),
    ], p4a)
    # Part4B (same cave, +0x18): 0xFCA74 -> 0xFD411. 1.26 interleaves a movss
    # and uses r13 for what was r14, so the jump covers 12 bytes and the
    # cave replays them.
    p4b = p4a + 0x18
    part4b = asm([
        bytes.fromhex('488BC8'),          # mov rcx, rax
        bytes.fromhex('0FBF4140'),        # movsx eax, word [rcx+0x40]
        bytes.fromhex('83F8FF'),          # cmp eax, -1
        ('jcc8', 0x74, 'keep'),
        bytes.fromhex('0FB74140'),        # movzx eax, word [rcx+0x40]
        ('label', 'keep'),
        bytes.fromhex('F30F10460C'),      # movss xmm0, [rsi+0xc]
        bytes.fromhex('4403CB'),          # add r9d, ebx
        bytes.fromhex('498B4D10'),        # mov rcx, [r13+0x10]
        ('jmp32', ADDR[0xFCA84]),
    ], p4b)
    assert len(part4a) <= 0x18 and 0x18 + len(part4b) <= 0x50
    code4 = part4a + b'\xCC' * (0x18 - len(part4a)) + part4b + b'\xCC' * 4
    P['JMP_Part4A_Alt2'] = (S['part4a'], ['0FBF 58 40', '0FBF 78 3C', '85 DB', '78 69', 'B9 04000000', '49 8B 45 00', '8B 04 01'],
                            jmp_write(S['part4a'], p4a, 8), None)
    P['JMP_Part4B_Alt2'] = (S['part4b'], ['0FBF 40 40', 'XX XXXX XX XX', 'F3 0F10 46 0C', '44 03 CB', '49 8B 4D 10', '66 83 F8 FF'],
                            '90909090 XX XXXXXXXX ' + jmp_write(S['part4b_jmp'], p4b, 12), None)
    P['NewCode_Part4'] = (p4a, zero_pattern(), spaced(code4), None)

    # Part5: cmovle -> cmove, 0xFCA95 -> 0xFD438.
    P['Edit_Part5'] = (S['part5'], ['66 83 F8 FF', 'F3 0F11 44 24 38', '8D 14 18', '44 89 74 24 30', '0F4E D0'],
                       '0F44 D0', 4)
    P['NewCode_Part6'] = (cave(0x1B77750), zero_pattern(), spaced(relocate_cave(old_write('NewCode_Part6'), 0x1B77750, [
        ('4C 8B 8C 24 E8 00 00 00', '4C 8B 8C 24 F0 00 00 00'),
    ])), None)

    # --- RevampExtra ----------------------------------------------------
    # Unchanged patterns, new search start.
    P['AddRevampCPK'] = (S['cpk_search'], orig['AddRevampCPK'][0], orig['AddRevampCPK'][1], 2)
    # Title BGM: same code, ebp -> r15d in the stores around it.
    P['ChangeCueID1'] = (S['bgm1'], ['45 33 C9', '44 89 ' + R + ' 24 50', '45 33 C0', '44 89 ' + R + ' 24 48', '44 89 ' + R + ' 24 40',
                                    'F3 0F11 44 24 38', '41 8D 51 1D', '4C 89 ' + R + ' 24 30', '8D 4A E4'],
                         '418D51 53', 6)
    P['ChangeCueID2'] = (S['bgm2'], ['33 C0', '89 44 24 50', '89 44 24 48', '89 44 24 40', 'F3 0F10 05 XXXXXXXX',
                                    'F3 0F11 44 24 38', '48 89 44 24 30', '48 89 44 24 28', '48 89 44 24 20',
                                    '45 33 C9', '45 33 C0', '8D 50 1D'],
                         '8D50 53', 11)
    # Revamp wrote lea ecx,[rcx+1], which is 1 only if rcx happens to be 0
    # after the preceding call. ecx must be 1 (the BGM cue sheet); edx is now
    # 0x53, so lea ecx,[rdx-0x52]. Change 0x52 too if you change the cue.
    P['EnableCueIDChange'] = (S['bgm1'], ['45 33 C9', '44 89 ' + R + ' 24 50', '45 33 C0', '44 89 ' + R + ' 24 48', '44 89 ' + R + ' 24 40',
                                         'F3 0F11 44 24 38', '41 8D 51 XX', '4C 89 ' + R + ' 24 30', '8D 4A E4'],
                              '8D 4A AE', 8)
    # Partner colour fix: identical code; the int3 padding it uses is at the
    # same distance (+0x66).
    # (1.26 writes the -1 as mov eax,-1 instead of or eax,-1.)
    P['ColorJumpCode'] = (S['color_search'], ['74 0E', '48 8B C3', '0FB7 44 59 0E', '48 83 C4 20', '5B', 'C3',
                                     'B8 FFFFFFFF', '48 83 C4 20', '5B', 'C3'],
                          orig['ColorJumpCode'][1], 3)
    P['ColorNewCode'] = (S['color_search'], orig['ColorNewCode'][0], orig['ColorNewCode'][1], 5)
    # Partner character codes: 1.26 dropped VTB from the string list and
    # moved it; the 20 new 4-byte code slots go in the unreferenced zeros
    # right before it (0x10985D0..0x109861F, same 64 KB window as the RSV
    # string the partner rows point at, so the 2-byte pointer rewrite works).
    P['AddCharCodes'] = (S['codes_search'], ['00000000'] * 20 + [
        '474B3400 54504F00 52313700 474B4700', '53443100 42555500 4A524E00 47475400', '53423200 474B4200 23534156'],
        orig['AddCharCodes'][1], None)

    # Quest voice (BAQ vox): identical site, 0x853EDA -> 0x7F2184.
    P['Jump_BAQVoxName'] = (S['baq_search'], orig['Jump_BAQVoxName'][0], jmp_write(S['baq'], cave(0x1B77500), 7), None)
    P['NewCode_BAQVoxName'] = (cave(0x1B77500), orig['NewCode_BAQVoxName'][0],
                               spaced(relocate_cave(old_write('NewCode_BAQVoxName'), 0x1B77500)), None)

    # Body change by body type: 0xFD7A7 -> 0xFE389, r14 -> r13.
    P['Original_Code'] = (S['body_search'], ['F3 0F2C 56 0C', '85 D2', '79 0A', '49 8B 4D 10', '8B 91 C4000000', '0FB7 4E 02'],
                          jmp_write(S['body'], cave(0x1B777F0), 5), None)
    P['New_Code'] = (cave(0x1B777F0), zero_pattern(), spaced(relocate_cave(old_write('New_Code'), 0x1B777F0, [
        ('49 8B 0E 90', '49 8B 4D 00'),  # mov rcx,[r13] (was mov rcx,[r14]; nop)
    ])), None)

    # CUS aura fix 1: call replaces movsx r9d,[rdx+3A] at 0xFD415. The bytes
    # after it are wildcards because JMP_Part4B may already have rewritten them.
    P['Misc_CusAuraFix1Jump'] = (S['aura1_search'], ['44 0FBF 4A 3A', 'XX XXXX XX XX', 'XX XX XX', 'XX XX XX XX', '66 83 F8 FF',
                                           'F3 0F11 44 24 38', '8D 14 18', '44 89 74 24 30', 'XXXX XX'],
                                 call_write(S['aura1'], cave(0x1B77870), 5), None)
    P['Misc_CusAuraFix1Code'] = (cave(0x1B77870), zero_pattern(), spaced(relocate_cave(old_write('Misc_CusAuraFix1Code'), 0x1B77870, [
        ('49 8B 4E 10', '49 8B 4D 10'),  # mov rcx,[r13+0x10]
    ])), None)

    # CUS aura fix 2: 1.26 loads the aura id into eax via movsx eax,[r12+3A]
    # (was r15d via [r13+3A]) *before* the flag test, so the call no longer
    # sits between the test and its branch as it did in 1.25.02. r14 is free:
    # it is reloaded right after.
    a2 = cave(0x1B778C0)
    code2 = asm([
        bytes.fromhex('410FBF44243A'),    # movsx eax, word [r12+0x3a]
        bytes.fromhex('663DFA00'),        # cmp ax, 0xfa
        ('jcc8', 0x72, 'ret'),
        bytes.fromhex('663DFC00'),        # cmp ax, 0xfc
        ('jcc8', 0x76, 'race'),
        ('label', 'ret'), b'\xC3',
        ('label', 'race'),
        bytes.fromhex('448BB1B4000000'),  # mov r14d, [rcx+0xb4]  (character id)
        bytes.fromhex('4183FE6C'),        # cmp r14d, 0x6c
        ('jcc8', 0x77, 'ret'),
        bytes.fromhex('4183FE68'),        # cmp r14d, 0x68
        ('jcc8', 0x72, 'ret'),
        bytes.fromhex('663DFA00'),        # cmp ax, 0xfa
        ('jcc8', 0x75, 'fb'),
        bytes.fromhex('66B81E00'), b'\xC3',  # mov ax, 0x1e
        ('label', 'fb'),
        bytes.fromhex('663DFB00'),        # cmp ax, 0xfb
        ('jcc8', 0x75, 'fc'),
        bytes.fromhex('66B81C00'), b'\xC3',  # mov ax, 0x1c
        ('label', 'fc'),
        bytes.fromhex('66B80A00'), b'\xC3',  # mov ax, 0x0a
    ], a2)
    assert a2 + len(code2) <= cave(0x1B77910)
    P['Misc_CusAuraFix2Jump'] = (S['aura2_search'], ['41 0FBF 44 24 3A', '41 F6 44 24 58 04', '4C 63 B1 F0220000', '46 8D 2C 30',
                                           '44 0F44 E8', '44 3B A9 EC220000'],
                                 call_write(S['aura2'], a2, 6), None)
    P['Misc_CusAuraFix2Code'] = (a2, zero_pattern(), spaced(code2 + b'\xCC' * 4), None)

    # CUS aura fix 3: identical code, character-struct field moved (+0x20)
    # only in the untouched instruction after it.
    P['Misc_CusAuraFix3Jump'] = (S['aura3_search'], ['0FB7 40 3A', '33 C9', '66 83 F8 FF', '7E 09', '8B 8B EC220000', '98', '2B C8'],
                                 call_write(S['aura3'], cave(0x1B77910), 6), None)
    P['Misc_CusAuraFix3Code'] = (cave(0x1B77910), zero_pattern(),
                                 spaced(relocate_cave(old_write('Misc_CusAuraFix3Code'), 0x1B77910)), None)
    return P


def port_partner(name, instrs, write):
    """AddUnusedPartnerIDnn: 0x45 -> 0x44 in the rows, new terminator row,
    slot pointer low bytes rebased from 0x113FE50.. to the AddCharCodes slots
    (0x10985D0.. in 1.26.01, 0x10A6770.. in 1.27)."""
    rows = [i.replace('45000000', '44000000') for i in instrs[:-1]]
    rows.append('FFFFFFFF00000000 XXXXXXXXXXXXXXXX')
    w = hexs(write)
    low = int.from_bytes(bytes.fromhex(w[:4]), 'little')
    base = T['partner_low']
    new_low = low - 0xFE50 + base
    assert base <= new_low < base + 0x50
    return (T['site']['partner_search'], rows, spaced(new_low.to_bytes(2, 'little')) + ' ' + w[4:], None)


# ---------------------------------------------------------------- XML I/O

PATCH_RE = re.compile(r'(<!--)?\s*<Patch\s[^>]*name="([^"]+)"[^>]*>(.*?)</Patch>\s*(-->)?', re.S)


def parse(text):
    out = {}
    for m in PATCH_RE.finditer(text):
        body = m.group(3)
        instrs = re.findall(r'<Instruction\s+code="([^"]*)"', body)
        w = re.search(r'<Write\b[^>]*value="([^"]*)"', body)
        out[m.group(2)] = (instrs, w.group(1) if w else None, bool(m.group(1)))
    return out


def render(name, ss, instrs, write, idx, commented=False):
    lines = [f'\t<Patch name="{name}" type="write" search_start="{ss:#X}">'.replace('0X', '0x')]
    lines += [f'\t\t<Instruction code="{c}" />' for c in instrs]
    ia = f' inst_index="{idx}"' if idx is not None else ''
    lines.append(f'\t\t<Write{ia} value="{write}" />')
    lines.append('\t</Patch>')
    s = '\n'.join(lines)
    return f'\t<!--{s.strip()}-->' if commented else s


def main():
    global T, ADDR
    z = zipfile.ZipFile(sys.argv[1])
    files = (('RevampDynamicPartsets.xml', 'Dynamic Transformation Engine 5.4'), ('RevampExtra.xml', 'RevampMiscEdits'))
    parsed = {f: parse(z.read('Epatches/' + f).decode('utf-8-sig')) for f, _ in files}
    orig_all = {k: (v[0], v[1]) for p in parsed.values() for k, v in p.items() if v[1] is not None}
    for build, T in BUILDS.items():
        ADDR = T['addr']
        outdir = os.path.join(sys.argv[2], build)
        os.makedirs(outdir, exist_ok=True)
        ported = port(orig_all)
        for fname, title in files:
            body = []
            for name, (instrs, write, commented) in parsed[fname].items():
                if name.startswith('AddUnusedPartnerID'):
                    ss, ins, w, idx = port_partner(name, instrs, write)
                elif name in ported:
                    ss, ins, w, idx = ported[name]
                else:
                    raise SystemExit(f'{fname}: no {build} port for patch {name}')
                body.append(render(name, ss, ins, w, idx, commented))
            xml = ('<?xml version="1.0" encoding="utf-8" ?>\n'
                   f'<EternityPatchFile name="{title} (DBXV2 {build}, ported by XV2_RevampBridge)">\n'
                   + '\n'.join(body) + '\n</EternityPatchFile>\n')
            open(os.path.join(outdir, fname), 'w', encoding='utf-8').write(xml)
            print(f'{build}\\{fname}: {len(body)} patches')


if __name__ == '__main__':
    main()

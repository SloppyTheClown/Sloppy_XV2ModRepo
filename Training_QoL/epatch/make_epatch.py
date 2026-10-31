"""Build Training_QoL.xml, the XV2Patcher-only version of Training QoL, for one game version.

Assembles training_qol_cave.asm with ml64 (found through vswhere) for that
version, takes the code sections out of the object file and writes an
Eternity patch file (XV2Patcher Epatch) with:
  * the cave (its .text and CAVE2 sections), written into zero slack of the
    exe as GAMES[version]['slots'] lays out
  * the three calls of Training's CPU Status apply pointed at the cave's
    PerFrameApply / OtherApply
  * the two online-only checks in front of the HUD-hidden flag removed (the
    DLL patches the same bytes, kHudOnlineCheckJumps)

usage: make_epatch.py <game version> <out.xml> [decrypted exe image to check against]
       game version: 1.27, 1.26.01 or 1.25.02

The image (offset == RVA, e.g. from XV2_RevampBridge\\tools\\dumpimage.py)
is optional: with it the patches are applied the way XV2Patcher does (first
match from search_start) and the result is checked.
"""
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ASM = os.path.join(HERE, 'training_qol_cave.asm')
VERSION_FILE = os.path.join(HERE, '..', 'VERSION')
TEXT_TAIL = '48C705 XXXXXXXX 00000000 C3'   # mov qword [rip+...], 0 / ret: the last instructions of .text in both versions

# Per game version: the include with the cave's addresses (CAVE_RVA etc. are
# read from it), where the cave's sections go and the sites patched.
#   slots: (name, anchor RVA, anchor bytes, first RVA, limit RVA, sections,
#           gap) - the sections are laid out from the first RVA (16-aligned)
#           and found through the anchor, so they land there or nowhere (the
#           calls jump to fixed addresses); gap = the pattern between the
#           anchor and the first RVA: 'zero', or 'any' where other mods' caves
#           may already be written there
#   apply_calls: (name, call RVA, the 6 bytes in front of the call, entry point)
#   hud_checks: HUD draw: cmp [rcx+0xc0],0 (online) / je +0xd / cmp [rcx+HUD hidden],0
GAMES = {
    '1.27': {
        'include': 'game_1.27.inc',
        'define': 'GAME_1_27',
        'slots': [
            # .text VirtualAddress 0x1000 + VirtualSize 0xf585ec; next section 0xf5a000
            ('Cave', 0xF595E0, TEXT_TAIL, 0xF59600, 0xF5A000, ['.text', 'CAVE2'], 'zero'),
        ],
        'apply_calls': [
            ('PerFrameApply', 0x192EA5, '0F28CE 488BCE', 'PerFrameApply'),   # MenuTraining update 0x192db0 (1.26: 0x192de5)
            ('SettingChangedApply', 0x191FC3, '0F57C9 488BCB', 'OtherApply'),  # vtable slot 9 0x191f00 (1.26: 0x191f03)
            ('MenuSetupApply', 0x192914, '0F57C9 XXXXXX', 'OtherApply'),      # mov rcx,rdi here (1.26: 0x192854)
        ],
        'hud_checks': [('HudOnlineCheck1', 0x163817), ('HudOnlineCheck2', 0x163B14)],   # (1.26: 0x163757, 0x163a54)
        'hud_check_bytes': ['83B9C0000000 00', '740D', '83B928D50000 00'],
    },
    '1.26.01': {
        'include': 'game_1.26.01.inc',
        'define': None,
        'slots': [
            # .text VirtualAddress 0x1000 + VirtualSize 0xf4a05c; next section 0xf4c000
            ('Cave', 0xF4B050, TEXT_TAIL, 0xF4B400, 0xF4C000, ['.text', 'CAVE2'], 'zero'),
        ],
        'apply_calls': [
            ('PerFrameApply', 0x192DE5, '0F28CE 488BCE', 'PerFrameApply'),   # MenuTraining update 0x192cf0: movaps xmm1,xmm6 / mov rcx,rsi
            ('SettingChangedApply', 0x191F03, '0F57C9 488BCB', 'OtherApply'),  # vtable slot 9 0x191e40: xorps xmm1,xmm1 / mov rcx,rbx
            ('MenuSetupApply', 0x192854, '0F57C9 XXXXXX', 'OtherApply'),      # 0x192770: xorps xmm1,xmm1 / mov rcx,...
        ],
        'hud_checks': [('HudOnlineCheck1', 0x163757), ('HudOnlineCheck2', 0x163A54)],
        'hud_check_bytes': ['83B9C0000000 00', '740D', '83B928D50000 00'],
    },
    '1.25.02': {
        'include': 'game_1.25.02.inc',
        'define': 'GAME_1_25_02',
        'slots': [
            # .bind (executable) ends at 0x1b77448 with 0x1b77440 = 1D 31 61 33 83 7F 8F 67;
            # Revamp 5.1.1 writes its caves at 0x1b77500..0x1b77960, so the gap is 'any'
            ('Cave', 0x1B77440, '1D316133837F8F67', 0x1B77960, 0x1B78000, ['.text'], 'any'),
            # .text ends at 0xfd8d9c (VirtualSize 0xfd7d9c); next section 0xfd9000
            ('Cave2', 0xFD8D90, TEXT_TAIL, 0xFD8DA0, 0xFD9000, ['CAVE2'], 'zero'),
        ],
        'apply_calls': [
            ('PerFrameApply', 0x1961C3, '0F28CE 488BCE', 'PerFrameApply'),   # MenuTraining update 0x1960d0 (1.26: 0x192de5)
            ('SettingChangedApply', 0x195544, '0F57C9 488BCB', 'OtherApply'),  # vtable slot 9 (1.26: 0x191f03)
            ('MenuSetupApply', 0x196A62, '0F57C9 XXXXXX', 'OtherApply'),      # mov rcx,rdi here (1.26: 0x192854)
        ],
        'hud_checks': [('HudOnlineCheck1', 0x1679A7), ('HudOnlineCheck2', 0x167C84)],
        'hud_check_bytes': ['83B9C0000000 00', '740D', '83B924D50000 00'],   # HUD hidden at +0xd524
    },
}


def include_equates(game):
    """name -> value of the 'NAME equ 1234h' lines of the game's include"""
    text = open(os.path.join(HERE, game['include']), encoding='utf-8').read()
    return {name: int(value, 16) for name, value in re.findall(r'^(\w+)\s+equ\s+([0-9A-Fa-f]+)h', text, re.M)}


def find_ml64():
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
                           r'Microsoft Visual Studio\Installer\vswhere.exe')
    install = subprocess.run([vswhere, '-latest', '-products', '*', '-requires',
                              'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                             capture_output=True, text=True, check=True).stdout.strip()
    tools = os.path.join(install, r'VC\Tools\MSVC')
    for version in sorted(os.listdir(tools), reverse=True):
        path = os.path.join(tools, version, r'bin\Hostx64\x64\ml64.exe')
        if os.path.exists(path):
            return path
    sys.exit('ml64.exe not found (Visual Studio C++ x64 tools)')


def assemble(game):
    """-> {section name: (code bytes, [(offset, symbol index)])}, {symbol index: (section, value)}, {public name: (section, value)}"""
    defines = ['/D' + game['define']] if game['define'] else []
    with tempfile.TemporaryDirectory() as temp:
        obj_path = os.path.join(temp, 'cave.obj')
        result = subprocess.run([find_ml64(), '/nologo', '/c', '/I', HERE, *defines, '/Fo' + obj_path, ASM],
                                capture_output=True, text=True)
        if result.returncode:
            sys.exit(result.stdout + result.stderr)
        obj = open(obj_path, 'rb').read()

    # COFF: file header, section headers, symbol table (+ string table)
    _, section_count, _, symbol_table, symbol_count, optional_size, _ = struct.unpack_from('<HHIIIHH', obj, 0)
    sections, section_names = {}, {}
    IMAGE_REL_AMD64_REL32 = 4
    for index in range(section_count):
        header = 20 + optional_size + index * 40
        name = obj[header:header + 8].rstrip(b'\0').decode()
        name = name.split('$')[0]                     # .code is .text$mn
        size, raw, relocation_table = struct.unpack_from('<III', obj, header + 16)
        relocation_count = struct.unpack_from('<H', obj, header + 32)[0]
        if name not in ('.text', 'CAVE2'):
            continue
        relocations = []
        for i in range(relocation_count):
            offset, symbol_index, relocation_type = struct.unpack_from('<IIH', obj, relocation_table + 10 * i)
            # ml64 leaves rip-relative references to the cave's own labels as
            # REL32 relocations; anything else (an absolute address) can't work
            if relocation_type != IMAGE_REL_AMD64_REL32:
                sys.exit(f'{name}: relocation type {relocation_type} at +{offset:#x} - the cave must be position independent')
            relocations.append((offset, symbol_index))
        sections[name] = (bytearray(obj[raw:raw + size]), relocations)
        section_names[index + 1] = name
    strings = symbol_table + symbol_count * 18
    symbols, publics = {}, {}
    index = 0
    while index < symbol_count:
        entry = symbol_table + index * 18
        raw_name = obj[entry:entry + 8]
        if raw_name[:4] == b'\0\0\0\0':
            offset = struct.unpack_from('<I', raw_name, 4)[0]
            name = obj[strings + offset:obj.index(b'\0', strings + offset)]
        else:
            name = raw_name.rstrip(b'\0')
        value, section, _, _, aux_count = struct.unpack_from('<IhHBB', obj, entry + 8)
        if section in section_names:
            symbols[index] = (section_names[section], value)
            publics[name.decode()] = (section_names[section], value)
        index += 1 + aux_count
    return sections, symbols, publics


def layout(game, sections):
    """-> {section name: RVA} following the game's slots"""
    placed = {}
    for slot_name, _, _, first, limit, slot_sections, _ in game['slots']:
        address = first
        for name in slot_sections:
            address = (address + 15) & ~15
            placed[name] = address
            address += len(sections[name][0])
        if address > limit:
            sys.exit(f'{slot_name}: the cave sections are {address - first} bytes - they no longer fit before {limit:#x}')
    return placed


def link(sections, symbols, placed):
    """Resolve the REL32 relocations for where each section lands -> {section name: bytes}"""
    linked = {}
    for name, (code, relocations) in sections.items():
        code = bytearray(code)
        for offset, symbol_index in relocations:
            if symbol_index not in symbols:
                sys.exit(f'{name}: relocation at +{offset:#x} to a symbol outside the cave')
            target_section, value = symbols[symbol_index]
            addend = struct.unpack_from('<i', code, offset)[0]
            target = placed[target_section] + value + addend
            struct.pack_into('<i', code, offset, target - (placed[name] + offset + 4))
        linked[name] = bytes(code)
    return linked


def hex_bytes(data):
    return ' '.join(f'{byte:02X}' for byte in data)


def call_bytes(site, target):
    return b'\xE8' + struct.pack('<i', target - (site + 5))


def build_patches(game, equates, linked, placed, entries):
    """-> list of (name, search_start, [instruction hex], write inst_index, write bytes)"""
    patches = []
    for slot_name, anchor, anchor_bytes, first, _, slot_sections, gap in game['slots']:
        # the slot's sections with the zero padding between them
        content = b''
        for name in slot_sections:
            content += bytes(placed[name] - first - len(content)) + linked[name]
        anchor_end = anchor + len(anchor_bytes.replace(' ', '')) // 2
        gap_bytes = first - anchor_end
        gap_pattern = hex_bytes(bytes(gap_bytes)) if gap == 'zero' else ' '.join(['XX'] * gap_bytes)
        patches.append((f'TrainingQoL_{slot_name}', anchor, [anchor_bytes, gap_pattern, hex_bytes(bytes(len(content)))],
                        2, content))
    for name, site, before, entry in game['apply_calls']:
        patches.append((f'TrainingQoL_{name}', site - 6,
                        [before, hex_bytes(call_bytes(site, equates['FN_TRAINING_APPLY']))],
                        1, call_bytes(site, entries[entry])))
    for name, check in game['hud_checks']:
        patches.append((f'TrainingQoL_{name}', check, game['hud_check_bytes'], 1, b'\x90\x90'))
    return patches


def write_xml(path, version, patches):
    mod_version = open(VERSION_FILE).read().strip()
    lines = ['<?xml version="1.0" encoding="utf-8" ?>',
             '<!-- Training QoL (XV2Patcher version) - made by make_epatch.py from training_qol_cave.asm, do not edit by hand -->',
             f'<EternityPatchFile name="Training QoL {mod_version} (DBXV2 {version})">']
    for name, start, instructions, inst_index, write in patches:
        lines.append(f'\t<Patch name="{name}" type="write" search_start="{start:#X}">'.replace('0X', '0x'))
        for instruction in instructions:
            lines.append(f'\t\t<Instruction code="{instruction}" />')
        lines.append(f'\t\t<Write inst_index="{inst_index}" value="{hex_bytes(write)}" />')
        lines.append('\t</Patch>')
    lines.append('</EternityPatchFile>')
    with open(path, 'w', encoding='utf-8', newline='\r\n') as file:
        file.write('\n'.join(lines) + '\n')


def pattern(instructions):
    regex = b''
    for token in re.findall(r'[0-9A-Fa-fX]{2}', ''.join(instructions).replace(' ', '')):
        regex += b'.' if token.upper() == 'XX' else re.escape(bytes([int(token, 16)]))
    return re.compile(regex, re.S)


def check(image, game, equates, patches, linked, placed, entries, publics):
    """Apply the patches like XV2Patcher (first match from search_start) and check the result."""
    patched = bytearray(image)
    for name, start, instructions, inst_index, write in patches:
        match = pattern(instructions).search(patched, start)
        if not match:
            sys.exit(f'{name}: pattern not found after {start:#x}')
        everywhere = len(pattern(instructions).findall(image))
        offset = match.start() + sum(len(i.replace(' ', '')) // 2 for i in instructions[:inst_index])
        patched[offset:offset + len(write)] = write
        print(f'  {name:30} writes {len(write):4} bytes at {offset:#x}  ({everywhere} match(es) in the image)')
    for name, address in placed.items():
        if patched[address:address + len(linked[name])] != linked[name]:
            sys.exit(f'section {name} did not land at {address:#x}')
    state = equates['STATE_RVA']
    if any(image[state:state + 0x80]):
        sys.exit(f'the state block at {state:#x} is not zero in the image')
    for name, site, _, entry in game['apply_calls']:
        target = site + 5 + struct.unpack_from('<i', patched, site + 1)[0]
        if target != entries[entry]:
            sys.exit(f'{name}: call goes to {target:#x}')
        print(f'  call at {site:#x} -> {target:#x} ({entry})')
    try:
        import capstone
    except ImportError:
        print('  (capstone not installed - cave not disassembled)')
        return
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    for name, code in linked.items():
        data_start = publics['XInputNames'][1] if publics['XInputNames'][0] == name else len(code)
        decoded = sum(i.size for i in disassembler.disasm(code[:data_start], placed[name]))
        if decoded != data_start:
            sys.exit(f'section {name} does not disassemble cleanly (stopped at +{decoded:#x})')
        print(f'  {name}: {len(code)} bytes, {placed[name]:#x}..{placed[name] + len(code):#x}')
    print('  entries ' + ', '.join(f'{entry} {address:#x}' for entry, address in entries.items()))


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in GAMES:
        sys.exit(__doc__)
    version = sys.argv[1]
    game = GAMES[version]
    equates = include_equates(game)
    sections, symbols, publics = assemble(game)
    placed = layout(game, sections)
    if placed['.text'] != equates['CAVE_RVA']:
        sys.exit(f"{game['include']}: CAVE_RVA is {equates['CAVE_RVA']:#x} but the main piece lands at {placed['.text']:#x}")
    linked = link(sections, symbols, placed)
    entries = {entry: placed[publics[entry][0]] + publics[entry][1] for entry in ('PerFrameApply', 'OtherApply')}
    patches = build_patches(game, equates, linked, placed, entries)
    write_xml(sys.argv[2], version, patches)
    print(f'wrote {sys.argv[2]} (game {version})')
    if len(sys.argv) > 3:
        check(open(sys.argv[3], 'rb').read(), game, equates, patches, linked, placed, entries, publics)


if __name__ == '__main__':
    main()

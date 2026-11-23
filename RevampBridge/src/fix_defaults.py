"""Restore the type of `DefaultValue = <literal>` attribute arguments.

ILSpy drops the type of object-typed attribute arguments (`DefaultValue = 0`
on a ushort property recompiles as a boxed int, which YAXLib cannot assign).
The true types come from attribute dumps of the original DLLs (attrdump.exe).

usage: fix_defaults.py <source dir> <dump> [<dump> ...]
"""
import os, re, sys

CS = {'Byte': 'byte', 'SByte': 'sbyte', 'Int16': 'short', 'UInt16': 'ushort', 'Int32': 'int',
      'UInt32': 'uint', 'Int64': 'long', 'UInt64': 'ulong', 'Single': 'float', 'Double': 'double',
      'Boolean': 'bool'}

truth = {}  # "Ns.Type::Member" -> C# type of its DefaultValue
dv_re = re.compile(r'^(\S+::\w+) \[YAXErrorIfMissedAttribute\(.*DefaultValue=(\w+):')
for dump in sys.argv[2:]:
    for line in open(dump, encoding='utf-8', errors='replace'):
        m = dv_re.match(line)
        if m and m.group(2) in CS:
            truth[m.group(1)] = CS[m.group(2)]

lit = r'-?[0-9][0-9.eE+-]*[fFdDuUlLmM]*|true|false'
attr_re = re.compile(r'DefaultValue\s*=\s*(?:\(\w+\))?(' + lit + r')(?=[,)\]])')
ns_re = re.compile(r'^namespace\s+([\w.]+)')
cls_re = re.compile(r'^(\t*)(?:public|internal|private|protected)?[\w\s]*\b(?:class|struct)\s+(\w+)')
member_re = re.compile(r'^\s*(?:public|internal|private|protected)[\w\s<>\[\],.?]*?\s(\w+)\s*(?:\{|=|;|$)')

fixed = 0
for dp, _, fs in os.walk(sys.argv[1]):
    for f in fs:
        if not f.endswith('.cs'):
            continue
        p = os.path.join(dp, f)
        lines = open(p, encoding='utf-8-sig').read().split('\n')
        ns, stack, pending, dirty = '', [], [], False
        for i, line in enumerate(lines):
            m = ns_re.match(line)
            if m:
                ns = m.group(1)
            m = cls_re.match(line)
            if m and not line.strip().startswith('['):
                depth = len(m.group(1))
                stack = [s for s in stack if s[0] < depth] + [(depth, m.group(2))]
                pending = []
                continue
            if 'DefaultValue' in line and attr_re.search(line):
                pending.append(i)
                continue
            if pending and line.strip() and not line.strip().startswith('['):
                m = member_re.match(line)
                if m and stack:
                    key = f"{ns}.{'+'.join(s[1] for s in stack)}::{m.group(1)}"
                    t = truth.get(key)
                    if t:
                        for j in pending:
                            new = attr_re.sub(lambda a: f'DefaultValue = ({t}){a.group(1)}', lines[j])
                            if new != lines[j]:
                                lines[j], dirty = new, True
                                fixed += 1
                pending = []
        if dirty:
            open(p, 'w', encoding='utf-8').write('\n'.join(lines))
print(f'fix_defaults: {fixed} attribute(s) retyped ({len(truth)} known)')

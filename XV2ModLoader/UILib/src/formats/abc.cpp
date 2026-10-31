// AS3 ABC parse / write and the instruction coder (see abc.h).
#include "abc.h"
#include <algorithm>
#include <map>

namespace xv2ui {
namespace abc {

namespace {

void ReadTraits(Reader& r, std::vector<Trait>& traits)
{
    traits.resize(r.u30());
    for (Trait& t : traits) {
        t.name = r.u30();
        t.kindByte = r.u8();
        switch (t.kind()) {
        case kSlot: case kConst:
            t.slotId = r.u30();
            t.typeName = r.u30();
            t.valueIndex = r.u30();
            if (t.valueIndex) t.valueKind = r.u8();
            break;
        case kClassTrait: case kFunction: case kMethodTrait: case kGetter: case kSetter:
            t.slotId = r.u30();
            t.index = r.u30();
            break;
        default:
            r.Fail("unknown trait kind");
        }
        if (t.attributes() & kAttrMetadata) {
            t.metadata.resize(r.u30());
            for (uint32_t& m : t.metadata) m = r.u30();
        }
    }
}

void WriteTraits(Writer& w, const std::vector<Trait>& traits)
{
    w.u30((uint32_t)traits.size());
    for (const Trait& t : traits) {
        w.u30(t.name);
        w.u8(t.kindByte);
        switch (t.kind()) {
        case kSlot: case kConst:
            w.u30(t.slotId);
            w.u30(t.typeName);
            w.u30(t.valueIndex);
            if (t.valueIndex) w.u8(t.valueKind);
            break;
        default:
            w.u30(t.slotId);
            w.u30(t.index);
            break;
        }
        if (t.attributes() & kAttrMetadata) {
            w.u30((uint32_t)t.metadata.size());
            for (uint32_t m : t.metadata) w.u30(m);
        }
    }
}

// The count of a pool table: entries + the implicit 0 (an empty table is written as 0).
template <class T> uint32_t PoolCount(const std::vector<T>& table) { return table.size() <= 1 ? 0 : (uint32_t)table.size(); }

}  // namespace

void AbcFile::Parse(const uint8_t* data, size_t size)
{
    Reader r(data, size, "abc");
    minorVersion = r.u16();
    majorVersion = r.u16();
    pool = ConstantPool();
    // count 0 and 1 both mean "no entries" (only the implicit 0)
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) pool.ints.push_back(r.s32());
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) pool.uints.push_back(r.u30());
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) pool.doubles.push_back(r.f64());
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) {
        const uint32_t length = r.u30();
        Bytes text = r.bytes(length);
        pool.strings.emplace_back(text.begin(), text.end());
    }
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) {
        Namespace ns;
        ns.kind = r.u8();
        ns.name = r.u30();
        pool.namespaces.push_back(ns);
    }
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) {
        std::vector<uint32_t> set(r.u30());
        for (uint32_t& ns : set) ns = r.u30();
        pool.nsSets.push_back(std::move(set));
    }
    for (uint32_t i = 1, n = r.u30(); i < n; ++i) {
        Multiname m;
        m.kind = r.u8();
        switch (m.kind) {
        case kQName: case kQNameA: m.ns = r.u30(); m.name = r.u30(); break;
        case kRTQName: case kRTQNameA: m.name = r.u30(); break;
        case kRTQNameL: case kRTQNameLA: break;
        case kMultiname: case kMultinameA: m.name = r.u30(); m.nsSet = r.u30(); break;
        case kMultinameL: case kMultinameLA: m.nsSet = r.u30(); break;
        case kTypeName:
            m.base = r.u30();
            m.params.resize(r.u30());
            for (uint32_t& p : m.params) p = r.u30();
            break;
        default: r.Fail("unknown multiname kind " + Reader::Hex(m.kind));
        }
        pool.multinames.push_back(std::move(m));
    }

    methods.resize(r.u30());
    for (Method& m : methods) {
        const uint32_t paramCount = r.u30();
        m.returnType = r.u30();
        m.paramTypes.resize(paramCount);
        for (uint32_t& p : m.paramTypes) p = r.u30();
        m.name = r.u30();
        m.flags = r.u8();
        if (m.flags & kHasOptional) {
            m.options.resize(r.u30());
            for (OptionValue& o : m.options) { o.value = r.u30(); o.kind = r.u8(); }
        }
        if (m.flags & kHasParamNames) {
            m.paramNames.resize(paramCount);
            for (uint32_t& p : m.paramNames) p = r.u30();
        }
    }
    metadata.resize(r.u30());
    for (Metadata& md : metadata) {
        md.name = r.u30();
        md.items.resize(r.u30());
        for (MetadataItem& item : md.items) item.key = r.u30();
        for (MetadataItem& item : md.items) item.value = r.u30();
    }
    const uint32_t classCount = r.u30();
    instances.resize(classCount);
    for (Instance& in : instances) {
        in.name = r.u30();
        in.superName = r.u30();
        in.flags = r.u8();
        if (in.flags & kProtectedNs) in.protectedNs = r.u30();
        in.interfaces.resize(r.u30());
        for (uint32_t& i : in.interfaces) i = r.u30();
        in.iinit = r.u30();
        ReadTraits(r, in.traits);
    }
    classes.resize(classCount);
    for (Class& c : classes) {
        c.cinit = r.u30();
        ReadTraits(r, c.traits);
    }
    scripts.resize(r.u30());
    for (Script& s : scripts) {
        s.init = r.u30();
        ReadTraits(r, s.traits);
    }
    bodies.resize(r.u30());
    for (MethodBody& b : bodies) {
        b.method = r.u30();
        b.maxStack = r.u30();
        b.localCount = r.u30();
        b.initScopeDepth = r.u30();
        b.maxScopeDepth = r.u30();
        b.code = r.bytes(r.u30());
        b.exceptions.resize(r.u30());
        for (ExceptionInfo& e : b.exceptions) {
            e.from = r.u30(); e.to = r.u30(); e.target = r.u30(); e.excType = r.u30(); e.varName = r.u30();
        }
        ReadTraits(r, b.traits);
    }
    if (!r.atEnd()) r.Fail("bytes after the method bodies");
}

Bytes AbcFile::Write() const
{
    Writer w;
    w.u16(minorVersion);
    w.u16(majorVersion);
    w.u30(PoolCount(pool.ints));
    for (size_t i = 1; i < pool.ints.size(); ++i) w.s32(pool.ints[i]);
    w.u30(PoolCount(pool.uints));
    for (size_t i = 1; i < pool.uints.size(); ++i) w.u30(pool.uints[i]);
    w.u30(PoolCount(pool.doubles));
    for (size_t i = 1; i < pool.doubles.size(); ++i) w.f64(pool.doubles[i]);
    w.u30(PoolCount(pool.strings));
    for (size_t i = 1; i < pool.strings.size(); ++i) {
        w.u30((uint32_t)pool.strings[i].size());
        w.Raw(pool.strings[i].data(), pool.strings[i].size());
    }
    w.u30(PoolCount(pool.namespaces));
    for (size_t i = 1; i < pool.namespaces.size(); ++i) { w.u8(pool.namespaces[i].kind); w.u30(pool.namespaces[i].name); }
    w.u30(PoolCount(pool.nsSets));
    for (size_t i = 1; i < pool.nsSets.size(); ++i) {
        w.u30((uint32_t)pool.nsSets[i].size());
        for (uint32_t ns : pool.nsSets[i]) w.u30(ns);
    }
    w.u30(PoolCount(pool.multinames));
    for (size_t i = 1; i < pool.multinames.size(); ++i) {
        const Multiname& m = pool.multinames[i];
        w.u8(m.kind);
        switch (m.kind) {
        case kQName: case kQNameA: w.u30(m.ns); w.u30(m.name); break;
        case kRTQName: case kRTQNameA: w.u30(m.name); break;
        case kRTQNameL: case kRTQNameLA: break;
        case kMultiname: case kMultinameA: w.u30(m.name); w.u30(m.nsSet); break;
        case kMultinameL: case kMultinameLA: w.u30(m.nsSet); break;
        case kTypeName:
            w.u30(m.base);
            w.u30((uint32_t)m.params.size());
            for (uint32_t p : m.params) w.u30(p);
            break;
        }
    }
    w.u30((uint32_t)methods.size());
    for (const Method& m : methods) {
        w.u30((uint32_t)m.paramTypes.size());
        w.u30(m.returnType);
        for (uint32_t p : m.paramTypes) w.u30(p);
        w.u30(m.name);
        w.u8(m.flags);
        if (m.flags & kHasOptional) {
            w.u30((uint32_t)m.options.size());
            for (const OptionValue& o : m.options) { w.u30(o.value); w.u8(o.kind); }
        }
        if (m.flags & kHasParamNames)
            for (size_t i = 0; i < m.paramTypes.size(); ++i) w.u30(i < m.paramNames.size() ? m.paramNames[i] : 0);
    }
    w.u30((uint32_t)metadata.size());
    for (const Metadata& md : metadata) {
        w.u30(md.name);
        w.u30((uint32_t)md.items.size());
        for (const MetadataItem& item : md.items) w.u30(item.key);
        for (const MetadataItem& item : md.items) w.u30(item.value);
    }
    w.u30((uint32_t)instances.size());
    for (const Instance& in : instances) {
        w.u30(in.name);
        w.u30(in.superName);
        w.u8(in.flags);
        if (in.flags & kProtectedNs) w.u30(in.protectedNs);
        w.u30((uint32_t)in.interfaces.size());
        for (uint32_t i : in.interfaces) w.u30(i);
        w.u30(in.iinit);
        WriteTraits(w, in.traits);
    }
    for (const Class& c : classes) {
        w.u30(c.cinit);
        WriteTraits(w, c.traits);
    }
    w.u30((uint32_t)scripts.size());
    for (const Script& s : scripts) {
        w.u30(s.init);
        WriteTraits(w, s.traits);
    }
    w.u30((uint32_t)bodies.size());
    for (const MethodBody& b : bodies) {
        w.u30(b.method);
        w.u30(b.maxStack);
        w.u30(b.localCount);
        w.u30(b.initScopeDepth);
        w.u30(b.maxScopeDepth);
        w.u30((uint32_t)b.code.size());
        w.Append(b.code);
        w.u30((uint32_t)b.exceptions.size());
        for (const ExceptionInfo& e : b.exceptions) { w.u30(e.from); w.u30(e.to); w.u30(e.target); w.u30(e.excType); w.u30(e.varName); }
        WriteTraits(w, b.traits);
    }
    return w.out;
}

int AbcFile::BodyOf(uint32_t method) const
{
    for (size_t i = 0; i < bodies.size(); ++i)
        if (bodies[i].method == method) return (int)i;
    return -1;
}

const std::string& AbcFile::String(uint32_t index) const
{
    static const std::string none;
    return index < pool.strings.size() ? pool.strings[index] : none;
}

std::string AbcFile::NamespaceText(uint32_t index) const
{
    if (index == 0 || index >= pool.namespaces.size()) return "*";
    const Namespace& ns = pool.namespaces[index];
    const char* kind = "ns";
    switch (ns.kind) {
    case kNsPrivate: kind = "private"; break;
    case kNsPackage: kind = "package"; break;
    case kNsPackageInternal: kind = "internal"; break;
    case kNsProtected: kind = "protected"; break;
    case kNsExplicit: kind = "explicit"; break;
    case kNsStaticProtected: kind = "staticprotected"; break;
    }
    return std::string(kind) + ":" + String(ns.name);
}

std::string AbcFile::MultinameText(uint32_t index) const
{
    if (index == 0 || index >= pool.multinames.size()) return "*";
    const Multiname& m = pool.multinames[index];
    switch (m.kind) {
    case kQName: case kQNameA: {
        const Namespace* ns = m.ns < pool.namespaces.size() ? &pool.namespaces[m.ns] : nullptr;
        const std::string& package = ns ? String(ns->name) : String(0);
        return (package.empty() ? "" : package + "::") + String(m.name);
    }
    case kRTQName: case kRTQNameA: case kMultiname: case kMultinameA: return String(m.name);
    case kTypeName: {
        std::string text = MultinameText(m.base) + ".<";
        for (size_t i = 0; i < m.params.size(); ++i) text += (i ? "," : "") + MultinameText(m.params[i]);
        return text + ">";
    }
    default: return "[runtime name]";
    }
}

// ----------------------------------------------------------------- code

namespace {

struct OpcodeTable {
    OpcodeInfo entries[256] = {};
    OpcodeTable()
    {
        auto set = [&](uint8_t op, const char* name, OperandKind first = kOpNone, OperandKind second = kOpNone) {
            entries[op] = {name, first, second};
        };
        set(0x01, "bkpt"); set(0x02, "nop"); set(0x03, "throw");
        set(0x04, "getsuper", kOpMultiname); set(0x05, "setsuper", kOpMultiname);
        set(0x06, "dxns", kOpString); set(0x07, "dxnslate"); set(0x08, "kill", kOpU30); set(0x09, "label");
        const char* branches[] = {"ifnlt", "ifnle", "ifngt", "ifnge", "jump", "iftrue", "iffalse", "ifeq", "ifne",
                                  "iflt", "ifle", "ifgt", "ifge", "ifstricteq", "ifstrictne"};
        for (int i = 0; i < 15; ++i) set((uint8_t)(0x0c + i), branches[i], kOpBranch);
        set(0x1b, "lookupswitch", kOpSwitch);
        set(0x1c, "pushwith"); set(0x1d, "popscope"); set(0x1e, "nextname"); set(0x1f, "hasnext");
        set(0x20, "pushnull"); set(0x21, "pushundefined"); set(0x23, "nextvalue");
        set(0x24, "pushbyte", kOpU8); set(0x25, "pushshort", kOpU30);
        set(0x26, "pushtrue"); set(0x27, "pushfalse"); set(0x28, "pushnan"); set(0x29, "pop"); set(0x2a, "dup"); set(0x2b, "swap");
        set(0x2c, "pushstring", kOpString); set(0x2d, "pushint", kOpInt); set(0x2e, "pushuint", kOpUInt);
        set(0x2f, "pushdouble", kOpDouble); set(0x30, "pushscope"); set(0x31, "pushnamespace", kOpNamespace);
        set(0x32, "hasnext2", kOpU30, kOpU30);
        // Alchemy memory ops (no operands)
        const char* memory[] = {"li8", "li16", "li32", "lf32", "lf64", "si8", "si16", "si32", "sf32", "sf64"};
        for (int i = 0; i < 10; ++i) set((uint8_t)(0x35 + i), memory[i]);
        set(0x40, "newfunction", kOpMethod); set(0x41, "call", kOpU30); set(0x42, "construct", kOpU30);
        set(0x43, "callmethod", kOpU30, kOpU30); set(0x44, "callstatic", kOpMethod, kOpU30);
        set(0x45, "callsuper", kOpMultiname, kOpU30); set(0x46, "callproperty", kOpMultiname, kOpU30);
        set(0x47, "returnvoid"); set(0x48, "returnvalue"); set(0x49, "constructsuper", kOpU30);
        set(0x4a, "constructprop", kOpMultiname, kOpU30); set(0x4c, "callproplex", kOpMultiname, kOpU30);
        set(0x4e, "callsupervoid", kOpMultiname, kOpU30); set(0x4f, "callpropvoid", kOpMultiname, kOpU30);
        set(0x50, "sxi1"); set(0x51, "sxi8"); set(0x52, "sxi16");
        set(0x53, "applytype", kOpU30); set(0x55, "newobject", kOpU30); set(0x56, "newarray", kOpU30);
        set(0x57, "newactivation"); set(0x58, "newclass", kOpClass); set(0x59, "getdescendants", kOpMultiname);
        set(0x5a, "newcatch", kOpException);
        set(0x5d, "findpropstrict", kOpMultiname); set(0x5e, "findproperty", kOpMultiname);
        set(0x5f, "finddef", kOpMultiname); set(0x60, "getlex", kOpMultiname); set(0x61, "setproperty", kOpMultiname);
        set(0x62, "getlocal", kOpU30); set(0x63, "setlocal", kOpU30); set(0x64, "getglobalscope");
        set(0x65, "getscopeobject", kOpU8); set(0x66, "getproperty", kOpMultiname); set(0x67, "getouterscope", kOpU30);
        set(0x68, "initproperty", kOpMultiname); set(0x6a, "deleteproperty", kOpMultiname);
        set(0x6c, "getslot", kOpU30); set(0x6d, "setslot", kOpU30); set(0x6e, "getglobalslot", kOpU30); set(0x6f, "setglobalslot", kOpU30);
        set(0x70, "convert_s"); set(0x71, "esc_xelem"); set(0x72, "esc_xattr"); set(0x73, "convert_i"); set(0x74, "convert_u");
        set(0x75, "convert_d"); set(0x76, "convert_b"); set(0x77, "convert_o"); set(0x78, "checkfilter");
        set(0x80, "coerce", kOpMultiname); set(0x81, "coerce_b"); set(0x82, "coerce_a"); set(0x83, "coerce_i");
        set(0x84, "coerce_d"); set(0x85, "coerce_s"); set(0x86, "astype", kOpMultiname); set(0x87, "astypelate");
        set(0x88, "coerce_u"); set(0x89, "coerce_o");
        set(0x90, "negate"); set(0x91, "increment"); set(0x92, "inclocal", kOpU30); set(0x93, "decrement");
        set(0x94, "declocal", kOpU30); set(0x95, "typeof"); set(0x96, "not"); set(0x97, "bitnot");
        const char* binary[] = {"add", "subtract", "multiply", "divide", "modulo", "lshift", "rshift", "urshift", "bitand",
                                "bitor", "bitxor", "equals", "strictequals", "lessthan", "lessequals", "greaterthan",
                                "greaterequals", "instanceof", "istype", "istypelate", "in"};
        for (int i = 0; i < 21; ++i) set((uint8_t)(0xa0 + i), binary[i]);
        set(0xb2, "istype", kOpMultiname);
        set(0xc0, "increment_i"); set(0xc1, "decrement_i"); set(0xc2, "inclocal_i", kOpU30); set(0xc3, "declocal_i", kOpU30);
        set(0xc4, "negate_i"); set(0xc5, "add_i"); set(0xc6, "subtract_i"); set(0xc7, "multiply_i");
        set(0xd0, "getlocal0"); set(0xd1, "getlocal1"); set(0xd2, "getlocal2"); set(0xd3, "getlocal3");
        set(0xd4, "setlocal0"); set(0xd5, "setlocal1"); set(0xd6, "setlocal2"); set(0xd7, "setlocal3");
        set(0xef, "debug", kOpDebug); set(0xf0, "debugline", kOpU30); set(0xf1, "debugfile", kOpString);
        set(0xf2, "bkptline", kOpU30); set(0xf3, "timestamp");
    }
};

const OpcodeTable& Table()
{
    static const OpcodeTable table;
    return table;
}

}  // namespace

const OpcodeInfo* Opcode(uint8_t op)
{
    const OpcodeInfo& info = Table().entries[op];
    return info.name ? &info : nullptr;
}

std::vector<Instruction> Decode(const Bytes& code)
{
    std::vector<Instruction> out;
    Reader r(code.data(), code.size(), "abc code");
    while (!r.atEnd()) {
        Instruction in;
        in.offset = (uint32_t)r.position();
        in.op = r.u8();
        const OpcodeInfo* info = Opcode(in.op);
        if (!info) r.Fail("unknown opcode " + Reader::Hex(in.op));
        auto operand = [&](OperandKind kind, uint32_t& value) {
            switch (kind) {
            case kOpNone: break;
            case kOpU8: value = r.u8(); break;
            case kOpBranch: { const int32_t delta = r.s24(); in.branch = (int32_t)r.position() + delta; break; }
            case kOpSwitch: {
                // offsets are relative to the lookupswitch's own start
                in.cases.push_back((int32_t)in.offset + r.s24());
                const uint32_t count = r.u30();
                for (uint32_t i = 0; i <= count; ++i) in.cases.push_back((int32_t)in.offset + r.s24());
                break;
            }
            case kOpDebug: in.debugType = r.u8(); in.a = r.u30(); in.debugReg = r.u8(); in.b = r.u30(); break;
            default: value = r.u30(); break;
            }
        };
        operand(info->first, in.a);
        operand(info->second, in.b);
        out.push_back(std::move(in));
    }
    // A branch that doesn't land on an instruction (or the end) is kept as
    // its raw offsets: the game's compiler leaves such jumps in dead code
    // (cmn_syuren.iggy method 6: a jump past the end after a lookupswitch's
    // returnvalue cases), which the VM's verifier never reaches.
    std::vector<bool> starts(code.size() + 1, false);
    for (const Instruction& in : out) starts[in.offset] = true;
    starts[code.size()] = true;
    auto lands = [&](int32_t target) { return target >= 0 && target <= (int32_t)code.size() && starts[target]; };
    for (Instruction& in : out) {
        const OpcodeInfo* info = Opcode(in.op);
        if (info->first == kOpBranch && !lands(in.branch)) in.unreachableTarget = true;
        for (int32_t c : in.cases)
            if (!lands(c)) in.unreachableTarget = true;
    }
    return out;
}

Bytes Encode(const std::vector<Instruction>& code, size_t oldCodeSize, std::vector<std::pair<uint32_t, uint32_t>>* newOffsets)
{
    // Sizes are known up front: only u30 operands vary, branches are fixed s24.
    auto sizeOf = [](const Instruction& in) {
        const OpcodeInfo* info = Opcode(in.op);
        size_t size = 1;
        for (int which = 0; which < 2; ++which) {
            // by position, not kind: hasnext2's two operands are both u30
            const OperandKind kind = which == 0 ? info->first : info->second;
            const uint32_t value = which == 0 ? in.a : in.b;
            switch (kind) {
            case kOpNone: break;
            case kOpU8: size += 1; break;
            case kOpBranch: size += 3; break;
            case kOpSwitch: size += 3 + U30Size((uint32_t)in.cases.size() - 2) + 3 * (in.cases.size() - 1); break;
            case kOpDebug: size += 1 + U30Size(in.a) + 1 + U30Size(in.b); break;
            default: size += U30Size(value); break;
            }
        }
        return size;
    };
    std::map<uint32_t, uint32_t> remap;   // old offset -> new
    uint32_t at = 0;
    for (const Instruction& in : code) { remap[in.offset] = at; at += (uint32_t)sizeOf(in); }
    remap[(uint32_t)oldCodeSize] = at;
    auto target = [&](int32_t old) {
        auto found = remap.find((uint32_t)old);
        if (found == remap.end()) throw FormatError("abc code: branch target not an instruction");
        return (int32_t)found->second;
    };
    Writer w;
    for (const Instruction& in : code) {
        const uint32_t start = (uint32_t)w.size();
        w.u8(in.op);
        const OpcodeInfo* info = Opcode(in.op);
        for (int which = 0; which < 2; ++which) {
            const OperandKind kind = which == 0 ? info->first : info->second;
            const uint32_t value = which == 0 ? in.a : in.b;
            switch (kind) {
            case kOpNone: break;
            case kOpU8: w.u8((uint8_t)value); break;
            case kOpBranch:
                // unreachable targets keep their original distance
                w.s24(in.unreachableTarget ? in.branch - (int32_t)(in.offset + 4) : target(in.branch) - (int32_t)(start + 4));
                break;
            case kOpSwitch: {
                auto relative = [&](int32_t old) { return in.unreachableTarget ? old - (int32_t)in.offset : target(old) - (int32_t)start; };
                w.s24(relative(in.cases[0]));
                w.u30((uint32_t)in.cases.size() - 2);
                for (size_t i = 1; i < in.cases.size(); ++i) w.s24(relative(in.cases[i]));
                break;
            }
            case kOpDebug: w.u8(in.debugType); w.u30(in.a); w.u8(in.debugReg); w.u30(in.b); break;
            default: w.u30(value); break;
            }
        }
    }
    if (newOffsets) newOffsets->assign(remap.begin(), remap.end());
    return w.out;
}

}  // namespace abc
}  // namespace xv2ui

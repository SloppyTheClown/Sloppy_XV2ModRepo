// ABC merge (see abc_merge.h).
#include "abc_merge.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>
#include "diff3.h"

namespace xv2ui {
namespace abc {

namespace {

// Constant kinds of a slot default / optional parameter value (avm2overview 4.5).
enum ValueKind : uint8_t {
    kValueUndefined = 0x00, kValueUtf8 = 0x01, kValueInt = 0x03, kValueUInt = 0x04, kValueDouble = 0x06,
    kValueFalse = 0x0A, kValueTrue = 0x0B, kValueNull = 0x0C,
};
bool IsNamespaceKind(uint8_t kind)
{
    return kind == kNsPrivate || kind == kNsNamespace || kind == kNsPackage || kind == kNsPackageInternal ||
           kind == kNsProtected || kind == kNsExplicit || kind == kNsStaticProtected;
}
bool IsMethodTrait(uint8_t kind) { return kind == kMethodTrait || kind == kGetter || kind == kSetter || kind == kFunction; }
// debug, debugline, debugfile: no effect on what the code does
bool IsDebugOp(uint8_t op) { return op == 0xef || op == 0xf0 || op == 0xf1; }

// ---------------------------------------------------------------- canonical text
// A text form of names, values and methods that is the same in two ABCs when
// they mean the same thing, whatever their indices.
class Canon {
public:
    explicit Canon(const AbcFile& file) : file_(file) {}

    std::string String(uint32_t index) const
    {
        if (index == 0) return "-";
        const std::string& text = file_.String(index);
        return std::to_string(text.size()) + "'" + text + "'";
    }
    std::string Namespace(uint32_t index) const
    {
        if (index == 0 || index >= file_.pool.namespaces.size()) return "*";
        const abc::Namespace& ns = file_.pool.namespaces[index];
        // A private namespace's name means nothing: each is its class's or
        // script's own, and compilers name them differently (the game's
        // CHARASELE.iggy: "", FFDec's rebuild: "action_script:CharaSele").
        // Within one trait list or method the owner is the same, so the kind
        // alone identifies it.
        if (ns.kind == kNsPrivate) return "ns-private";
        return "ns" + std::to_string(ns.kind) + ":" + String(ns.name);
    }
    std::string NsSet(uint32_t index) const
    {
        if (index == 0 || index >= file_.pool.nsSets.size()) return "[]";
        std::string text = "[";
        for (uint32_t ns : file_.pool.nsSets[index]) text += Namespace(ns) + ",";
        return text + "]";
    }
    std::string Multiname(uint32_t index) const
    {
        if (index == 0 || index >= file_.pool.multinames.size()) return "*";
        const abc::Multiname& m = file_.pool.multinames[index];
        std::string text = "mn" + std::to_string(m.kind) + "(";
        switch (m.kind) {
        case kQName: case kQNameA: text += Namespace(m.ns) + "::" + String(m.name); break;
        case kRTQName: case kRTQNameA: text += String(m.name); break;
        case kMultiname: case kMultinameA: text += String(m.name) + NsSet(m.nsSet); break;
        case kMultinameL: case kMultinameLA: text += NsSet(m.nsSet); break;
        case kTypeName:
            text += Multiname(m.base) + "<";
            for (uint32_t p : m.params) text += Multiname(p) + ",";
            text += ">";
            break;
        }
        return text + ")";
    }
    std::string Value(uint8_t kind, uint32_t index) const
    {
        if (index == 0) return "none";
        char number[40];
        switch (kind) {
        case kValueUtf8: return "s" + String(index);
        case kValueInt: return "i" + std::to_string(index < file_.pool.ints.size() ? file_.pool.ints[index] : 0);
        case kValueUInt: return "u" + std::to_string(index < file_.pool.uints.size() ? file_.pool.uints[index] : 0);
        case kValueDouble:
            snprintf(number, sizeof(number), "d%.17g", index < file_.pool.doubles.size() ? file_.pool.doubles[index] : 0.0);
            return number;
        default:
            if (IsNamespaceKind(kind)) return "n" + Namespace(index);
            return "k" + std::to_string(kind);   // undefined / true / false / null: the kind is the value
        }
    }
    std::string ClassName(uint32_t classIndex) const
    {
        return classIndex < file_.instances.size() ? Multiname(file_.instances[classIndex].name) : "?class";
    }

    // The trait's identity within its list: kind group + name.
    std::string TraitKey(const Trait& trait) const
    {
        const uint8_t kind = trait.kind() == kConst ? kSlot : trait.kind();   // slot and const are one group
        return std::to_string(kind) + Multiname(trait.name);
    }
    std::string TraitText(const Trait& trait) const
    {
        std::string text = TraitKey(trait) + " a" + std::to_string(trait.attributes() & ~kAttrMetadata);
        switch (trait.kind()) {
        case kSlot: case kConst:
            text += " k" + std::to_string(trait.kind()) + " " + Multiname(trait.typeName) + "=" + Value(trait.valueKind, trait.valueIndex);
            break;
        case kClassTrait: text += " class " + ClassName(trait.index); break;
        default: text += " " + Method(trait.index); break;
        }
        return text;
    }
    std::string TraitsText(const std::vector<Trait>& traits) const
    {
        std::string text;
        for (const Trait& trait : traits) text += TraitText(trait) + ";";
        return text;
    }

    std::string Method(uint32_t index) const
    {
        auto known = methods_.find(index);
        if (known != methods_.end()) return known->second;
        if (index >= file_.methods.size()) return "?method";
        if (++depth_ > 64) throw FormatError("abc merge: functions nested too deep");
        std::string text = Signature(index);
        const int body = file_.BodyOf(index);
        text += body < 0 ? "native" : Body(file_.bodies[body]);
        --depth_;
        methods_[index] = text;
        return text;
    }
    // Forgets cached method texts (after methods of this file were replaced).
    void Forget() { methods_.clear(); }

    // A method's return type, parameters, flags and defaults.
    std::string Signature(uint32_t index) const
    {
        const abc::Method& method = file_.methods[index];
        std::string text = "fn(" + Multiname(method.returnType) + ":";
        for (uint32_t p : method.paramTypes) text += Multiname(p) + ",";
        text += " f" + std::to_string(method.flags & ~kHasParamNames);
        for (const OptionValue& option : method.options) text += " =" + Value(option.kind, option.value);
        return text + ")";
    }

    // One instruction as text for lining up two versions of a method: like
    // Body's, but a branch without its target (targets are instruction
    // numbers, which an insertion anywhere before them shifts).
    std::string InstructionKey(const Instruction& in) const
    {
        const OpcodeInfo* info = Opcode(in.op);
        std::string text = info->name;
        for (int which = 0; which < 2; ++which) {
            const OperandKind kind = which == 0 ? info->first : info->second;
            if (kind == kOpBranch) continue;
            if (kind == kOpSwitch) { text += " cases" + std::to_string(in.cases.size()); continue; }
            text += " " + Operand(kind, which == 0 ? in.a : in.b);
        }
        return text;
    }

private:
    std::string Body(const MethodBody& body) const
    {
        const std::vector<Instruction> code = Decode(body.code);
        // instruction numbers without the debug ops; a target on a debug op is the next real one
        std::map<uint32_t, size_t> number;
        size_t count = 0;
        for (const Instruction& in : code) {
            number[in.offset] = count;
            if (!IsDebugOp(in.op)) ++count;
        }
        number[(uint32_t)body.code.size()] = count;
        auto label = [&](int64_t offset) {
            auto found = number.find((uint32_t)offset);
            return "L" + (found == number.end() ? std::string("?") : std::to_string(found->second));
        };
        std::string text = "{";
        for (const Instruction& in : code) {
            if (IsDebugOp(in.op)) continue;
            const OpcodeInfo* info = Opcode(in.op);
            text += info->name;
            for (int which = 0; which < 2; ++which) {
                const OperandKind kind = which == 0 ? info->first : info->second;
                text += " ";
                if (kind == kOpBranch) text += label(in.branch);
                else if (kind == kOpSwitch) for (int32_t target : in.cases) text += label(target) + ",";
                else text += Operand(kind, which == 0 ? in.a : in.b);
            }
            text += ";";
        }
        for (const ExceptionInfo& e : body.exceptions)
            text += "catch " + label(e.from) + "-" + label(e.to) + ">" + label(e.target) + " " + Multiname(e.excType) + " " +
                    Multiname(e.varName) + ";";
        return text + TraitsText(body.traits) + "}";
    }

    // An operand's value as text (not branches: see Body / InstructionKey).
    std::string Operand(OperandKind kind, uint32_t value) const
    {
        switch (kind) {
        case kOpNone: return "";
        case kOpString: return String(value);
        case kOpInt: return Value(kValueInt, value);
        case kOpUInt: return Value(kValueUInt, value);
        case kOpDouble: return Value(kValueDouble, value);
        case kOpNamespace: return Namespace(value);
        case kOpMultiname: return Multiname(value);
        case kOpMethod: return Method(value);
        case kOpClass: return ClassName(value);
        default: return std::to_string(value);
        }
    }

    const AbcFile& file_;
    mutable std::map<uint32_t, std::string> methods_;
    mutable int depth_ = 0;
};

// ---------------------------------------------------------------- importing
// Copies names, values, methods and traits of one ABC (`from`, the mod's)
// into another (`to`, the current script), reusing equal pool entries.
class Importer {
public:
    Importer(const AbcFile& from, AbcFile& to, std::map<std::string, uint32_t>& toClasses)
        : from_(from), to_(to), fromCanon_(from), toClasses_(toClasses)
    {
        const ConstantPool& pool = to_.pool;
        for (size_t i = 1; i < pool.strings.size(); ++i) strings_.emplace(pool.strings[i], (uint32_t)i);
        for (size_t i = 1; i < pool.ints.size(); ++i) ints_.emplace(pool.ints[i], (uint32_t)i);
        for (size_t i = 1; i < pool.uints.size(); ++i) uints_.emplace(pool.uints[i], (uint32_t)i);
        for (size_t i = 1; i < pool.doubles.size(); ++i) doubles_.emplace(Bits(pool.doubles[i]), (uint32_t)i);
        for (size_t i = 1; i < pool.namespaces.size(); ++i)
            if (pool.namespaces[i].kind != kNsPrivate) namespaces_.emplace(NamespaceKey(pool.namespaces[i]), (uint32_t)i);
        for (size_t i = 1; i < pool.nsSets.size(); ++i) nsSets_.emplace(pool.nsSets[i], (uint32_t)i);
        for (size_t i = 1; i < pool.multinames.size(); ++i) multinames_.emplace(MultinameKey(pool.multinames[i]), (uint32_t)i);
    }

    // A private namespace of `from` that is the same as one of `to` (found by
    // matching the traits of a class both have).
    void MapPrivate(uint32_t fromNs, uint32_t toNs) { privates_.emplace(fromNs, toNs); }

    uint32_t String(uint32_t index)
    {
        if (index == 0) return 0;
        const std::string& text = from_.String(index);
        auto found = strings_.find(text);
        if (found != strings_.end()) return found->second;
        to_.pool.strings.push_back(text);
        return strings_[text] = (uint32_t)to_.pool.strings.size() - 1;
    }
    uint32_t Int(uint32_t index)
    {
        if (index == 0 || index >= from_.pool.ints.size()) return 0;
        const int32_t value = from_.pool.ints[index];
        auto found = ints_.find(value);
        if (found != ints_.end()) return found->second;
        to_.pool.ints.push_back(value);
        return ints_[value] = (uint32_t)to_.pool.ints.size() - 1;
    }
    uint32_t UInt(uint32_t index)
    {
        if (index == 0 || index >= from_.pool.uints.size()) return 0;
        const uint32_t value = from_.pool.uints[index];
        auto found = uints_.find(value);
        if (found != uints_.end()) return found->second;
        to_.pool.uints.push_back(value);
        return uints_[value] = (uint32_t)to_.pool.uints.size() - 1;
    }
    uint32_t Double(uint32_t index)
    {
        if (index == 0 || index >= from_.pool.doubles.size()) return 0;
        const double value = from_.pool.doubles[index];
        auto found = doubles_.find(Bits(value));
        if (found != doubles_.end()) return found->second;
        to_.pool.doubles.push_back(value);
        return doubles_[Bits(value)] = (uint32_t)to_.pool.doubles.size() - 1;
    }
    uint32_t Namespace(uint32_t index)
    {
        if (index == 0 || index >= from_.pool.namespaces.size()) return 0;
        const abc::Namespace& source = from_.pool.namespaces[index];
        abc::Namespace ns{source.kind, String(source.name)};
        if (ns.kind == kNsPrivate) {
            auto mapped = privates_.find(index);
            if (mapped != privates_.end()) return mapped->second;
            // each private namespace is its own; unmatched ones are new
            to_.pool.namespaces.push_back(ns);
            return privates_[index] = (uint32_t)to_.pool.namespaces.size() - 1;
        }
        const std::string key = NamespaceKey(ns);
        auto found = namespaces_.find(key);
        if (found != namespaces_.end()) return found->second;
        to_.pool.namespaces.push_back(ns);
        return namespaces_[key] = (uint32_t)to_.pool.namespaces.size() - 1;
    }
    uint32_t NsSet(uint32_t index)
    {
        if (index == 0 || index >= from_.pool.nsSets.size()) return 0;
        std::vector<uint32_t> set;
        for (uint32_t ns : from_.pool.nsSets[index]) set.push_back(Namespace(ns));
        auto found = nsSets_.find(set);
        if (found != nsSets_.end()) return found->second;
        to_.pool.nsSets.push_back(set);
        return nsSets_[set] = (uint32_t)to_.pool.nsSets.size() - 1;
    }
    uint32_t Multiname(uint32_t index)
    {
        if (index == 0 || index >= from_.pool.multinames.size()) return 0;
        const abc::Multiname& source = from_.pool.multinames[index];
        abc::Multiname m;
        m.kind = source.kind;
        m.ns = Namespace(source.ns);
        m.name = String(source.name);
        m.nsSet = NsSet(source.nsSet);
        m.base = Multiname(source.base);
        for (uint32_t p : source.params) m.params.push_back(Multiname(p));
        const std::string key = MultinameKey(m);
        auto found = multinames_.find(key);
        if (found != multinames_.end()) return found->second;
        to_.pool.multinames.push_back(m);
        return multinames_[key] = (uint32_t)to_.pool.multinames.size() - 1;
    }
    uint32_t Value(uint8_t kind, uint32_t index)
    {
        if (index == 0) return 0;
        switch (kind) {
        case kValueUtf8: return String(index);
        case kValueInt: return Int(index);
        case kValueUInt: return UInt(index);
        case kValueDouble: return Double(index);
        default: return IsNamespaceKind(kind) ? Namespace(index) : index;
        }
    }
    uint32_t ClassIndex(uint32_t fromClass)
    {
        const std::string name = fromCanon_.ClassName(fromClass);
        auto found = toClasses_.find(name);
        if (found == toClasses_.end()) throw FormatError("abc merge: class " + from_.ClassName(fromClass) + " is not in the script");
        return found->second;
    }

    // `from` method as a new method of `to` (with its body).
    uint32_t NewMethod(uint32_t fromMethod)
    {
        const uint32_t index = (uint32_t)to_.methods.size();
        to_.methods.emplace_back();
        ReplaceMethod(index, fromMethod);
        return index;
    }
    // Overwrites `to` method `toMethod` with `from` method `fromMethod`.
    void ReplaceMethod(uint32_t toMethod, uint32_t fromMethod)
    {
        if (fromMethod >= from_.methods.size()) throw FormatError("abc merge: method index out of range");
        abc::Method method = Signature(from_.methods[fromMethod]);
        to_.methods[toMethod] = std::move(method);
        const int fromBody = from_.BodyOf(fromMethod);
        if (fromBody < 0) return;   // native: no body to copy
        MethodBody body = Body(from_.bodies[fromBody], toMethod);   // may add methods and bodies
        const int toBody = to_.BodyOf(toMethod);
        if (toBody >= 0) to_.bodies[toBody] = std::move(body);
        else to_.bodies.push_back(std::move(body));
    }

    Trait TranslateTrait(const Trait& source)
    {
        Trait trait = source;
        trait.name = Multiname(source.name);
        switch (source.kind()) {
        case kSlot: case kConst:
            trait.typeName = Multiname(source.typeName);
            trait.valueIndex = Value(source.valueKind, source.valueIndex);
            break;
        case kClassTrait: trait.index = ClassIndex(source.index); break;
        default: trait.index = NewMethod(source.index); break;
        }
        trait.metadata.clear();
        for (uint32_t m : source.metadata) trait.metadata.push_back(Metadata(m));
        return trait;
    }
    // An instruction of `from` with its operands in `to`'s pools (branch
    // targets untouched).
    Instruction TranslateInstruction(Instruction in)
    {
        const OpcodeInfo* info = Opcode(in.op);
        if (info->first == kOpDebug) { in.a = String(in.a); return in; }
        in.a = Operand(info->first, in.a);
        in.b = Operand(info->second, in.b);
        return in;
    }

    std::vector<Trait> TranslateTraits(const std::vector<Trait>& traits)
    {
        std::vector<Trait> out;
        for (const Trait& trait : traits) out.push_back(TranslateTrait(trait));
        return out;
    }

private:
    static uint64_t Bits(double value) { uint64_t bits; memcpy(&bits, &value, 8); return bits; }
    static std::string NamespaceKey(const abc::Namespace& ns) { return std::to_string(ns.kind) + "," + std::to_string(ns.name); }
    static std::string MultinameKey(const abc::Multiname& m)
    {
        std::string key = std::to_string(m.kind) + "," + std::to_string(m.ns) + "," + std::to_string(m.name) + "," +
                          std::to_string(m.nsSet) + "," + std::to_string(m.base);
        for (uint32_t p : m.params) key += "," + std::to_string(p);
        return key;
    }

    uint32_t Metadata(uint32_t index)
    {
        if (index >= from_.metadata.size()) throw FormatError("abc merge: metadata index out of range");
        const abc::Metadata& source = from_.metadata[index];
        abc::Metadata md{String(source.name), {}};
        for (const MetadataItem& item : source.items) md.items.push_back({String(item.key), String(item.value)});
        to_.metadata.push_back(md);
        return (uint32_t)to_.metadata.size() - 1;
    }

    abc::Method Signature(const abc::Method& source)
    {
        abc::Method method;
        method.returnType = Multiname(source.returnType);
        for (uint32_t p : source.paramTypes) method.paramTypes.push_back(Multiname(p));
        method.name = String(source.name);
        method.flags = source.flags;
        for (const OptionValue& option : source.options) method.options.push_back({Value(option.kind, option.value), option.kind});
        for (uint32_t p : source.paramNames) method.paramNames.push_back(String(p));
        return method;
    }

    MethodBody Body(const MethodBody& source, uint32_t toMethod)
    {
        MethodBody body;
        body.method = toMethod;
        body.maxStack = source.maxStack;
        body.localCount = source.localCount;
        body.initScopeDepth = source.initScopeDepth;
        body.maxScopeDepth = source.maxScopeDepth;
        std::vector<Instruction> code = Decode(source.code);
        for (Instruction& in : code) in = TranslateInstruction(in);
        std::vector<std::pair<uint32_t, uint32_t>> offsets;
        body.code = Encode(code, source.code.size(), &offsets);
        const std::map<uint32_t, uint32_t> moved(offsets.begin(), offsets.end());
        auto move = [&](uint32_t offset) {
            auto found = moved.find(offset);
            if (found == moved.end()) throw FormatError("abc merge: exception range not on an instruction");
            return found->second;
        };
        for (const ExceptionInfo& e : source.exceptions)
            body.exceptions.push_back({move(e.from), move(e.to), move(e.target), Multiname(e.excType), Multiname(e.varName)});
        body.traits = TranslateTraits(source.traits);
        return body;
    }

    uint32_t Operand(OperandKind kind, uint32_t value)
    {
        switch (kind) {
        case kOpString: return String(value);
        case kOpInt: return Int(value);
        case kOpUInt: return UInt(value);
        case kOpDouble: return Double(value);
        case kOpNamespace: return Namespace(value);
        case kOpMultiname: return Multiname(value);
        case kOpMethod: return NewMethod(value);
        case kOpClass: return ClassIndex(value);
        default: return value;   // counts, registers, slots, exception numbers
        }
    }

    const AbcFile& from_;
    AbcFile& to_;
    Canon fromCanon_;
    std::map<std::string, uint32_t>& toClasses_;
    std::unordered_map<std::string, uint32_t> strings_, namespaces_, multinames_;
    std::map<int32_t, uint32_t> ints_;
    std::map<uint32_t, uint32_t> uints_;
    std::map<uint64_t, uint32_t> doubles_;
    std::map<std::vector<uint32_t>, uint32_t> nsSets_;
    std::map<uint32_t, uint32_t> privates_;
};

// ---------------------------------------------------------------- merging

std::map<std::string, uint32_t> ClassesByName(const AbcFile& file, const Canon& canon)
{
    std::map<std::string, uint32_t> classes;
    for (size_t i = 0; i < file.instances.size(); ++i) classes.emplace(canon.ClassName((uint32_t)i), (uint32_t)i);
    return classes;
}

const Trait* FindTrait(const std::vector<Trait>& traits, const Canon& canon, const std::string& key)
{
    for (const Trait& trait : traits)
        if (canon.TraitKey(trait) == key) return &trait;
    return nullptr;
}

// The script of `file` that defines `traitKey`, or -1.
int ScriptDefining(const AbcFile& file, const Canon& canon, const std::string& traitKey)
{
    for (size_t i = 0; i < file.scripts.size(); ++i)
        if (FindTrait(file.scripts[i].traits, canon, traitKey)) return (int)i;
    return -1;
}

// A mod's script matched to the base's / current one: by any name both define.
int MatchScript(const Script& script, const Canon& scriptCanon, const AbcFile& other, const Canon& otherCanon)
{
    for (const Trait& trait : script.traits) {
        const int found = ScriptDefining(other, otherCanon, scriptCanon.TraitKey(trait));
        if (found >= 0) return found;
    }
    return -1;
}

// A method body's code without its debug ops, plus where each offset lands
// (a debug op's offset on the next real instruction, the end on the size).
struct RealCode {
    std::vector<Instruction> instructions;
    std::map<int32_t, size_t> indexOf;
};
bool StripDebug(const Bytes& code, RealCode& out)
{
    const std::vector<Instruction> decoded = Decode(code);
    std::vector<int32_t> pending;   // offsets of debug ops waiting for the next instruction
    for (const Instruction& in : decoded) {
        if (in.unreachableTarget) return false;   // raw offsets can't be moved
        if (IsDebugOp(in.op)) { pending.push_back((int32_t)in.offset); continue; }
        for (int32_t offset : pending) out.indexOf[offset] = out.instructions.size();
        pending.clear();
        out.indexOf[(int32_t)in.offset] = out.instructions.size();
        out.instructions.push_back(in);
    }
    for (int32_t offset : pending) out.indexOf[offset] = out.instructions.size();
    out.indexOf[(int32_t)code.size()] = out.instructions.size();
    return true;
}

// Moves local registers from `first` up by `shift` (the mod's new locals past
// the ones an earlier change added). getlocal0..3 / setlocal0..3 name their
// register in the opcode; a moved one becomes the general getlocal / setlocal.
void MoveRegisters(Instruction& in, uint32_t first, uint32_t shift)
{
    auto move = [&](uint32_t& reg) { if (reg >= first) reg += shift; };
    switch (in.op) {
    case 0x08: case 0x62: case 0x63: case 0x92: case 0x94: case 0xc2: case 0xc3:   // kill get/setlocal inc/declocal(_i)
        move(in.a);
        break;
    case 0x32:   // hasnext2 object register, index register
        move(in.a);
        move(in.b);
        break;
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7: {
        const bool isGet = in.op <= 0xd3;
        uint32_t reg = (uint32_t)(in.op - (isGet ? 0xd0 : 0xd4));
        if (reg < first) break;
        move(reg);
        in.op = isGet ? 0x62 : 0x63;
        in.a = reg;
        break;
    }
    default: break;
    }
}

class Merger {
public:
    // game: the game's own script; base: what the mod was made from (the
    // game's script, or the same rebuilt by the mod's compiler).
    Merger(const AbcFile& game, const AbcFile& base, AbcFile& current, const AbcFile& mod, MergeReport& report)
        : game_(game), base_(base), current_(current), mod_(mod), report_(report), gameCanon_(game), baseCanon_(base),
          currentCanon_(current), modCanon_(mod), gameClasses_(ClassesByName(game, gameCanon_)),
          baseClasses_(ClassesByName(base, baseCanon_)), currentClasses_(ClassesByName(current, currentCanon_)),
          importer_(mod, current, currentClasses_)
    {
    }

    void Run()
    {
        MapPrivateNamespaces();
        ImportNewClasses();
        for (size_t i = 0; i < mod_.instances.size(); ++i) MergeClass((uint32_t)i);
        for (size_t i = 0; i < mod_.scripts.size(); ++i) MergeScriptEntry(i);
    }

private:
    // Private namespaces of classes all three have, paired through their traits.
    void MapPrivateNamespaces()
    {
        for (size_t i = 0; i < mod_.instances.size(); ++i) {
            auto current = currentClasses_.find(modCanon_.ClassName((uint32_t)i));
            if (current == currentClasses_.end()) continue;
            PairPrivates(mod_.instances[i].traits, current_.instances[current->second].traits);
            PairPrivates(mod_.classes[i].traits, current_.classes[current->second].traits);
        }
        for (const Script& script : mod_.scripts) {
            const int current = MatchScript(script, modCanon_, current_, currentCanon_);
            if (current >= 0) PairPrivates(script.traits, current_.scripts[current].traits);
        }
    }
    void PairPrivates(const std::vector<Trait>& modTraits, const std::vector<Trait>& currentTraits)
    {
        for (const Trait& modTrait : modTraits) {
            const uint32_t modNs = PrivateNamespaceOf(mod_, modTrait);
            if (!modNs) continue;
            const std::string& name = mod_.String(mod_.pool.multinames[modTrait.name].name);
            for (const Trait& currentTrait : currentTraits) {
                const uint32_t currentNs = PrivateNamespaceOf(current_, currentTrait);
                if (currentNs && currentTrait.kind() == modTrait.kind() &&
                    current_.String(current_.pool.multinames[currentTrait.name].name) == name) {
                    importer_.MapPrivate(modNs, currentNs);
                    break;
                }
            }
        }
    }
    static uint32_t PrivateNamespaceOf(const AbcFile& file, const Trait& trait)
    {
        if (trait.name == 0 || trait.name >= file.pool.multinames.size()) return 0;
        const Multiname& m = file.pool.multinames[trait.name];
        if ((m.kind != kQName && m.kind != kQNameA) || m.ns == 0 || m.ns >= file.pool.namespaces.size()) return 0;
        return file.pool.namespaces[m.ns].kind == kNsPrivate ? m.ns : 0;
    }

    // Classes only the mod has: placeholders first (so classes and code can
    // name each other), then filled.
    void ImportNewClasses()
    {
        std::vector<std::pair<uint32_t, uint32_t>> added;   // mod class -> current class
        for (size_t i = 0; i < mod_.instances.size(); ++i) {
            const std::string name = modCanon_.ClassName((uint32_t)i);
            if (baseClasses_.count(name)) continue;
            if (currentClasses_.count(name)) {
                Note("class " + mod_.ClassName(i) + " was already added by an earlier change - kept that one");
                continue;
            }
            const uint32_t index = (uint32_t)current_.instances.size();
            current_.instances.emplace_back();
            current_.classes.emplace_back();
            currentClasses_[name] = index;
            added.emplace_back((uint32_t)i, index);
            ++report_.classesAdded;
        }
        for (const auto& pair : added) {
            const Instance& source = mod_.instances[pair.first];
            Instance instance;
            instance.name = importer_.Multiname(source.name);
            instance.superName = importer_.Multiname(source.superName);
            instance.flags = source.flags;
            instance.protectedNs = importer_.Namespace(source.protectedNs);
            for (uint32_t i : source.interfaces) instance.interfaces.push_back(importer_.Multiname(i));
            instance.iinit = importer_.NewMethod(source.iinit);
            instance.traits = importer_.TranslateTraits(source.traits);
            Class klass;
            klass.cinit = importer_.NewMethod(mod_.classes[pair.first].cinit);
            klass.traits = importer_.TranslateTraits(mod_.classes[pair.first].traits);
            current_.instances[pair.second] = std::move(instance);
            current_.classes[pair.second] = std::move(klass);
            skipClasses_.insert(pair.first);
        }
        if (!added.empty()) currentCanon_.Forget();
    }

    void MergeClass(uint32_t modClass)
    {
        if (skipClasses_.count(modClass)) return;
        const std::string name = modCanon_.ClassName(modClass);
        auto base = baseClasses_.find(name);
        if (base == baseClasses_.end()) return;   // new: imported (or skipped) above
        auto current = currentClasses_.find(name);
        const std::string label = "class " + mod_.ClassName(modClass);
        if (current == currentClasses_.end()) {
            if (Differs(base->second, modClass)) Note(label + " changed but is gone from the current script - skipped");
            return;
        }
        const Instance& modInstance = mod_.instances[modClass];
        const Instance& baseInstance = base_.instances[base->second];
        if (modCanon_.Multiname(modInstance.superName) != baseCanon_.Multiname(baseInstance.superName) ||
            modInstance.flags != baseInstance.flags || modInstance.interfaces.size() != baseInstance.interfaces.size())
            Note(label + ": a changed superclass / flags / interfaces is not merged");

        // the game's own class: what "no earlier change" looks like
        auto game = gameClasses_.find(name);
        const bool inGame = game != gameClasses_.end();
        const std::string gameIinit = inGame ? gameCanon_.Method(game_.instances[game->second].iinit) : baseCanon_.Method(baseInstance.iinit);
        const std::string gameCinit = inGame ? gameCanon_.Method(game_.classes[game->second].cinit)
                                             : baseCanon_.Method(base_.classes[base->second].cinit);
        MergeMethod(label + " constructor", gameIinit, baseInstance.iinit, modInstance.iinit, current_.instances[current->second].iinit);
        MergeMethod(label + " static init", gameCinit, base_.classes[base->second].cinit, mod_.classes[modClass].cinit,
                    current_.classes[current->second].cinit);
        MergeTraits(label, inGame ? &game_.instances[game->second].traits : nullptr, base_.instances[base->second].traits,
                    modInstance.traits, current->second, false);
        MergeTraits(label + " (static)", inGame ? &game_.classes[game->second].traits : nullptr, base_.classes[base->second].traits,
                    mod_.classes[modClass].traits, current->second, true);
    }

    bool Differs(uint32_t baseClass, uint32_t modClass) const
    {
        return baseCanon_.Method(base_.instances[baseClass].iinit) != modCanon_.Method(mod_.instances[modClass].iinit) ||
               baseCanon_.Method(base_.classes[baseClass].cinit) != modCanon_.Method(mod_.classes[modClass].cinit) ||
               baseCanon_.TraitsText(base_.instances[baseClass].traits) != modCanon_.TraitsText(mod_.instances[modClass].traits) ||
               baseCanon_.TraitsText(base_.classes[baseClass].traits) != modCanon_.TraitsText(mod_.classes[modClass].traits);
    }

    // gameText: the method as the game has it (to see whether an earlier
    // change edited the current one too).
    void MergeMethod(const std::string& label, const std::string& gameText, uint32_t baseMethod, uint32_t modMethod,
                     uint32_t currentMethod)
    {
        const std::string modText = modCanon_.Method(modMethod);
        if (modText == baseCanon_.Method(baseMethod)) return;   // the mod didn't change it
        const std::string currentText = currentCanon_.Method(currentMethod);
        if (currentText == modText) return;                     // an earlier change did the same
        if (currentText != gameText) {
            if (MergeCode(baseMethod, modMethod, currentMethod)) {
                Merged(label);
                return;
            }
            Conflict(label);
        }
        importer_.ReplaceMethod(currentMethod, modMethod);
        currentCanon_.Forget();
        ++report_.methodsChanged;
    }

    // Both this mod and an earlier change edited the method: puts the mod's
    // edit (base -> mod) into the current code, instruction by instruction
    // (diff3.h), keeping the earlier edit. False (nothing changed) when the
    // edits overlap or the method has what this doesn't merge: a changed
    // signature or scope depth, try blocks, activation traits.
    bool MergeCode(uint32_t baseMethod, uint32_t modMethod, uint32_t currentMethod)
    {
        if (baseCanon_.Signature(baseMethod) != modCanon_.Signature(modMethod)) return false;
        const int baseBody = base_.BodyOf(baseMethod), modBody = mod_.BodyOf(modMethod), currentBody = current_.BodyOf(currentMethod);
        if (baseBody < 0 || modBody < 0 || currentBody < 0) return false;
        const MethodBody& base = base_.bodies[baseBody];
        const MethodBody& mod = mod_.bodies[modBody];
        MethodBody& current = current_.bodies[currentBody];
        if (!base.exceptions.empty() || !mod.exceptions.empty() || !current.exceptions.empty() || !base.traits.empty() ||
            !mod.traits.empty() || !current.traits.empty() || base.initScopeDepth != mod.initScopeDepth)
            return false;
        RealCode baseCode, modCode, currentCode;
        if (!StripDebug(base.code, baseCode) || !StripDebug(mod.code, modCode) || !StripDebug(current.code, currentCode)) return false;

        // registers the mod added go past the ones the earlier change added
        const uint32_t shift = current.localCount > base.localCount ? current.localCount - base.localCount : 0;
        if (shift)
            for (Instruction& in : modCode.instructions) MoveRegisters(in, base.localCount, shift);

        auto keys = [](const Canon& canon, const RealCode& code) {
            std::vector<std::string> lines;
            for (const Instruction& in : code.instructions) lines.push_back(canon.InstructionKey(in));
            return lines;
        };
        std::vector<Diff3Pick> picks;
        std::vector<size_t> modPosition, currentPosition;
        if (!Diff3(keys(baseCanon_, baseCode), keys(modCanon_, modCode), keys(currentCanon_, currentCode), picks, modPosition,
                   currentPosition, &bothInserted_))
            return false;

        // branch targets: old offset -> instruction index -> place in the result
        auto place = [&](bool fromMod, int32_t target, int32_t& placed) {
            const RealCode& code = fromMod ? modCode : currentCode;
            auto found = code.indexOf.find(target);
            if (found == code.indexOf.end()) return false;
            const size_t position = (fromMod ? modPosition : currentPosition)[found->second];
            if (position == SIZE_MAX) return false;
            placed = (int32_t)position;
            return true;
        };
        std::vector<Instruction> merged;
        for (const Diff3Pick& pick : picks) {
            Instruction in = (pick.fromTheirs ? modCode : currentCode).instructions[pick.index];
            if (Opcode(in.op)->first == kOpBranch && !place(pick.fromTheirs, in.branch, in.branch)) return false;
            for (int32_t& target : in.cases)
                if (!place(pick.fromTheirs, target, target)) return false;
            in.offset = (uint32_t)merged.size();   // Encode maps targets by these
            merged.push_back(in);
        }
        // only now import the mod's operands (it may add pool entries and functions)
        for (size_t i = 0; i < picks.size(); ++i)
            if (picks[i].fromTheirs) {
                const Instruction translated = importer_.TranslateInstruction(merged[i]);
                merged[i].a = translated.a;
                merged[i].b = translated.b;
            }
        current.code = Encode(merged, merged.size(), nullptr);
        current.maxStack = std::max(current.maxStack, mod.maxStack);
        current.localCount = std::max(current.localCount, mod.localCount + shift);
        current.maxScopeDepth = std::max(current.maxScopeDepth, mod.maxScopeDepth);
        currentCanon_.Forget();
        return true;
    }

    // The trait list of a current class (instance or static) or script.
    std::vector<Trait>& CurrentTraits(uint32_t owner, bool isStatic, bool isScript)
    {
        if (isScript) return current_.scripts[owner].traits;
        return isStatic ? current_.classes[owner].traits : current_.instances[owner].traits;
    }

    // gameTraits: the same list in the game's script (null when it has none).
    void MergeTraits(const std::string& label, const std::vector<Trait>* gameTraits, const std::vector<Trait>& baseTraits,
                     const std::vector<Trait>& modTraits, uint32_t owner, bool isStatic, bool isScript = false)
    {
        for (const Trait& modTrait : modTraits) {
            const std::string key = modCanon_.TraitKey(modTrait);
            const std::string what = label + " " + mod_.MultinameText(modTrait.name);
            const Trait* baseTrait = FindTrait(baseTraits, baseCanon_, key);
            const std::string modText = modCanon_.TraitText(modTrait);
            if (baseTrait && baseCanon_.TraitText(*baseTrait) == modText) continue;   // unchanged

            std::vector<Trait>& currentTraits = CurrentTraits(owner, isStatic, isScript);
            int currentIndex = -1;
            for (size_t i = 0; i < currentTraits.size(); ++i)
                if (currentCanon_.TraitKey(currentTraits[i]) == key) { currentIndex = (int)i; break; }

            if (currentIndex >= 0) {
                const Trait currentTrait = currentTraits[currentIndex];
                const std::string currentText = currentCanon_.TraitText(currentTrait);
                if (currentText == modText) continue;   // an earlier change did the same
                const Trait* gameTrait = gameTraits ? FindTrait(*gameTraits, gameCanon_, key) : nullptr;
                const std::string gameText = gameTrait ? gameCanon_.TraitText(*gameTrait) : baseTrait ? baseCanon_.TraitText(*baseTrait) : "";
                if (!baseTrait || currentText != gameText) {
                    // both edited the same method: merge the code when only the code changed
                    if (baseTrait && IsMethodTrait(modTrait.kind()) && baseTrait->kind() == modTrait.kind() &&
                        currentTrait.kind() == modTrait.kind() && baseTrait->attributes() == modTrait.attributes() &&
                        MergeCode(baseTrait->index, modTrait.index, currentTrait.index)) {
                        Merged(what);
                        continue;
                    }
                    Conflict(what);
                }
                ReplaceTrait(owner, isStatic, isScript, currentIndex, modTrait);
                ++report_.methodsChanged;
            } else {
                if (baseTrait) Note(what + " changed but is gone from the current script - added back");
                Trait trait = importer_.TranslateTrait(modTrait);
                std::vector<Trait>& traits = CurrentTraits(owner, isStatic, isScript);   // importing may have grown nothing here, but re-fetch
                trait.slotId = FreeId(traits, trait);
                traits.push_back(std::move(trait));
                ++report_.traitsAdded;
            }
            currentCanon_.Forget();
        }
        for (const Trait& baseTrait : baseTraits)
            if (!FindTrait(modTraits, modCanon_, baseCanon_.TraitKey(baseTrait)))
                Note(label + " " + base_.MultinameText(baseTrait.name) + " was removed by this change - kept");
    }

    void ReplaceTrait(uint32_t owner, bool isStatic, bool isScript, int currentIndex, const Trait& modTrait)
    {
        Trait existing = CurrentTraits(owner, isStatic, isScript)[currentIndex];
        if (IsMethodTrait(modTrait.kind()) && IsMethodTrait(existing.kind())) {
            importer_.ReplaceMethod(existing.index, modTrait.index);
            existing.kindByte = (uint8_t)((existing.kindByte & 0xf0 & (kAttrMetadata << 4)) | (modTrait.kindByte & ~(kAttrMetadata << 4)));
        } else {
            Trait trait = importer_.TranslateTrait(modTrait);
            trait.slotId = existing.slotId;   // code may address it by slot number
            existing = std::move(trait);
        }
        CurrentTraits(owner, isStatic, isScript)[currentIndex] = std::move(existing);
    }

    // Slots, consts, classes and functions share one slot id space; methods,
    // getters and setters share the disp id space.
    static bool IsSlotTrait(uint8_t kind) { return kind == kSlot || kind == kConst || kind == kClassTrait || kind == kFunction; }

    // The slot / disp id for `added` going into `traits`. Iggy needs them
    // unique and numbered when the list numbers its own: the game's compiler
    // numbers static traits (1..n) and leaves instance traits at 0. An id of
    // 0 among numbered ones, or the mod's own id when it collides with
    // another mod's addition, is the likely cause of a blank CHARASELE (2026-10-19:
    // Battle Styles' and Chara Search's added static methods both had disp
    // ids 3.., their consts 0). So: 0 when the list uses no ids, else the
    // next one past the highest.
    static uint32_t FreeId(const std::vector<Trait>& traits, const Trait& added)
    {
        const bool slotSpace = IsSlotTrait(added.kind());
        uint32_t highest = 0;
        for (const Trait& trait : traits)
            if (IsSlotTrait(trait.kind()) == slotSpace) highest = std::max(highest, trait.slotId);
        return highest ? highest + 1 : 0;
    }

    void MergeScriptEntry(size_t modScript)
    {
        const Script& script = mod_.scripts[modScript];
        const int base = MatchScript(script, modCanon_, base_, baseCanon_);
        if (base < 0) {
            if (MatchScript(script, modCanon_, current_, currentCanon_) >= 0) {
                Note("a new script of this change defines names an earlier change added - skipped");
                return;
            }
            Script added;
            added.init = importer_.NewMethod(script.init);
            added.traits = importer_.TranslateTraits(script.traits);
            // the last script is the entry point: new ones go before it
            current_.scripts.insert(current_.scripts.empty() ? current_.scripts.end() : current_.scripts.end() - 1, std::move(added));
            currentCanon_.Forget();
            ++report_.scriptsAdded;
            return;
        }
        const int current = MatchScript(script, modCanon_, current_, currentCanon_);
        if (current < 0) { Note("a changed script is gone from the current script - skipped"); return; }
        const int game = MatchScript(script, modCanon_, game_, gameCanon_);
        const std::string gameInit = game >= 0 ? gameCanon_.Method(game_.scripts[game].init) : baseCanon_.Method(base_.scripts[base].init);
        MergeMethod("script init", gameInit, base_.scripts[base].init, script.init, current_.scripts[current].init);
        MergeTraits("script", game >= 0 ? &game_.scripts[game].traits : nullptr, base_.scripts[base].traits, script.traits,
                    (uint32_t)current, false, true);
    }

    void Note(const std::string& line) { report_.notes.push_back(line); }
    void Conflict(const std::string& what)
    {
        ++report_.conflicts;
        Note(what + ": also changed by an earlier change - this one wins");
    }
    void Merged(const std::string& what)
    {
        ++report_.methodsChanged;
        ++report_.methodsMerged;
        Note(what + ": also changed by an earlier change - both edits merged" +
             (bothInserted_ ? std::string(" (both added code at the same place: this change's runs after)") : std::string()));
    }

    const AbcFile& game_;
    const AbcFile& base_;
    AbcFile& current_;
    const AbcFile& mod_;
    MergeReport& report_;
    Canon gameCanon_, baseCanon_, currentCanon_, modCanon_;
    std::map<std::string, uint32_t> gameClasses_, baseClasses_, currentClasses_;
    Importer importer_;
    std::set<uint32_t> skipClasses_;   // mod classes imported whole
    bool bothInserted_ = false;        // the last MergeCode kept two inserts at one place
};

}  // namespace

std::string MergeReport::Summary() const
{
    if (!Changed() && notes.empty()) return "no changes";
    return std::to_string(methodsChanged) + " changed (" + std::to_string(methodsMerged) + " merged with an earlier edit), " +
           std::to_string(traitsAdded) + " traits added, " + std::to_string(classesAdded) + " classes added, " +
           std::to_string(scriptsAdded) + " scripts added, " + std::to_string(conflicts) + " conflicts";
}

Bytes MergeScript(const Bytes& game, const Bytes& current, const Bytes& mod, MergeReport* report, const Bytes* modBase)
{
    AbcFile gameFile, baseFile, currentFile, modFile;
    gameFile.Parse(game.data(), game.size());
    if (modBase) baseFile.Parse(modBase->data(), modBase->size());
    currentFile.Parse(current.data(), current.size());
    modFile.Parse(mod.data(), mod.size());
    MergeReport local;
    Merger(gameFile, modBase ? baseFile : gameFile, currentFile, modFile, report ? *report : local).Run();
    return currentFile.Write();
}

namespace {

// The text of every class part (constructor, static init, trait) by a key
// independent of indices: "<class>|iinit", "<class>|cinit",
// "<class>|i|<trait key>", "<class>|s|<trait key>".
std::map<std::string, std::string> ClassPartTexts(const AbcFile& file, const Canon& canon)
{
    std::map<std::string, std::string> texts;
    for (const auto& entry : ClassesByName(file, canon)) {
        texts[entry.first + "|iinit"] = canon.Method(file.instances[entry.second].iinit);
        texts[entry.first + "|cinit"] = canon.Method(file.classes[entry.second].cinit);
        for (const Trait& t : file.instances[entry.second].traits) texts[entry.first + "|i|" + canon.TraitKey(t)] = canon.TraitText(t);
        for (const Trait& t : file.classes[entry.second].traits) texts[entry.first + "|s|" + canon.TraitKey(t)] = canon.TraitText(t);
    }
    return texts;
}

}  // namespace

void CompareScripts(const Bytes& expected, const Bytes& got, std::string& differences, const Bytes* game, const Bytes* ffdecBase)
{
    AbcFile a, b;
    a.Parse(expected.data(), expected.size());
    b.Parse(got.data(), got.size());
    const Canon ca(a), cb(b);
    const std::map<std::string, uint32_t> classesA = ClassesByName(a, ca), classesB = ClassesByName(b, cb);
    // a part the expected script has in FFDec's form and the result in the
    // game's (no mod changed it) is the same thing
    std::map<std::string, std::string> gameTexts, baseTexts;
    if (game && ffdecBase) {
        AbcFile gameFile, baseFile;
        gameFile.Parse(game->data(), game->size());
        baseFile.Parse(ffdecBase->data(), ffdecBase->size());
        gameTexts = ClassPartTexts(gameFile, Canon(gameFile));
        baseTexts = ClassPartTexts(baseFile, Canon(baseFile));
    }
    auto same = [&](const std::string& key, const std::string& expectedText, const std::string& gotText) {
        if (expectedText == gotText) return true;
        auto gameText = gameTexts.find(key), baseText = baseTexts.find(key);
        return gameText != gameTexts.end() && baseText != baseTexts.end() && baseText->second == expectedText &&
               gameText->second == gotText;
    };
    auto line = [&](const std::string& text) { differences += "    " + text + "\n"; };
    auto traits = [&](const std::string& label, const std::string& keyPrefix, const std::vector<Trait>& ta, const std::vector<Trait>& tb) {
        for (const Trait& t : ta) {
            const Trait* other = FindTrait(tb, cb, ca.TraitKey(t));
            if (!other) line(label + " " + a.MultinameText(t.name) + ": missing");
            else if (!same(keyPrefix + ca.TraitKey(t), ca.TraitText(t), cb.TraitText(*other)))
                line(label + " " + a.MultinameText(t.name) + ": differs");
        }
        for (const Trait& t : tb)
            if (!FindTrait(ta, ca, cb.TraitKey(t))) line(label + " " + b.MultinameText(t.name) + ": extra");
    };
    for (const auto& entry : classesA) {
        const std::string label = "class " + a.ClassName(entry.second);
        auto other = classesB.find(entry.first);
        if (other == classesB.end()) { line(label + ": missing"); continue; }
        if (!same(entry.first + "|iinit", ca.Method(a.instances[entry.second].iinit), cb.Method(b.instances[other->second].iinit)))
            line(label + " constructor: differs");
        if (!same(entry.first + "|cinit", ca.Method(a.classes[entry.second].cinit), cb.Method(b.classes[other->second].cinit)))
            line(label + " static init: differs");
        traits(label, entry.first + "|i|", a.instances[entry.second].traits, b.instances[other->second].traits);
        traits(label + " (static)", entry.first + "|s|", a.classes[entry.second].traits, b.classes[other->second].traits);
    }
    for (const auto& entry : classesB)
        if (!classesA.count(entry.first)) line("class " + b.ClassName(entry.second) + ": extra");
    for (const Script& script : a.scripts) {
        const int other = MatchScript(script, ca, b, cb);
        if (other < 0) { line("a script is missing"); continue; }
        if (ca.Method(script.init) != cb.Method(b.scripts[other].init)) line("script init: differs");
        traits("script", "", script.traits, b.scripts[other].traits);
    }
    if (a.scripts.size() != b.scripts.size())
        line("script count " + std::to_string(a.scripts.size()) + " vs " + std::to_string(b.scripts.size()));
}

}  // namespace abc
}  // namespace xv2ui

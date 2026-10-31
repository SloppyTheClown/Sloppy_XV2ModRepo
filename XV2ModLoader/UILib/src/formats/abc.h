// AS3 bytecode (ABC, the DoABC payload Iggy keeps in a movie): the complete
// model, after Adobe's "ActionScript Virtual Machine 2 Overview" (May 2007)
// chapter 4. Parse + Write reproduce the game's ABCs byte for byte
// (xv2ui.exe roundtrip). Indices are kept as the file has them; index 0 of
// every constant pool table is the implicit "none" entry and is present in
// the vectors (so pool[i] is entry i).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "bytes.h"

namespace xv2ui {
namespace abc {

// Namespace kinds (avm2overview 4.4.1).
enum NamespaceKind : uint8_t {
    kNsPrivate = 0x05, kNsNamespace = 0x08, kNsPackage = 0x16, kNsPackageInternal = 0x17,
    kNsProtected = 0x18, kNsExplicit = 0x19, kNsStaticProtected = 0x1A,
};

// Multiname kinds (4.4.3).
enum MultinameKind : uint8_t {
    kQName = 0x07, kQNameA = 0x0D, kRTQName = 0x0F, kRTQNameA = 0x10, kRTQNameL = 0x11, kRTQNameLA = 0x12,
    kMultiname = 0x09, kMultinameA = 0x0E, kMultinameL = 0x1B, kMultinameLA = 0x1C, kTypeName = 0x1D,
};

struct Namespace { uint8_t kind = 0; uint32_t name = 0; };   // name: string index
struct Multiname {
    uint8_t kind = 0;
    uint32_t ns = 0;        // QName(A): namespace index
    uint32_t name = 0;      // QName/RTQName/Multiname: string index
    uint32_t nsSet = 0;     // Multiname(L)(A)
    uint32_t base = 0;      // TypeName: the generic's multiname
    std::vector<uint32_t> params;  // TypeName: parameter multinames
};

struct ConstantPool {
    std::vector<int32_t> ints{0};
    std::vector<uint32_t> uints{0};
    std::vector<double> doubles{0.0};
    std::vector<std::string> strings{std::string()};
    std::vector<Namespace> namespaces{Namespace()};
    std::vector<std::vector<uint32_t>> nsSets{{}};
    std::vector<Multiname> multinames{Multiname()};
};

// Method flags (4.5).
enum MethodFlag : uint8_t {
    kNeedArguments = 0x01, kNeedActivation = 0x02, kNeedRest = 0x04, kHasOptional = 0x08,
    kSetDxns = 0x40, kHasParamNames = 0x80,
};
struct OptionValue { uint32_t value = 0; uint8_t kind = 0; };   // value: index into the pool `kind` names
struct Method {
    uint32_t returnType = 0;               // multiname
    std::vector<uint32_t> paramTypes;      // multinames
    uint32_t name = 0;                     // string
    uint8_t flags = 0;
    std::vector<OptionValue> options;      // kHasOptional
    std::vector<uint32_t> paramNames;      // kHasParamNames: strings
};

struct MetadataItem { uint32_t key = 0, value = 0; };   // strings (key 0 = keyless)
struct Metadata { uint32_t name = 0; std::vector<MetadataItem> items; };

// Trait kinds (4.8) - the low 4 bits; the high 4 are attributes.
enum TraitKind : uint8_t { kSlot = 0, kMethodTrait = 1, kGetter = 2, kSetter = 3, kClassTrait = 4, kFunction = 5, kConst = 6 };
enum TraitAttribute : uint8_t { kAttrFinal = 0x1, kAttrOverride = 0x2, kAttrMetadata = 0x4 };
struct Trait {
    uint32_t name = 0;        // multiname (a QName)
    uint8_t kindByte = 0;     // kind | attributes << 4
    uint32_t slotId = 0;      // slot/const/class/function: slot id; method/getter/setter: disp id
    uint32_t typeName = 0;    // slot/const: multiname
    uint32_t valueIndex = 0;  // slot/const: default value index (0 = none)
    uint8_t valueKind = 0;    //   and its pool (when valueIndex != 0)
    uint32_t index = 0;       // class: class index; function/method/getter/setter: method index
    std::vector<uint32_t> metadata;

    uint8_t kind() const { return kindByte & 0x0f; }
    uint8_t attributes() const { return kindByte >> 4; }
};

enum InstanceFlag : uint8_t { kSealed = 0x01, kFinal = 0x02, kInterface = 0x04, kProtectedNs = 0x08 };
struct Instance {
    uint32_t name = 0, superName = 0;     // multinames
    uint8_t flags = 0;
    uint32_t protectedNs = 0;             // kProtectedNs
    std::vector<uint32_t> interfaces;     // multinames
    uint32_t iinit = 0;                   // method
    std::vector<Trait> traits;
};
struct Class { uint32_t cinit = 0; std::vector<Trait> traits; };
struct Script { uint32_t init = 0; std::vector<Trait> traits; };

struct ExceptionInfo { uint32_t from = 0, to = 0, target = 0, excType = 0, varName = 0; };
struct MethodBody {
    uint32_t method = 0;
    uint32_t maxStack = 0, localCount = 0, initScopeDepth = 0, maxScopeDepth = 0;
    Bytes code;
    std::vector<ExceptionInfo> exceptions;
    std::vector<Trait> traits;
};

struct AbcFile {
    uint16_t minorVersion = 16, majorVersion = 46;
    ConstantPool pool;
    std::vector<Method> methods;
    std::vector<Metadata> metadata;
    std::vector<Instance> instances;    // instances[i] and classes[i] are one class
    std::vector<Class> classes;
    std::vector<Script> scripts;
    std::vector<MethodBody> bodies;

    void Parse(const uint8_t* data, size_t size);
    Bytes Write() const;

    // The body of method `method`, or -1.
    int BodyOf(uint32_t method) const;
    // Text of string / multiname for messages and keys ("" for none).
    const std::string& String(uint32_t index) const;
    std::string NamespaceText(uint32_t index) const;
    std::string MultinameText(uint32_t index) const;
    // "package::Name" of a class's instance name.
    std::string ClassName(size_t classIndex) const { return MultinameText(instances[classIndex].name); }
};

// ----------------------------------------------------------------- code

// Operand kinds of an instruction (avm2overview chapter 5).
enum OperandKind : uint8_t {
    kOpNone, kOpU8, kOpU30, kOpString, kOpInt, kOpUInt, kOpDouble, kOpNamespace, kOpMultiname,
    kOpMethod, kOpClass, kOpException, kOpBranch, kOpSwitch, kOpDebug,
};
// What an opcode carries: up to two operands (the second is a plain u30 arg count / register).
struct OpcodeInfo { const char* name; OperandKind first; OperandKind second; };
// The table entry for `op`, or null for an opcode the AVM2 doesn't define.
const OpcodeInfo* Opcode(uint8_t op);

struct Instruction {
    uint32_t offset = 0;              // byte offset in the body's code
    uint8_t op = 0;
    uint32_t a = 0, b = 0;            // operands (pool indices, counts, registers)
    uint8_t debugType = 0, debugReg = 0;   // 0xef debug: u8 type, u30 string (a), u8 reg, u30 extra (b)
    int32_t branch = 0;               // branches: target byte offset (absolute in the code)
    std::vector<int32_t> cases;       // lookupswitch: absolute targets, [0] = default
    bool unreachableTarget = false;   // a target is no instruction (dead code): offsets kept raw
};

// Decodes a body's code; throws on an unknown opcode. A branch that lands
// on no instruction is marked unreachableTarget (see Decode).
std::vector<Instruction> Decode(const Bytes& code);
// Encodes instructions back: branch/switch targets are absolute offsets of
// the ORIGINAL layout (Instruction::offset) and are remapped to the new
// layout; returns the new code and fills `newOffsets` (old offset -> new,
// including the end of the code) for exception ranges.
Bytes Encode(const std::vector<Instruction>& code, size_t oldCodeSize, std::vector<std::pair<uint32_t, uint32_t>>* newOffsets);

}  // namespace abc
}  // namespace xv2ui

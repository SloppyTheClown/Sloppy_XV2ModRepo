// ---------------------------------------------------------------------------
//  Functions the Lua scripts can call. Two sets, in every script's state:
//
//  * LuaBackend's (LuaEngine v5.0 - https://github.com/Sirius902/LuaBackend,
//    DOCUMENT.md): ReadInt(address, absolute), WriteFloat(address, value,
//    absolute), ReadArray, GetPointer, WriteExec, ConsolePrint, ... Addresses
//    are offsets into DBXV2.exe unless "absolute" is true or the A-suffixed
//    variant (ReadIntA) is used. A bad address reads as 0 / an empty table
//    and a write to one is dropped, as in LuaBackend.
//  * Cheat Engine's: readInteger, writeFloat, readBytes, readString,
//    AOBScan, getAddress, ... Addresses are absolute and may also be strings
//    in CE's notation: "DBXV2.exe+1A2B", "[[DBXV2.exe+10]+8]+4". A bad
//    address reads as nil and a write to one returns false, as in CE.
//
//  Written from those two documented APIs (names, arguments, results); no
//  code is taken from either project.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "lua_scripts.h"

// ---------------------------------------------------------------------------
//  Guarded memory access
// ---------------------------------------------------------------------------

static int AccessFaultFilter(DWORD code)
{
    return (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_GUARD_PAGE)
        ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

// memcpy that returns false instead of crashing the game on a bad address -
// scripts follow arbitrary pointers. No C++ objects in here: __try can't mix
// with destructors in one function.
static bool CopyGuarded(void* destination, const void* source, size_t size)
{
    __try {
        memcpy(destination, source, size);
        return true;
    } __except (AccessFaultFilter(GetExceptionCode())) {
        return false;
    }
}

static bool ReadMemory(uintptr_t address, void* out, size_t size)
{
    return CopyGuarded(out, reinterpret_cast<const void*>(address), size);
}

// Writes straight away when the memory is writable; otherwise (the exe's code
// and .rdata) unprotects it for the write and restores the protection.
static bool WriteMemory(uintptr_t address, const void* data, size_t size)
{
    void* target = reinterpret_cast<void*>(address);
    if (CopyGuarded(target, data, size)) return true;
    DWORD oldProtection = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtection)) return false;
    const bool written = CopyGuarded(target, data, size);
    VirtualProtect(target, size, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    return written;
}

static uintptr_t ExeBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)); }

// ---------------------------------------------------------------------------
//  Argument helpers
// ---------------------------------------------------------------------------

// An integer argument; a float is truncated rather than rejected, since
// scripts often compute addresses or values with "/".
static uint64_t ArgInteger(lua_State* L, int index)
{
    int isInteger = 0;
    const lua_Integer value = lua_tointegerx(L, index, &isInteger);
    if (isInteger) return static_cast<uint64_t>(value);
    return static_cast<uint64_t>(static_cast<lua_Integer>(luaL_checknumber(L, index)));
}

template <typename T>
static T ArgValue(lua_State* L, int index)
{
    if constexpr (std::is_floating_point_v<T>) return static_cast<T>(luaL_checknumber(L, index));
    else return static_cast<T>(ArgInteger(L, index));
}

template <typename T>
static void PushValue(lua_State* L, T value)
{
    if constexpr (std::is_floating_point_v<T>) lua_pushnumber(L, value);
    else lua_pushinteger(L, static_cast<lua_Integer>(value));
}

// A table of byte values {0x90, 0x90} (1-based, as Lua writes them).
static std::vector<uint8_t> ArgByteTable(lua_State* L, int index)
{
    luaL_checktype(L, index, LUA_TTABLE);
    std::vector<uint8_t> bytes(static_cast<size_t>(luaL_len(L, index)));
    for (size_t i = 0; i < bytes.size(); ++i) {
        lua_geti(L, index, static_cast<lua_Integer>(i + 1));
        bytes[i] = static_cast<uint8_t>(ArgInteger(L, -1));
        lua_pop(L, 1);
    }
    return bytes;
}

static void PushByteTable(lua_State* L, const std::vector<uint8_t>& bytes)
{
    lua_createtable(L, static_cast<int>(bytes.size()), 0);
    for (size_t i = 0; i < bytes.size(); ++i) {
        lua_pushinteger(L, bytes[i]);
        lua_seti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

// A length argument, refused when negative so it can't turn into a huge size.
static size_t ArgLength(lua_State* L, int index)
{
    const lua_Integer length = luaL_checkinteger(L, index);
    luaL_argcheck(L, length >= 0, index, "length must not be negative");
    return static_cast<size_t>(length);
}

// ---------------------------------------------------------------------------
//  LuaBackend functions
// ---------------------------------------------------------------------------

// Address argument 1, made absolute: LuaBackend adds the exe's base unless the
// function is an A-variant or its "absolute" argument (at absoluteIndex) is true.
template <bool kAbsoluteOnly>
static uintptr_t BackendAddress(lua_State* L, int absoluteIndex)
{
    uintptr_t address = static_cast<uintptr_t>(ArgInteger(L, 1));
    if constexpr (!kAbsoluteOnly) {
        if (!lua_toboolean(L, absoluteIndex)) address += ExeBase();
    }
    return address;
}

// ReadByte/Short/Int/Long/Float/Boolean(address, absolute) and their A-variants.
template <typename T, bool kAbsoluteOnly>
static int BackendRead(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 2);
    if constexpr (std::is_same_v<T, bool>) {
        uint8_t byte = 0;
        ReadMemory(address, &byte, 1);
        lua_pushboolean(L, byte != 0);
    } else {
        T value{};
        ReadMemory(address, &value, sizeof(value));     // stays 0 on a bad address
        PushValue(L, value);
    }
    return 1;
}

// WriteByte/Short/Int/Long/Float/Boolean(address, value, absolute) and A-variants.
template <typename T, bool kAbsoluteOnly>
static int BackendWrite(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 3);
    if constexpr (std::is_same_v<T, bool>) {
        const uint8_t byte = lua_toboolean(L, 2) ? 1 : 0;
        WriteMemory(address, &byte, 1);
    } else {
        const T value = ArgValue<T>(L, 2);
        WriteMemory(address, &value, sizeof(value));
    }
    return 0;
}

// ReadArray(address, length, absolute) -> {byte, ...}; empty on a bad address.
template <bool kAbsoluteOnly>
static int BackendReadArray(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 3);
    std::vector<uint8_t> bytes(ArgLength(L, 2));
    if (!ReadMemory(address, bytes.data(), bytes.size())) bytes.clear();
    PushByteTable(L, bytes);
    return 1;
}

// ReadString(address, length, absolute) -> the raw bytes, zeros included.
template <bool kAbsoluteOnly>
static int BackendReadString(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 3);
    std::string text(ArgLength(L, 2), '\0');
    if (!ReadMemory(address, text.data(), text.size())) text.clear();
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

// WriteArray(address, {byte, ...}, absolute)
template <bool kAbsoluteOnly>
static int BackendWriteArray(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 3);
    const std::vector<uint8_t> bytes = ArgByteTable(L, 2);
    WriteMemory(address, bytes.data(), bytes.size());
    return 0;
}

// WriteString(address, text, absolute) - no terminator added.
template <bool kAbsoluteOnly>
static int BackendWriteString(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 3);
    size_t size = 0;
    const char* text = luaL_checklstring(L, 2, &size);
    WriteMemory(address, text, size);
    return 0;
}

// WriteExec(address, {byte, ...}) - address is always an offset into the exe.
// (LuaBackend lets the base address differ from the exe's; here they match.)
static int BackendWriteExec(lua_State* L)
{
    const uintptr_t address = ExeBase() + static_cast<uintptr_t>(ArgInteger(L, 1));
    const std::vector<uint8_t> bytes = ArgByteTable(L, 2);
    WriteMemory(address, bytes.data(), bytes.size());
    return 0;
}

// GetPointer(address, offset, absolute) -> [address] + offset
template <bool kAbsoluteOnly>
static int BackendGetPointer(lua_State* L)
{
    const uintptr_t address = BackendAddress<kAbsoluteOnly>(L, 3);
    uint64_t pointer = 0;
    ReadMemory(address, &pointer, sizeof(pointer));
    lua_pushinteger(L, static_cast<lua_Integer>(pointer + ArgInteger(L, 2)));
    return 1;
}

// ULShift32(base, shift) - a 32-bit left shift that doesn't spill into 64 bits.
static int BackendShiftLeft32(lua_State* L)
{
    const uint32_t value = static_cast<uint32_t>(ArgInteger(L, 1));
    const uint32_t shift = static_cast<uint32_t>(ArgInteger(L, 2)) & 31;
    lua_pushinteger(L, static_cast<uint32_t>(value << shift));
    return 1;
}

// ConsolePrint(text, type) - type 0..3 = MESSAGE/SUCCESS/WARNING/ERROR.
static int BackendConsolePrint(lua_State* L)
{
    size_t size = 0;
    const char* text = luaL_tolstring(L, 1, &size);
    const int type = lua_isnoneornil(L, 2) ? kPlain : static_cast<int>(luaL_checkinteger(L, 2));
    ScriptOutput(L, type, std::string(text, size));
    return 0;
}

// GetHertz / SetHertz(60|120|240). Kept for scripts that call them: scripts
// run once per game frame whatever is set, as in LuaBackend Hook.
static int g_hertz = 60;

static int BackendGetHertz(lua_State* L)
{
    lua_pushinteger(L, g_hertz);
    return 1;
}

static int BackendSetHertz(lua_State* L)
{
    const lua_Integer hertz = luaL_checkinteger(L, 1);
    g_hertz = (hertz == 120 || hertz == 240) ? static_cast<int>(hertz) : 60;
    return 0;
}

// InitializeRPC, UpdateDetails, ...: Discord Rich Presence is left out (the
// loader makes no network connections). They do nothing, so scripts that
// call them still run; one warning per script.
static int BackendDiscordStub(lua_State* L)
{
    if (lua_getfield(L, LUA_REGISTRYINDEX, "xv2_discord_warned") == LUA_TNIL) {
        ScriptOutput(L, kWarning, "Discord Rich Presence is not supported here; the calls do nothing.");
        lua_pushboolean(L, 1);
        lua_setfield(L, LUA_REGISTRYINDEX, "xv2_discord_warned");
    }
    lua_pop(L, 1);
    return 0;
}

// Lua's print(), sent to the console/log like ConsolePrint without a type.
static int LuaPrint(lua_State* L)
{
    std::string line;
    const int count = lua_gettop(L);
    for (int i = 1; i <= count; ++i) {
        size_t size = 0;
        const char* text = luaL_tolstring(L, i, &size);
        if (i > 1) line += '\t';
        line.append(text, size);
        lua_pop(L, 1);
    }
    ScriptOutput(L, kPlain, line);
    return 0;
}

// ---------------------------------------------------------------------------
//  Cheat Engine addresses: integers or strings like "[[DBXV2.exe+10]+8]-4"
// ---------------------------------------------------------------------------

static void SkipSpaces(const char*& text)
{
    while (*text == ' ' || *text == '\t') ++text;
}

static bool ParseAddressSum(const char*& text, uintptr_t& out);

// One term: "[sum]" (the 8-byte pointer stored there), a module name
// (DBXV2.exe, optionally in quotes) for its base address, or a hex number.
static bool ParseAddressTerm(const char*& text, uintptr_t& out)
{
    SkipSpaces(text);
    if (*text == '[') {
        ++text;
        uintptr_t pointerAddress = 0;
        if (!ParseAddressSum(text, pointerAddress)) return false;
        SkipSpaces(text);
        if (*text != ']') return false;
        ++text;
        return ReadMemory(pointerAddress, &out, sizeof(out));
    }
    std::string token;
    if (*text == '"') {
        for (++text; *text && *text != '"'; ++text) token += *text;
        if (*text != '"') return false;
        ++text;
    } else {
        while (*text && !strchr("+-[] \t", *text)) token += *text++;
    }
    if (token.empty()) return false;
    if (HMODULE module = GetModuleHandleA(token.c_str())) {
        out = reinterpret_cast<uintptr_t>(module);
        return true;
    }
    const char* digits = token.c_str();
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
    if (!*digits) return false;
    char* end = nullptr;
    out = static_cast<uintptr_t>(_strtoui64(digits, &end, 16));
    return *end == 0;
}

static bool ParseAddressSum(const char*& text, uintptr_t& out)
{
    if (!ParseAddressTerm(text, out)) return false;
    for (;;) {
        SkipSpaces(text);
        const char sign = *text;
        if (sign != '+' && sign != '-') return true;
        ++text;
        uintptr_t term = 0;
        if (!ParseAddressTerm(text, term)) return false;
        out = sign == '+' ? out + term : out - term;
    }
}

static bool ResolveAddressText(const char* text, uintptr_t& out)
{
    if (!ParseAddressSum(text, out)) return false;
    SkipSpaces(text);
    return *text == 0;
}

// Address argument: a number, or a string in CE's notation.
static bool ArgAddress(lua_State* L, int index, uintptr_t& out)
{
    if (lua_type(L, index) == LUA_TNUMBER) {
        out = static_cast<uintptr_t>(ArgInteger(L, index));
        return true;
    }
    if (lua_type(L, index) == LUA_TSTRING) return ResolveAddressText(lua_tostring(L, index), out);
    return false;
}

// ---------------------------------------------------------------------------
//  Cheat Engine functions
// ---------------------------------------------------------------------------

// readSmallInteger/readInteger(address, signed), readQword/readPointer,
// readFloat/readDouble(address) -> value, or nil on a bad address.
template <typename T>
static int CeRead(lua_State* L)
{
    uintptr_t address = 0;
    T value{};
    if (!ArgAddress(L, 1, address) || !ReadMemory(address, &value, sizeof(value))) {
        lua_pushnil(L);
        return 1;
    }
    if constexpr (std::is_integral_v<T> && sizeof(T) < 8) {
        if (lua_toboolean(L, 2)) {                      // CE's "signed" argument
            PushValue(L, static_cast<std::make_signed_t<T>>(value));
            return 1;
        }
    }
    PushValue(L, value);
    return 1;
}

// writeSmallInteger/writeInteger/writeQword/writePointer/writeFloat/
// writeDouble(address, value) -> true if written.
template <typename T>
static int CeWrite(lua_State* L)
{
    uintptr_t address = 0;
    const T value = ArgValue<T>(L, 2);
    lua_pushboolean(L, ArgAddress(L, 1, address) && WriteMemory(address, &value, sizeof(value)));
    return 1;
}

// readBytes(address, count = 1, returnAsTable = false) -> bytes as separate
// results, or as one table; nil on a bad address.
static int CeReadBytes(lua_State* L)
{
    uintptr_t address = 0;
    std::vector<uint8_t> bytes(lua_isnoneornil(L, 2) ? 1 : ArgLength(L, 2));
    if (!ArgAddress(L, 1, address) || !ReadMemory(address, bytes.data(), bytes.size())) {
        lua_pushnil(L);
        return 1;
    }
    if (lua_toboolean(L, 3)) {
        PushByteTable(L, bytes);
        return 1;
    }
    luaL_checkstack(L, static_cast<int>(bytes.size()), "too many bytes to return; pass returnAsTable");
    for (uint8_t byte : bytes) lua_pushinteger(L, byte);
    return static_cast<int>(bytes.size());
}

// writeBytes(address, {byte, ...}) or writeBytes(address, byte, byte, ...)
static int CeWriteBytes(lua_State* L)
{
    uintptr_t address = 0;
    std::vector<uint8_t> bytes;
    if (lua_istable(L, 2)) {
        bytes = ArgByteTable(L, 2);
    } else {
        for (int i = 2; i <= lua_gettop(L); ++i) bytes.push_back(static_cast<uint8_t>(ArgInteger(L, i)));
    }
    lua_pushboolean(L, ArgAddress(L, 1, address) && WriteMemory(address, bytes.data(), bytes.size()));
    return 1;
}

// readString(address, maxLength = 1000, wideChar = false) -> text up to the
// first 0 character, UTF-16 converted to UTF-8; nil on a bad address.
static int CeReadString(lua_State* L)
{
    constexpr size_t kPageSize = 0x1000;               // reads stop at the first unreadable page
    uintptr_t address = 0;
    const size_t maxLength = lua_isnoneornil(L, 2) ? 1000 : ArgLength(L, 2);
    const bool wide = lua_toboolean(L, 3) != 0;
    const size_t charSize = wide ? 2 : 1;
    const size_t maxBytes = maxLength * charSize;
    if (!ArgAddress(L, 1, address)) {
        lua_pushnil(L);
        return 1;
    }
    std::string bytes;
    bool terminated = false;
    while (bytes.size() < maxBytes && !terminated) {
        const uintptr_t cursor = address + bytes.size();
        const size_t chunk = min(kPageSize - (cursor & (kPageSize - 1)), maxBytes - bytes.size());
        char buffer[kPageSize];
        if (!ReadMemory(cursor, buffer, chunk)) break;
        bytes.append(buffer, chunk);
        for (size_t at = 0; at + charSize <= bytes.size(); at += charSize) {
            if (bytes[at] == 0 && (!wide || bytes[at + 1] == 0)) {
                bytes.resize(at);
                terminated = true;
                break;
            }
        }
    }
    if (bytes.empty() && !terminated) {                 // nothing readable at all
        lua_pushnil(L);
        return 1;
    }
    if (!wide) {
        lua_pushlstring(L, bytes.data(), bytes.size());
        return 1;
    }
    const wchar_t* chars = reinterpret_cast<const wchar_t*>(bytes.data());
    const int charCount = static_cast<int>(bytes.size() / 2);
    std::string utf8(WideCharToMultiByte(CP_UTF8, 0, chars, charCount, nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, chars, charCount, utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
    lua_pushlstring(L, utf8.data(), utf8.size());
    return 1;
}

// writeString(address, text, wideChar = false) -> true if written. No
// terminator, as in CE; wideChar writes the UTF-8 text as UTF-16.
static int CeWriteString(lua_State* L)
{
    uintptr_t address = 0;
    size_t size = 0;
    const char* text = luaL_checklstring(L, 2, &size);
    bool written = false;
    if (ArgAddress(L, 1, address)) {
        if (lua_toboolean(L, 3)) {
            std::wstring wide(MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(size), nullptr, 0), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(size), wide.data(), static_cast<int>(wide.size()));
            written = WriteMemory(address, wide.data(), wide.size() * sizeof(wchar_t));
        } else {
            written = WriteMemory(address, text, size);
        }
    }
    lua_pushboolean(L, written);
    return 1;
}

// getAddress("DBXV2.exe+1A2B") -> integer; an error if it can't be resolved.
static int CeGetAddress(lua_State* L)
{
    uintptr_t address = 0;
    if (!ArgAddress(L, 1, address)) return luaL_error(L, "getAddress: can't resolve \"%s\"", luaL_tolstring(L, 1, nullptr));
    lua_pushinteger(L, static_cast<lua_Integer>(address));
    return 1;
}

// getAddressSafe(text) -> integer, or nil.
static int CeGetAddressSafe(lua_State* L)
{
    uintptr_t address = 0;
    if (ArgAddress(L, 1, address)) lua_pushinteger(L, static_cast<lua_Integer>(address));
    else lua_pushnil(L);
    return 1;
}

// ---------------------------------------------------------------------------
//  AOB scans. CE scans the whole process; here a scan covers one module's
//  image (DBXV2.exe unless one is named), which keeps it bounded - a few
//  tens of ms for the exe. Still: scan in _OnInit, never in _OnFrame.
// ---------------------------------------------------------------------------

constexpr size_t kMaxScanResults = 4096;                // more hits than this are useless for finding code

struct BytePattern {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> known;                         // 0 = wildcard
};

static int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "48 8B ?? 05" or "488B??05"; ?, ??, * and x are whole-byte wildcards.
static bool ParsePattern(const char* text, BytePattern& pattern)
{
    const bool spaced = strchr(text, ' ') != nullptr;
    while (*text) {
        if (*text == ' ') { ++text; continue; }
        std::string token;
        if (spaced) while (*text && *text != ' ') token += *text++;
        else { token += *text++; if (*text) token += *text++; }
        if (token == "?" || token == "??" || token == "*" || token == "**" || token == "x" || token == "xx") {
            pattern.bytes.push_back(0);
            pattern.known.push_back(0);
            continue;
        }
        if (token.size() > 2) return false;
        int value = 0;
        for (char c : token) {
            const int digit = HexDigit(c);
            if (digit < 0) return false;
            value = value * 16 + digit;
        }
        pattern.bytes.push_back(static_cast<uint8_t>(value));
        pattern.known.push_back(1);
    }
    return !pattern.bytes.empty();
}

// Matches inside one readable region; stops at maxResults. Guarded in case
// the region is unmapped while scanning. Returns the number of hits written.
static size_t ScanRegionGuarded(const uint8_t* start, size_t size, const uint8_t* bytes,
                                const uint8_t* known, size_t length, uintptr_t* results, size_t maxResults)
{
    size_t found = 0;
    __try {
        for (size_t offset = 0; offset + length <= size && found < maxResults; ++offset) {
            size_t i = 0;
            while (i < length && (!known[i] || start[offset + i] == bytes[i])) ++i;
            if (i == length) results[found++] = reinterpret_cast<uintptr_t>(start + offset);
        }
    } __except (AccessFaultFilter(GetExceptionCode())) {
    }
    return found;
}

// CE's protection flags: "+X" executable only, "-X" not executable, same
// for W (writable); "*X"/C and others are ignored.
struct ProtectionFilter {
    int executable = -1;                                // -1 any, 0 must not be, 1 must be
    int writable = -1;
};

static ProtectionFilter ParseProtection(const char* text)
{
    ProtectionFilter filter;
    for (; text && *text; ++text) {
        if (*text != '+' && *text != '-') continue;
        const int wanted = *text == '+' ? 1 : 0;
        const char flag = text[1];
        if (flag == 'X' || flag == 'x') filter.executable = wanted;
        if (flag == 'W' || flag == 'w') filter.writable = wanted;
    }
    return filter;
}

static bool RegionMatches(DWORD protection, const ProtectionFilter& filter)
{
    if (protection & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const bool executable = (protection & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    const bool writable = (protection & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    if (protection & PAGE_EXECUTE) return false;        // execute-only: not readable
    if (filter.executable >= 0 && executable != (filter.executable == 1)) return false;
    if (filter.writable >= 0 && writable != (filter.writable == 1)) return false;
    return true;
}

// All hits of pattern in module's image (SizeOfImage from its PE header).
static std::vector<uintptr_t> ScanModule(HMODULE module, const BytePattern& pattern,
                                         const ProtectionFilter& filter, size_t maxResults)
{
    std::vector<uintptr_t> results;
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const uint8_t* end = base + nt->OptionalHeader.SizeOfImage;
    std::vector<uintptr_t> regionHits(maxResults);
    MEMORY_BASIC_INFORMATION region;
    for (const uint8_t* cursor = base; cursor < end && results.size() < maxResults;) {
        if (!VirtualQuery(cursor, &region, sizeof(region))) break;
        const auto* regionStart = static_cast<const uint8_t*>(region.BaseAddress);
        const uint8_t* regionEnd = min(regionStart + region.RegionSize, end);
        if (region.State == MEM_COMMIT && RegionMatches(region.Protect, filter)) {
            const size_t found = ScanRegionGuarded(cursor, regionEnd - cursor, pattern.bytes.data(),
                                                   pattern.known.data(), pattern.bytes.size(),
                                                   regionHits.data(), maxResults - results.size());
            results.insert(results.end(), regionHits.begin(), regionHits.begin() + found);
        }
        cursor = regionEnd;
    }
    return results;
}

static std::vector<uintptr_t> ScanArgs(lua_State* L, HMODULE module, int patternIndex, size_t maxResults)
{
    BytePattern pattern;
    const char* text = luaL_checkstring(L, patternIndex);
    if (!ParsePattern(text, pattern)) luaL_error(L, "bad AOB pattern \"%s\"", text);
    const ProtectionFilter filter = ParseProtection(luaL_optstring(L, patternIndex + 1, nullptr));
    return ScanModule(module, pattern, filter, maxResults);
}

// A stand-in for CE's StringList result: hits as hex strings at [0]..[Count-1],
// with Count, getCount(), getString(i) and destroy() (which does nothing).
static int StringListGetCount(lua_State* L)
{
    lua_getfield(L, lua_upvalueindex(1), "Count");
    return 1;
}

static int StringListGetString(lua_State* L)
{
    const int indexArgument = lua_istable(L, 1) ? 2 : 1;   // list:getString(i) or list.getString(i)
    lua_geti(L, lua_upvalueindex(1), luaL_checkinteger(L, indexArgument));
    return 1;
}

static int StringListDestroy(lua_State*) { return 0; }

static void PushStringList(lua_State* L, const std::vector<uintptr_t>& addresses)
{
    lua_createtable(L, static_cast<int>(addresses.size()), 4);
    for (size_t i = 0; i < addresses.size(); ++i) {
        char hex[24];
        sprintf_s(hex, "%llX", static_cast<unsigned long long>(addresses[i]));
        lua_pushstring(L, hex);
        lua_seti(L, -2, static_cast<lua_Integer>(i));
    }
    lua_pushinteger(L, static_cast<lua_Integer>(addresses.size()));
    lua_setfield(L, -2, "Count");
    const struct { const char* name; lua_CFunction function; } methods[] = {
        {"getCount", StringListGetCount}, {"getString", StringListGetString}, {"destroy", StringListDestroy},
    };
    for (const auto& method : methods) {
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, method.function, 1);
        lua_setfield(L, -2, method.name);
    }
}

// AOBScan(pattern, protectionFlags) -> StringList of hex addresses, or nil.
static int CeAobScan(lua_State* L)
{
    const std::vector<uintptr_t> hits = ScanArgs(L, GetModuleHandleW(nullptr), 1, kMaxScanResults);
    if (hits.empty()) lua_pushnil(L);
    else PushStringList(L, hits);
    return 1;
}

// AOBScanUnique(pattern, protectionFlags) -> first hit as an integer, or nil.
static int CeAobScanUnique(lua_State* L)
{
    const std::vector<uintptr_t> hits = ScanArgs(L, GetModuleHandleW(nullptr), 1, 1);
    if (hits.empty()) lua_pushnil(L);
    else lua_pushinteger(L, static_cast<lua_Integer>(hits[0]));
    return 1;
}

// AOBScanModuleUnique(moduleName, pattern, protectionFlags) -> first hit or nil.
static int CeAobScanModuleUnique(lua_State* L)
{
    HMODULE module = GetModuleHandleA(luaL_checkstring(L, 1));
    if (!module) {
        lua_pushnil(L);
        return 1;
    }
    const std::vector<uintptr_t> hits = ScanArgs(L, module, 2, 1);
    if (hits.empty()) lua_pushnil(L);
    else lua_pushinteger(L, static_cast<lua_Integer>(hits[0]));
    return 1;
}

// ---------------------------------------------------------------------------
//  Memory blocks and calling game functions
// ---------------------------------------------------------------------------

// allocateMemory(size) -> address of a fresh zeroed block that can hold code
// (read/write/execute), or nil. As CE's; base address and protection
// arguments are not supported.
static int CeAllocateMemory(lua_State* L)
{
    const lua_Integer size = luaL_checkinteger(L, 1);
    luaL_argcheck(L, size > 0 && size <= 0x10000000, 1, "size must be 1..256 MB");
    void* block = VirtualAlloc(nullptr, static_cast<SIZE_T>(size), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (block) lua_pushinteger(L, static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(block)));
    else lua_pushnil(L);
    return 1;
}

// deAlloc(address) -> true when a block from allocateMemory was freed.
static int CeDeAlloc(lua_State* L)
{
    uintptr_t address = 0;
    lua_pushboolean(L, ArgAddress(L, 1, address) && VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE));
    return 1;
}

using GameFunction = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Calls function(arguments) and reports an access violation inside it as
// false instead of crashing. No C++ objects in here (__try).
static bool CallGuarded(GameFunction function, const uint64_t* arguments, uint64_t& result)
{
    __try {
        result = function(arguments[0], arguments[1], arguments[2], arguments[3],
                          arguments[4], arguments[5], arguments[6], arguments[7]);
        return true;
    } __except (AccessFaultFilter(GetExceptionCode())) {
        return false;
    }
}

// CallFunction(address, ...) -> rax as an integer, or nil, error.
// Not from CE or LuaBackend. Calls a game function (absolute address) with
// up to 8 integer/pointer arguments in the Windows x64 convention (extra
// arguments are passed as 0 and ignored by the callee); no float arguments
// or results. Runs on the render thread, between frames, where the game also
// runs its menus. A fault inside the function is caught, but the game's
// state may then be half updated - only call functions you have read.
static int LoaderCallFunction(lua_State* L)
{
    uintptr_t address = 0;
    if (!ArgAddress(L, 1, address) || address < 0x10000) return luaL_argerror(L, 1, "bad function address");
    const int argumentCount = lua_gettop(L) - 1;
    luaL_argcheck(L, argumentCount <= 8, 10, "at most 8 arguments");
    uint64_t arguments[8] = {};
    for (int i = 0; i < argumentCount; ++i) {
        if (lua_isboolean(L, i + 2)) arguments[i] = lua_toboolean(L, i + 2);
        else if (!lua_isnil(L, i + 2)) arguments[i] = ArgInteger(L, i + 2);
    }
    uint64_t result = 0;
    if (!CallGuarded(reinterpret_cast<GameFunction>(address), arguments, result)) {
        lua_pushnil(L);
        lua_pushstring(L, "access violation inside the called function");
        return 2;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(result));
    return 1;
}

// ---------------------------------------------------------------------------
//  Keyboard (Cheat Engine's isKeyPressed and VK_* names)
// ---------------------------------------------------------------------------

// isKeyPressed(key) -> true while the key is held, and only while the game's
// window has the focus (CE reads it regardless; keys typed into another
// window must not fly the character). key: a VK_* value or a letter "W".
static int CeIsKeyPressed(lua_State* L)
{
    int key = 0;
    if (lua_type(L, 1) == LUA_TSTRING) {
        const char* text = lua_tostring(L, 1);
        key = (text[0] && !text[1]) ? toupper(static_cast<unsigned char>(text[0])) : 0;
    } else {
        key = static_cast<int>(luaL_checkinteger(L, 1));
    }
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    const bool focused = foregroundProcess == GetCurrentProcessId();
    lua_pushboolean(L, focused && key > 0 && key < 256 && (GetAsyncKeyState(key) & 0x8000));
    return 1;
}

// ---------------------------------------------------------------------------
//  Controller (XInput) - this loader's own; CE and LuaBackend have none
// ---------------------------------------------------------------------------

// XINPUT_STATE as XInputGetState fills it (XInput.h; declared here so the
// loader needs no SDK header or import library).
struct PadState {
    DWORD packetNumber;
    WORD buttons;            // XINPUT_GAMEPAD_* bits, the PAD_* globals below
    BYTE leftTrigger, rightTrigger;
    SHORT thumbLX, thumbLY, thumbRX, thumbRY;
};
using XInputGetStateFunction = DWORD(WINAPI*)(DWORD userIndex, PadState* state);

// The buttons held on any connected controller. Read through the game's own
// xinput1_3.dll (loaded by the game; on the Deck it is the native override).
// Polling an empty slot is slow (XInput enumerates devices), so an empty slot
// is asked again only once a second, and all slots are read at most every
// 5 ms however many scripts ask. Render thread only, like every Lua call.
static WORD HeldPadButtons()
{
    static XInputGetStateFunction getState = nullptr;
    static bool looked = false;
    static ULONGLONG readAt = 0, emptyRetryAt[4] = {};
    static WORD held = 0;
    if (!looked) {
        looked = true;
        HMODULE xinput = GetModuleHandleW(L"xinput1_3.dll");
        if (!xinput) xinput = GetModuleHandleW(L"xinput1_4.dll");
        if (xinput) getState = reinterpret_cast<XInputGetStateFunction>(GetProcAddress(xinput, "XInputGetState"));
    }
    if (!getState) { looked = false; return 0; }   // not loaded yet: look again next time
    const ULONGLONG now = GetTickCount64();
    if (now - readAt < 5) return held;
    readAt = now;
    held = 0;
    for (DWORD slot = 0; slot < 4; ++slot) {
        if (now < emptyRetryAt[slot]) continue;
        PadState state = {};
        if (getState(slot, &state) == ERROR_SUCCESS) held |= state.buttons;
        else emptyRetryAt[slot] = now + 1000;
    }
    return held;
}

// isPadButtonPressed(button) -> true while that button is held on any
// controller, and only while the game's window has the focus. button: a
// PAD_* global (XInput's XINPUT_GAMEPAD_* bit).
static int LoaderIsPadButtonPressed(lua_State* L)
{
    const lua_Integer button = luaL_checkinteger(L, 1);
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    const bool focused = foregroundProcess == GetCurrentProcessId();
    lua_pushboolean(L, focused && button > 0 && button <= 0xFFFF && (HeldPadButtons() & button) == button);
    return 1;
}

// Globals PAD_A, PAD_LEFT_THUMB, ... (XInput.h XINPUT_GAMEPAD_* values).
static void SetPadButtonGlobals(lua_State* L)
{
    static const struct { const char* name; int bit; } kButtons[] = {
        {"PAD_DPAD_UP", 0x0001},    {"PAD_DPAD_DOWN", 0x0002},   {"PAD_DPAD_LEFT", 0x0004},
        {"PAD_DPAD_RIGHT", 0x0008}, {"PAD_START", 0x0010},       {"PAD_BACK", 0x0020},
        {"PAD_LEFT_THUMB", 0x0040}, {"PAD_RIGHT_THUMB", 0x0080}, {"PAD_LEFT_SHOULDER", 0x0100},
        {"PAD_RIGHT_SHOULDER", 0x0200}, {"PAD_A", 0x1000},       {"PAD_B", 0x2000},
        {"PAD_X", 0x4000},          {"PAD_Y", 0x8000},
    };
    for (const auto& button : kButtons) {
        lua_pushinteger(L, button.bit);
        lua_setglobal(L, button.name);
    }
}

// Globals VK_SHIFT, VK_F1, VK_A, VK_0, VK_NUMPAD0, ... (Windows virtual-key codes).
static void SetKeyCodeGlobals(lua_State* L)
{
    static const struct { const char* name; int code; } kNamed[] = {
        {"VK_LBUTTON", VK_LBUTTON}, {"VK_RBUTTON", VK_RBUTTON}, {"VK_MBUTTON", VK_MBUTTON},
        {"VK_BACK", VK_BACK},       {"VK_TAB", VK_TAB},         {"VK_RETURN", VK_RETURN},
        {"VK_SHIFT", VK_SHIFT},     {"VK_CONTROL", VK_CONTROL}, {"VK_MENU", VK_MENU},
        {"VK_ESCAPE", VK_ESCAPE},   {"VK_SPACE", VK_SPACE},     {"VK_PRIOR", VK_PRIOR},
        {"VK_NEXT", VK_NEXT},       {"VK_END", VK_END},         {"VK_HOME", VK_HOME},
        {"VK_LEFT", VK_LEFT},       {"VK_UP", VK_UP},           {"VK_RIGHT", VK_RIGHT},
        {"VK_DOWN", VK_DOWN},       {"VK_INSERT", VK_INSERT},   {"VK_DELETE", VK_DELETE},
        {"VK_LSHIFT", VK_LSHIFT},   {"VK_RSHIFT", VK_RSHIFT},   {"VK_LCONTROL", VK_LCONTROL},
        {"VK_RCONTROL", VK_RCONTROL}, {"VK_ADD", VK_ADD},       {"VK_SUBTRACT", VK_SUBTRACT},
    };
    for (const auto& key : kNamed) {
        lua_pushinteger(L, key.code);
        lua_setglobal(L, key.name);
    }
    char name[16];
    for (int i = 0; i < 26; ++i) {                      // VK_A..VK_Z = 'A'..'Z'
        snprintf(name, sizeof(name), "VK_%c", 'A' + i);
        lua_pushinteger(L, 'A' + i);
        lua_setglobal(L, name);
    }
    for (int i = 0; i < 10; ++i) {                      // VK_0..VK_9, VK_NUMPAD0..9
        snprintf(name, sizeof(name), "VK_%d", i);
        lua_pushinteger(L, '0' + i);
        lua_setglobal(L, name);
        snprintf(name, sizeof(name), "VK_NUMPAD%d", i);
        lua_pushinteger(L, VK_NUMPAD0 + i);
        lua_setglobal(L, name);
    }
    for (int i = 1; i <= 24; ++i) {                     // VK_F1..VK_F24
        snprintf(name, sizeof(name), "VK_F%d", i);
        lua_pushinteger(L, VK_F1 + i - 1);
        lua_setglobal(L, name);
    }
}

// ---------------------------------------------------------------------------

void RegisterLuaApi(lua_State* L)
{
    static const luaL_Reg functions[] = {
        // LuaBackend, relative to the exe unless "absolute"
        {"ReadByte",      BackendRead<uint8_t,  false>},
        {"ReadShort",     BackendRead<uint16_t, false>},
        {"ReadInt",       BackendRead<uint32_t, false>},
        {"ReadLong",      BackendRead<uint64_t, false>},
        {"ReadFloat",     BackendRead<float,    false>},
        {"ReadBoolean",   BackendRead<bool,     false>},
        {"ReadArray",     BackendReadArray<false>},
        {"ReadString",    BackendReadString<false>},
        {"WriteByte",     BackendWrite<uint8_t,  false>},
        {"WriteShort",    BackendWrite<uint16_t, false>},
        {"WriteInt",      BackendWrite<uint32_t, false>},
        {"WriteLong",     BackendWrite<uint64_t, false>},
        {"WriteFloat",    BackendWrite<float,    false>},
        {"WriteBoolean",  BackendWrite<bool,     false>},
        {"WriteArray",    BackendWriteArray<false>},
        {"WriteString",   BackendWriteString<false>},
        {"WriteExec",     BackendWriteExec},
        {"GetPointer",    BackendGetPointer<false>},
        // LuaBackend, absolute
        {"ReadByteA",     BackendRead<uint8_t,  true>},
        {"ReadShortA",    BackendRead<uint16_t, true>},
        {"ReadIntA",      BackendRead<uint32_t, true>},
        {"ReadLongA",     BackendRead<uint64_t, true>},
        {"ReadFloatA",    BackendRead<float,    true>},
        {"ReadBooleanA",  BackendRead<bool,     true>},
        {"ReadArrayA",    BackendReadArray<true>},
        {"ReadStringA",   BackendReadString<true>},
        {"WriteByteA",    BackendWrite<uint8_t,  true>},
        {"WriteShortA",   BackendWrite<uint16_t, true>},
        {"WriteIntA",     BackendWrite<uint32_t, true>},
        {"WriteLongA",    BackendWrite<uint64_t, true>},
        {"WriteFloatA",   BackendWrite<float,    true>},
        {"WriteBooleanA", BackendWrite<bool,     true>},
        {"WriteArrayA",   BackendWriteArray<true>},
        {"WriteStringA",  BackendWriteString<true>},
        {"GetPointerA",   BackendGetPointer<true>},
        // LuaBackend, other
        {"ULShift32",     BackendShiftLeft32},
        {"ConsolePrint",  BackendConsolePrint},
        {"GetHertz",      BackendGetHertz},
        {"SetHertz",      BackendSetHertz},
        {"InitializeRPC", BackendDiscordStub},
        {"UpdateDetails", BackendDiscordStub},
        {"UpdateState",   BackendDiscordStub},
        {"UpdateLImage",  BackendDiscordStub},
        {"UpdateSImage",  BackendDiscordStub},
        {"print",         LuaPrint},
        // Cheat Engine
        {"readSmallInteger",  CeRead<uint16_t>},
        {"readShortInteger",  CeRead<uint16_t>},
        {"readInteger",       CeRead<uint32_t>},
        {"readQword",         CeRead<int64_t>},
        {"readPointer",       CeRead<int64_t>},
        {"readFloat",         CeRead<float>},
        {"readDouble",        CeRead<double>},
        {"readBytes",         CeReadBytes},
        {"readString",        CeReadString},
        {"writeSmallInteger", CeWrite<uint16_t>},
        {"writeShortInteger", CeWrite<uint16_t>},
        {"writeInteger",      CeWrite<uint32_t>},
        {"writeQword",        CeWrite<int64_t>},
        {"writePointer",      CeWrite<int64_t>},
        {"writeFloat",        CeWrite<float>},
        {"writeDouble",       CeWrite<double>},
        {"writeBytes",        CeWriteBytes},
        {"writeString",       CeWriteString},
        {"getAddress",        CeGetAddress},
        {"getAddressSafe",    CeGetAddressSafe},
        {"AOBScan",           CeAobScan},
        {"AOBScanUnique",     CeAobScanUnique},
        {"AOBScanModuleUnique", CeAobScanModuleUnique},
        {"isKeyPressed",      CeIsKeyPressed},
        {"allocateMemory",    CeAllocateMemory},
        {"deAlloc",           CeDeAlloc},
        // this loader's own
        {"CallFunction",      LoaderCallFunction},
        {"isPadButtonPressed", LoaderIsPadButtonPressed},
        {nullptr, nullptr},
    };
    lua_pushglobaltable(L);
    luaL_setfuncs(L, functions, 0);
    lua_pop(L, 1);
    SetKeyCodeGlobals(L);
    SetPadButtonGlobals(L);
}

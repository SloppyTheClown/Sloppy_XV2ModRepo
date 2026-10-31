// Little-endian byte reading/writing shared by the UI file formats. A read
// past the end throws FormatError, so a parser can't run off a damaged file.
#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace xv2ui {

using Bytes = std::vector<uint8_t>;

// A file that doesn't parse (or can't be written back): what and where.
struct FormatError : std::runtime_error {
    explicit FormatError(const std::string& what) : std::runtime_error(what) {}
};

// FNV-1a 64 - the hash the mods and ClaudeTools\iggy\*_abc.py builders print
// for an ABC (offset basis 0xcbf29ce484222325, prime 0x100000001b3).
inline uint64_t Fnv64(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
    return hash;
}

// Sequential reader over a byte range.
class Reader {
public:
    Reader(const uint8_t* data, size_t size, const char* what = "file") : data_(data), size_(size), what_(what) {}

    size_t position() const { return position_; }
    size_t size() const { return size_; }
    size_t left() const { return size_ - position_; }
    bool atEnd() const { return position_ >= size_; }
    void seek(size_t position) { if (position > size_) Fail("seek"); position_ = position; }
    const uint8_t* here() const { return data_ + position_; }

    uint8_t u8() { Need(1); return data_[position_++]; }
    uint16_t u16() { Need(2); uint16_t v; memcpy(&v, data_ + position_, 2); position_ += 2; return v; }
    uint32_t u32() { Need(4); uint32_t v; memcpy(&v, data_ + position_, 4); position_ += 4; return v; }
    uint64_t u64() { Need(8); uint64_t v; memcpy(&v, data_ + position_, 8); position_ += 8; return v; }
    double f64() { Need(8); double v; memcpy(&v, data_ + position_, 8); position_ += 8; return v; }
    // AVM2 signed 24-bit (branch offsets)
    int32_t s24()
    {
        Need(3);
        int32_t v = data_[position_] | (data_[position_ + 1] << 8) | (data_[position_ + 2] << 16);
        position_ += 3;
        return (v & 0x800000) ? v - 0x1000000 : v;
    }
    // AVM2 variable-length unsigned (u30/u32: 7 bits per byte, up to 5 bytes)
    uint32_t u30()
    {
        uint32_t value = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            uint8_t byte = u8();
            value |= (uint32_t)(byte & 0x7f) << shift;
            if (!(byte & 0x80)) return value;
        }
        Fail("u30 longer than 5 bytes");
        return 0;
    }
    // AVM2 s32: read like u32 and truncated (avmplus reads it with the u32
    // code, no sign extension: -1 is ff ff ff ff 0f)
    int32_t s32() { return (int32_t)u30(); }
    void bytes(void* out, size_t count) { Need(count); memcpy(out, data_ + position_, count); position_ += count; }
    Bytes bytes(size_t count) { Need(count); Bytes out(data_ + position_, data_ + position_ + count); position_ += count; return out; }
    void skip(size_t count) { Need(count); position_ += count; }

    [[noreturn]] void Fail(const std::string& what) const
    {
        throw FormatError(std::string(what_) + ": " + what + " at 0x" + Hex(position_));
    }
    static std::string Hex(uint64_t value)
    {
        char text[24];
        snprintf(text, sizeof(text), "%llx", (unsigned long long)value);
        return text;
    }

private:
    void Need(size_t count) const { if (count > size_ - position_) Fail("unexpected end"); }
    const uint8_t* data_;
    size_t size_;
    size_t position_ = 0;
    const char* what_;
};

// Appending writer.
class Writer {
public:
    Bytes out;

    size_t size() const { return out.size(); }
    void u8(uint8_t v) { out.push_back(v); }
    void u16(uint16_t v) { Raw(&v, 2); }
    void u32(uint32_t v) { Raw(&v, 4); }
    void u64(uint64_t v) { Raw(&v, 8); }
    void f64(double v) { Raw(&v, 8); }
    void s24(int32_t v)
    {
        if (v < -0x800000 || v > 0x7fffff) throw FormatError("branch offset does not fit in 24 bits");
        u8(v & 0xff); u8((v >> 8) & 0xff); u8((v >> 16) & 0xff);
    }
    void u30(uint32_t v)
    {
        do {
            uint8_t byte = v & 0x7f;
            v >>= 7;
            if (v) byte |= 0x80;
            out.push_back(byte);
        } while (v);
    }
    void s32(int32_t v) { u30((uint32_t)v); }
    void Raw(const void* data, size_t count)
    {
        const uint8_t* bytes = (const uint8_t*)data;
        out.insert(out.end(), bytes, bytes + count);
    }
    void Append(const Bytes& bytes) { out.insert(out.end(), bytes.begin(), bytes.end()); }
    void Align(size_t alignment) { while (out.size() % alignment) out.push_back(0); }
    void PutU32At(size_t at, uint32_t v) { memcpy(&out[at], &v, 4); }
    void PutU64At(size_t at, uint64_t v) { memcpy(&out[at], &v, 8); }
};

// Bytes of a size that u30 encodes in.
inline size_t U30Size(uint32_t v)
{
    size_t size = 1;
    while (v >>= 7) ++size;
    return size;
}

inline uint32_t GetU32(const uint8_t* at) { uint32_t v; memcpy(&v, at, 4); return v; }
inline uint64_t GetU64(const uint8_t* at) { uint64_t v; memcpy(&v, at, 8); return v; }
inline void PutU32(uint8_t* at, uint32_t v) { memcpy(at, &v, 4); }
inline void PutU64(uint8_t* at, uint64_t v) { memcpy(at, &v, 8); }

}  // namespace xv2ui

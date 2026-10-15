// Small helpers for reading and patching the game's little-endian binary
// files (EAN, ESK, EMD, BCS, BDM, BAC), shared by ean_fit.cpp and
// prop_fit.cpp.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace binary {

// Bounds-checked reads from a file in memory.
struct Reader {
    const uint8_t* data;
    size_t size;
    bool Has(size_t offset, size_t length) const { return offset <= size && length <= size - offset; }
    template <class T> bool Get(size_t offset, T& value) const
    {
        if (!Has(offset, sizeof(T))) return false;
        memcpy(&value, data + offset, sizeof(T));
        return true;
    }
    // A zero-terminated name at `offset`, or empty.
    std::string Name(size_t offset) const
    {
        if (offset >= size) return {};
        const char* start = (const char*)data + offset;
        return std::string(start, strnlen(start, size - offset));
    }
};

// Unchecked access to a buffer the caller has already validated.
template <class T> T Load(const std::vector<uint8_t>& bytes, size_t offset)
{
    T value;
    memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}
template <class T> void Store(std::vector<uint8_t>& bytes, size_t offset, T value)
{
    memcpy(bytes.data() + offset, &value, sizeof(T));
}

inline void Align16(std::vector<uint8_t>& out) { out.resize((out.size() + 15) & ~(size_t)15, 0); }

// Appends out[from, from + length) to out (a temporary copy: inserting a
// vector's own range into it is undefined once it reallocates).
inline void AppendOwnBytes(std::vector<uint8_t>& out, size_t from, size_t length)
{
    std::vector<uint8_t> copy(out.begin() + from, out.begin() + from + length);
    out.insert(out.end(), copy.begin(), copy.end());
}

// Appends a zero-terminated string; returns where it starts.
inline size_t AppendString(std::vector<uint8_t>& out, const std::string& text)
{
    size_t at = out.size();
    out.insert(out.end(), text.begin(), text.end());
    out.push_back(0);
    return at;
}

inline float HalfToFloat(uint16_t half)
{
    uint32_t sign = (half >> 15) & 1, exponent = (half >> 10) & 0x1f, mantissa = half & 0x3ff;
    float value;
    if (exponent == 0) value = std::ldexp((float)mantissa, -24);
    else if (exponent == 31) value = mantissa ? NAN : INFINITY;
    else value = std::ldexp((float)(mantissa | 0x400), (int)exponent - 25);
    return sign ? -value : value;
}

inline uint16_t FloatToHalf(float value)
{
    uint16_t sign = std::signbit(value) ? 0x8000 : 0;
    float magnitude = std::fabs(value);
    if (std::isnan(value)) return 0x7e00;
    if (magnitude >= 65520.0f) return sign | 0x7c00;          // overflow -> infinity
    if (magnitude < std::ldexp(1.0f, -24) / 2) return sign;   // underflow -> zero
    int exponent;
    float fraction = std::frexp(magnitude, &exponent);  // magnitude = fraction * 2^exponent, fraction in [0.5, 1)
    int halfExponent = exponent + 14;
    if (halfExponent <= 0) {  // subnormal half: units of 2^-24
        return sign | (uint16_t)std::lround(std::ldexp(magnitude, 24));
    }
    long mantissa = std::lround((fraction * 2.0f - 1.0f) * 1024.0f);
    if (mantissa == 1024) { mantissa = 0; ++halfExponent; }
    if (halfExponent >= 31) return sign | 0x7c00;
    return sign | (uint16_t)(halfExponent << 10) | (uint16_t)mantissa;
}

}  // namespace binary

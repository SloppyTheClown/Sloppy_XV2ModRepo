#include "x2d_read.h"
#include <windows.h>
#include <algorithm>
#include <cstring>

namespace {

// Zip layout (PKWARE APPNOTE): end record, central directory entries, local headers.
constexpr uint32_t kZipEndSignature = 0x06054b50, kZipCentralSignature = 0x02014b50, kZipLocalSignature = 0x04034b50;
constexpr size_t kZipEndSize = 22, kZipCentralSize = 46, kZipLocalSize = 30;
constexpr size_t kZipMaxComment = 0xffff;
constexpr uint16_t kZipStored = 0, kZipDeflated = 8;
constexpr uint32_t kMaxEntryBytes = 16 * 1024 * 1024; // an x2m.xml is kilobytes; refuses a damaged size

uint16_t Le16(const uint8_t *data) { return static_cast<uint16_t>(data[0] | data[1] << 8); }
uint32_t Le32(const uint8_t *data) { return Le16(data) | static_cast<uint32_t>(Le16(data + 2)) << 16; }

// DEFLATE (RFC 1951), decoding Huffman codes a bit at a time: the inputs are small and
// read once per listed mod blast.
class Inflater {
public:
    Inflater(const uint8_t *in, size_t size, std::vector<uint8_t> &out, size_t limit)
        : in_(in), size_(size), out_(out), limit_(limit) {}

    bool Run()
    {
        bool last = false;
        while (!last) {
            int header = Bits(3);
            if (failed_) return false;
            last = header & 1;
            int type = header >> 1;
            bool ok = type == 0 ? Stored() : type == 1 ? Fixed() : type == 2 ? Dynamic() : false;
            if (!ok) return false;
        }
        return !failed_;
    }

private:
    static constexpr int kMaxBits = 15, kLiteralCodes = 288, kDistanceCodes = 30, kLengthCodeCount = 19;
    struct Huffman { short count[kMaxBits + 1]; short symbol[kLiteralCodes]; };

    int Bits(int need)
    {
        int value = 0;
        for (int got = 0; got < need; got++) {
            if (bitCount_ == 0) {
                if (pos_ >= size_) { failed_ = true; return 0; }
                bitBuffer_ = in_[pos_++];
                bitCount_ = 8;
            }
            value |= (bitBuffer_ & 1) << got;
            bitBuffer_ >>= 1;
            bitCount_--;
        }
        return value;
    }

    // False on an over-subscribed set of lengths (not a valid code).
    static bool Build(Huffman &table, const short *lengths, int count)
    {
        memset(table.count, 0, sizeof(table.count));
        for (int i = 0; i < count; i++) table.count[lengths[i]]++;
        int left = 1;
        for (int bits = 1; bits <= kMaxBits; bits++) {
            left = left * 2 - table.count[bits];
            if (left < 0) return false;
        }
        short offsets[kMaxBits + 1] = {};
        for (int bits = 1; bits < kMaxBits; bits++) offsets[bits + 1] = static_cast<short>(offsets[bits] + table.count[bits]);
        for (int i = 0; i < count; i++)
            if (lengths[i]) table.symbol[offsets[lengths[i]]++] = static_cast<short>(i);
        table.count[0] = 0;
        return true;
    }

    int Decode(const Huffman &table)
    {
        int code = 0, first = 0, index = 0;
        for (int bits = 1; bits <= kMaxBits; bits++) {
            code |= Bits(1);
            if (failed_) return -1;
            int count = table.count[bits];
            if (code - count < first) return table.symbol[index + (code - first)];
            index += count;
            first = (first + count) * 2;
            code *= 2;
        }
        return -1;
    }

    bool Stored()
    {
        bitCount_ = 0; // the rest of this byte is padding
        if (pos_ + 4 > size_) return false;
        size_t length = Le16(in_ + pos_);
        pos_ += 4;
        if (pos_ + length > size_ || out_.size() + length > limit_) return false;
        out_.insert(out_.end(), in_ + pos_, in_ + pos_ + length);
        pos_ += length;
        return true;
    }

    bool Codes(const Huffman &literals, const Huffman &distances)
    {
        // RFC 1951 3.2.5
        static const short lengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const short lengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const short distanceBase[kDistanceCodes] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const short distanceExtra[kDistanceCodes] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            int symbol = Decode(literals);
            if (symbol < 0) return false;
            if (symbol == 256) return true;
            if (symbol < 256) {
                if (out_.size() >= limit_) return false;
                out_.push_back(static_cast<uint8_t>(symbol));
                continue;
            }
            symbol -= 257;
            if (symbol >= 29) return false;
            size_t length = lengthBase[symbol] + static_cast<size_t>(Bits(lengthExtra[symbol]));
            int code = Decode(distances);
            if (code < 0 || code >= kDistanceCodes) return false;
            size_t distance = static_cast<uint16_t>(distanceBase[code]) + static_cast<size_t>(Bits(distanceExtra[code]));
            if (failed_ || distance > out_.size() || out_.size() + length > limit_) return false;
            for (size_t i = 0; i < length; i++) out_.push_back(out_[out_.size() - distance]);
        }
    }

    bool Fixed()
    {
        short lengths[kLiteralCodes + kDistanceCodes];
        for (int i = 0; i < kLiteralCodes; i++) lengths[i] = static_cast<short>(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
        for (int i = 0; i < kDistanceCodes; i++) lengths[kLiteralCodes + i] = 5;
        Huffman literals, distances;
        return Build(literals, lengths, kLiteralCodes) && Build(distances, lengths + kLiteralCodes, kDistanceCodes) && Codes(literals, distances);
    }

    bool Dynamic()
    {
        static const uint8_t order[kLengthCodeCount] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        int literalCount = Bits(5) + 257, distanceCount = Bits(5) + 1, lengthCodes = Bits(4) + 4;
        if (failed_ || literalCount > kLiteralCodes || distanceCount > kDistanceCodes) return false;
        short lengths[kLiteralCodes + kDistanceCodes] = {};
        for (int i = 0; i < lengthCodes; i++) lengths[order[i]] = static_cast<short>(Bits(3));
        Huffman codeLengths;
        if (!Build(codeLengths, lengths, kLengthCodeCount)) return false;
        memset(lengths, 0, sizeof(lengths));
        for (int i = 0; i < literalCount + distanceCount;) {
            int symbol = Decode(codeLengths);
            if (symbol < 0) return false;
            if (symbol < 16) { lengths[i++] = static_cast<short>(symbol); continue; }
            short previous = 0;
            int repeat;
            if (symbol == 16) {
                if (i == 0) return false;
                previous = lengths[i - 1];
                repeat = 3 + Bits(2);
            } else {
                repeat = symbol == 17 ? 3 + Bits(3) : 11 + Bits(7);
            }
            if (failed_ || i + repeat > literalCount + distanceCount) return false;
            while (repeat--) lengths[i++] = previous;
        }
        if (lengths[256] == 0) return false; // no end-of-block code
        Huffman literals, distances;
        return Build(literals, lengths, literalCount) && Build(distances, lengths + literalCount, distanceCount) && Codes(literals, distances);
    }

    const uint8_t *in_;
    size_t size_, pos_ = 0;
    std::vector<uint8_t> &out_;
    size_t limit_;
    int bitBuffer_ = 0, bitCount_ = 0;
    bool failed_ = false;
};

// x2m.xml files are UTF-8, or Latin-1 from older installers ("Rose" with an accent).
std::wstring DecodeText(const std::string &text)
{
    if (text.empty()) return L"";
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int chars = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (chars <= 0) {
        codePage = 1252;
        flags = 0;
        chars = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
    }
    if (chars <= 0) return L"";
    std::wstring wide(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), &wide[0], chars);
    return wide;
}

std::wstring DecodeXmlEntities(const std::wstring &text)
{
    static const struct { const wchar_t *entity; wchar_t character; } named[] = {
        {L"&amp;", L'&'}, {L"&apos;", L'\''}, {L"&quot;", L'"'}, {L"&lt;", L'<'}, {L"&gt;", L'>'},
    };
    std::wstring decoded;
    for (size_t i = 0; i < text.size();) {
        bool matched = false;
        if (text[i] == L'&')
            for (const auto &item : named) {
                size_t length = wcslen(item.entity);
                if (text.compare(i, length, item.entity) != 0) continue;
                decoded += item.character;
                i += length;
                matched = true;
                break;
            }
        if (!matched) decoded += text[i++];
    }
    return decoded;
}

} // namespace

bool ReadZipEntry(const std::vector<uint8_t> &zip, const char *name, std::vector<uint8_t> &out)
{
    if (zip.size() < kZipEndSize) return false;
    size_t end = zip.size() - kZipEndSize;
    const size_t lowest = end > kZipMaxComment ? end - kZipMaxComment : 0;
    while (Le32(zip.data() + end) != kZipEndSignature) {
        if (end == lowest) return false;
        end--;
    }
    const uint32_t entries = Le16(zip.data() + end + 10);
    size_t at = Le32(zip.data() + end + 16);
    const size_t nameLength = strlen(name);
    for (uint32_t i = 0; i < entries; i++) {
        if (at + kZipCentralSize > zip.size() || Le32(zip.data() + at) != kZipCentralSignature) return false;
        const uint8_t *central = zip.data() + at;
        const size_t entryNameLength = Le16(central + 28);
        const size_t entryEnd = at + kZipCentralSize + entryNameLength + Le16(central + 30) + Le16(central + 32);
        if (entryEnd > zip.size()) return false;
        if (entryNameLength == nameLength && memcmp(central + kZipCentralSize, name, nameLength) == 0) {
            const uint16_t method = Le16(central + 10);
            const uint32_t packed = Le32(central + 20), unpacked = Le32(central + 24);
            const size_t local = Le32(central + 42);
            if (unpacked > kMaxEntryBytes || local + kZipLocalSize > zip.size() || Le32(zip.data() + local) != kZipLocalSignature) return false;
            const size_t data = local + kZipLocalSize + Le16(zip.data() + local + 26) + Le16(zip.data() + local + 28);
            if (data > zip.size() || packed > zip.size() - data) return false;
            out.clear();
            if (method == kZipStored) {
                out.assign(zip.begin() + data, zip.begin() + data + packed);
                return true;
            }
            if (method != kZipDeflated) return false;
            out.reserve(unpacked);
            return Inflater(zip.data() + data, packed, out, unpacked).Run() && out.size() == unpacked;
        }
        at = entryEnd;
    }
    return false;
}

std::wstring X2mXmlValue(const std::vector<uint8_t> &xml, const char *tag)
{
    const std::string text(xml.begin(), xml.end());
    const std::string open = std::string("<") + tag;
    const size_t tagAt = text.find(open);
    if (tagAt == std::string::npos) return L"";
    const size_t tagEnd = text.find('>', tagAt);
    const size_t value = text.find("value=\"", tagAt);
    if (tagEnd == std::string::npos || value == std::string::npos || value > tagEnd) return L"";
    const size_t start = value + 7;
    const size_t stop = text.find('"', start);
    if (stop == std::string::npos) return L"";
    return DecodeXmlEntities(DecodeText(text.substr(start, stop - start)));
}

std::string X2mIniGuid(const std::vector<uint8_t> &ini)
{
    const std::string text(ini.begin(), ini.end());
    const size_t key = text.find("GUID");
    if (key == std::string::npos) return "";
    const size_t start = text.find('"', key);
    if (start == std::string::npos) return "";
    const size_t stop = text.find('"', start + 1);
    if (stop == std::string::npos) return "";
    std::string guid = text.substr(start + 1, stop - start - 1);
    const bool valid = !guid.empty() && std::all_of(guid.begin(), guid.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-';
    });
    return valid ? guid : "";
}

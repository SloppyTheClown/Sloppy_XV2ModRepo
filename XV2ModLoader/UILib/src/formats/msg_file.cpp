// MSG parse/write + keyed text edits (layout in msg_file.h).
#include "msg_file.h"
#include <algorithm>

namespace xv2ui {

namespace {

constexpr size_t kHeaderSize = 32;
constexpr uint16_t kUnicodeNamesFlag = 256;   // u16 at 4 (Xv2CoreLib Parser.cs)
constexpr uint16_t kUnicodeTextFlag = 1;      // u16 at 6

// One string at `offset` of `characters` characters, UTF-16 or 8-bit (each
// byte widened, so an 8-bit file writes back byte for byte).
std::u16string ReadString(const uint8_t* data, size_t size, uint32_t offset, uint32_t characters, bool unicode)
{
    const size_t bytes = (size_t)characters * (unicode ? 2 : 1);
    if (offset > size || bytes > size - offset) throw FormatError("msg: string at 0x" + Reader::Hex(offset) + " runs past the end");
    std::u16string text(characters, u'\0');
    for (uint32_t i = 0; i < characters; ++i)
        text[i] = unicode ? (char16_t)(data[offset + i * 2] | (data[offset + i * 2 + 1] << 8)) : (char16_t)data[offset + i];
    return text;
}

void WriteString(Writer& writer, const std::u16string& text, bool unicode)
{
    for (char16_t c : text) {
        if (unicode) writer.u16(c);
        else writer.u8((uint8_t)c);
    }
    if (unicode) writer.u16(0);
    else writer.u8(0);
}

uint32_t StringSize(const std::u16string& text, bool unicode)
{
    return (uint32_t)(unicode ? text.size() * 2 + 2 : text.size() + 1);
}

// Format parameters in a text ("%ls", "%d"; "%%" is a literal %) - what a
// line record's parameter count holds in the game's files.
uint32_t CountParameters(const std::u16string& text)
{
    uint32_t count = 0;
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        if (text[i] != u'%') continue;
        if (text[i + 1] == u'%') { ++i; continue; }
        ++count;
    }
    return count;
}

}  // namespace

void MsgFile::Parse(const uint8_t* data, size_t size)
{
    if (!Looks(data, size)) throw FormatError("msg: not a #MSG file");
    Reader header(data, size, "msg");
    header.seek(4);
    unicodeNames = header.u16() == kUnicodeNamesFlag;
    unicodeText = header.u16() == kUnicodeTextFlag;
    const uint32_t count = header.u32();
    const uint32_t namesAt = header.u32(), idsAt = header.u32(), listsAt = header.u32();
    if (count > size / 16) throw FormatError("msg: entry count " + std::to_string(count) + " too large");

    entries.assign(count, MsgEntry());
    Reader names(data, size, "msg names"), ids(data, size, "msg ids"), lists(data, size, "msg line lists");
    names.seek(namesAt);
    ids.seek(idsAt);
    lists.seek(listsAt);
    for (MsgEntry& entry : entries) {
        const uint32_t nameOffset = names.u32(), nameLength = names.u32();
        names.u32();                                   // size in bytes: rewritten from the length
        entry.nameUnknown = names.u32();
        entry.name = ReadString(data, size, nameOffset, nameLength, unicodeNames);
        entry.id = ids.u32();

        const uint32_t lineCount = lists.u32(), linesAt = lists.u32();
        if (lineCount > size / 16) throw FormatError("msg: line count too large");
        Reader lines(data, size, "msg lines");
        lines.seek(linesAt);
        entry.lines.resize(lineCount);
        for (MsgLine& line : entry.lines) {
            const uint32_t textOffset = lines.u32(), textLength = lines.u32();
            lines.u32();
            line.parameters = lines.u32();
            line.text = ReadString(data, size, textOffset, textLength, unicodeText);
        }
    }
}

// Written in Xv2CoreLib's order (Deserializer.cs): header, name records, ids,
// line lists, every line record, name strings, line strings.
Bytes MsgFile::Write() const
{
    Writer writer;
    writer.Raw("#MSG", 4);
    writer.u16(unicodeNames ? kUnicodeNamesFlag : 0);
    writer.u16(unicodeText ? kUnicodeTextFlag : 0);
    writer.u32((uint32_t)entries.size());
    size_t lineTotal = 0;
    for (const MsgEntry& entry : entries) lineTotal += entry.lines.size();
    const size_t namesAt = kHeaderSize;
    const size_t idsAt = namesAt + entries.size() * 16;
    const size_t listsAt = idsAt + entries.size() * 4;
    const size_t linesAt = listsAt + entries.size() * 8;
    // An empty file points nowhere (41 of the game's, e.g. qx_target01_*.msg).
    const bool empty = entries.empty();
    writer.u32(empty ? 0 : (uint32_t)namesAt);
    writer.u32(empty ? 0 : (uint32_t)idsAt);
    writer.u32(empty ? 0 : (uint32_t)listsAt);
    writer.u32((uint32_t)lineTotal);
    writer.u32(empty ? 0 : (uint32_t)linesAt);

    // String offsets follow from the sizes, so the records are written in
    // one pass: names start after the line records, texts after the names.
    size_t stringAt = linesAt + lineTotal * 16;
    for (const MsgEntry& entry : entries) {
        writer.u32((uint32_t)stringAt);
        writer.u32((uint32_t)entry.name.size());
        writer.u32(StringSize(entry.name, unicodeNames));
        writer.u32(entry.nameUnknown);
        stringAt += StringSize(entry.name, unicodeNames);
    }
    for (const MsgEntry& entry : entries) writer.u32(entry.id);
    size_t lineRecordAt = linesAt;
    for (const MsgEntry& entry : entries) {
        writer.u32((uint32_t)entry.lines.size());
        writer.u32((uint32_t)lineRecordAt);
        lineRecordAt += entry.lines.size() * 16;
    }
    for (const MsgEntry& entry : entries) {
        for (const MsgLine& line : entry.lines) {
            writer.u32((uint32_t)stringAt);
            writer.u32((uint32_t)line.text.size());
            writer.u32(StringSize(line.text, unicodeText));
            writer.u32(line.parameters);
            stringAt += StringSize(line.text, unicodeText);
        }
    }
    for (const MsgEntry& entry : entries) WriteString(writer, entry.name, unicodeNames);
    for (const MsgEntry& entry : entries)
        for (const MsgLine& line : entry.lines) WriteString(writer, line.text, unicodeText);
    if (writer.size() > 0xffffffffull) throw FormatError("msg: file over 4 GB");
    return writer.out;
}

int MsgFile::Find(const std::u16string& name) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == name) return (int)i;
    return -1;
}

std::u16string MsgText(const std::string& utf8)
{
    std::u16string out;
    for (size_t i = 0; i < utf8.size();) {
        const uint8_t lead = (uint8_t)utf8[i];
        uint32_t codePoint;
        size_t extra;
        if (lead < 0x80) { codePoint = lead; extra = 0; }
        else if ((lead & 0xe0) == 0xc0) { codePoint = lead & 0x1f; extra = 1; }
        else if ((lead & 0xf0) == 0xe0) { codePoint = lead & 0x0f; extra = 2; }
        else if ((lead & 0xf8) == 0xf0) { codePoint = lead & 0x07; extra = 3; }
        else throw FormatError("text is not UTF-8 (byte " + std::to_string(i) + ")");
        if (extra > utf8.size() - i - 1) throw FormatError("text is not UTF-8 (cut off at the end)");
        for (size_t k = 1; k <= extra; ++k) {
            const uint8_t next = (uint8_t)utf8[i + k];
            if ((next & 0xc0) != 0x80) throw FormatError("text is not UTF-8 (byte " + std::to_string(i + k) + ")");
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        i += extra + 1;
        switch (codePoint) {   // the entities the game's own text uses
        case '&': out += u"&amp;"; continue;
        case '<': out += u"&lt;"; continue;
        case '>': out += u"&gt;"; continue;
        case '\'': out += u"&apos;"; continue;
        case '"': out += u"&quot;"; continue;
        }
        if (codePoint >= 0x10000) {
            codePoint -= 0x10000;
            out += (char16_t)(0xd800 + (codePoint >> 10));
            out += (char16_t)(0xdc00 + (codePoint & 0x3ff));
        } else {
            out += (char16_t)codePoint;
        }
    }
    return out;
}

std::u16string MsgName(const std::string& ascii)
{
    return std::u16string(ascii.begin(), ascii.end());
}

std::string MessageReport::Summary() const
{
    std::string text = std::to_string(replaced) + " message" + (replaced == 1 ? "" : "s") + " replaced";
    if (overEarlier) text += " (" + std::to_string(overEarlier) + " over an earlier mod's)";
    text += ", " + std::to_string(added) + " added";
    if (unchanged) text += ", " + std::to_string(unchanged) + " already the same";
    return text;
}

std::string CheckMessage(const std::string& path, const std::string& name)
{
    std::string extension = path.size() >= 4 ? path.substr(path.size() - 4) : "";
    for (char& c : extension) c = (char)tolower((unsigned char)c);
    if (extension != ".msg") return "not a .msg file";
    if (name.empty()) return "no entry name";
    for (char c : name)
        if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7e) return "the entry name must be plain ASCII without spaces";
    return "";
}

Bytes ApplyMessages(const Bytes& originalBytes, const Bytes& currentBytes, const std::map<std::string, std::u16string>& messages,
                    MessageReport* report)
{
    MessageReport local;
    MessageReport& result = report ? *report : local;
    MsgFile original, current;
    original.Parse(originalBytes.data(), originalBytes.size());
    current.Parse(currentBytes.data(), currentBytes.size());

    bool changed = false;
    for (const auto& [name, text] : messages) {
        const std::u16string key = MsgName(name);
        const int index = current.Find(key);
        if (index < 0) {
            uint32_t nextId = 0;
            for (const MsgEntry& entry : current.entries) nextId = std::max(nextId, entry.id + 1);
            MsgEntry entry;
            entry.name = key;
            entry.id = nextId;
            entry.lines.push_back({text, CountParameters(text)});
            current.entries.push_back(std::move(entry));
            ++result.added;
            changed = true;
            continue;
        }
        MsgEntry& entry = current.entries[index];
        if (entry.lines.empty()) entry.lines.push_back({u"", CountParameters(text)});
        if (entry.lines[0].text == text) { ++result.unchanged; continue; }
        const int originalIndex = original.Find(key);
        const bool earlier = originalIndex < 0
            ? true
            : original.entries[originalIndex].lines.empty() || original.entries[originalIndex].lines[0].text != entry.lines[0].text;
        if (earlier) {
            ++result.overEarlier;
            result.notes.push_back("message " + name + " was changed by an earlier mod; this one replaces it");
        }
        entry.lines[0].text = text;
        ++result.replaced;
        changed = true;
    }
    if (!changed) return currentBytes;
    // A file that stores 8-bit text can't take other characters.
    if (!current.unicodeText)
        for (const auto& [name, text] : messages)
            for (char16_t c : text)
                if (c > 0xff) throw FormatError("message " + name + ": this msg file stores 8-bit text, the text has wider characters");
    return current.Write();
}

}  // namespace xv2ui

// Display-list edits of an .iggy menu. Record layouts (all found in
// AVATAR.iggy 1.27, Research_Notes\ui-iggy-display-list.md): offsets are movie
// offsets, pointers are index fields (iggy_file.h), 1 = null.
#include "display_edit.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <set>
#include "iggy_file.h"

namespace xv2ui {

namespace {

// Sprite definition (character type 03 ff).
const uint8_t kSpriteTypeByte = 0x03, kSpriteMarkByte = 0xff;
const uint32_t kSpriteIdField = 2;          // u16 character id
const uint32_t kSpriteFrameCount = 0x28;    // u16
const uint32_t kSpriteDepthCount = 0x2a;    // u16: highest depth + 1
const uint32_t kSpriteFirstTag = 0x38;      // pointer
const uint32_t kSpriteLastTag = 0x40;       // pointer
const uint32_t kSpriteMinSize = 0x48;

// Timeline tags: a singly linked list, +0 next pointer, byte +8 the type.
const uint32_t kTagNext = 0, kTagType = 8, kTagFlags = 9;
enum TagType : uint8_t { kShowFrame = 1, kPlaceObject = 2, kRemoveObject = 3, kFrameLabel = 5 };
const uint32_t kLabelText = 0x10;           // FrameLabel: pointer to inline UTF-16
const uint32_t kRemoveDepth = 0xa;          // Remove: u16 (seen 1, 2 in AVATAR's depth-1 sprites; unverified)
const uint32_t kTagMinSize = 0x18;          // the smallest tag (Remove)

// PlaceObject (type 2).
const uint32_t kPlaceMatrix = 0x10;         // 6 x f32: a b c d tx ty
const uint32_t kPlaceTx = kPlaceMatrix + 16, kPlaceTy = kPlaceMatrix + 20;
const uint32_t kPlaceDepth = 0x28;          // u64
const uint32_t kPlaceCharacter = 0x30;      // pointer
const uint32_t kPlaceMinSize = 0x60;
// Flags byte -> record size and where the name pointer is (0 = no name). The
// 0x41 and 0x21 records can keep the name inline 0x60 bytes in (16 bytes).
const uint32_t kInlineNameOffset = 0x60, kInlineNameBytes = 16;

struct PlaceLayout { uint8_t flags; uint32_t size; uint32_t namePointer; };
const PlaceLayout kPlaceLayouts[] = {
    {0x41, 0x70, 0x40},
    {0x07, 0x80, 0},
    {0x05, 0x60, 0},
    {0x21, 0x80, 0x50},
};

const PlaceLayout* LayoutOf(uint8_t flags)
{
    for (const PlaceLayout& layout : kPlaceLayouts)
        if (layout.flags == flags) return &layout;
    return nullptr;
}

// Name pointer offset of any PlaceObject flags (0 = none), also for flags we
// can't copy: only used to find names.
uint32_t NamePointerOf(uint8_t flags)
{
    const PlaceLayout* layout = LayoutOf(flags);
    return layout ? layout->namePointer : 0;
}

void Need(const IggyMovie& movie, uint64_t at, uint64_t size)
{
    if (at + size > movie.data.size()) throw FormatError("display list: offset 0x" + Reader::Hex(at) + " outside the movie");
}

uint16_t U16(const IggyMovie& movie, uint32_t at)
{
    Need(movie, at, 2);
    uint16_t value;
    memcpy(&value, &movie.data[at], 2);
    return value;
}

float F32(const IggyMovie& movie, uint32_t at)
{
    Need(movie, at, 4);
    float value;
    memcpy(&value, &movie.data[at], 4);
    return value;
}

void PutF32(IggyMovie& movie, uint32_t at, float value)
{
    Need(movie, at, 4);
    memcpy(&movie.data[at], &value, 4);
}

// The ASCII text of the UTF-16 string at `at` (other characters become '?').
std::string WideAscii(const IggyMovie& movie, uint32_t at)
{
    std::string text;
    for (uint32_t i = 0; i < 256; ++i) {
        const uint16_t unit = U16(movie, at + 2 * i);
        if (!unit) return text;
        text += unit < 0x80 ? (char)unit : '?';
    }
    throw FormatError("display list: unterminated name at 0x" + Reader::Hex(at));
}

std::string WideAsciiAt(const IggyMovie& movie, uint32_t pointerField)
{
    const int64_t target = movie.Pointer(pointerField);
    return target < 0 ? std::string() : WideAscii(movie, (uint32_t)target);
}

// --- Reading sprites ---------------------------------------------------------

// Every sprite definition: the targets of the index's pointers that start
// 03 ff and have a first-tag pointer field.
std::vector<uint32_t> SpriteDefinitions(const IggyMovie& movie)
{
    std::set<uint32_t> found;
    for (const auto& field : movie.fields) {
        if (field.second != kFieldPointer) continue;
        const int64_t target = movie.Pointer(field.first);
        if (target < 0 || (uint64_t)target + kSpriteMinSize > movie.data.size()) continue;
        const uint32_t at = (uint32_t)target;
        if (movie.data[at] != kSpriteTypeByte || movie.data[at + 1] != kSpriteMarkByte) continue;
        const auto first = movie.fields.find(at + kSpriteFirstTag);
        if (first == movie.fields.end() || first->second != kFieldPointer) continue;
        found.insert(at);
    }
    return std::vector<uint32_t>(found.begin(), found.end());
}

// The sprite's tags in list order.
std::vector<uint32_t> TagsOf(const IggyMovie& movie, uint32_t sprite)
{
    std::vector<uint32_t> tags;
    int64_t tag = movie.Pointer(sprite + kSpriteFirstTag);
    while (tag >= 0) {
        Need(movie, (uint64_t)tag, kTagMinSize);
        tags.push_back((uint32_t)tag);
        if (tags.size() > movie.data.size() / kTagMinSize) throw FormatError("display list: tag list loops");
        tag = movie.Pointer((uint32_t)tag + kTagNext);
    }
    return tags;
}

uint8_t TagTypeOf(const IggyMovie& movie, uint32_t tag) { return movie.data[tag + kTagType]; }

// PlaceObject's flags, checked to be a complete record.
uint8_t PlaceFlags(const IggyMovie& movie, uint32_t tag)
{
    Need(movie, tag, kPlaceMinSize);
    return movie.data[tag + kTagFlags];
}

std::string PlaceName(const IggyMovie& movie, uint32_t tag)
{
    const uint32_t pointer = NamePointerOf(PlaceFlags(movie, tag));
    return pointer ? WideAsciiAt(movie, tag + pointer) : std::string();
}

uint32_t PlaceDepth(const IggyMovie& movie, uint32_t tag)
{
    Need(movie, tag, kPlaceMinSize);
    uint64_t depth = GetU64(&movie.data[tag + kPlaceDepth]);
    if (depth > 0xffff) throw FormatError("display list: depth " + std::to_string(depth) + " out of range");
    return (uint32_t)depth;
}

// The sprite a text names: "#<id>" or the instance name of its placements.
uint32_t ResolveSprite(const IggyMovie& movie, const std::string& text)
{
    const std::vector<uint32_t> sprites = SpriteDefinitions(movie);
    if (!text.empty() && text[0] == '#') {
        const unsigned long id = strtoul(text.c_str() + 1, nullptr, 10);
        uint32_t found = 0;
        size_t count = 0;
        for (uint32_t sprite : sprites)
            if (U16(movie, sprite + kSpriteIdField) == id) { found = sprite; ++count; }
        if (count != 1) throw FormatError("sprite " + text + ": " + (count ? "several sprites with that id" : "no such sprite"));
        return found;
    }
    std::set<uint32_t> targets;
    for (uint32_t sprite : sprites) {
        for (uint32_t tag : TagsOf(movie, sprite)) {
            if (TagTypeOf(movie, tag) != kPlaceObject || PlaceName(movie, tag) != text) continue;
            const int64_t character = movie.Pointer(tag + kPlaceCharacter);
            if (character >= 0) targets.insert((uint32_t)character);
        }
    }
    if (targets.empty()) throw FormatError("sprite " + text + ": no placement has that name");
    if (targets.size() > 1) throw FormatError("sprite " + text + ": placements of that name are different sprites");
    const uint32_t sprite = *targets.begin();
    if (!std::binary_search(sprites.begin(), sprites.end(), sprite))
        throw FormatError("sprite " + text + ": the placements are not a sprite");
    return sprite;
}

// The depth "<name>" (first placement with it) or "@<depth>" means in `tags`.
uint32_t ResolveDepth(const IggyMovie& movie, const std::vector<uint32_t>& tags, const std::string& text)
{
    if (!text.empty() && text[0] == '@') {
        char* end = nullptr;
        const unsigned long depth = strtoul(text.c_str() + 1, &end, 10);
        if (end == text.c_str() + 1 || *end || depth > 0xffff) throw FormatError("bad depth " + text);
        return (uint32_t)depth;
    }
    for (uint32_t tag : tags)
        if (TagTypeOf(movie, tag) == kPlaceObject && PlaceName(movie, tag) == text) return PlaceDepth(movie, tag);
    throw FormatError("no placement named " + text + " in the sprite");
}

bool HasPlacementNamed(const IggyMovie& movie, const std::vector<uint32_t>& tags, const std::string& name)
{
    for (uint32_t tag : tags)
        if (TagTypeOf(movie, tag) == kPlaceObject && PlaceName(movie, tag) == name) return true;
    return false;
}

// --- clone -------------------------------------------------------------------

// Copies the record at `source` (size bytes, with the index fields in it) to
// the end of the movie; returns the copy.
uint32_t CopyRecord(IggyMovie& movie, uint32_t source, uint32_t size)
{
    Need(movie, source, size);
    const Bytes bytes(movie.data.begin() + source, movie.data.begin() + source + size);
    const uint32_t copy = movie.Append(bytes.data(), bytes.size(), 16);
    std::vector<std::pair<uint32_t, uint8_t>> inside;
    for (auto it = movie.fields.lower_bound(source); it != movie.fields.end() && it->first < source + size; ++it)
        inside.emplace_back(it->first - source, it->second);
    for (const auto& field : inside) {
        if (field.second == kFieldPointer) {
            int64_t target = movie.Pointer(source + field.first);
            // a pointer into the record itself (the inline name) follows the copy
            if (target >= (int64_t)source && target < (int64_t)(source + size)) target = target - source + copy;
            movie.SetPointer(copy + field.first, target);
        } else {
            movie.fields[copy + field.first] = field.second;
        }
    }
    return copy;
}

// Points the copy's name at `name` (UTF-16): inline in the record when the
// source's name was inline too and it fits (7 characters + the terminator in
// the 16 bytes), else a string of its own.
void RenameCopy(IggyMovie& movie, uint32_t source, uint32_t copy, uint32_t namePointer, const std::string& name)
{
    const bool inline_ = movie.Pointer(source + namePointer) == (int64_t)(source + kInlineNameOffset);
    Bytes text;
    for (char c : name) { text.push_back((uint8_t)c); text.push_back(0); }
    text.push_back(0);
    text.push_back(0);
    if (inline_ && text.size() <= kInlineNameBytes) {
        memset(&movie.data[copy + kInlineNameOffset], 0, kInlineNameBytes);
        memcpy(&movie.data[copy + kInlineNameOffset], text.data(), text.size());
        movie.SetPointer(copy + namePointer, (int64_t)(copy + kInlineNameOffset));
    } else {
        if (inline_) memset(&movie.data[copy + kInlineNameOffset], 0, kInlineNameBytes);
        movie.SetPointer(copy + namePointer, (int64_t)movie.Append(text.data(), text.size(), 8));
    }
}

void Clone(IggyMovie& movie, const DisplayCommand& command, DisplayReport& report)
{
    const uint32_t sprite = ResolveSprite(movie, command.sprite);
    const std::vector<uint32_t> tags = TagsOf(movie, sprite);
    if (HasPlacementNamed(movie, tags, command.name)) {
        ++report.unchanged;
        report.notes.push_back("line " + std::to_string(command.line) + ": " + command.sprite + " already has " +
                               command.name + " (already there)");
        return;
    }
    const uint32_t sourceDepth = ResolveDepth(movie, tags, command.source);

    std::vector<uint32_t> sources;
    for (uint32_t tag : tags) {
        const uint8_t type = TagTypeOf(movie, tag);
        if (type == kRemoveObject && U16(movie, tag + kRemoveDepth) == sourceDepth)
            throw FormatError("line " + std::to_string(command.line) + ": depth " + std::to_string(sourceDepth) +
                              " is removed in the timeline (can't clone it)");
        if (type == kPlaceObject && PlaceDepth(movie, tag) == sourceDepth) sources.push_back(tag);
    }
    if (sources.empty()) throw FormatError("line " + std::to_string(command.line) + ": nothing is placed at depth " + std::to_string(sourceDepth));
    for (uint32_t tag : sources)
        if (!LayoutOf(PlaceFlags(movie, tag)))
            throw FormatError("line " + std::to_string(command.line) + ": placement record with flags 0x" +
                              Reader::Hex(PlaceFlags(movie, tag)) + " (unknown layout)");

    const uint32_t newDepth = U16(movie, sprite + kSpriteDepthCount);
    if (newDepth >= 0xffff) throw FormatError("line " + std::to_string(command.line) + ": the sprite has no free depth");

    bool named = false;
    for (uint32_t source : sources) {
        const PlaceLayout& layout = *LayoutOf(movie.data[source + kTagFlags]);
        const uint32_t copy = CopyRecord(movie, source, layout.size);
        PutU64(&movie.data[copy + kPlaceDepth], newDepth);
        PutF32(movie, copy + kPlaceTx, F32(movie, copy + kPlaceTx) + command.dx);
        PutF32(movie, copy + kPlaceTy, F32(movie, copy + kPlaceTy) + command.dy);
        if (layout.namePointer && movie.Pointer(source + layout.namePointer) >= 0) {
            RenameCopy(movie, source, copy, layout.namePointer, command.name);
            named = true;
        }
        // the copy right after its source in the timeline
        movie.SetPointer(copy + kTagNext, movie.Pointer(source + kTagNext));
        movie.SetPointer(source + kTagNext, (int64_t)copy);
        if (movie.Pointer(sprite + kSpriteLastTag) == (int64_t)source) movie.SetPointer(sprite + kSpriteLastTag, (int64_t)copy);
    }
    if (!named) throw FormatError("line " + std::to_string(command.line) + ": " + command.source + " has no name to replace in the copy");
    const uint16_t depthCount = (uint16_t)(newDepth + 1);
    memcpy(&movie.data[sprite + kSpriteDepthCount], &depthCount, 2);
    ++report.cloned;
}

// --- matrix ------------------------------------------------------------------

bool IsNumber(const std::string& text)
{
    if (text.empty()) return false;
    for (char c : text) if (c < '0' || c > '9') return false;
    return true;
}

void SetMatrix(IggyMovie& movie, const DisplayCommand& command, DisplayReport& report)
{
    const uint32_t sprite = ResolveSprite(movie, command.sprite);
    const std::vector<uint32_t> tags = TagsOf(movie, sprite);
    const uint32_t depth = ResolveDepth(movie, tags, command.source);
    const unsigned long wanted = IsNumber(command.frame) ? strtoul(command.frame.c_str(), nullptr, 10) : 0;
    const std::string where = "line " + std::to_string(command.line) + ": ";

    std::vector<uint32_t> targets;
    bool frameFound = false;
    unsigned long frame = 1;
    std::vector<uint32_t> inFrame;           // the frame's PlaceObjects so far
    bool labelMatches = false;
    auto endFrame = [&]() {
        if (frame == wanted || labelMatches) {
            frameFound = true;
            for (uint32_t tag : inFrame) targets.push_back(tag);
        }
        inFrame.clear();
        labelMatches = false;
        ++frame;
    };
    for (uint32_t tag : tags) {
        const uint8_t type = TagTypeOf(movie, tag);
        if (type == kShowFrame) endFrame();
        else if (type == kFrameLabel) { if (!wanted && WideAsciiAt(movie, tag + kLabelText) == command.frame) labelMatches = true; }
        else if (type == kPlaceObject && PlaceDepth(movie, tag) == depth) inFrame.push_back(tag);
    }
    if (!frameFound) throw FormatError(where + "the sprite has no frame " + command.frame);
    if (targets.empty())
        throw FormatError(where + command.source + " is not placed in frame " + command.frame);

    bool changed = false;
    for (uint32_t tag : targets) {
        for (int i = 0; i < 6; ++i) {
            if (command.keep[i]) continue;
            const uint32_t at = tag + kPlaceMatrix + 4 * i;
            const float value = command.add[i] ? F32(movie, at) + command.values[i] : command.values[i];
            if (F32(movie, at) == value) continue;
            PutF32(movie, at, value);
            changed = true;
        }
    }
    if (changed) {
        ++report.matrices;
    } else {
        ++report.unchanged;
        report.notes.push_back(where + "the matrix already has those values");
    }
}

// --- parsing -----------------------------------------------------------------

std::vector<std::string> Words(const std::string& line)
{
    std::vector<std::string> words;
    size_t at = 0;
    while (at < line.size()) {
        while (at < line.size() && (line[at] == ' ' || line[at] == '\t' || line[at] == '\r')) ++at;
        const size_t start = at;
        while (at < line.size() && line[at] != ' ' && line[at] != '\t' && line[at] != '\r') ++at;
        if (at > start) words.push_back(line.substr(start, at - start));
    }
    return words;
}

float ParseFloat(const std::string& word, int line)
{
    char* end = nullptr;
    const float value = strtof(word.c_str(), &end);
    if (end == word.c_str() || *end || !std::isfinite(value))
        throw FormatError("line " + std::to_string(line) + ": '" + word + "' is not a number");
    return value;
}

}  // namespace

std::string DisplayReport::Summary() const
{
    std::string text = std::to_string(cloned) + " placement" + (cloned == 1 ? "" : "s") + " added, " +
                       std::to_string(matrices) + " matri" + (matrices == 1 ? "x" : "ces") + " set";
    if (unchanged) text += ", " + std::to_string(unchanged) + " already there";
    return text;
}

std::vector<DisplayCommand> ParseDisplayCommands(const std::string& text)
{
    std::vector<DisplayCommand> commands;
    size_t start = 0;
    int number = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        ++number;
        // A comment is a '#' that starts a word and isn't a sprite id ("#45").
        size_t cut = std::string::npos;
        for (size_t i = line.find('#'); i != std::string::npos; i = line.find('#', i + 1)) {
            const bool wordStart = i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t';
            const bool idReference = i + 1 < line.size() && line[i + 1] >= '0' && line[i + 1] <= '9';
            if (wordStart && !idReference) { cut = i; break; }
        }
        if (cut != std::string::npos) line.erase(cut);
        const std::vector<std::string> words = Words(line);
        if (words.empty()) continue;

        DisplayCommand command;
        command.line = number;
        const std::string where = "line " + std::to_string(number) + ": ";
        if (words[0] == "clone") {
            if (words.size() != 6) throw FormatError(where + "clone <sprite> <source> <name> <dx> <dy>");
            command.clone = true;
            command.sprite = words[1];
            command.source = words[2];
            command.name = words[3];
            command.dx = ParseFloat(words[4], number);
            command.dy = ParseFloat(words[5], number);
            if (command.name.empty() || command.name.size() > 200) throw FormatError(where + "bad name");
            for (char c : command.name)
                if (c < 0x21 || c > 0x7e) throw FormatError(where + "the name must be plain ASCII");
        } else if (words[0] == "matrix") {
            if (words.size() != 10) throw FormatError(where + "matrix <sprite> <target> <frame> <a> <b> <c> <d> <tx> <ty>");
            command.sprite = words[1];
            command.source = words[2];
            command.frame = words[3];
            for (int i = 0; i < 6; ++i) {
                if (words[4 + i] == "*") {
                    command.keep[i] = true;
                } else if (words[4 + i].rfind("+=", 0) == 0) {
                    command.add[i] = true;
                    command.values[i] = ParseFloat(words[4 + i].substr(2), number);
                } else {
                    command.values[i] = ParseFloat(words[4 + i], number);
                }
            }
        } else {
            throw FormatError(where + "unknown command '" + words[0] + "' (clone, matrix)");
        }
        commands.push_back(std::move(command));
    }
    if (commands.empty()) throw FormatError("no commands");
    return commands;
}

Bytes ApplyDisplay(const Bytes& current, const std::vector<DisplayCommand>& commands, DisplayReport* report)
{
    DisplayReport local;
    DisplayReport& result = report ? *report : local;
    IggyDocument document;
    document.Parse(current.data(), current.size());
    for (const DisplayCommand& command : commands) {
        if (command.clone) Clone(document.movie, command, result);
        else SetMatrix(document.movie, command, result);
    }
    return result.Changed() ? document.Write() : current;
}

}  // namespace xv2ui

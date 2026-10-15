// Battle Styles - character sound bank merge (see acb_merge.h)
#include "acb_merge.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <tuple>

namespace {

// ------------------------------------------------------------- @UTF ---
// Table: "@UTF", u32 size, then a big-endian body: u16 version, u16 rows
// offset, u32 strings offset, u32 data offset, u32 table name (string
// offset), u16 columns, u16 row width, u32 rows; column definitions from
// +24 (flag byte + u32 name, + the value of a constant column). Offsets
// count from the body (after the 8-byte header). Flag: high nibble =
// storage, low nibble = type (Xv2CoreLib UTF/UTF_File.cs).
const uint8_t kStorageZero = 1, kStorageConstant = 3, kStoragePerRow = 5;
const uint8_t kTypeString = 0xa, kTypeData = 0xb;
const size_t kUtfHeader = 8, kUtfTableHeader = 24;
const size_t kTopAlignment = 32;  // a bank's top table: data and file end 32-aligned (measured, all 77 banks)
const uint16_t kNone = 0xFFFF;    // "no row" in every u16 index column

// Bytes of one value of `type` (0..7 ints of 1/1/2/2/4/4/8/8 bytes, 8 float,
// 0xa string offset, 0xb data offset + size); 0 = unknown type.
size_t TypeSize(uint8_t type)
{
    static const size_t sizes[] = {1, 1, 2, 2, 4, 4, 8, 8, 4, 0, 4, 8};
    return type < sizeof(sizes) / sizeof(sizes[0]) ? sizes[type] : 0;
}

uint64_t ReadBig(const uint8_t* at, size_t size)
{
    uint64_t value = 0;
    for (size_t i = 0; i < size; ++i) value = value << 8 | at[i];
    return value;
}

void PutBig(std::vector<uint8_t>& out, uint64_t value, size_t size)
{
    for (size_t i = size; i-- > 0;) out.push_back((uint8_t)(value >> (8 * i)));
}

void StoreBig16(std::vector<uint8_t>& bytes, size_t at, uint16_t value)
{
    bytes[at] = (uint8_t)(value >> 8);
    bytes[at + 1] = (uint8_t)value;
}

// One cell: numbers (and float bits) in `number`, strings in `text`, data
// (nested tables, the AFS2 archive, index lists) in `data`.
struct Value {
    uint64_t number = 0;
    std::string text;
    std::vector<uint8_t> data;
    bool operator==(const Value& other) const
    {
        return number == other.number && text == other.text && data == other.data;
    }
};

struct Column {
    std::string name;
    uint8_t storage = kStorageZero;
    uint8_t type = 0;
    Value constant;
};

struct Table {
    uint16_t version = 0;
    std::string name;
    std::vector<Column> columns;
    std::vector<std::vector<Value>> rows;

    int Find(const char* column) const
    {
        for (size_t c = 0; c < columns.size(); ++c)
            if (columns[c].name == column) return (int)c;
        return -1;
    }
    // Null when the table has no such column.
    Value* Cell(size_t row, const char* column)
    {
        int c = Find(column);
        return c < 0 ? nullptr : &rows[row][c];
    }
};

bool ReadTable(const uint8_t* bytes, size_t size, Table& table)
{
    if (size < kUtfHeader + kUtfTableHeader || memcmp(bytes, "@UTF", 4) != 0) return false;
    const size_t length = (size_t)ReadBig(bytes + 4, 4);
    if (length < kUtfTableHeader || length > size - kUtfHeader) return false;
    const uint8_t* body = bytes + kUtfHeader;
    table.version = (uint16_t)ReadBig(body, 2);
    const size_t rowsAt = (size_t)ReadBig(body + 2, 2), stringsAt = (size_t)ReadBig(body + 4, 4),
                 dataAt = (size_t)ReadBig(body + 8, 4), nameAt = (size_t)ReadBig(body + 12, 4),
                 columnCount = (size_t)ReadBig(body + 16, 2), rowWidth = (size_t)ReadBig(body + 18, 2),
                 rowCount = (size_t)ReadBig(body + 20, 4);
    auto readString = [&](uint64_t offset, std::string& out) {
        if (stringsAt + offset >= length) return false;
        const uint8_t* start = body + stringsAt + offset;
        const void* end = memchr(start, 0, length - (size_t)(stringsAt + offset));
        if (!end) return false;
        out.assign((const char*)start, (const char*)end);
        return true;
    };
    auto readValue = [&](uint8_t type, size_t at, Value& value) {
        size_t width = TypeSize(type);
        if (!width || at + width > length) return false;
        if (type == kTypeString) return readString(ReadBig(body + at, 4), value.text);
        if (type == kTypeData) {
            uint64_t offset = ReadBig(body + at, 4), count = ReadBig(body + at + 4, 4);
            if (dataAt + offset + count > length) return false;
            value.data.assign(body + dataAt + offset, body + dataAt + offset + count);
            return true;
        }
        value.number = ReadBig(body + at, width);
        return true;
    };
    if (!readString(nameAt, table.name)) return false;
    size_t at = kUtfTableHeader;
    table.columns.resize(columnCount);
    for (Column& column : table.columns) {
        if (at + 5 > length) return false;
        column.storage = body[at] >> 4;
        column.type = body[at] & 0xf;
        if (!TypeSize(column.type) || !readString(ReadBig(body + at + 1, 4), column.name)) return false;
        at += 5;
        if (column.storage == kStorageConstant) {
            if (!readValue(column.type, at, column.constant)) return false;
            at += TypeSize(column.type);
        } else if (column.storage != kStorageZero && column.storage != kStoragePerRow) {
            return false;
        }
    }
    table.rows.assign(rowCount, std::vector<Value>(columnCount));
    for (size_t r = 0; r < rowCount; ++r) {
        at = rowsAt + r * rowWidth;
        for (size_t c = 0; c < columnCount; ++c) {
            const Column& column = table.columns[c];
            if (column.storage == kStoragePerRow) {
                if (!readValue(column.type, at, table.rows[r][c])) return false;
                at += TypeSize(column.type);
            } else {
                table.rows[r][c] = column.constant;  // zero storage: the default Value
            }
        }
    }
    return true;
}

// Writes the table back as the game's tools do (rules measured on the 77
// banks, see acb_merge.h): strings in the order they are met (table name,
// then each column name with its constant, then the rows), no merging of
// equal strings. A bank's top table (`topLevel`) puts its data 32-aligned
// in the file, starts every data cell but the first after a gap that ends
// 32-aligned (a whole 32 bytes when already aligned), empty cells at 0, and
// pads the file to 32; nested tables pack their data and pad to 4. A
// constant or zero column whose rows no longer all hold that value is
// written per row.
std::vector<uint8_t> WriteTable(const Table& table, bool topLevel)
{
    std::vector<uint8_t> strings, data, columnBytes, rowBytes;
    auto addString = [&](const std::string& text) {
        uint32_t at = (uint32_t)strings.size();
        strings.insert(strings.end(), text.begin(), text.end());
        strings.push_back(0);
        return at;
    };
    auto encode = [&](uint8_t type, const Value& value, std::vector<uint8_t>& to) {
        if (type == kTypeString) {
            PutBig(to, addString(value.text), 4);
        } else if (type == kTypeData) {
            if (value.data.empty()) {
                PutBig(to, 0, 8);
                return;
            }
            if (topLevel && !data.empty()) data.resize(data.size() + kTopAlignment - data.size() % kTopAlignment);
            PutBig(to, data.size(), 4);
            PutBig(to, value.data.size(), 4);
            data.insert(data.end(), value.data.begin(), value.data.end());
        } else {
            PutBig(to, value.number, TypeSize(type));
        }
    };
    std::vector<uint8_t> storage(table.columns.size());
    size_t rowWidth = 0;
    for (size_t c = 0; c < table.columns.size(); ++c) {
        const Column& column = table.columns[c];
        storage[c] = column.storage;
        const Value reference = column.storage == kStorageConstant ? column.constant : Value{};
        for (const std::vector<Value>& row : table.rows)
            if (storage[c] != kStoragePerRow && !(row[c] == reference)) storage[c] = kStoragePerRow;
        if (storage[c] == kStoragePerRow) rowWidth += TypeSize(column.type);
    }
    const uint32_t nameAt = addString(table.name);
    for (size_t c = 0; c < table.columns.size(); ++c) {
        const Column& column = table.columns[c];
        columnBytes.push_back((uint8_t)(storage[c] << 4 | column.type));
        PutBig(columnBytes, addString(column.name), 4);
        if (storage[c] == kStorageConstant) encode(column.type, column.constant, columnBytes);
    }
    for (const std::vector<Value>& row : table.rows)
        for (size_t c = 0; c < table.columns.size(); ++c)
            if (storage[c] == kStoragePerRow) encode(table.columns[c].type, row[c], rowBytes);
    const size_t rowsAt = kUtfTableHeader + columnBytes.size(), stringsAt = rowsAt + rowBytes.size();
    size_t dataAt = stringsAt + strings.size();
    if (topLevel) {
        size_t pad = (kTopAlignment - (dataAt + kUtfHeader) % kTopAlignment) % kTopAlignment;
        strings.resize(strings.size() + pad);
        dataAt += pad;
    }
    std::vector<uint8_t> body;
    PutBig(body, table.version, 2);
    PutBig(body, rowsAt, 2);
    PutBig(body, stringsAt, 4);
    PutBig(body, dataAt, 4);
    PutBig(body, nameAt, 4);
    PutBig(body, table.columns.size(), 2);
    PutBig(body, rowWidth, 2);
    PutBig(body, table.rows.size(), 4);
    for (const std::vector<uint8_t>* part : {&columnBytes, &rowBytes, &strings, &data})
        body.insert(body.end(), part->begin(), part->end());
    if (topLevel)
        while ((body.size() + kUtfHeader) % kTopAlignment) body.push_back(0);
    else
        while (body.size() % 4) body.push_back(0);
    std::vector<uint8_t> out = {'@', 'U', 'T', 'F'};
    PutBig(out, body.size(), 4);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

// ------------------------------------------------------------- AFS2 ---
// The bank's audio: "AFS2", u8 version, u8 offset size, u16 id size, u32
// count, u32 alignment, the ids, then count + 1 offsets (entry i starts at
// offset i rounded up to the alignment, ends at offset i + 1). Xv2CoreLib
// AFS2/AFS2_File.cs.
struct Audio {
    uint32_t alignment = 32;  // all 77 banks: 32
    std::map<uint16_t, std::vector<uint8_t>> entries;  // awb id -> encoded audio (HCA)
};

bool ReadAudio(const std::vector<uint8_t>& awb, Audio& audio)
{
    if (awb.empty()) return true;
    if (awb.size() < 16 || memcmp(awb.data(), "AFS2", 4) != 0) return false;
    const size_t offsetSize = awb[5], idSize = awb[6] | awb[7] << 8;
    const size_t count = awb[8] | awb[9] << 8 | awb[10] << 16 | (size_t)awb[11] << 24;
    audio.alignment = awb[12] | awb[13] << 8 | awb[14] << 16 | (uint32_t)awb[15] << 24;
    if ((offsetSize != 2 && offsetSize != 4) || idSize != 2 || !audio.alignment ||
        16 + count * idSize + (count + 1) * offsetSize > awb.size())
        return false;
    auto little = [&](size_t at, size_t size) {
        size_t value = 0;
        for (size_t i = size; i-- > 0;) value = value << 8 | awb[at + i];
        return value;
    };
    const size_t offsets = 16 + count * idSize;
    for (size_t i = 0; i < count; ++i) {
        size_t start = little(offsets + i * offsetSize, offsetSize), end = little(offsets + (i + 1) * offsetSize, offsetSize);
        start = (start + audio.alignment - 1) / audio.alignment * audio.alignment;
        if (start > end || end > awb.size()) return false;
        audio.entries[(uint16_t)little(16 + i * idSize, idSize)].assign(awb.begin() + start, awb.begin() + end);
    }
    return true;
}

// Version 1, 4-byte offsets, 2-byte ids, entries in id order (as
// Xv2CoreLib writes it).
std::vector<uint8_t> WriteAudio(const Audio& audio)
{
    std::vector<uint8_t> out = {'A', 'F', 'S', '2', 1, 4, 2, 0};
    auto little = [&](uint64_t value, size_t size) {
        for (size_t i = 0; i < size; ++i) out.push_back((uint8_t)(value >> (8 * i)));
    };
    little(audio.entries.size(), 4);
    little(audio.alignment, 4);
    for (const auto& entry : audio.entries) little(entry.first, 2);
    const size_t offsets = out.size();
    out.resize(out.size() + 4 * (audio.entries.size() + 1));
    size_t index = 0;
    auto setOffset = [&](size_t i) {
        for (size_t b = 0; b < 4; ++b) out[offsets + 4 * i + b] = (uint8_t)(out.size() >> (8 * b));
    };
    for (const auto& entry : audio.entries) {
        setOffset(index++);
        while (out.size() % audio.alignment) out.push_back(0);
        out.insert(out.end(), entry.second.begin(), entry.second.end());
    }
    setOffset(index);
    return out;
}

// --------------------------------------------------------------- bank ---
struct Bank {
    Table top;                            // one row
    std::map<std::string, Table> nested;  // top cells that hold a table
};

bool ReadBank(const uint8_t* bytes, size_t size, Bank& bank)
{
    if (!ReadTable(bytes, size, bank.top) || bank.top.rows.size() != 1) return false;
    for (size_t c = 0; c < bank.top.columns.size(); ++c) {
        const std::vector<uint8_t>& cell = bank.top.rows[0][c].data;
        if (cell.size() >= 4 && memcmp(cell.data(), "@UTF", 4) == 0 &&
            !ReadTable(cell.data(), cell.size(), bank.nested[bank.top.columns[c].name]))
            return false;
    }
    return true;
}

size_t RowCount(const Bank& bank, const char* name)
{
    auto table = bank.nested.find(name);
    return table == bank.nested.end() ? 0 : table->second.rows.size();
}

// The race bank's table `name`; made from the style's columns when the race
// bank has none (an empty cell).
Table& RaceTable(Bank& race, const Bank& style, const char* name)
{
    auto table = race.nested.find(name);
    if (table != race.nested.end()) return table->second;
    Table& made = race.nested[name];
    auto model = style.nested.find(name);
    if (model != style.nested.end()) {
        made = model->second;
        made.rows.clear();
    }
    return made;
}

const Table& StyleTable(const Bank& style, const char* name)
{
    static const Table none;
    auto table = style.nested.find(name);
    return table == style.nested.end() ? none : table->second;
}

// Appends `from`'s rows to `to`, column by name; returns the first new row.
// A column only `from` has is added to `to` (its old rows: the default).
size_t AppendRows(Table& to, const Table& from)
{
    const size_t first = to.rows.size();
    for (const Column& column : from.columns)
        if (to.Find(column.name.c_str()) < 0) {
            Column added = column;
            added.storage = kStorageZero;
            added.constant = Value{};
            to.columns.push_back(added);
            for (std::vector<Value>& row : to.rows) row.emplace_back();
        }
    for (const std::vector<Value>& source : from.rows) {
        std::vector<Value> row(to.columns.size());
        for (size_t c = 0; c < to.columns.size(); ++c) {
            int at = from.Find(to.columns[c].name.c_str());
            row[c] = at >= 0 ? source[at] : to.columns[c].storage == kStorageConstant ? to.columns[c].constant : Value{};
        }
        to.rows.push_back(row);
    }
    return first;
}

// Index cells: moved by `offset` unless they say "none".
void Shift(Value* index, size_t offset)
{
    if (index && index->number != kNone) index->number += offset;
}

uint64_t Number(Table& table, size_t row, const char* column)
{
    const Value* cell = table.Cell(row, column);
    return cell ? cell->number : 0;
}

bool HasData(Table& table, size_t row, const char* column)
{
    const Value* cell = table.Cell(row, column);
    return cell && !cell->data.empty();
}

// Where the style's rows of each table start in the merged bank.
struct Offsets {
    size_t waveforms = 0, synths = 0, sequences = 0, tracks = 0, commands = 0, globalAisacs = 0, actionTracks = 0,
           waveformExtensions = 0;
    size_t cueLimitWorks = 0;       // the race header's NumCueLimitListWorks
    std::vector<uint16_t> strings;  // style StringValueTable row -> merged row
};

// Reference item (u16 type, u16 index; ReferenceType in Xv2CoreLib): the
// merged index, or false for a type the merge doesn't move.
bool MoveReference(uint16_t type, uint16_t& index, const Offsets& offsets)
{
    const size_t offset = type == 1 ? offsets.waveforms : type == 2 ? offsets.synths : type == 3 ? offsets.sequences
                                                                                              : SIZE_MAX;
    if (offset == SIZE_MAX || index + offset >= kNone) return false;
    index = (uint16_t)(index + offset);
    return true;
}

// Command codes that point at rows (Xv2CoreLib ACB_File.cs CommandType;
// commands are big-endian u16 code, u8 size, parameters).
const uint16_t kCommandReferenceItem = 0x07d0;   // u16 type, u16 index
const uint16_t kCommandReferenceItem2 = 0x07d3;  // u16 type, u16 index, u16
const uint16_t kCommandGlobalAisac = 0x004b;     // u16 GlobalAisacReferenceTable row
const uint16_t kCommandVolumeBus = 0x006f;       // u16 StringValueTable row, u16 volume
const uint16_t kCommandBus = 0x0055;             // u16 StringValueTable row
const uint16_t kCommandBeatSync = 0x0071;        // u16, u16 BeatSyncInfoTable row
// Cue limit: u16 limit, u16 cue limit work area [, u8]. The work area is an
// index into an array the bank's header sizes (NumCueLimitListWorks, nodes
// NumCueLimitNodeWorks); found by reverse engineering the exe's CRI command
// interpreter (1.26.01: 0x9cbca0 dispatches both codes to 0x9ccd2d, which
// hands the work index to 0x9b814c). An index past the array reads garbage:
// CAR_BTL_FIO_SE's cue (limit 1, work 0) merged into CAR_BTL_NMC_SE (0
// works) crashed the game at 0x9bba67.
const uint16_t kCommandCueLimit = 0x0049;
const uint16_t kCommandCueLimit2 = 0x004f;

bool MoveCommandRefs(std::vector<uint8_t>& command, const Offsets& offsets, std::string& note)
{
    for (size_t at = 0; at + 3 <= command.size();) {
        const uint16_t code = (uint16_t)ReadBig(&command[at], 2);
        const size_t size = command[at + 2], parameters = at + 3;
        if (parameters + size > command.size()) { note = "style command runs past its end"; return false; }
        if ((code == kCommandReferenceItem || code == kCommandReferenceItem2) && size >= 4) {
            uint16_t index = (uint16_t)ReadBig(&command[parameters + 2], 2);
            if (!MoveReference((uint16_t)ReadBig(&command[parameters], 2), index, offsets)) {
                note = "style command references an unsupported item type";
                return false;
            }
            StoreBig16(command, parameters + 2, index);
        } else if (code == kCommandGlobalAisac && size >= 2) {
            StoreBig16(command, parameters, (uint16_t)(ReadBig(&command[parameters], 2) + offsets.globalAisacs));
        } else if ((code == kCommandVolumeBus || code == kCommandBus) && size >= 2) {
            size_t row = (size_t)ReadBig(&command[parameters], 2);
            if (row >= offsets.strings.size()) { note = "style bus command names a missing string"; return false; }
            StoreBig16(command, parameters, offsets.strings[row]);
        } else if ((code == kCommandCueLimit || code == kCommandCueLimit2) && size >= 4) {
            size_t work = (size_t)ReadBig(&command[parameters + 2], 2) + offsets.cueLimitWorks;
            if (work >= kNone) { note = "too many cue limit work areas"; return false; }
            StoreBig16(command, parameters + 2, (uint16_t)work);
        } else if (code == kCommandBeatSync) {
            note = "style bank uses beat sync";
            return false;
        }
        at = parameters + size;
    }
    return true;
}

// The global AISAC start of an appended row, when it has references.
void ShiftGlobalAisacs(Table& table, size_t row, size_t offset)
{
    if (Number(table, row, "GlobalAisacNumRefs")) Shift(table.Cell(row, "GlobalAisacStartIndex"), offset);
}

void ShiftActionTracks(Table& table, size_t row, size_t offset)
{
    if (Number(table, row, "NumActionTracks")) Shift(table.Cell(row, "ActionTrackStartIndex"), offset);
}

}  // namespace

bool MergeAcb(const uint8_t* race, size_t raceSize, const uint8_t* style, size_t styleSize,
              std::vector<uint8_t>& out, std::map<uint16_t, uint16_t>& cueIds, std::string& note)
{
    Bank raceBank, styleBank;
    if (!ReadBank(race, raceSize, raceBank)) { note = "race bank is not a readable ACB"; return false; }
    if (!ReadBank(style, styleSize, styleBank)) { note = "style bank is not a readable ACB"; return false; }

    // What the merge would have to remap but doesn't: no character SE bank
    // of the game uses these (ClaudeTools\battle_styles\acbtables.py all: AISAC /
    // graph rows only in XBT's, the race-side ones are kept as they are).
    for (const char* name : {"AisacTable", "GraphTable", "AisacNameTable", "AisacControlNameTable",
                             "AutoModulationTable", "OutsideLinkTable", "BlockSequenceTable", "BlockTable",
                             "EventTable", "BeatSyncInfoTable", "CueLimitWorkTable"})
        if (RowCount(styleBank, name)) { note = std::string("style bank uses ") + name; return false; }
    for (const char* name : {"StreamAwbAfs2Header", "StreamAwbTocWork"})
        if (Value* cell = styleBank.top.Cell(0, name); cell && !cell->data.empty()) {
            note = "style bank streams audio";
            return false;
        }

    Offsets offsets;
    offsets.waveforms = RowCount(raceBank, "WaveformTable");
    offsets.synths = RowCount(raceBank, "SynthTable");
    offsets.sequences = RowCount(raceBank, "SequenceTable");
    offsets.tracks = RowCount(raceBank, "TrackTable");
    offsets.commands = RowCount(raceBank, "CommandTable");
    offsets.globalAisacs = RowCount(raceBank, "GlobalAisacReferenceTable");
    offsets.actionTracks = RowCount(raceBank, "ActionTrackTable");
    offsets.waveformExtensions = RowCount(raceBank, "WaveformExtensionDataTable");
    const size_t raceCues = RowCount(raceBank, "CueTable");
    offsets.cueLimitWorks = Number(raceBank.top, 0, "NumCueLimitListWorks");

    // Cue limit work areas: the style's go after the race's (the commands
    // move with offsets.cueLimitWorks), so the header counts are the sums.
    for (const char* name : {"NumCueLimitListWorks", "NumCueLimitNodeWorks"}) {
        const uint64_t styleCount = Number(styleBank.top, 0, name);
        if (!styleCount) continue;
        Value* count = raceBank.top.Cell(0, name);
        if (!count) { note = std::string("race bank has no ") + name; return false; }
        if (count->number + styleCount >= kNone) { note = std::string("too many ") + name; return false; }
        count->number += styleCount;
    }
    // CueLimitWorkTable is the work areas' memory, stored zeroed in the file:
    // 8 bytes + 32 per list work + 32 per node work (every bank that has
    // works: FIO 1+1 -> 72 bytes, GBR 14+14 -> 904, GAL 45+45 -> 2888;
    // ClaudeTools\battle_styles\acbcuelimit.py). The race's own buffer stays empty
    // when it has no works, so without this the style's work areas point past
    // it: GAL merged into HUM crashed in Training (strlen(NULL) in the CRI
    // play logging at 0x9af298 for cue 53).
    {
        const uint64_t works = Number(raceBank.top, 0, "NumCueLimitListWorks") +
                               Number(raceBank.top, 0, "NumCueLimitNodeWorks");
        if (works) {
            Value* buffer = raceBank.top.Cell(0, "CueLimitWorkTable");
            if (!buffer) { note = "race bank has no CueLimitWorkTable"; return false; }
            buffer->data.assign((size_t)(8 + 32 * works), 0);
        }
    }

    // Strings (bus names): shared by value.
    {
        Table& strings = RaceTable(raceBank, styleBank, "StringValueTable");
        const Table& styleStrings = StyleTable(styleBank, "StringValueTable");
        const int column = strings.Find("StringValue"), styleColumn = styleStrings.Find("StringValue");
        for (const std::vector<Value>& row : styleStrings.rows) {
            const std::string& text = styleColumn >= 0 ? row[styleColumn].text : std::string();
            size_t found = 0;
            while (found < strings.rows.size() && !(column >= 0 && strings.rows[found][column].text == text)) ++found;
            if (found == strings.rows.size()) {
                Table one = styleStrings;
                one.rows = {row};
                AppendRows(strings, one);
            }
            offsets.strings.push_back((uint16_t)found);
        }
    }

    // Audio: the style's entries under new AWB ids after the race's.
    Audio raceAudio, styleAudio;
    Value* raceAwb = raceBank.top.Cell(0, "AwbFile");
    Value* styleAwb = styleBank.top.Cell(0, "AwbFile");
    if (!raceAwb || !styleAwb || !ReadAudio(raceAwb->data, raceAudio) || !ReadAudio(styleAwb->data, styleAudio)) {
        note = "audio archive (AFS2) not readable";
        return false;
    }
    std::map<uint16_t, uint16_t> awbIds;
    uint32_t nextAwbId = raceAudio.entries.empty() ? 0 : raceAudio.entries.rbegin()->first + 1u;
    for (const auto& entry : styleAudio.entries) {
        if (nextAwbId >= kNone) { note = "too many waveforms"; return false; }
        awbIds[entry.first] = (uint16_t)nextAwbId;
        raceAudio.entries[(uint16_t)nextAwbId++] = entry.second;
    }

    // Waveforms.
    {
        Table& waveforms = RaceTable(raceBank, styleBank, "WaveformTable");
        for (size_t row = AppendRows(waveforms, StyleTable(styleBank, "WaveformTable")); row < waveforms.rows.size(); ++row) {
            if (Number(waveforms, row, "Streaming")) { note = "style bank streams audio"; return false; }
            Value* id = waveforms.Cell(row, "MemoryAwbId");
            auto mapped = id ? awbIds.find((uint16_t)id->number) : awbIds.end();
            if (mapped == awbIds.end()) { note = "style waveform without audio"; return false; }
            id->number = mapped->second;
            Shift(waveforms.Cell(row, "ExtensionData"), offsets.waveformExtensions);
        }
        AppendRows(RaceTable(raceBank, styleBank, "WaveformExtensionDataTable"),
                   StyleTable(styleBank, "WaveformExtensionDataTable"));
    }
    AppendRows(RaceTable(raceBank, styleBank, "GlobalAisacReferenceTable"),
               StyleTable(styleBank, "GlobalAisacReferenceTable"));

    // Commands: their row references moved.
    {
        Table& commands = RaceTable(raceBank, styleBank, "CommandTable");
        for (size_t row = AppendRows(commands, StyleTable(styleBank, "CommandTable")); row < commands.rows.size(); ++row)
            if (Value* command = commands.Cell(row, "Command"); command && !MoveCommandRefs(command->data, offsets, note))
                return false;
    }

    // Synths: reference items (u16 type, u16 index pairs), command, own index.
    {
        Table& synths = RaceTable(raceBank, styleBank, "SynthTable");
        for (size_t row = AppendRows(synths, StyleTable(styleBank, "SynthTable")); row < synths.rows.size(); ++row) {
            if (HasData(synths, row, "LocalAisacs")) { note = "style synth uses local AISACs"; return false; }
            if (Value* items = synths.Cell(row, "ReferenceItems")) {
                for (size_t at = 0; at + 4 <= items->data.size(); at += 4) {
                    uint16_t index = (uint16_t)ReadBig(&items->data[at + 2], 2);
                    if (!MoveReference((uint16_t)ReadBig(&items->data[at], 2), index, offsets)) {
                        note = "style synth references an unsupported item type";
                        return false;
                    }
                    StoreBig16(items->data, at + 2, index);
                }
            }
            Shift(synths.Cell(row, "CommandIndex"), offsets.commands);
            ShiftGlobalAisacs(synths, row, offsets.globalAisacs);
            ShiftActionTracks(synths, row, offsets.actionTracks);
            // the row's own index in every bank (scratch survey of the 77)
            Shift(synths.Cell(row, "ControlWorkArea1"), offsets.synths);
            Shift(synths.Cell(row, "ControlWorkArea2"), offsets.synths);
        }
    }

    // Tracks: event and command rows.
    {
        Table& tracks = RaceTable(raceBank, styleBank, "TrackTable");
        for (size_t row = AppendRows(tracks, StyleTable(styleBank, "TrackTable")); row < tracks.rows.size(); ++row) {
            if (HasData(tracks, row, "LocalAisacs")) { note = "style track uses local AISACs"; return false; }
            Shift(tracks.Cell(row, "EventIndex"), offsets.commands);
            Shift(tracks.Cell(row, "CommandIndex"), offsets.commands);
            ShiftGlobalAisacs(tracks, row, offsets.globalAisacs);
        }
    }

    // Sequences: track list (big-endian u16 rows), command, own index.
    {
        Table& sequences = RaceTable(raceBank, styleBank, "SequenceTable");
        for (size_t row = AppendRows(sequences, StyleTable(styleBank, "SequenceTable")); row < sequences.rows.size(); ++row) {
            if (HasData(sequences, row, "LocalAisacs")) { note = "style sequence uses local AISACs"; return false; }
            if (Value* list = sequences.Cell(row, "TrackIndex"))
                for (size_t at = 0; at + 2 <= list->data.size(); at += 2)
                    StoreBig16(list->data, at, (uint16_t)(ReadBig(&list->data[at], 2) + offsets.tracks));
            Shift(sequences.Cell(row, "CommandIndex"), offsets.commands);
            ShiftGlobalAisacs(sequences, row, offsets.globalAisacs);
            ShiftActionTracks(sequences, row, offsets.actionTracks);
            Shift(sequences.Cell(row, "ControlWorkArea1"), offsets.sequences);
            Shift(sequences.Cell(row, "ControlWorkArea2"), offsets.sequences);
        }
    }

    // Cues: new ids after the race's highest.
    uint64_t nextCueId = 0;
    {
        Table& cues = RaceTable(raceBank, styleBank, "CueTable");
        for (size_t row = 0; row < cues.rows.size(); ++row)
            nextCueId = std::max(nextCueId, Number(cues, row, "CueId") + 1);
        const uint64_t firstNewId = nextCueId;
        for (size_t row = AppendRows(cues, StyleTable(styleBank, "CueTable")); row < cues.rows.size(); ++row) {
            // AisacControlMap is kept as it is: it holds ACF (global) control
            // ids, not bank rows - every CAR_BTL_*_SE cue has the same one
            // byte 02 (ClaudeTools\battle_styles\acbtables.py), and Xv2CoreLib
            // copies it unchanged when it moves cues between banks.
            Value* id = cues.Cell(row, "CueId");
            Value* reference = cues.Cell(row, "ReferenceIndex");
            if (!id || !reference || nextCueId >= kNone) { note = "style cue without id, or too many cues"; return false; }
            uint16_t index = (uint16_t)reference->number;
            if (!MoveReference((uint16_t)Number(cues, row, "ReferenceType"), index, offsets)) {
                note = "style cue references an unsupported item type";
                return false;
            }
            reference->number = index;
            cueIds[(uint16_t)id->number] = (uint16_t)nextCueId;
            id->number = nextCueId++;
        }
        char text[160];
        snprintf(text, sizeof(text), "%zu style cues appended as ids %llu..%llu after %zu race cues, %zu waveforms",
                 cueIds.size(), (unsigned long long)firstNewId, (unsigned long long)nextCueId - 1, raceCues,
                 styleAudio.entries.size());
        note = text;
    }

    // Cue names: the style's point at their appended cue; a name the race
    // bank already has gets a suffix (two names on one cue crashed the game,
    // Research_Notes\sound.md), all sorted by name as the game's banks are.
    {
        Table& names = RaceTable(raceBank, styleBank, "CueNameTable");
        const int nameColumn = names.Find("CueName");
        std::set<std::string> taken;
        for (const std::vector<Value>& row : names.rows)
            if (nameColumn >= 0) taken.insert(row[nameColumn].text);
        for (size_t row = AppendRows(names, StyleTable(styleBank, "CueNameTable")); row < names.rows.size(); ++row) {
            Shift(names.Cell(row, "CueIndex"), raceCues);
            if (nameColumn < 0) continue;
            std::string& name = names.rows[row][nameColumn].text;
            std::string unique = name;
            for (int n = 2; taken.count(unique); ++n) unique = name + "_STYLE" + std::to_string(n);
            name = unique;
            taken.insert(name);
        }
        if (nameColumn >= 0)
            std::stable_sort(names.rows.begin(), names.rows.end(),
                             [&](const std::vector<Value>& a, const std::vector<Value>& b) {
                                 return a[nameColumn].text < b[nameColumn].text;
                             });
    }

    // Action tracks (play/stop another cue): command row; a target cue of
    // the style bank itself moves to its new id.
    {
        Table& actions = RaceTable(raceBank, styleBank, "ActionTrackTable");
        const std::string styleName = styleBank.top.Cell(0, "Name") ? styleBank.top.Cell(0, "Name")->text : "";
        const std::string raceName = raceBank.top.Cell(0, "Name") ? raceBank.top.Cell(0, "Name")->text : "";
        for (size_t row = AppendRows(actions, StyleTable(styleBank, "ActionTrackTable")); row < actions.rows.size(); ++row) {
            Shift(actions.Cell(row, "CommandIndex"), offsets.commands);
            Value* bankName = actions.Cell(row, "TargetAcbName");
            Value* target = actions.Cell(row, "TargetId");
            if (bankName && (bankName->text.empty() || bankName->text == styleName)) {
                if (!bankName->text.empty()) bankName->text = raceName;
                auto mapped = target ? cueIds.find((uint16_t)target->number) : cueIds.end();
                if (mapped != cueIds.end()) target->number = mapped->second;
            }
        }
    }

    // ACF references (categories, buses, AISAC names): the union.
    {
        Table& references = RaceTable(raceBank, styleBank, "AcfReferenceTable");
        const Table& styleReferences = StyleTable(styleBank, "AcfReferenceTable");
        auto key = [](Table& table, size_t row) {
            auto text = [&](const char* column) {
                const Value* cell = table.Cell(row, column);
                return cell ? cell->text : std::string();
            };
            return std::make_tuple(Number(table, row, "Type"), text("Name"), text("Name2"), Number(table, row, "Id"));
        };
        std::set<std::tuple<uint64_t, std::string, std::string, uint64_t>> known;
        for (size_t row = 0; row < references.rows.size(); ++row) known.insert(key(references, row));
        Table missing = styleReferences;
        missing.rows.clear();
        for (size_t row = 0; row < styleReferences.rows.size(); ++row)
            if (!known.count(key(const_cast<Table&>(styleReferences), row))) missing.rows.push_back(styleReferences.rows[row]);
        AppendRows(references, missing);
    }

    for (const auto& table : raceBank.nested)
        if (table.second.rows.size() >= kNone) { note = table.first + " has too many rows"; return false; }

    // Back into the top table, which keeps the race bank's header, name and
    // hashes.
    for (const auto& table : raceBank.nested)
        if (Value* cell = raceBank.top.Cell(0, table.first.c_str())) cell->data = WriteTable(table.second, false);
    raceAwb->data = WriteAudio(raceAudio);
    out = WriteTable(raceBank.top, true);
    return true;
}

std::string CharaSeBank(const uint8_t* cso, size_t size, int charaId)
{
    const size_t kHeader = 16, kEntrySize = 32, kSeOffset = 8;  // see acb_merge.h
    if (size < kHeader || memcmp(cso, "#CSO", 4) != 0) return {};
    int32_t count, table;
    memcpy(&count, cso + 8, 4);
    memcpy(&table, cso + 12, 4);
    if (count < 0 || table < 0 || (size_t)table + (size_t)count * kEntrySize > size) return {};
    std::string first;  // the chara's first entry, when it has no costume 0 one
    bool seen = false;
    for (int32_t i = 0; i < count; ++i) {
        const uint8_t* entry = cso + table + i * kEntrySize;
        int32_t chara;
        uint32_t costume, name;
        memcpy(&chara, entry, 4);
        memcpy(&costume, entry + 4, 4);
        memcpy(&name, entry + kSeOffset, 4);
        if (chara != charaId) continue;
        std::string bank;
        if (name && name < size) bank.assign((const char*)cso + name, strnlen((const char*)cso + name, size - name));
        if (costume == 0) return bank;
        if (!seen) first = bank;
        seen = true;
    }
    return first;
}

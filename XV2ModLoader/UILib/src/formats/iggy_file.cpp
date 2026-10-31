// .iggy container, movie and pointer index (see iggy_file.h).
#include "iggy_file.h"
#include <algorithm>

namespace xv2ui {

namespace {

// Container header (eterniti's IGGYHeader): signature, version 0x900,
// platform 01 40 01 00, a u32 (43 or 9 in the game's files), ... and the
// subfile count at 0x1C; entries {id, size, size2, offset} from 0x20.
const size_t kSubfileCountField = 0x1C, kSubfileTable = 0x20, kSubfileEntry = 16;

// AS3 block header {u64 1, u32 ABC size}; the u64 is a (null) pointer the
// index must walk (Research_Notes "Dead end": unwalked it crashed LOBYHUD).
const size_t kScriptBlockHeader = 12;
const uint64_t kNullPointer = 1;   // a relative pointer's null

// Index op codes (iggy_w64.dll 0x59860..0x59bb1).
const uint8_t kOpManyStructs = 0x80;   // 80..bf t: (op - 0x7f) structs of type t
const uint8_t kOpSkipEven = 0xc0;      // c0..cf: skip 2 * (op - 0xbf) bytes
const uint8_t kOpValues = 0xd0;        // d0..df n: n + 1 values of kind (op & 15)
const uint8_t kOpNothing = 0xfc;       // fc x
const uint8_t kOpStruct = 0xfd;        // fd size k (offset kind)*k
const uint8_t kOpSkip = 0xfe;          // fe n: skip n + 1
const uint8_t kOpSkipLong = 0xff;      // ff u32: skip u32

struct IndexType {
    uint8_t size = 0;
    std::vector<std::pair<uint8_t, uint8_t>> fields;  // (offset, kind)
};

}  // namespace

size_t IggyFieldSize(uint8_t kind)
{
    switch (kind) {
    case 0: case 1: case kFieldPointer: case kFieldSwap64: return 8;
    case kFieldSwap16: return 2;
    case kFieldBitReverse: case kFieldSwap32: return 4;
    case kFieldNothing: return 0;
    default: return kind >= kFieldRuntimeByte ? 1 : 8;   // 8..11 u64, 12..15 byte
    }
}

// ---------------------------------------------------------------- container

void IggyFile::Parse(const uint8_t* data, size_t size)
{
    Reader reader(data, size, "iggy");
    if (!Looks(data, size)) reader.Fail("not an .iggy (signature)");
    header.assign(data, data + kSubfileCountField);
    reader.seek(kSubfileCountField);
    const uint32_t count = reader.u32();
    if (count == 0 || count > 64) reader.Fail("subfile count");
    subfiles.clear();
    size_t expected = kSubfileTable + count * kSubfileEntry;
    for (uint32_t i = 0; i < count; ++i) {
        IggySubfile subfile;
        subfile.id = reader.u32();
        const uint32_t subfileSize = reader.u32();
        subfile.size2 = reader.u32();
        const uint32_t offset = reader.u32();
        // Write() puts the subfiles back to back after the table, as the game's are.
        if (offset != expected || (uint64_t)offset + subfileSize > size) reader.Fail("subfile " + std::to_string(i) + " layout");
        subfile.data.assign(data + offset, data + offset + subfileSize);
        expected = offset + subfileSize;
        subfiles.push_back(std::move(subfile));
    }
    if (expected != size) reader.Fail("bytes after the last subfile");
}

Bytes IggyFile::Write() const
{
    Writer writer;
    writer.Append(header);
    writer.u32((uint32_t)subfiles.size());
    uint32_t offset = (uint32_t)(kSubfileTable + subfiles.size() * kSubfileEntry);
    for (const IggySubfile& subfile : subfiles) {
        writer.u32(subfile.id);
        writer.u32((uint32_t)subfile.data.size());
        writer.u32(subfile.size2 == 0 || subfile.size2 == subfile.data.size() ? (uint32_t)subfile.data.size() : subfile.size2);
        writer.u32(offset);
        offset += (uint32_t)subfile.data.size();
    }
    for (const IggySubfile& subfile : subfiles) writer.Append(subfile.data);
    return writer.out;
}

// ---------------------------------------------------------------- index

void IggyMovie::Load(const Bytes& movie, const std::vector<const Bytes*>& indexChunks, IggyIndexStats* stats)
{
    data = movie;
    loadedSize = movie.size();
    fields.clear();
    size_t walkedTo = 0;
    for (const Bytes* chunk : indexChunks) {
        Reader code(chunk->data(), chunk->size(), "iggy index");
        std::vector<IndexType> types(code.u8());
        for (IndexType& type : types) {
            type.size = code.u8();
            type.fields.resize(code.u8());
            for (auto& field : type.fields) { field.first = code.u8(); field.second = code.u8(); }
        }
        size_t position = 0;   // every chunk walks from the movie's start
        auto addField = [&](size_t at, uint8_t kind) {
            if (at + IggyFieldSize(kind) > data.size()) code.Fail("field past the movie end");
            fields[(uint32_t)at] = kind;
            if (stats) {
                if (kind == kFieldPointer) {
                    ++stats->pointers;
                    if (GetU64(&data[at]) == kNullPointer) ++stats->nullPointers;
                } else {
                    ++stats->otherFields;
                }
            }
        };
        auto addStruct = [&](const IndexType& type) {
            for (auto& field : type.fields) addField(position + field.first, field.second);
            position += type.size;
        };
        while (!code.atEnd()) {
            const uint8_t op = code.u8();
            if (stats) ++stats->opcodes[op < kOpManyStructs ? 0 : op < kOpSkipEven ? kOpManyStructs : op < kOpValues ? kOpSkipEven
                                                                   : op < 0xe0 ? op : op];
            if (op < kOpManyStructs) {
                if (op >= types.size()) code.Fail("unknown struct type");
                addStruct(types[op]);
            } else if (op < kOpSkipEven) {
                const uint8_t type = code.u8();
                if (type >= types.size()) code.Fail("unknown struct type");
                for (int i = 0; i < op - 0x7f; ++i) addStruct(types[type]);
            } else if (op < kOpValues) {
                position += 2 * (op - 0xbf);
            } else if (op < 0xe0) {
                const uint8_t kind = op & 15;
                const int count = code.u8() + 1;
                for (int i = 0; i < count; ++i) {
                    addField(position, kind);
                    position += IggyFieldSize(kind);
                }
            } else if (op == kOpStruct) {
                IndexType type;
                type.size = code.u8();
                type.fields.resize(code.u8());
                for (auto& field : type.fields) { field.first = code.u8(); field.second = code.u8(); }
                addStruct(type);
            } else if (op == kOpSkip) {
                position += code.u8() + 1;
            } else if (op == kOpSkipLong) {
                position += code.u32();
            } else if (op == kOpNothing) {
                code.u8();
            } else {
                code.Fail("unknown index op " + Reader::Hex(op));
            }
            if (position > data.size()) code.Fail("walks past the movie end");
        }
        walkedTo = position;
        if (stats) ++stats->chunks;
    }
    if (stats) stats->walkedTo = walkedTo;
    if (walkedTo != data.size())
        throw FormatError("iggy index: walks to 0x" + Reader::Hex(walkedTo) + ", movie is 0x" + Reader::Hex(data.size()));
}

std::vector<Bytes> IggyMovie::BuildIndex() const
{
    // One single-field type per kind used; pointers are type 0.
    std::vector<uint8_t> kinds{kFieldPointer};
    for (auto& field : fields)
        if (std::find(kinds.begin(), kinds.end(), field.second) == kinds.end()) kinds.push_back(field.second);
    if (kinds.size() > 0x7f) throw FormatError("iggy index: too many field kinds");
    Writer typeTable;
    typeTable.u8((uint8_t)kinds.size());
    for (uint8_t kind : kinds) {
        typeTable.u8((uint8_t)std::max<size_t>(IggyFieldSize(kind), 1));
        typeTable.u8(1);
        typeTable.u8(0);
        typeTable.u8(kind);
    }

    std::vector<Bytes> chunks;
    Writer chunk;
    size_t position = 0;
    auto skip = [&](size_t gap) {
        while (gap > 0) {
            if (gap % 2 == 0 && gap <= 32) { chunk.u8((uint8_t)(kOpSkipEven + gap / 2 - 1)); gap = 0; }
            else if (gap <= 256) { chunk.u8(kOpSkip); chunk.u8((uint8_t)(gap - 1)); gap = 0; }
            else { const uint32_t step = (uint32_t)std::min<size_t>(gap, 0xffffffffu); chunk.u8(kOpSkipLong); chunk.u32(step); gap -= step; }
        }
    };
    auto startChunk = [&]() {
        chunk = Writer();
        chunk.Append(typeTable.out);
        skip(position);   // chunks 2+ start with ff <where the last one ended>, as the game's do
    };
    startChunk();
    for (auto it = fields.begin(); it != fields.end();) {
        if (chunk.size() > kMaxChunk) {
            chunks.push_back(std::move(chunk.out));
            startChunk();
        }
        const uint32_t at = it->first;
        const uint8_t kind = it->second;
        const size_t size = std::max<size_t>(IggyFieldSize(kind), 1);
        if (at < position) throw FormatError("iggy index: overlapping fields at 0x" + Reader::Hex(at));
        skip(at - position);
        position = at;
        // a run of back-to-back fields of this kind
        size_t run = 0;
        auto next = it;
        while (next != fields.end() && next->second == kind && next->first == at + run * size && run < 256) { ++run; ++next; }
        const uint8_t type = (uint8_t)(std::find(kinds.begin(), kinds.end(), kind) - kinds.begin());
        if (run == 1) chunk.u8(type);
        else if (run <= 64) { chunk.u8((uint8_t)(0x7f + run)); chunk.u8(type); }
        else if (kind == kFieldPointer) { chunk.u8(kOpValues | kFieldPointer); chunk.u8((uint8_t)(run - 1)); }
        else { run = 64; chunk.u8((uint8_t)(0x7f + run)); chunk.u8(type); }
        position += run * size;
        for (size_t i = 0; i < run; ++i) ++it;
    }
    if (position > data.size()) throw FormatError("iggy index: field past the movie end");
    skip(data.size() - position);
    chunks.push_back(std::move(chunk.out));
    return chunks;
}

// ---------------------------------------------------------------- edits

void IggyMovie::CheckField(uint32_t at, size_t size) const
{
    if ((uint64_t)at + size > data.size()) throw FormatError("iggy movie: offset 0x" + Reader::Hex(at) + " outside the movie");
}

int64_t IggyMovie::Pointer(uint32_t at) const
{
    CheckField(at, 8);
    const uint64_t relative = GetU64(&data[at]);
    if (relative == kNullPointer) return -1;
    return (int64_t)at + (int64_t)relative;
}

void IggyMovie::SetPointer(uint32_t at, int64_t target)
{
    CheckField(at, 8);
    PutU64(&data[at], target < 0 ? kNullPointer : (uint64_t)(target - (int64_t)at));
    fields[at] = kFieldPointer;
}

uint32_t IggyMovie::Append(const uint8_t* bytes, size_t size, size_t alignment)
{
    while (alignment > 1 && data.size() % alignment) data.push_back(0);
    const uint32_t at = (uint32_t)data.size();
    data.insert(data.end(), bytes, bytes + size);
    return at;
}

void IggyMovie::Insert(uint32_t at, uint32_t count)
{
    if (at > data.size()) throw FormatError("iggy movie: insert outside the movie");
    // Pointer values are relative: re-derive every pointer's target first.
    std::map<uint32_t, int64_t> targets;
    for (auto& field : fields)
        if (field.second == kFieldPointer) targets[field.first] = Pointer(field.first);
    data.insert(data.begin() + at, count, 0);
    std::map<uint32_t, uint8_t> moved;
    for (auto& field : fields) moved[field.first >= at ? field.first + count : field.first] = field.second;
    fields.swap(moved);
    for (auto& entry : targets) {
        const uint32_t field = entry.first >= at ? entry.first + count : entry.first;
        int64_t target = entry.second;
        if (target >= (int64_t)at) target += count;
        PutU64(&data[field], target < 0 ? kNullPointer : (uint64_t)(target - (int64_t)field));
    }
}

void IggyMovie::Erase(uint32_t at, uint32_t count)
{
    if ((uint64_t)at + count > data.size()) throw FormatError("iggy movie: erase outside the movie");
    const uint32_t end = at + count;
    std::map<uint32_t, int64_t> targets;
    for (auto& field : fields) {
        if (field.second != kFieldPointer || (field.first >= at && field.first < end)) continue;
        const int64_t target = Pointer(field.first);
        if (target >= (int64_t)at && target < (int64_t)end)
            throw FormatError("iggy movie: erase would cut the target of the pointer at 0x" + Reader::Hex(field.first));
        targets[field.first] = target;
    }
    data.erase(data.begin() + at, data.begin() + end);
    std::map<uint32_t, uint8_t> moved;
    for (auto& field : fields) {
        if (field.first >= at && field.first < end) continue;
        moved[field.first >= end ? field.first - count : field.first] = field.second;
    }
    fields.swap(moved);
    for (auto& entry : targets) {
        const uint32_t field = entry.first >= end ? entry.first - count : entry.first;
        int64_t target = entry.second;
        if (target >= (int64_t)end) target -= count;
        PutU64(&data[field], target < 0 ? kNullPointer : (uint64_t)(target - (int64_t)field));
    }
}

bool IggyMovie::Script(const uint8_t*& abc, uint32_t& size) const
{
    if (data.size() < kHeaderSize) return false;
    const int64_t block = Pointer(kScriptField);
    if (block < 0 || (uint64_t)block + kScriptBlockHeader > data.size() || GetU64(&data[(size_t)block]) != kNullPointer) return false;
    size = GetU32(&data[(size_t)block + 8]);
    if ((uint64_t)block + kScriptBlockHeader + size > data.size()) return false;
    abc = &data[(size_t)block + kScriptBlockHeader];
    return true;
}

void IggyMovie::SetScript(const uint8_t* abc, uint32_t size)
{
    if (data.size() < kHeaderSize) throw FormatError("iggy movie: too small for a script");
    const int64_t oldBlock = Pointer(kScriptField);
    if (oldBlock >= 0 && (uint64_t)oldBlock + kScriptBlockHeader <= data.size()) {
        const uint64_t oldEnd = ((uint64_t)oldBlock + kScriptBlockHeader + GetU32(&data[(size_t)oldBlock + 8]) + 7) & ~7ull;
        // Ours from an earlier edit (past the loaded movie, nothing after it):
        // written over, not left behind. The game's own block always stays.
        if (oldEnd >= data.size() && oldBlock >= (int64_t)loadedSize) {
            data.resize((size_t)oldBlock);
            fields.erase(fields.lower_bound((uint32_t)oldBlock), fields.end());
        }
    }
    Bytes block(kScriptBlockHeader);
    PutU64(&block[0], kNullPointer);
    PutU32(&block[8], size);
    block.insert(block.end(), abc, abc + size);
    while (block.size() % 8) block.push_back(0);   // the movie ends 8-aligned (as ReplaceIggyAbc's)
    const uint32_t at = Append(block.data(), block.size(), 16);
    fields[at] = kFieldPointer;                      // the block's null +0
    SetPointer((uint32_t)kScriptField, at);
}

// ---------------------------------------------------------------- document

void IggyDocument::Parse(const uint8_t* bytes, size_t size, IggyIndexStats* stats)
{
    file.Parse(bytes, size);
    indexSubfiles.clear();
    movieSubfile = SIZE_MAX;
    std::vector<const Bytes*> chunks;
    for (size_t i = 0; i < file.subfiles.size(); ++i) {
        if (file.subfiles[i].id == 1 && movieSubfile == SIZE_MAX) movieSubfile = i;
        else if (file.subfiles[i].id == 0) { indexSubfiles.push_back(i); chunks.push_back(&file.subfiles[i].data); }
        else throw FormatError("iggy: unknown subfile id " + std::to_string(file.subfiles[i].id));
    }
    if (movieSubfile == SIZE_MAX || chunks.empty()) throw FormatError("iggy: no movie or no index");
    if (movieSubfile != 0) throw FormatError("iggy: the movie is not the first subfile");
    movie.Load(file.subfiles[movieSubfile].data, chunks, stats);
    originalIndex.clear();
    for (const Bytes* chunk : chunks) originalIndex.push_back(*chunk);
    originalFields = movie.fields;
    originalMovieSize = movie.data.size();
}

// When the movie only grew at its end (the old part's fields unchanged, new
// fields all past the old end - e.g. a swapped script), the original index
// plus commands for the new part: what ReplaceIggyAbc has done in game. Uses
// only type-free commands (d2 / fd), so the chunk's own type table is enough.
bool IggyDocument::AppendedIndex(std::vector<Bytes>& index) const
{
    if (movie.data.size() < originalMovieSize || originalIndex.empty()) return false;
    auto firstNew = movie.fields.lower_bound((uint32_t)originalMovieSize);
    if (!std::equal(movie.fields.begin(), firstNew, originalFields.begin(), originalFields.end())) return false;
    index = originalIndex;
    Writer commands;
    size_t position = originalMovieSize;
    auto skip = [&](size_t gap) {
        while (gap > 0) {
            if (gap <= 256) { commands.u8(kOpSkip); commands.u8((uint8_t)(gap - 1)); gap = 0; }
            else { const uint32_t step = (uint32_t)std::min<size_t>(gap, 0xffffffffu); commands.u8(kOpSkipLong); commands.u32(step); gap -= step; }
        }
    };
    for (auto it = firstNew; it != movie.fields.end();) {
        const uint32_t at = it->first;
        const uint8_t kind = it->second;
        if (at < position) return false;
        skip(at - position);
        position = at;
        if (kind == kFieldPointer) {
            size_t run = 0;
            while (it != movie.fields.end() && it->second == kFieldPointer && it->first == at + run * 8 && run < 256) { ++run; ++it; }
            commands.u8(kOpValues | kFieldPointer);
            commands.u8((uint8_t)(run - 1));
            position += run * 8;
        } else {
            const size_t size = std::max<size_t>(IggyFieldSize(kind), 1);
            commands.u8(kOpStruct); commands.u8((uint8_t)size); commands.u8(1); commands.u8(0); commands.u8(kind);
            position += size;
            ++it;
        }
    }
    if (position > movie.data.size()) return false;
    skip(movie.data.size() - position);
    Bytes& last = index.back();
    if (last.size() + commands.size() > IggyMovie::kMaxChunk + 384) {
        // a new chunk: the first one's type table, then skip to the old end
        Reader table(index.front().data(), index.front().size());
        const uint8_t count = table.u8();
        for (uint8_t i = 0; i < count; ++i) { table.u8(); table.skip(2 * table.u8()); }
        Writer chunk;
        chunk.Raw(index.front().data(), table.position());
        size_t gap = originalMovieSize;
        while (gap > 0) { const uint32_t step = (uint32_t)std::min<size_t>(gap, 0xffffffffu); chunk.u8(kOpSkipLong); chunk.u32(step); gap -= step; }
        chunk.Append(commands.out);
        index.push_back(std::move(chunk.out));
    } else {
        last.insert(last.end(), commands.out.begin(), commands.out.end());
    }
    return true;
}

Bytes IggyDocument::Write() const
{
    IggyFile out;
    out.header = file.header;
    IggySubfile movieFile;
    movieFile.id = 1;
    movieFile.data = movie.data;
    out.subfiles.push_back(std::move(movieFile));
    std::vector<Bytes> index;
    if (movie.fields == originalFields && movie.data.size() == originalMovieSize) index = originalIndex;
    else if (!AppendedIndex(index)) index = movie.BuildIndex();
    for (Bytes& chunk : index) {
        IggySubfile indexFile;
        indexFile.id = 0;
        indexFile.data = std::move(chunk);
        out.subfiles.push_back(std::move(indexFile));
    }
    // keep each original subfile's second size field where it still applies
    for (size_t i = 0; i < out.subfiles.size() && i < file.subfiles.size(); ++i)
        if (file.subfiles[i].size2 != file.subfiles[i].data.size()) out.subfiles[i].size2 = file.subfiles[i].size2;
    return out.Write();
}

}  // namespace xv2ui

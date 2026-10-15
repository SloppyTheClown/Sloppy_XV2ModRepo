// ---------------------------------------------------------------------------
//  Game file reading - a file of the game's data as the game finds it: loose
//  <game>\data first (what XV2Patcher loads, so x2m and Revamp files count),
//  then the CRIWARE .cpk archives in <game>\cpk. For mods that need the
//  game's own data (item lists, names, ...) without shipping a copy:
//  Lua's files.ReadGameFile, DLL mods' XV2Files_ReadGameFile / FindGameFile /
//  ReadGameFilePart (sdk\xv2_files.h). Read-only; patches other mods register
//  are not applied.
//
//  CPK reading, ported from XV2DataEdit\xv2data\cpk.py (itself after XV2-Tools
//  Xv2CoreLib/CPK/CriPakTools/CPK.cs): a CPK header @UTF table, a TOC @UTF
//  table listing every file, and optionally CRILAYLA-compressed file data.
//  Compressed entries can be decoded from their end only (written for Battle
//  Styles' style list, which needs the names at the end of ~1.2 GB of EANs).
// ---------------------------------------------------------------------------
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "game_files.h"
#include "host.h"

namespace {

uint16_t BigU16(const uint8_t* at) { return (uint16_t)(at[0] << 8 | at[1]); }
uint32_t BigU32(const uint8_t* at) { return (uint32_t)at[0] << 24 | (uint32_t)at[1] << 16 | (uint32_t)at[2] << 8 | at[3]; }
uint64_t BigU64(const uint8_t* at) { return (uint64_t)BigU32(at) << 32 | BigU32(at + 4); }

std::string Lower(std::string text)
{
    for (char& c : text) {
        if (c == '\\') c = '/';
        else c = (char)tolower((unsigned char)c);
    }
    return text;
}

// One @UTF cell: numbers widened to u64, strings as text (data blobs unused).
struct Cell {
    uint64_t number = 0;
    std::string text;
};
typedef std::map<std::string, Cell> Row;

// @UTF table -> rows. Tables may be XOR-obfuscated (seed 0x655F, multiplier
// 0x4115, per cpk.py _decrypt_utf).
bool ParseUtf(std::vector<uint8_t> data, std::vector<Row>& rows)
{
    if (data.size() < 32) return false;
    if (memcmp(data.data(), "@UTF", 4) != 0) {
        uint32_t mask = 0x655F;
        for (uint8_t& byte : data) { byte ^= (uint8_t)mask; mask *= 0x4115; }
        if (memcmp(data.data(), "@UTF", 4) != 0) return false;
    }
    const size_t base = 8;  // offsets in the table header count from here
    const uint8_t* bytes = data.data();
    size_t rowsAt = BigU32(bytes + 8) + base, stringsAt = BigU32(bytes + 12) + base;
    uint16_t columnCount = BigU16(bytes + 24), rowLength = BigU16(bytes + 26);
    uint32_t rowCount = BigU32(bytes + 28);

    auto text = [&](uint32_t offset) {
        size_t start = stringsAt + offset;
        if (start >= data.size()) return std::string();
        return std::string((const char*)bytes + start, strnlen((const char*)bytes + start, data.size() - start));
    };
    // Reads one value of `type` at `pos`; returns its size (0 = unknown type).
    auto read = [&](int type, size_t pos, Cell& cell) -> size_t {
        static const size_t kSizes[] = {1, 1, 2, 2, 4, 4, 8, 8, 4, 8};
        if (type == 0xA) { if (pos + 4 > data.size()) return 0; cell.text = text(BigU32(bytes + pos)); return 4; }
        if (type == 0xB) return 8;  // data blob: offset + size, not needed
        if (type > 9 || pos + kSizes[type] > data.size()) return 0;
        switch (kSizes[type]) {
        case 1: cell.number = bytes[pos]; break;
        case 2: cell.number = BigU16(bytes + pos); break;
        case 4: cell.number = BigU32(bytes + pos); break;
        default: cell.number = BigU64(bytes + pos); break;
        }
        return kSizes[type];
    };

    // Column storage (high nibble): 0x10 zero, 0x30/0x70 constant, 0x50 per row.
    struct Column { std::string name; int storage, type; Cell constant; };
    std::vector<Column> columns;
    size_t pos = base + 24;
    for (int c = 0; c < columnCount; ++c) {
        if (pos + 5 > data.size()) return false;
        Column column{text(BigU32(bytes + pos + 1)), bytes[pos] & 0xF0, bytes[pos] & 0x0F, {}};
        pos += 5;
        if (column.storage == 0x30 || column.storage == 0x70) {
            size_t size = read(column.type, pos, column.constant);
            if (!size) return false;
            pos += size;
        }
        columns.push_back(column);
    }
    rows.resize(rowCount);
    for (uint32_t r = 0; r < rowCount; ++r) {
        pos = rowsAt + (size_t)r * rowLength;
        for (const Column& column : columns) {
            Cell& cell = rows[r][column.name];
            if (column.storage == 0x50) {
                size_t size = read(column.type, pos, cell);
                if (!size) return false;
                pos += size;
            } else if (column.storage == 0x30 || column.storage == 0x70) {
                cell = column.constant;
            }
        }
    }
    return true;
}

struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    bool Open(const std::wstring& path)
    {
        handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        return handle != INVALID_HANDLE_VALUE;
    }
    bool Read(uint64_t offset, size_t size, std::vector<uint8_t>& out)
    {
        out.resize(size);
        LARGE_INTEGER at; at.QuadPart = (LONGLONG)offset;
        DWORD got = 0;
        return SetFilePointerEx(handle, at, nullptr, FILE_BEGIN) && ReadFile(handle, out.data(), (DWORD)size, &got, nullptr) &&
               got == size;
    }
};

// A CPK packet: 4-byte tag, 4 bytes, u64 little-endian size, then @UTF.
bool ReadPacket(File& file, uint64_t offset, std::vector<Row>& rows)
{
    std::vector<uint8_t> head, table;
    if (!file.Read(offset, 16, head)) return false;
    uint64_t size;
    memcpy(&size, head.data() + 8, 8);
    if (size > (256u << 20)) return false;
    return file.Read(offset + 16, (size_t)size, table) && ParseUtf(std::move(table), rows);
}

// CRILAYLA: LZ-style, read backwards from the end, with a 0x100-byte raw
// prefix stored after the compressed body (cpk.py decompress_crilayla).
// Stored layout: "CRILAYLA", +8 u32 uncompressed size (without the prefix),
// +12 u32 prefix offset (from +0x10), body, prefix. The decoded file is the
// prefix followed by the body's output, written from the file's end down.
static const size_t kCrilaylaHeader = 0x10, kCrilaylaPrefix = 0x100;

enum class Decoded { Done, Corrupt, NeedsMore };

// Decodes a CRILAYLA body from the file's end backwards down to file offset
// `stopAt` (>= kCrilaylaPrefix), into `tail` = file bytes [stopAt, fileSize).
// `window` holds the stored bytes from `windowStart` to `storedSize`; the
// decoder only moves backwards, so a window covering the end is enough for
// the end of the file (NeedsMore: it reached `windowStart` first).
Decoded DecodeCrilaylaTail(const uint8_t* window, uint64_t windowStart, uint64_t storedSize, uint64_t fileSize,
                           uint64_t stopAt, std::vector<uint8_t>& tail)
{
    tail.assign((size_t)(fileSize - stopAt), 0);
    long long readAt = (long long)storedSize - (long long)kCrilaylaPrefix - 1;
    uint8_t pool = 0;
    int bitsLeft = 0;
    bool outOfWindow = false;
    auto bits = [&](int count) {
        uint32_t value = 0;
        while (count) {
            if (bitsLeft == 0) {
                if (readAt < (long long)windowStart) { outOfWindow = true; return 0u; }
                pool = window[(size_t)(readAt-- - (long long)windowStart)];
                bitsLeft = 8;
            }
            int take = std::min(bitsLeft, count);
            value = (value << take) | ((pool >> (bitsLeft - take)) & ((1u << take) - 1));
            bitsLeft -= take;
            count -= take;
        }
        return value;
    };
    long long writeAt = (long long)fileSize - 1, stop = (long long)stopAt;
    while (writeAt >= stop && !outOfWindow) {
        if (bits(1)) {
            long long copyFrom = writeAt + bits(13) + 3;
            uint32_t length = 3;
            static const int kLengthBits[] = {2, 3, 5, 8};
            bool more = true;
            for (int lengthBits : kLengthBits) {
                uint32_t part = bits(lengthBits);
                length += part;
                if (part != (1u << lengthBits) - 1) { more = false; break; }
            }
            while (more) {
                uint32_t part = bits(8);
                length += part;
                more = part == 255 && !outOfWindow;
            }
            for (uint32_t i = 0; i < length && writeAt >= stop; ++i) {
                if (copyFrom >= (long long)fileSize) return Decoded::Corrupt;
                tail[(size_t)(writeAt-- - stop)] = tail[(size_t)(copyFrom-- - stop)];
            }
        } else {
            tail[(size_t)(writeAt-- - stop)] = (uint8_t)bits(8);
        }
    }
    if (outOfWindow) return windowStart == 0 ? Decoded::Corrupt : Decoded::NeedsMore;
    return Decoded::Done;
}

struct Entry { uint64_t offset, storedSize; };
struct Archive {
    std::wstring path;
    std::map<std::string, Entry> files;  // lower-case "dir/name"
};

bool LoadArchive(const std::wstring& path, Archive& archive)
{
    File file;
    std::vector<uint8_t> tag;
    std::vector<Row> header, toc;
    if (!file.Open(path) || !file.Read(0, 4, tag) || memcmp(tag.data(), "CPK ", 4) != 0) return false;
    if (!ReadPacket(file, 0, header) || header.empty()) return false;
    uint64_t tocAt = header[0]["TocOffset"].number, contentAt = header[0]["ContentOffset"].number;
    if (!tocAt || tocAt == ~0ull) return true;  // no TOC: nothing to find in it
    // File offsets count from the TOC or the content, whichever comes first.
    uint64_t offsetBase = (!contentAt || tocAt < contentAt) ? tocAt : contentAt;
    if (!ReadPacket(file, tocAt, toc)) return false;
    archive.path = path;
    for (Row& row : toc) {
        std::string directory = row["DirName"].text, name = row["FileName"].text;
        std::string key = Lower(directory.empty() ? name : directory + "/" + name);
        archive.files[key] = {row["FileOffset"].number + offsetBase, row["FileSize"].number};
    }
    return true;
}

// Every data*.cpk, in the priority order cpk.py's CpkSet uses: DLC archives
// (data_dNN_..., highest NN first) before the base ones, base ones in
// XV2-Tools' CPK_Reader order; regional (_as) and _develop archives skipped.
bool g_archivesLoaded = false;  // XV2Files_SetGameFolder only works before

std::vector<std::unique_ptr<Archive>>& Archives(const std::wstring& gameDir)
{
    static std::vector<std::unique_ptr<Archive>> archives;
    if (g_archivesLoaded) return archives;
    g_archivesLoaded = true;
    static const char* kBaseOrder[] = {"data_d4_5_xv1.cpk", "data_d6_dlc.cpk", "data_d0_stv.cpk", "data2.cpk",
                                       "data1.cpk", "data0.cpk", "data.cpk"};
    std::vector<std::pair<double, std::string>> extra;
    std::vector<std::string> names;
    WIN32_FIND_DATAA found;
    std::string cpkDir(gameDir.begin(), gameDir.end());  // game paths are ASCII
    cpkDir += "\\cpk\\";
    HANDLE search = FindFirstFileA(cpkDir.append("data*.cpk").c_str(), &found);
    cpkDir.resize(cpkDir.size() - strlen("data*.cpk"));
    if (search != INVALID_HANDLE_VALUE) {
        do names.push_back(Lower(found.cFileName));
        while (FindNextFileA(search, &found));
        FindClose(search);
    }
    std::vector<std::string> order;
    for (const std::string& name : names) {
        if (name.size() > 7 && (name.compare(name.size() - 7, 7, "_as.cpk") == 0)) continue;
        if (name.find("_develop.cpk") != std::string::npos) continue;
        if (std::find(std::begin(kBaseOrder), std::end(kBaseOrder), name) != std::end(kBaseOrder)) continue;
        int major = 0, minor = 0;
        double rank = 0;
        if (sscanf_s(name.c_str(), "data_d%d_%d_", &major, &minor) == 2) rank = major + minor / 10.0;
        else if (sscanf_s(name.c_str(), "data_d%d_", &major) == 1) rank = major;
        extra.push_back({-rank, name});
    }
    std::stable_sort(extra.begin(), extra.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (auto& item : extra) order.push_back(item.second);
    for (const char* name : kBaseOrder)
        if (std::find(names.begin(), names.end(), name) != names.end()) order.push_back(name);
    for (const std::string& name : order) {
        auto archive = std::make_unique<Archive>();
        std::string full = cpkDir + name;
        if (LoadArchive(std::wstring(full.begin(), full.end()), *archive)) archives.push_back(std::move(archive));
    }
    return archives;
}

// The game folder: the exe is <game>\bin\DBXV2.exe (offline tools may set
// another with XV2Files_SetGameFolder).
std::wstring g_gameFolder;

const std::wstring& GameDir()
{
    if (g_gameFolder.empty()) {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        g_gameFolder = exe;
        for (int level = 0; level < 2; ++level) g_gameFolder.resize(g_gameFolder.find_last_of(L'\\'));
    }
    return g_gameFolder;
}

// "data\Msg\X.msg", "/msg/x.msg" -> "msg/x.msg": '/' separators, no leading
// slash or data folder (case kept: loose file names are case-insensitive anyway).
std::string NormaliseGamePath(const char* path)
{
    std::string normal(path);
    std::replace(normal.begin(), normal.end(), '\\', '/');
    while (!normal.empty() && normal[0] == '/') normal.erase(0, 1);
    if (Lower(normal.substr(0, 5)) == "data/") normal.erase(0, 5);
    return normal;
}

// Biggest file handed out (the same cap as a file patch): the game's files
// are far smaller, so a bigger size means a damaged archive entry.
const uint64_t kMaxReadBytes = 512ull << 20;

// Where a game file is: loose under data\, or an entry of an archive.
struct Location {
    std::wstring filePath;    // the loose file or the .cpk
    uint64_t offset = 0;      // where its stored bytes start in filePath
    uint64_t storedSize = 0;  // stored bytes (compressed size for CRILAYLA)
    uint64_t fileSize = 0;    // the game file's real size
    bool compressed = false;  // CRILAYLA-compressed archive entry
    uint32_t prefixAt = 0;    // CRILAYLA: the raw prefix's offset (from +0x10)
    std::string source;       // "loose" or the archive's file name
};

// Finds `path` (relative to data, '/') - loose first, then the archives in
// priority order - reading at most a CRILAYLA header. False if found nowhere
// or the entry is damaged.
bool Locate(const std::string& path, Location& location)
{
    std::wstring loose = GameDir() + L"\\data\\" + std::wstring(path.begin(), path.end());
    std::replace(loose.begin(), loose.end(), L'/', L'\\');
    File file;
    if (file.Open(loose)) {
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file.handle, &size) || (uint64_t)size.QuadPart > kMaxReadBytes) return false;
        location.filePath = loose;
        location.storedSize = location.fileSize = (uint64_t)size.QuadPart;
        location.source = "loose";
        return true;
    }
    std::string key = Lower("data/" + path);
    for (auto& archive : Archives(GameDir())) {
        auto found = archive->files.find(key);
        if (found == archive->files.end()) continue;
        const Entry& entry = found->second;
        if (entry.storedSize > kMaxReadBytes) return false;
        location.filePath = archive->path;
        location.offset = entry.offset;
        location.storedSize = location.fileSize = entry.storedSize;
        location.source = std::string(archive->path.begin(), archive->path.end());
        location.source = location.source.substr(location.source.find_last_of('\\') + 1);
        std::vector<uint8_t> header;
        if (!file.Open(archive->path) || !file.Read(entry.offset, (size_t)std::min<uint64_t>(kCrilaylaHeader, entry.storedSize), header))
            return false;
        if (header.size() == kCrilaylaHeader && memcmp(header.data(), "CRILAYLA", 8) == 0) {
            uint32_t uncompressedSize;
            memcpy(&uncompressedSize, header.data() + 8, 4);
            memcpy(&location.prefixAt, header.data() + 12, 4);
            if ((uint64_t)location.prefixAt + kCrilaylaHeader + kCrilaylaPrefix > entry.storedSize) return false;
            location.compressed = true;
            location.fileSize = (uint64_t)uncompressedSize + kCrilaylaPrefix;
            if (location.fileSize > kMaxReadBytes) return false;
        }
        return true;
    }
    return false;
}

// Reads the game file's bytes [from, from + size) (cut at its end) into `out`.
// A compressed entry is decoded from its end backwards, so a part near the
// end (or inside the first 0x100 bytes, kept raw) costs little; the rest of
// the file's start is neither read nor decoded.
bool ReadPart(const Location& location, uint64_t from, uint64_t size, std::vector<uint8_t>& out)
{
    if (from > location.fileSize) return false;
    size = std::min(size, location.fileSize - from);
    File file;
    if (!file.Open(location.filePath)) return false;
    if (!location.compressed) return file.Read(location.offset + from, (size_t)size, out);

    std::vector<uint8_t> prefix, window, tail;
    if (!file.Read(location.offset + kCrilaylaHeader + location.prefixAt, kCrilaylaPrefix, prefix)) return false;
    const uint64_t until = from + size;
    if (until > kCrilaylaPrefix) {
        // The body decodes from the file's end down to `stopAt`. Reading the
        // whole stored entry at once when the decode reaches the start
        // anyway; otherwise a window at the stored end, 4x larger each time
        // the decode needs more of it (64 KB covers an EAN's names).
        const uint64_t stopAt = std::max<uint64_t>(from, kCrilaylaPrefix);
        const uint64_t firstWindow = stopAt == kCrilaylaPrefix ? location.storedSize : 64 << 10;
        for (uint64_t windowSize = firstWindow;; windowSize *= 4) {
            uint64_t windowStart = location.storedSize > windowSize ? location.storedSize - windowSize : 0;
            if (!file.Read(location.offset + windowStart, (size_t)(location.storedSize - windowStart), window)) return false;
            Decoded result = DecodeCrilaylaTail(window.data(), windowStart, location.storedSize, location.fileSize, stopAt, tail);
            if (result == Decoded::Corrupt) return false;
            if (result == Decoded::Done) break;
        }
    }
    // file bytes [0, 0x100) = prefix, [0x100, fileSize) = the decoded body
    out.clear();
    out.reserve((size_t)size);
    if (from < kCrilaylaPrefix)
        out.insert(out.end(), prefix.begin() + from, prefix.begin() + std::min<uint64_t>(until, kCrilaylaPrefix));
    if (until > kCrilaylaPrefix) {
        const uint64_t tailStart = std::max<uint64_t>(from, kCrilaylaPrefix);  // = the decode's stopAt
        out.insert(out.end(), tail.begin(), tail.begin() + (size_t)(until - tailStart));
    }
    return true;
}

// Lua scripts (render thread) and DLL mods (any thread) share the archive
// tables, which are built on the first read.
SRWLOCK g_readLock = SRWLOCK_INIT;

// Locates `path` and, when `read` is set, reads its bytes [from, from + size)
// into `out` - under the lock. False if found nowhere or unreadable.
bool FindAndRead(const char* path, Location& location, bool read = false, uint64_t from = 0,
                 uint64_t size = ~0ull, std::vector<uint8_t>* out = nullptr)
{
    if (!path || !*path) return false;
    const std::string normal = NormaliseGamePath(path);
    AcquireSRWLockExclusive(&g_readLock);
    bool found = Locate(normal, location) && (!read || ReadPart(location, from, size, *out));
    ReleaseSRWLockExclusive(&g_readLock);
    return found;
}

// Copies `text` into a caller's buffer, cut to fit, always 0-terminated.
void CopyText(const std::string& text, char* out, int outBytes)
{
    if (!out || outBytes <= 0) return;
    size_t length = std::min(text.size(), (size_t)outBytes - 1);
    memcpy(out, text.data(), length);
    out[length] = 0;
}

}  // namespace

// files.ReadGameFile(path) -> contents (a byte string), source ("loose" or
// the .cpk's name) | nil, "not found".
int FilesReadGameFile(lua_State* L)
{
    Location location;
    std::vector<uint8_t> bytes;
    if (!FindAndRead(luaL_checkstring(L, 1), location, true, 0, ~0ull, &bytes)) {
        lua_pushnil(L);
        lua_pushstring(L, "not found");
        return 2;
    }
    lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
    lua_pushstring(L, location.source.c_str());
    return 2;
}

// The exports below are for DLL mods (sdk\xv2_files.h documents them).

// Copies the whole file into `out` when it fits in `outBytes`; out may be null
// to ask for the size, which reads no file data. Returns the file's size, or
// -1 when it was found nowhere or can't be read.
extern "C" __declspec(dllexport) long long XV2Files_ReadGameFile(const char* path, void* out, size_t outBytes)
{
    Location location;
    if (!FindAndRead(path, location)) return -1;
    if (!out || location.fileSize > outBytes) return (long long)location.fileSize;
    std::vector<uint8_t> bytes;
    if (!FindAndRead(path, location, true, 0, ~0ull, &bytes)) return -1;
    memcpy(out, bytes.data(), bytes.size());
    return (long long)bytes.size();
}

// Size and source of a file without reading it: returns the size, or -1 when
// found nowhere; `source` gets "loose" or the .cpk's name (may be null).
extern "C" __declspec(dllexport) long long XV2Files_FindGameFile(const char* path, char* source, int sourceBytes)
{
    Location location;
    if (!FindAndRead(path, location)) return -1;
    CopyText(location.source, source, sourceBytes);
    return (long long)location.fileSize;
}

// Copies the file's bytes [offset, offset + outBytes), cut at its end, into
// `out`. Returns how many were copied, or -1 when found nowhere, unreadable
// or offset is past the end.
extern "C" __declspec(dllexport) long long XV2Files_ReadGameFilePart(const char* path, unsigned long long offset, void* out,
                                                                    size_t outBytes)
{
    Location location;
    std::vector<uint8_t> bytes;
    if (!out || !FindAndRead(path, location, true, offset, outBytes, &bytes)) return -1;
    memcpy(out, bytes.data(), bytes.size());
    return (long long)bytes.size();
}

// For offline tools that load the library outside the game: the game folder
// to read from (otherwise two levels above the exe). Only before the first
// read; returns 0 after it.
extern "C" __declspec(dllexport) int XV2Files_SetGameFolder(const wchar_t* folder)
{
    AcquireSRWLockExclusive(&g_readLock);
    const bool unused = !g_archivesLoaded;
    if (unused && folder) g_gameFolder = folder;
    ReleaseSRWLockExclusive(&g_readLock);
    return unused ? 1 : 0;
}

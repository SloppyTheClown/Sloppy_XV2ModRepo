// .iggy (RAD Game Tools' Iggy, as in DBXV2 1.26.01): the container, the
// movie subfile and its pointer index. Layout and the index byte code are in
// Research_Notes\ui-iggy-and-textures.md ("Formats", "Editing an .iggy's
// ActionScript"); the container follows eterniti's IggyFile.cpp
// (ClaudeTools\eternity_common\Misc), the index was found by disassembling
// iggy_w64.dll (0x59860..0x59bb1).
//
//   IggyFile    header + subfiles {id, bytes}; Parse/Write is byte-identical.
//   IggyMovie   the movie subfile (id 1) as bytes + a field map decoded from
//               the index subfiles (id 0): every offset Iggy rewrites at load
//               (relative pointers, runtime values). Edits go through it so the
//               index can be regenerated: Append, SetPointer, Insert/Erase
//               (pointers across the edit are fixed up), and the AS3 script.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "bytes.h"

namespace xv2ui {

struct IggySubfile {
    uint32_t id = 0;        // 1 = movie, 0 = pointer index (one or more, 16 KB each at most)
    uint32_t size2 = 0;     // the second size field (equal to the size in every game file)
    Bytes data;
};

struct IggyFile {
    static const uint32_t kSignature = 0xED0A6749;
    Bytes header;                       // the first 0x1C bytes, kept as they are
    std::vector<IggySubfile> subfiles;

    static bool Looks(const uint8_t* data, size_t size) { return size >= 0x20 && GetU32(data) == kSignature; }
    void Parse(const uint8_t* data, size_t size);
    Bytes Write() const;
};

// Field kinds of the index (iggy_w64.dll 0x590a0 / 0x592a0).
enum IggyFieldKind : uint8_t {
    kFieldPointer = 2,       // self-relative u64 -> absolute pointer (1 = null)
    kFieldBitReverse = 3,    // only on other platforms
    kFieldSwap16 = 4, kFieldSwap32 = 5, kFieldSwap64 = 6,   // endian swaps (no-ops on PC)
    kFieldNothing = 7,
    kFieldRuntimeU64 = 8,    // 8..11: replaced by a runtime u64 (context[0][kind-8])
    kFieldRuntimeByte = 12,  // 12..15: replaced by a runtime byte (context[1][kind-12])
};
size_t IggyFieldSize(uint8_t kind);

// One decoded index stream (for dumps and checks).
struct IggyIndexStats {
    size_t chunks = 0, pointers = 0, nullPointers = 0, otherFields = 0, walkedTo = 0;
    std::map<uint8_t, size_t> opcodes;  // first byte class -> count
};

class IggyMovie {
public:
    // The movie header (IGGYFlashHeader64, eterniti): fields used here.
    static const size_t kHeaderSize = 0xB8;
    static const size_t kWidthField = 0x38, kHeightField = 0x3C;   // f32? kept raw
    static const size_t kNamesField = 0x70;       // pointer: name table
    static const size_t kScriptField = 0x98;      // pointer: AS3 block {u64 1, u32 abc size, abc}
    static const size_t kScriptNamesField = 0xA0; // pointer: AS3 names

    Bytes data;                              // the movie subfile
    std::map<uint32_t, uint8_t> fields;      // offset -> kind (every field the index names)
    size_t loadedSize = 0;                   // the movie's size as loaded

    // Decodes the index subfiles over `movie`. Throws on a stream that
    // doesn't walk to the movie's end.
    void Load(const Bytes& movie, const std::vector<const Bytes*>& indexChunks, IggyIndexStats* stats = nullptr);
    // A fresh index for the current fields: chunks of at most kMaxChunk bytes,
    // each its own stream from movie offset 0 (like the game's multi-index files).
    std::vector<Bytes> BuildIndex() const;
    static const size_t kMaxChunk = 16000;

    // Pointers: the absolute target of the pointer field at `at`, or -1 for null.
    int64_t Pointer(uint32_t at) const;
    // Makes `at` a pointer field to `target` (-1 = null).
    void SetPointer(uint32_t at, int64_t target);
    // Appends bytes at `alignment` and returns where they start.
    uint32_t Append(const uint8_t* bytes, size_t size, size_t alignment = 16);
    // Inserts `count` zero bytes at `at` / erases [at, at+count): fields and
    // pointers move, targets at or after `at` move with them (erasing a
    // pointer's target is refused).
    void Insert(uint32_t at, uint32_t count);
    void Erase(uint32_t at, uint32_t count);

    // The AS3 script (ABC); false when the movie has none.
    bool Script(const uint8_t*& abc, uint32_t& size) const;
    // Swaps the script: written at the end (the old block is dropped when it
    // is the last thing in the movie, i.e. one we appended earlier).
    void SetScript(const uint8_t* abc, uint32_t size);

private:
    void CheckField(uint32_t at, size_t size) const;
};

// The whole .iggy as an editable document.
struct IggyDocument {
    IggyFile file;
    IggyMovie movie;
    size_t movieSubfile = 0;
    std::vector<size_t> indexSubfiles;
    std::vector<Bytes> originalIndex;          // kept while the fields are unchanged
    std::map<uint32_t, uint8_t> originalFields;
    size_t originalMovieSize = 0;

    void Parse(const uint8_t* data, size_t size, IggyIndexStats* stats = nullptr);
    // Writes it back: the original index when nothing structural changed,
    // otherwise a regenerated one.
    Bytes Write() const;

private:
    bool AppendedIndex(std::vector<Bytes>& index) const;
};

}  // namespace xv2ui

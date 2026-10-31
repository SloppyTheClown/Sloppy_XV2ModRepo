// The UI's texture containers.
//
//   IggyTextures  .iggytex / .iggyted: "oggytex\0", u32 count at 8, a u32 kept
//                 raw at 12, then per texture {u16 id, u16 file, u32 size,
//                 u32 offset, u32 0} from 16; each texture a DDS at 128-byte
//                 alignment (so sizes/offsets often end in 0x80). `file` is
//                 0 for every texture held here in the game's files; a
//                 reference to another file's texture has size and offset 0. Found by comparing the game's files
//                 (Research_Notes\ui-iggy-and-textures.md).
//   EmbFile       .emb (ui\texture, ui\sprite): "#EMB" + a 0x20 header,
//                 {u32 offset relative to the entry, u32 size} per file,
//                 optional name table; files at 64-byte alignment. Layout
//                 after eterniti's EmbFile.cpp (ClaudeTools\eternity_common\Dimps).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "bytes.h"

namespace xv2ui {

struct IggyTexture {
    uint32_t id = 0;         // low u16 = texture id, high u16 = file (0 when held here)
    bool external = false;   // size/offset 0: held by another file
    uint32_t reserved = 0;   // the entry's 4th u32 (0 in every game file)
    Bytes dds;
};

struct IggyTextures {
    static constexpr const char kMagic[8] = {'o', 'g', 'g', 'y', 't', 'e', 'x', 0};
    static const size_t kAlignment = 128;
    uint32_t headerField = 16;       // the u32 at 12 (16 in the game's files: the header size?)
    std::vector<IggyTexture> textures;

    static bool Looks(const uint8_t* data, size_t size) { return size >= 16 && memcmp(data, kMagic, 8) == 0; }
    void Parse(const uint8_t* data, size_t size);
    Bytes Write() const;
    // Index of texture `id` (the low u16) held in this file, or -1.
    int Find(uint32_t id) const;
};

struct EmbEntry {
    std::string name;
    Bytes data;
};

struct EmbFile {
    static const uint32_t kSignature = 0x424D4523;   // "#EMB"
    static const size_t kHeaderSize = 0x20, kAlignment = 0x40;
    Bytes header;            // the 0x20 header, kept (unknown fields)
    bool hasNames = false;
    std::vector<EmbEntry> entries;

    static bool Looks(const uint8_t* data, size_t size) { return size >= kHeaderSize && GetU32(data) == kSignature; }
    void Parse(const uint8_t* data, size_t size);
    Bytes Write() const;
    int Find(const std::string& name) const;
};

}  // namespace xv2ui

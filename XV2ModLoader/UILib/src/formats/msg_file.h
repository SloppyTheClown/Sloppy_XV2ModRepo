// The game's text files (data\msg\*.msg, "#MSG"): parse + write, and keyed
// text edits - one mod's set of entries, applied over a file as the mods
// before it left it. Entries the mod doesn't name stay as they are, so any
// number of mods can change different entries of one file. Used by
// XV2UserInterfaceLib's message stages (ui_lib.cpp) and `xv2ui messages`.
//
// Layout (Xv2CoreLib MSG/Parser.cs + Deserializer.cs, checked against every
// msg in the 1.26.01 cpks by `xv2ui roundtrip`):
//   0   "#MSG"
//   4   u16  256 = entry names are UTF-16, else 8-bit
//   6   u16  1 = text is UTF-16, else 8-bit
//   8   u32  entry count
//   12  u32  -> name records: per entry {u32 string offset, u32 length in
//            characters, u32 size in bytes with the 0 terminator, u32 unknown}
//   16  u32  -> ids: per entry a u32 (the game looks text up by NAME; mods
//            renumber ids freely - ui-iggy-and-textures.md "msg text")
//   20  u32  -> line lists: per entry {u32 line count, u32 -> its line records}
//   24  u32  line count of the whole file
//   28  u32  -> line records: {u32 string offset, u32 length in characters,
//            u32 size in bytes, u32 parameter count ("%ls"/"%d" in the text;
//            with 0 the game shows them as is)}
//   then the names' strings, then the lines' strings, each 0-terminated.
// Text is stored with XML entities for ' " & < > (`Future&apos;s Doom`).
#pragma once
#include <map>
#include <string>
#include <vector>
#include "bytes.h"

namespace xv2ui {

struct MsgLine {
    std::u16string text;               // as stored: entities, no terminator
    uint32_t parameters = 0;           // the record's 4th u32
};

struct MsgEntry {
    std::u16string name;               // UTF-16 or widened 8-bit, no terminator
    uint32_t id = 0;
    uint32_t nameUnknown = 0;          // the name record's 4th u32
    std::vector<MsgLine> lines;
};

struct MsgFile {
    bool unicodeNames = true, unicodeText = true;
    std::vector<MsgEntry> entries;

    static bool Looks(const uint8_t* data, size_t size) { return size >= 32 && memcmp(data, "#MSG", 4) == 0; }
    void Parse(const uint8_t* data, size_t size);   // throws FormatError
    Bytes Write() const;
    int Find(const std::u16string& name) const;     // entry index, -1 = none
};

// Plain UTF-8 text -> as a msg file stores it: UTF-16 with ' " & < > as
// entities (like the game's own text). Throws FormatError on bad UTF-8.
std::u16string MsgText(const std::string& utf8);
// An ASCII entry name widened ("name_4293").
std::u16string MsgName(const std::string& ascii);

struct MessageReport {
    size_t replaced = 0, added = 0, unchanged = 0;
    size_t overEarlier = 0;            // replaced text an earlier mod had changed
    std::vector<std::string> notes;    // one line per entry over an earlier mod's
    std::string Summary() const;
    bool Changed() const { return replaced + added > 0; }
};

// Checks one edit before it is registered: `path` is a .msg and `name` a
// plain ASCII entry name. Empty = fine, else why not.
std::string CheckMessage(const std::string& path, const std::string& name);

// `current` with `messages` (entry name -> text as MsgText makes it) applied:
// each named entry's first line gets the text (its other lines and the
// line's parameter count stay); an entry the file lacks is added at the end
// with one line, the next free id and the parameter count of its text.
// `original` (the game's file) tells which entries an earlier mod changed.
// Throws FormatError on a file that doesn't parse.
Bytes ApplyMessages(const Bytes& original, const Bytes& current, const std::map<std::string, std::u16string>& messages,
                    MessageReport* report = nullptr);

}  // namespace xv2ui

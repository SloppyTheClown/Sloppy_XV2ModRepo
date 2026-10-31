// Image edits of the UI's texture containers (.iggytex / .iggyted, .emb):
// one mod's set of images, applied over a file as the mods before it left
// it - each named image replaced, or added when the file has none by that
// name. Images the mod doesn't name stay as they are, so any number of mods
// can change different images of one file. Used by XV2UserInterfaceLib's
// image stages (ui_lib.cpp) and `xv2ui images`.
//
// Image names:
//   .iggytex / .iggyted   the texture id as decimal text ("14"): the id the
//                         .iggy refers to (textures.h); the data a DDS.
//   .emb                  the entry's name ("CHARA01.dds"); in an EMB
//                         without names, "#<index>" ("#3"). Adding to an
//                         unnamed EMB only works at the end ("#<count>").
#pragma once
#include <map>
#include <string>
#include <vector>
#include "bytes.h"

namespace xv2ui {

struct ImageReport {
    size_t replaced = 0, added = 0, unchanged = 0;
    size_t overEarlier = 0;            // replaced an image an earlier mod had changed
    std::vector<std::string> notes;    // one line per image over an earlier mod's
    std::string Summary() const;
    bool Changed() const { return replaced + added > 0; }
};

// The container kind a file's bytes are, or "" when neither.
const char* ImageContainerKind(const uint8_t* data, size_t size);

// Checks one image before it is registered: the name fits `path`'s kind
// (by extension) and an iggytex image is a DDS. Empty = fine, else why not.
std::string CheckImage(const std::string& path, const std::string& name, const Bytes& data);

// `current` with `images` (name -> data) applied; `original` (the game's
// file) tells which images an earlier mod changed. Throws FormatError on a
// file that doesn't parse or a name it can't place.
Bytes ApplyImages(const Bytes& original, const Bytes& current, const std::map<std::string, Bytes>& images,
                  ImageReport* report = nullptr);

}  // namespace xv2ui

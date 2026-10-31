// Image edits of .iggytex / .iggyted / .emb (see image_edit.h).
#include "image_edit.h"
#include <cctype>
#include "textures.h"

namespace xv2ui {

namespace {

// The texture id in an iggytex image name ("14"), or -1 when it isn't one.
long TextureIdFromName(const std::string& name)
{
    if (name.empty() || name.size() > 5) return -1;
    for (char c : name)
        if (!isdigit((unsigned char)c)) return -1;
    const long id = std::stol(name);
    return id <= 0xffff ? id : -1;
}

// The index in an "#<index>" EMB name, or -1 when it isn't one.
long EmbIndexFromName(const std::string& name)
{
    if (name.size() < 2 || name[0] != '#') return -1;
    return TextureIdFromName(name.substr(1));
}

// Lower-case extension of a path (".iggytex"), "" without one.
std::string Extension(const std::string& path)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
    std::string extension = path.substr(dot);
    for (char& c : extension) c = (char)tolower((unsigned char)c);
    return extension;
}

// A texture entry's id field for a texture held in this file: the texture
// id in the low u16, file 0 in the high one - what every held texture in
// the cpks' iggytex files has (3832 entries checked, ui-iggy-and-textures.md).
uint32_t HeldTextureField(uint32_t textureId) { return textureId; }

// Index of the entry for texture `id` (held here or a reference), or -1.
int FindAnyTexture(const IggyTextures& file, uint32_t id)
{
    for (size_t i = 0; i < file.textures.size(); ++i)
        if ((file.textures[i].id & 0xffff) == id) return (int)i;
    return -1;
}

// The image an earlier mod may have changed: data of `name` in the game's
// file, or null when it has none (or none held there).
const Bytes* OriginalTexture(const IggyTextures& original, uint32_t id)
{
    const int at = original.Find(id);
    return at < 0 ? nullptr : &original.textures[at].dds;
}

int FindEmbEntry(const EmbFile& file, const std::string& name)
{
    const long index = EmbIndexFromName(name);
    if (index >= 0) return index < (long)file.entries.size() ? (int)index : -1;
    return file.Find(name);
}

// Counts one image's outcome: unchanged, replaced (over an earlier mod's
// change or not) or added.
void Count(ImageReport* report, const std::string& name, bool found, bool same, bool earlierChanged)
{
    if (!report) return;
    if (!found) { ++report->added; return; }
    if (same) { ++report->unchanged; return; }
    ++report->replaced;
    if (earlierChanged) {
        ++report->overEarlier;
        report->notes.push_back("image " + name + " was changed by an earlier mod; this one replaces it");
    }
}

Bytes ApplyToIggyTextures(const Bytes& originalBytes, const Bytes& currentBytes,
                          const std::map<std::string, Bytes>& images, ImageReport* report)
{
    IggyTextures original, current;
    original.Parse(originalBytes.data(), originalBytes.size());
    current.Parse(currentBytes.data(), currentBytes.size());
    for (const auto& image : images) {
        const long id = TextureIdFromName(image.first);
        if (id < 0) throw FormatError("\"" + image.first + "\" is not a texture id");
        const int at = FindAnyTexture(current, (uint32_t)id);
        const bool found = at >= 0;
        IggyTexture* texture = nullptr;
        if (found) {
            texture = &current.textures[at];
        } else {
            current.textures.emplace_back();
            texture = &current.textures.back();
        }
        const bool same = found && !texture->external && texture->dds == image.second;
        const Bytes* before = OriginalTexture(original, (uint32_t)id);
        const bool earlierChanged = found && !texture->external && (!before || *before != texture->dds);
        Count(report, image.first, found, same, earlierChanged);
        // a held texture keeps its entry as it was; a new one, or a reference
        // to another file's texture, becomes one held here
        if (!found || texture->external) texture->id = HeldTextureField((uint32_t)id);
        texture->external = false;
        texture->dds = image.second;
    }
    return current.Write();
}

Bytes ApplyToEmb(const Bytes& originalBytes, const Bytes& currentBytes, const std::map<std::string, Bytes>& images,
                 ImageReport* report)
{
    EmbFile original, current;
    original.Parse(originalBytes.data(), originalBytes.size());
    current.Parse(currentBytes.data(), currentBytes.size());
    for (const auto& image : images) {
        const int at = FindEmbEntry(current, image.first);
        const bool found = at >= 0;
        EmbEntry* entry = nullptr;
        if (found) {
            entry = &current.entries[at];
        } else {
            // a new entry: by name in a named EMB, or "#<count>" at the end
            const long index = EmbIndexFromName(image.first);
            if (current.hasNames && index >= 0)
                throw FormatError("no entry " + image.first + " (add to a named EMB by name)");
            if (!current.hasNames && index != (long)current.entries.size())
                throw FormatError("no entry \"" + image.first + "\" (this EMB has no names: use #0..#" +
                                  std::to_string(current.entries.size()) + ")");
            current.entries.emplace_back();
            entry = &current.entries.back();
            if (current.hasNames) entry->name = image.first;
        }
        const bool same = found && entry->data == image.second;
        const int originalAt = FindEmbEntry(original, image.first);
        const bool earlierChanged = found && (originalAt < 0 || original.entries[originalAt].data != entry->data);
        Count(report, image.first, found, same, earlierChanged);
        entry->data = image.second;
    }
    return current.Write();
}

}  // namespace

std::string ImageReport::Summary() const
{
    std::string text = std::to_string(replaced) + " image" + (replaced == 1 ? "" : "s") + " replaced";
    if (overEarlier) text += " (" + std::to_string(overEarlier) + " over an earlier mod's)";
    text += ", " + std::to_string(added) + " added";
    if (unchanged) text += ", " + std::to_string(unchanged) + " already the same";
    return text;
}

const char* ImageContainerKind(const uint8_t* data, size_t size)
{
    if (IggyTextures::Looks(data, size)) return "iggytex";
    if (EmbFile::Looks(data, size)) return "emb";
    return "";
}

std::string CheckImage(const std::string& path, const std::string& name, const Bytes& data)
{
    const std::string extension = Extension(path);
    if (data.empty()) return "no image data";
    if (extension == ".iggytex" || extension == ".iggyted") {
        if (TextureIdFromName(name) < 0) return "the image name must be a texture id (0..65535) for " + extension;
        if (data.size() < 128 || memcmp(data.data(), "DDS ", 4) != 0) return "not a DDS file";
        return "";
    }
    if (extension == ".emb") {
        if (name.empty()) return "no image name (the entry's name, or #<index>)";
        return "";
    }
    return "images go into .iggytex, .iggyted or .emb files, not " + (extension.empty() ? path : extension);
}

Bytes ApplyImages(const Bytes& original, const Bytes& current, const std::map<std::string, Bytes>& images,
                  ImageReport* report)
{
    const std::string kind = ImageContainerKind(current.data(), current.size());
    if (kind != ImageContainerKind(original.data(), original.size()))
        throw FormatError("the file is not the game's kind of file any more");
    if (kind == "iggytex") return ApplyToIggyTextures(original, current, images, report);
    if (kind == "emb") return ApplyToEmb(original, current, images, report);
    throw FormatError("not an iggytex / iggyted / emb");
}

}  // namespace xv2ui

// .iggytex/.iggyted and .emb parse / write (see textures.h).
#include "textures.h"

namespace xv2ui {

constexpr const char IggyTextures::kMagic[8];

void IggyTextures::Parse(const uint8_t* data, size_t size)
{
    Reader r(data, size, "iggytex");
    if (!Looks(data, size)) r.Fail("not an iggytex");
    r.skip(8);
    const uint32_t count = r.u32();
    headerField = r.u32();
    textures.assign(count, IggyTexture());
    for (IggyTexture& t : textures) {
        t.id = r.u32();
        const uint32_t textureSize = r.u32(), offset = r.u32();
        t.reserved = r.u32();
        if (textureSize == 0 && offset == 0) { t.external = true; continue; }
        const size_t start = offset, length = textureSize;
        if (start > size || length > size - start) r.Fail("texture past the end");
        t.dds.assign(data + start, data + start + length);
    }
}

Bytes IggyTextures::Write() const
{
    Writer w;
    w.Raw(kMagic, 8);
    w.u32((uint32_t)textures.size());
    w.u32(headerField);
    size_t at = 16 + 16 * textures.size();
    std::vector<size_t> offsets;
    for (const IggyTexture& t : textures) {
        if (t.external) { offsets.push_back(0); continue; }
        at = (at + kAlignment - 1) / kAlignment * kAlignment;
        offsets.push_back(at);
        at += t.dds.size();
    }
    for (size_t i = 0; i < textures.size(); ++i) {
        const IggyTexture& t = textures[i];
        w.u32(t.id);
        w.u32(t.external ? 0 : (uint32_t)t.dds.size());
        w.u32(t.external ? 0 : (uint32_t)offsets[i]);
        w.u32(t.reserved);
    }
    for (size_t i = 0; i < textures.size(); ++i) {
        if (textures[i].external) continue;
        w.out.resize(offsets[i], 0);
        w.Append(textures[i].dds);
    }
    return w.out;
}

int IggyTextures::Find(uint32_t id) const
{
    for (size_t i = 0; i < textures.size(); ++i)
        if ((textures[i].id & 0xffff) == id && !textures[i].external) return (int)i;
    return -1;
}

void EmbFile::Parse(const uint8_t* data, size_t size)
{
    Reader r(data, size, "emb");
    if (!Looks(data, size)) r.Fail("not an emb");
    header.assign(data, data + kHeaderSize);
    const uint32_t count = GetU32(data + 0x0C), table = GetU32(data + 0x18), names = GetU32(data + 0x1C);
    hasNames = names != 0;
    if (table > size || (size - table) / 8 < count) r.Fail("entry table past the end");
    if (hasNames && (names > size || (size - names) / 4 < count)) r.Fail("name table past the end");
    entries.assign(count, EmbEntry());
    for (uint32_t i = 0; i < count; ++i) {
        const size_t entry = table + 8 * i;
        const size_t start = entry + GetU32(data + entry), length = GetU32(data + entry + 4);
        if (start > size || length > size - start) r.Fail("file past the end");
        entries[i].data.assign(data + start, data + start + length);
        if (hasNames) {
            const size_t nameAt = GetU32(data + names + 4 * i);
            if (nameAt >= size) r.Fail("name past the end");
            const uint8_t* end = (const uint8_t*)memchr(data + nameAt, 0, size - nameAt);
            if (!end) r.Fail("name without an end");
            entries[i].name.assign((const char*)data + nameAt, (const char*)end);
        }
    }
}

Bytes EmbFile::Write() const
{
    // eterniti's layout: header, entry table, name offsets, files at 64,
    // names (64-aligned start) at the end.
    Writer w;
    w.Append(header.size() == kHeaderSize ? header : Bytes(kHeaderSize, 0));
    PutU32(&w.out[0], kSignature);
    w.PutU32At(0x0C, (uint32_t)entries.size());
    w.PutU32At(0x18, (uint32_t)kHeaderSize);
    const size_t table = w.size();
    w.out.resize(table + 8 * entries.size(), 0);
    size_t names = 0;
    if (hasNames) { names = w.size(); w.out.resize(names + 4 * entries.size(), 0); }
    w.PutU32At(0x1C, (uint32_t)names);
    for (size_t i = 0; i < entries.size(); ++i) {
        w.Align(kAlignment);
        const size_t entry = table + 8 * i;
        w.PutU32At(entry, (uint32_t)(w.size() - entry));
        w.PutU32At(entry + 4, (uint32_t)entries[i].data.size());
        w.Append(entries[i].data);
    }
    if (hasNames) {
        w.Align(kAlignment);
        for (size_t i = 0; i < entries.size(); ++i) {
            w.PutU32At(names + 4 * i, (uint32_t)w.size());
            w.Raw(entries[i].name.c_str(), entries[i].name.size() + 1);
        }
    }
    return w.out;
}

int EmbFile::Find(const std::string& name) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == name) return (int)i;
    return -1;
}

}  // namespace xv2ui

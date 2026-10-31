// xv2ui.exe - offline checks and dumps for XV2UserInterfaceLib's file formats.
//
//   xv2ui roundtrip <file or folder>...
//       every .iggy / .iggytex / .iggyted / .emb / .msg: parse + write must give the
//       same bytes; for .iggy also the ABC (pool..bodies) and every method's
//       code (decode + encode), a regenerated index that decodes to the same
//       fields, and a script swap that reads back.
//   xv2ui dump <file.iggy>
//       subfiles, index statistics, the script's classes.
//   xv2ui code <file.iggy|.abc> <method>
//       a method's instructions, as far as they decode.
//   xv2ui merge <game.abc|.iggy> [-base <mod_base>] <mod.abc|.iggy>... -o <out.abc> [-expect <x.abc>]
//       merges the mods' script changes over the game's script, in order;
//       -base: what the following mods were made from (an FFDec rebuild).
//   xv2ui images <game .iggytex|.iggyted|.emb> <name>=<image>... [-then <name>=<image>...]... [-o out]
//       applies image edits as XV2UserInterfaceLib's image stages do; each
//       -then starts the next mod's set. The result must parse, round-trip
//       and hold every named image.
//   xv2ui messages <game .msg> <name>=<text>... [-then <name>=<text>...]... [-o out]
//       applies text edits as XV2UserInterfaceLib's message stages do (text
//       as UTF-8, plain: ' " & < > become entities); the result must parse,
//       round-trip and hold every named text.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "../src/formats/abc.h"
#include "../src/formats/abc_merge.h"
#include "../src/formats/iggy_file.h"
#include "../src/formats/image_edit.h"
#include "../src/formats/msg_file.h"
#include "../src/formats/textures.h"

namespace fs = std::filesystem;
using namespace xv2ui;

namespace {

Bytes ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool WriteFile(const fs::path& path, const Bytes& data)
{
    std::ofstream out(path, std::ios::binary);
    out.write((const char*)data.data(), (std::streamsize)data.size());
    return (bool)out;
}

// Where two byte strings first differ ("" when they are equal).
std::string Difference(const Bytes& expected, const Bytes& got)
{
    if (expected == got) return "";
    size_t at = 0;
    while (at < expected.size() && at < got.size() && expected[at] == got[at]) ++at;
    char text[96];
    snprintf(text, sizeof(text), "differs at 0x%zx (sizes 0x%zx / 0x%zx)", at, expected.size(), got.size());
    return text;
}

struct Totals { size_t files = 0, failed = 0, bodies = 0, scripts = 0; };

// Checks one ABC: write back, and every body's code through Decode/Encode.
std::string CheckAbc(const uint8_t* data, size_t size, Totals& totals)
{
    abc::AbcFile file;
    file.Parse(data, size);
    std::string diff = Difference(Bytes(data, data + size), file.Write());
    if (!diff.empty()) return "abc " + diff;
    for (const abc::MethodBody& body : file.bodies) {
        std::vector<abc::Instruction> code;
        try {
            code = abc::Decode(body.code);
        } catch (const FormatError& error) {
            return "method " + std::to_string(body.method) + ": " + error.what();
        }
        diff = Difference(body.code, abc::Encode(code, body.code.size(), nullptr));
        if (!diff.empty()) return "method " + std::to_string(body.method) + " code " + diff;
        ++totals.bodies;
    }
    ++totals.scripts;
    return "";
}

std::string CheckIggy(const Bytes& bytes, Totals& totals)
{
    IggyFile container;
    container.Parse(bytes.data(), bytes.size());
    std::string diff = Difference(bytes, container.Write());
    if (!diff.empty()) return "container " + diff;

    IggyDocument doc;
    doc.Parse(bytes.data(), bytes.size());
    diff = Difference(bytes, doc.Write());
    if (!diff.empty()) return "document " + diff;

    // a regenerated index must name the same fields
    {
        const std::vector<Bytes> index = doc.movie.BuildIndex();
        std::vector<const Bytes*> chunks;
        for (const Bytes& chunk : index) chunks.push_back(&chunk);
        IggyMovie again;
        again.Load(doc.movie.data, chunks);
        if (again.fields != doc.movie.fields) return "regenerated index names other fields";
    }

    const uint8_t* abcData = nullptr;
    uint32_t abcSize = 0;
    if (!doc.movie.Script(abcData, abcSize)) return "";
    diff = CheckAbc(abcData, abcSize, totals);
    if (!diff.empty()) return diff;

    // swap the script for itself and a grown copy: both must read back
    for (int grow = 0; grow < 2; ++grow) {
        Bytes abcCopy(abcData, abcData + abcSize);
        if (grow) abcCopy.insert(abcCopy.end(), 1000, 0);   // not a valid ABC - only the container is checked
        IggyDocument edited;
        edited.Parse(bytes.data(), bytes.size());
        edited.movie.SetScript(abcCopy.data(), (uint32_t)abcCopy.size());
        const Bytes written = edited.Write();
        IggyDocument reread;
        reread.Parse(written.data(), written.size());
        const uint8_t* back = nullptr;
        uint32_t backSize = 0;
        if (!reread.movie.Script(back, backSize) || Bytes(back, back + backSize) != abcCopy)
            return grow ? "grown script swap doesn't read back" : "script swap doesn't read back";
        if (reread.movie.fields != edited.movie.fields) return "script swap: fields differ after rereading";
        // the second swap over our own block must replace it, not stack another
        edited.movie.SetScript(abcCopy.data(), (uint32_t)abcCopy.size());
        if (edited.Write().size() != written.size()) return "second script swap grew the file";
    }
    return "";
}

std::string CheckFile(const fs::path& path, Totals& totals)
{
    const Bytes bytes = ReadFile(path);
    if (IggyFile::Looks(bytes.data(), bytes.size())) return CheckIggy(bytes, totals);
    if (IggyTextures::Looks(bytes.data(), bytes.size())) {
        IggyTextures textures;
        textures.Parse(bytes.data(), bytes.size());
        return Difference(bytes, textures.Write());
    }
    if (EmbFile::Looks(bytes.data(), bytes.size())) {
        EmbFile emb;
        emb.Parse(bytes.data(), bytes.size());
        return Difference(bytes, emb.Write());
    }
    if (MsgFile::Looks(bytes.data(), bytes.size())) {
        MsgFile msg;
        msg.Parse(bytes.data(), bytes.size());
        return Difference(bytes, msg.Write());
    }
    return "-";   // not a UI file
}

int Roundtrip(const std::vector<std::string>& paths)
{
    Totals totals;
    auto one = [&](const fs::path& path) {
        std::string result;
        try {
            result = CheckFile(path, totals);
        } catch (const std::exception& error) {
            result = std::string("error: ") + error.what();
        }
        if (result == "-") return;
        ++totals.files;
        if (!result.empty()) {
            ++totals.failed;
            printf("FAIL %s: %s\n", path.filename().string().c_str(), result.c_str());
        }
    };
    for (const std::string& path : paths) {
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::recursive_directory_iterator(path))
                if (entry.is_regular_file()) one(entry.path());
        } else {
            one(path);
        }
    }
    printf("%zu files, %zu failed; %zu scripts, %zu method bodies re-encoded\n", totals.files, totals.failed,
           totals.scripts, totals.bodies);
    return totals.failed ? 1 : 0;
}

int Dump(const std::string& path)
{
    const Bytes bytes = ReadFile(path);
    IggyDocument doc;
    IggyIndexStats stats;
    doc.Parse(bytes.data(), bytes.size(), &stats);
    printf("%zu subfiles:", doc.file.subfiles.size());
    for (const IggySubfile& sub : doc.file.subfiles) printf(" {id %u, 0x%zx}", sub.id, sub.data.size());
    printf("\nindex: %zu chunks, %zu pointers (%zu null), %zu other fields\nopcodes:", stats.chunks, stats.pointers,
           stats.nullPointers, stats.otherFields);
    for (const auto& op : stats.opcodes) printf(" %02x:%zu", op.first, op.second);
    printf("\n");
    const uint8_t* abcData = nullptr;
    uint32_t abcSize = 0;
    if (!doc.movie.Script(abcData, abcSize)) { printf("no script\n"); return 0; }
    abc::AbcFile file;
    file.Parse(abcData, abcSize);
    printf("script: 0x%x bytes, fnv64 %016llx, %zu strings, %zu multinames, %zu methods, %zu classes, %zu scripts\n",
           abcSize, (unsigned long long)Fnv64(abcData, abcSize), file.pool.strings.size(), file.pool.multinames.size(),
           file.methods.size(), file.classes.size(), file.scripts.size());
    for (size_t i = 0; i < file.classes.size(); ++i)
        printf("  class %s : %s (%zu + %zu traits)\n", file.ClassName(i).c_str(),
               file.MultinameText(file.instances[i].superName).c_str(), file.instances[i].traits.size(),
               file.classes[i].traits.size());
    return 0;
}

Bytes LoadAbc(const std::string& path);

// Lists a method's code byte by byte as far as it decodes (for finding why one doesn't).
int Code(const std::string& path, uint32_t method)
{
    const Bytes abcBytes = LoadAbc(path);
    abc::AbcFile file;
    file.Parse(abcBytes.data(), abcBytes.size());
    const int body = file.BodyOf(method);
    if (body < 0) { printf("method %u has no body\n", method); return 1; }
    const Bytes& code = file.bodies[body].code;
    printf("method %u: %zu bytes, %zu exceptions\n", method, code.size(), file.bodies[body].exceptions.size());
    for (const abc::ExceptionInfo& e : file.bodies[body].exceptions) printf("  try %x-%x -> %x\n", e.from, e.to, e.target);
    Reader r(code.data(), code.size(), "code");
    while (!r.atEnd()) {
        const size_t start = r.position();
        const uint8_t op = r.u8();
        const abc::OpcodeInfo* info = abc::Opcode(op);
        if (!info) { printf("  %04zx: unknown %02x\n", start, op); break; }
        printf("  %04zx: %-16s", start, info->name);
        for (abc::OperandKind kind : {info->first, info->second}) {
            switch (kind) {
            case abc::kOpNone: break;
            case abc::kOpU8: printf(" %u", r.u8()); break;
            case abc::kOpBranch: { const int32_t delta = r.s24(); printf(" -> %zx", r.position() + delta); break; }
            case abc::kOpSwitch: {
                printf(" default %zx", start + r.s24());
                const uint32_t count = r.u30();
                for (uint32_t i = 0; i <= count; ++i) printf(" %zx", start + r.s24());
                break;
            }
            case abc::kOpDebug: printf(" %u", r.u8()); printf(" %u", r.u30()); printf(" %u", r.u8()); printf(" %u", r.u30()); break;
            default: printf(" %u", r.u30()); break;
            }
        }
        printf("\n");
    }
    return 0;
}

// An ABC from .abc bytes or an .iggy's script.
Bytes AbcFromBytes(const Bytes& bytes, const std::string& name = "the menu")
{
    if (!IggyFile::Looks(bytes.data(), bytes.size())) return bytes;
    IggyDocument doc;
    doc.Parse(bytes.data(), bytes.size());
    const uint8_t* abcData = nullptr;
    uint32_t abcSize = 0;
    if (!doc.movie.Script(abcData, abcSize)) throw FormatError(name + ": no script");
    return Bytes(abcData, abcData + abcSize);
}

// An ABC from a .abc file or an .iggy's script.
Bytes LoadAbc(const std::string& path) { return AbcFromBytes(ReadFile(path), path); }

// Lists each class's instance and static traits with their full names and
// namespace kinds (for seeing why two builds' traits don't match up).
int Traits(const std::string& path, const std::string& onlyClass)
{
    const Bytes abcBytes = LoadAbc(path);
    abc::AbcFile file;
    file.Parse(abcBytes.data(), abcBytes.size());
    auto list = [&](const char* label, const std::vector<abc::Trait>& traits) {
        for (const abc::Trait& trait : traits) {
            const abc::Multiname& name = file.pool.multinames[trait.name];
            const unsigned nsKind = name.ns < file.pool.namespaces.size() ? file.pool.namespaces[name.ns].kind : 0;
            printf("    %s kind %u ns-kind 0x%02x %s (index %u, slot/disp id %u)\n", label, trait.kind(), nsKind,
                   file.MultinameText(trait.name).c_str(), trait.index, trait.slotId);
        }
    };
    for (size_t i = 0; i < file.classes.size(); ++i) {
        const std::string name = file.ClassName(i);
        if (!onlyClass.empty() && name.find(onlyClass) == std::string::npos) continue;
        printf("class %s (constructor method %u, static init method %u)\n", name.c_str(), file.instances[i].iinit, file.classes[i].cinit);
        list("instance", file.instances[i].traits);
        list("static  ", file.classes[i].traits);
    }
    return 0;
}

int Merge(const std::vector<std::string>& args)
{
    // each mod with the -base given before it (empty: the game's script)
    std::vector<std::pair<std::string, std::string>> mods;
    std::string game, modBase, output, expect;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-o" && i + 1 < args.size()) output = args[++i];
        else if (args[i] == "-expect" && i + 1 < args.size()) expect = args[++i];
        else if (args[i] == "-base" && i + 1 < args.size()) modBase = args[++i];
        else if (game.empty()) game = args[i];
        else mods.emplace_back(args[i], modBase);
    }
    if (mods.empty()) { printf("merge needs the game's script and at least one mod\n"); return 2; }
    const Bytes gameAbc = LoadAbc(game);
    Bytes current = gameAbc;
    for (const auto& entry : mods) {
        const Bytes mod = LoadAbc(entry.first);
        Bytes baseAbc;
        if (!entry.second.empty()) baseAbc = LoadAbc(entry.second);
        abc::MergeReport report;
        current = abc::MergeScript(gameAbc, current, mod, &report, entry.second.empty() ? nullptr : &baseAbc);
        printf("%s: %s\n", fs::path(entry.first).filename().string().c_str(), report.Summary().c_str());
        for (const std::string& line : report.notes) printf("    %s\n", line.c_str());
    }
    // the result must itself parse and decode
    Totals totals;
    const std::string check = CheckAbc(current.data(), current.size(), totals);
    if (!check.empty()) { printf("merged script doesn't round-trip: %s\n", check.c_str()); return 1; }
    printf("merged: 0x%zx bytes, fnv64 %016llx\n", current.size(), (unsigned long long)Fnv64(current.data(), current.size()));
    // -o x.iggy over a game .iggy: the menu with the merged script, as
    // XV2UserInterfaceLib's stage writes it (ui_lib.cpp MergeStage)
    Bytes written = current;
    if (fs::path(output).extension() == ".iggy") {
        const Bytes gameIggy = ReadFile(game);
        if (!IggyFile::Looks(gameIggy.data(), gameIggy.size())) { printf("-o .iggy needs a game .iggy\n"); return 2; }
        IggyDocument menu;
        menu.Parse(gameIggy.data(), gameIggy.size());
        menu.movie.SetScript(current.data(), (uint32_t)current.size());
        written = menu.Write();
        if (AbcFromBytes(written) != current) { printf("the written menu's script doesn't read back\n"); return 1; }
    }
    if (!output.empty() && !WriteFile(output, written)) { printf("can't write %s\n", output.c_str()); return 1; }
    if (!expect.empty()) {
        const Bytes expected = LoadAbc(expect);
        std::string compare;
        // an FFDec-made expected script: unchanged parts are in FFDec's form there
        Bytes ffdecBase;
        if (!mods.back().second.empty()) ffdecBase = LoadAbc(mods.back().second);
        abc::CompareScripts(expected, current, compare, &gameAbc, ffdecBase.empty() ? nullptr : &ffdecBase);
        printf("against %s:\n%s", fs::path(expect).filename().string().c_str(), compare.empty() ? "    same semantics\n" : compare.c_str());
    }
    return 0;
}

// Applies each mod's images in turn (ui_lib.cpp ImageStage) and checks the result.
int Images(const std::vector<std::string>& args)
{
    std::string gamePath, output;
    std::vector<std::map<std::string, Bytes>> mods(1);
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-o" && i + 1 < args.size()) { output = args[++i]; continue; }
        if (args[i] == "-then") { mods.emplace_back(); continue; }
        if (gamePath.empty()) { gamePath = args[i]; continue; }
        const size_t equals = args[i].find('=');
        if (equals == std::string::npos) { printf("expected <name>=<image>, got %s\n", args[i].c_str()); return 2; }
        const std::string name = args[i].substr(0, equals);
        const Bytes image = ReadFile(args[i].substr(equals + 1));
        const std::string problem = CheckImage(gamePath, name, image);
        if (!problem.empty()) { printf("%s: %s\n", name.c_str(), problem.c_str()); return 1; }
        mods.back()[name] = image;
    }
    const Bytes game = ReadFile(gamePath);
    Bytes current = game;
    for (size_t i = 0; i < mods.size(); ++i) {
        ImageReport report;
        current = ApplyImages(game, current, mods[i], &report);
        printf("mod %zu: %s\n", i + 1, report.Summary().c_str());
        for (const std::string& line : report.notes) printf("    %s\n", line.c_str());
    }
    // reads back: round-trips, and the last mod to name an image has it
    std::map<std::string, Bytes> expected;
    for (const auto& mod : mods)
        for (const auto& image : mod) expected[image.first] = image.second;
    if (IggyTextures::Looks(current.data(), current.size())) {
        IggyTextures file;
        file.Parse(current.data(), current.size());
        if (file.Write() != current) { printf("result doesn't round-trip\n"); return 1; }
        for (const auto& image : expected) {
            const int at = file.Find((uint32_t)std::stoul(image.first));
            if (at < 0 || file.textures[at].dds != image.second) { printf("texture %s doesn't read back\n", image.first.c_str()); return 1; }
        }
        printf("result: %zu textures, 0x%zx bytes\n", file.textures.size(), current.size());
    } else {
        EmbFile file;
        file.Parse(current.data(), current.size());
        if (file.Write() != current) { printf("result doesn't round-trip\n"); return 1; }
        for (const auto& image : expected) {
            const int at = image.first[0] == '#' ? std::stoi(image.first.substr(1)) : file.Find(image.first);
            if (at < 0 || at >= (int)file.entries.size() || file.entries[at].data != image.second) {
                printf("entry %s doesn't read back\n", image.first.c_str());
                return 1;
            }
        }
        printf("result: %zu entries, 0x%zx bytes\n", file.entries.size(), current.size());
    }
    if (!output.empty() && !WriteFile(output, current)) { printf("can't write %s\n", output.c_str()); return 1; }
    return 0;
}

// Applies each mod's texts in turn (ui_lib.cpp MessageStage) and checks the result.
int Messages(const std::vector<std::string>& args)
{
    std::string gamePath, output;
    std::vector<std::map<std::string, std::u16string>> mods(1);
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-o" && i + 1 < args.size()) { output = args[++i]; continue; }
        if (args[i] == "-then") { mods.emplace_back(); continue; }
        if (gamePath.empty()) { gamePath = args[i]; continue; }
        const size_t equals = args[i].find('=');
        if (equals == std::string::npos) { printf("expected <name>=<text>, got %s\n", args[i].c_str()); return 2; }
        const std::string name = args[i].substr(0, equals);
        const std::string problem = CheckMessage(gamePath, name);
        if (!problem.empty()) { printf("%s: %s\n", name.c_str(), problem.c_str()); return 1; }
        mods.back()[name] = MsgText(args[i].substr(equals + 1));
    }
    const Bytes game = ReadFile(gamePath);
    Bytes current = game;
    for (size_t i = 0; i < mods.size(); ++i) {
        MessageReport report;
        current = ApplyMessages(game, current, mods[i], &report);
        printf("mod %zu: %s\n", i + 1, report.Summary().c_str());
        for (const std::string& line : report.notes) printf("    %s\n", line.c_str());
    }
    // reads back: round-trips, and the last mod to name an entry has its text
    std::map<std::string, std::u16string> expected;
    for (const auto& mod : mods)
        for (const auto& message : mod) expected[message.first] = message.second;
    MsgFile file;
    file.Parse(current.data(), current.size());
    if (file.Write() != current) { printf("result doesn't round-trip\n"); return 1; }
    for (const auto& message : expected) {
        const int at = file.Find(MsgName(message.first));
        if (at < 0 || file.entries[at].lines.empty() || file.entries[at].lines[0].text != message.second) {
            printf("message %s doesn't read back\n", message.first.c_str());
            return 1;
        }
    }
    printf("result: %zu entries, 0x%zx bytes\n", file.entries.size(), current.size());
    if (!output.empty() && !WriteFile(output, current)) { printf("can't write %s\n", output.c_str()); return 1; }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("xv2ui roundtrip <file or folder>...\nxv2ui dump <file.iggy>\n"
               "xv2ui merge <game> [-base mod_base] <mod>... [-o out.abc|out.iggy] [-expect x.abc]\n"
               "xv2ui code <iggy|abc> <method>\nxv2ui traits <iggy|abc> [class]\n"
               "xv2ui images <game texture file> <name>=<image>... [-then ...] [-o out]\n"
               "xv2ui messages <game .msg> <name>=<text>... [-then ...] [-o out]\n");
        return 2;
    }
    const std::string command = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);
    try {
        if (command == "roundtrip") return Roundtrip(args);
        if (command == "dump") return Dump(args[0]);
        if (command == "merge") return Merge(args);
        if (command == "images") return Images(args);
        if (command == "messages") return Messages(args);
        if (command == "code" && args.size() == 2) return Code(args[0], (uint32_t)std::stoul(args[1]));
        if (command == "traits") return Traits(args[0], args.size() > 1 ? args[1] : "");
    } catch (const std::exception& error) {
        printf("error: %s\n", error.what());
        return 1;
    }
    printf("unknown command %s\n", command.c_str());
    return 2;
}

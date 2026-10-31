// Offline check of the replay NPC's file edits: runs EditNpcFile on a file
// from disk, with the built-in defaults.
//   npctest <game path, e.g. data/msg/lobby_text_en.msg> <in> <out>
//   npctest --title-dds <CMNTITLE_<lang>.iggytex> <out.dds>
//     the blanked texture 14 the mod hands the UI library (check.py titles)
//   npctest --messages <game path of a lobby_text / menu_title_text msg> <out.txt>
//     the entries the mod hands the UI library (check.py messages)
// ClaudeTools\gamefiles\verify_dll_files.py checks the results.
#include "../src/replay_npc.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc != 4) {
        printf("usage: npctest <game path> <in> <out> | --title-dds <iggytex> <out.dds> | --messages <msg path> <out.txt>\n");
        return 2;
    }
    std::ifstream in(argv[2], std::ios::binary);
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (std::string(argv[1]) == "--title-dds") {
        // As mod.cpp RegisterTitleArtImages: the texture table from the
        // file's head, then only texture 14.
        ReplayNpc npc;
        std::string note;
        uint32_t offset = 0, size = 0;
        std::vector<uint8_t> blanked;
        bool ok = FindMenuTitleTexture(src.data(), (std::min)(src.size(), kMenuTitleHeadBytes), src.size(), offset,
                                       size, note) &&
                  BlankMenuTitleArt(npc, src.data() + offset, size, blanked, note);
        printf("%s: %s (%s)\n", argv[2], ok ? "blanked" : "unchanged", note.c_str());
        if (ok) std::ofstream(argv[3], std::ios::binary).write((const char*)blanked.data(), (std::streamsize)blanked.size());
        return ok ? 0 : 1;
    }
    if (std::string(argv[1]) == "--messages") {
        // As mod.cpp RegisterNpcMessages: the entries handed to SetMessage,
        // one "name<TAB>text" line each (UTF-8).
        std::string path = argv[2];
        for (char& c : path) c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
        std::ofstream listing(argv[3], std::ios::binary);
        std::vector<NpcMessage> messages = NpcMessages(ClassifyNpcFile(path), ReplayNpc());
        for (const NpcMessage& message : messages) listing << message.key << '\t' << message.text << '\n';
        printf("%s: %zu messages\n", argv[2], messages.size());
        return messages.empty() ? 1 : 0;
    }
    std::string path = argv[1];  // as the mod sees it: lower case, '/'
    for (char& c : path) {
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    ReplayNpc npc;
    std::vector<uint8_t> out;
    std::string note;
    bool edited = EditNpcFile(ClassifyNpcFile(path), npc, src.data(), src.size(), out, note);
    printf("%s: %s (%s)\n", argv[1], edited ? "edited" : "unchanged", note.c_str());
    if (edited) {
        std::ofstream o(argv[3], std::ios::binary);
        o.write((const char*)out.data(), (std::streamsize)out.size());
    }
    return edited ? 0 : 1;
}

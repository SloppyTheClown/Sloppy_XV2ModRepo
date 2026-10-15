// Offline check of the replay NPC's file edits: runs EditNpcFile on a file
// from disk, with the built-in defaults.
//   npctest <game path, e.g. data/msg/lobby_text_en.msg> <in> <out>
// ClaudeTools\gamefiles\verify_dll_files.py checks the results.
#include "../src/replay_npc.h"
#include <cstdio>
#include <fstream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc != 4) { printf("usage: npctest <game path> <in> <out>\n"); return 2; }
    std::ifstream in(argv[2], std::ios::binary);
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
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

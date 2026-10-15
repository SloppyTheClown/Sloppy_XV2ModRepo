// Offline check of the mentor, desk and transfer robot patches:
//   tsrtest <in.tsd> <out.tsd>
// Patches the lobby event table like the mod does (every setting on), prints
// the number of conditions changed and writes the result for tests\check.py
// to compare.
#include "../src/lobby_npcs.h"
#include <cstdio>
#include <fstream>
#include <iterator>

static std::vector<uint8_t> Read(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

int main(int argc, char** argv)
{
    if (argc != 3) { printf("usage: tsrtest <in.tsd> <out.tsd>\n"); return 2; }
    std::vector<uint8_t> out;
    std::vector<uint8_t> src = Read(argv[1]);
    int n = TsdStoryAlways(src.data(), src.size(), kStoryNpcs, out);
    printf("event table, mentor and desk triggers changed: %d\n", n);
    if (n < 1) return 1;
    src.swap(out);
    n = TsdReplaceForNpcs(src.data(), src.size(), kPqTutorialNpcs, kPqTutorialDone, kAlwaysPqTutorial, out);
    printf("event table, parallel quest tutorial requirements changed: %d\n", n);
    if (n != 2) return 1;
    src.swap(out);
    n = TsdReplaceForNpcs(src.data(), src.size(), kStoryLockedNpcs, kPrologueLocked, kNeverPrologue, out);
    printf("event table, radio triggers changed: %d\n", n);
    if (n != 1) return 1;
    src.swap(out);
    n = TsdReplaceForNpcs(src.data(), src.size(), kTeleportLockedNpcs, kPrologueLocked, kNeverPrologue, out);
    printf("event table, transfer robot triggers changed: %d\n", n);
    if (n != 16) return 1;
    std::ofstream(argv[2], std::ios::binary).write((const char*)out.data(), out.size());
    return 0;
}

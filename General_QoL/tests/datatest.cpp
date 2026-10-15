// Offline check of the quest and item edits:
//   datatest qxd|tmq|idb|est <in> <out>   (tmq = tmq_data.qxd: requirements + unlock kind)
// Patches the file like the mod does (every setting on), prints the number of values changed
// and writes the result (even if unchanged) for tests\check.py to compare.
#include "../src/data.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc != 4) { printf("usage: datatest qxd|tmq|idb|est <in> <out>\n"); return 2; }
    std::ifstream in(argv[2], std::ios::binary);
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)), {});
    std::vector<uint8_t> out;
    bool parallelQuests = strcmp(argv[1], "tmq") == 0;
    int n = strcmp(argv[1], "qxd") == 0 || parallelQuests
                ? QxdClearRequirements(src.data(), src.size(), true, true, true, out)
            : strcmp(argv[1], "est") == 0 ? EstAllSeasons(src.data(), src.size(), out)
                                          : IdbClearStoryPoint(src.data(), src.size(), out);
    if (parallelQuests && n >= 0) {
        int unlocked = QxdUnlockFromStart(out);
        n = unlocked < 0 ? unlocked : n + unlocked;
    }
    printf("%d\n", n);
    if (n < 0) return 1;
    std::ofstream(argv[3], std::ios::binary).write((const char*)out.data(), out.size());
    return 0;
}

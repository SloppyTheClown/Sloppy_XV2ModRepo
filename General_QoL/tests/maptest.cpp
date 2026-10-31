// Offline check of the menu script edits: runs PatchLobbyHud (mentor names)
// or PatchAvatarFaculty (free attribute points) on the .iggy from disk, and
// AppendNameFields on fixed names.
//   maptest map|faculty <in .iggy> <out.iggy>
// tests\check.py map compares the results with ClaudeTools\iggy\lobyhud_abc.py's.
#include "../src/iggy_abc.h"
#include "../src/map_names.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>

static bool Expect(const wchar_t* name, size_t maxChars, const char* want)
{
    std::string line = "Event:id=3,x=1,y=2";
    bool appended = AppendNameFields(name, maxChars, line);
    std::string expected = std::string("Event:id=3,x=1,y=2") + want;
    if (line == expected && appended == (*want != 0)) return true;
    printf("AppendNameFields: got \"%s\", want \"%s\"\n", line.c_str(), expected.c_str());
    return false;
}

int main(int argc, char** argv)
{
    if (argc != 4 || (strcmp(argv[1], "map") && strcmp(argv[1], "faculty"))) {
        printf("usage: maptest map|faculty <in .iggy> <out.iggy>\n");
        return 2;
    }
    auto patch = strcmp(argv[1], "map") ? PatchAvatarFaculty : PatchLobbyHud;
    bool ok = Expect(L"Krillin", 64, ",c0=75,c1=114,c2=105,c3=108,c4=108,c5=105,c6=110") &&
              Expect(L"\x30af\x30ea", 64, ",c0=12463,c1=12522") &&  // Japanese "Kuri"
              Expect(L"", 64, "") &&
              Expect(L"bad\x01", 64, "") &&
              Expect(L"Toolong", 4, "");  // no terminator within the limit

    std::ifstream in(argv[2], std::ios::binary);
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> out;
    std::string note;
    bool edited = patch(src.data(), src.size(), out, note);
    printf("%s: %s (%s)\n", argv[2], edited ? "edited" : "unchanged", note.c_str());
    if (edited) {
        std::ofstream o(argv[3], std::ios::binary);
        o.write((const char*)out.data(), (std::streamsize)out.size());
    }
    // The edited file must not be edited again (its script is no longer vanilla).
    std::vector<uint8_t> twice;
    if (edited && patch(out.data(), out.size(), twice, note)) {
        printf("an edited file was edited again\n");
        ok = false;
    }
    // The shared attribute script as Combined Supers serves it (flags "0a",
    // as3\AvatarStatus.as FeatureFlags): only the free points flag is set.
    if (edited && patch == PatchAvatarFaculty) {
        const char marker[] = "XV2FACULTY:";
        auto found = std::search(out.begin(), out.end(), marker, marker + sizeof(marker) - 1);
        if (found == out.end()) {
            printf("no feature flags in the attribute script\n");
            ok = false;
        } else {
            const size_t flags = (size_t)(found - out.begin()) + sizeof(marker) - 1;
            std::vector<uint8_t> supersOnly = out, both;
            supersOnly[flags] = '0';
            supersOnly[flags + 1] = 'a';
            bool flagged = patch(supersOnly.data(), supersOnly.size(), both, note);
            printf("over Combined Supers' copy: %s (%s)\n", flagged ? "edited" : "unchanged", note.c_str());
            std::vector<uint8_t> expected = supersOnly;
            expected[flags] = '1';
            if (!flagged || both != expected) {
                printf("free points flag not set in Combined Supers' copy\n");
                ok = false;
            }
        }
    }
    return ok && edited ? 0 : 1;
}

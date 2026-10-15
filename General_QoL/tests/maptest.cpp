// Offline check of the lobby map edit (mentor names): runs PatchLobbyHud on
// LOBYHUD.iggy from disk and AppendNameFields on fixed names.
//   maptest <in LOBYHUD.iggy> <out.iggy>
// tests\check.py map compares the result with ClaudeTools\iggy\lobyhud_abc.py's.
#include "../src/map_names.h"
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
    if (argc != 3) { printf("usage: maptest <in LOBYHUD.iggy> <out.iggy>\n"); return 2; }
    bool ok = Expect(L"Krillin", 64, ",c0=75,c1=114,c2=105,c3=108,c4=108,c5=105,c6=110") &&
              Expect(L"\x30af\x30ea", 64, ",c0=12463,c1=12522") &&  // Japanese "Kuri"
              Expect(L"", 64, "") &&
              Expect(L"bad\x01", 64, "") &&
              Expect(L"Toolong", 4, "");  // no terminator within the limit

    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> out;
    std::string note;
    bool edited = PatchLobbyHud(src.data(), src.size(), out, note);
    printf("LOBYHUD.iggy: %s (%s)\n", edited ? "edited" : "unchanged", note.c_str());
    if (edited) {
        std::ofstream o(argv[2], std::ios::binary);
        o.write((const char*)out.data(), (std::streamsize)out.size());
    }
    // The edited file must not be edited again (its map script is no longer vanilla).
    std::vector<uint8_t> twice;
    if (edited && PatchLobbyHud(out.data(), out.size(), twice, note)) {
        printf("an edited file was edited again\n");
        ok = false;
    }
    return ok && edited ? 0 : 1;
}

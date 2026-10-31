// ---------------------------------------------------------------------------
//  Mentor names on the lobby map - LOBYHUD.iggy edit and name encoding.
//  The script swap itself is iggy_abc.cpp's PatchIggyAbc.
// ---------------------------------------------------------------------------
#include "map_names.h"
#include "iggy_abc.h"
#include "lobyhud_abc.h"

bool PatchLobbyHud(const uint8_t* data, size_t size, std::vector<uint8_t>& out, std::string& note)
{
    if (!PatchIggyAbc(data, size, kVanillaLobyhudAbcSize, kVanillaLobyhudAbcHash, kLobyhudAbc, sizeof(kLobyhudAbc),
                      out, note))
        return false;
    note = "map script with mentor names (" + std::to_string(out.size()) + " bytes)";
    return true;
}

bool AppendNameFields(const wchar_t* name, size_t maxChars, std::string& line)
{
    size_t length = 0;
    while (length < maxChars && name[length]) {
        if (name[length] < 0x20) return false;
        ++length;
    }
    if (length == 0 || length == maxChars) return false;  // empty, or no terminator
    for (size_t i = 0; i < length; ++i)
        line += ",c" + std::to_string(i) + "=" + std::to_string((unsigned)name[i]);
    return true;
}

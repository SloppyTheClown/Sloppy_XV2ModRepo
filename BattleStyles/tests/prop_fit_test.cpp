// Offline check of prop_fit: builds a style's prop files for a CaC race from
// the game's own files (loose data, then the cpks), as the mod does; read
// through the loader library, so build XV2ModLoader first.
//
//   prop_fit_test.exe <game folder> <output folder> STYLE:RACE[:EAN] ...
//
// EAN is the code of the moveset's body EAN when it isn't the style's own
// (CMS field 1; stylenames.py's "files" column: XAC:HUM:G18).
// Writes every file under <output> at its data-relative path (the fitted BCS
// as battle_styles/fit/RACE.STYLE.bcs) and prints each fit's note, so the
// results can be read back with the XML serializer.
#include "../src/game_files.h"
#include "../src/prop_fit.h"
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

static bool Write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0) return false;
    bool ok = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    fclose(file);
    return ok;
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        printf("usage: prop_fit_test <game folder> <output folder> STYLE:RACE ...\n");
        return 2;
    }
    std::string gameNarrow = argv[1];
    std::wstring game(gameNarrow.begin(), gameNarrow.end());
    if (!UseGameFilesLibrary(XV2_LOADER_LIBRARY, game)) {
        printf("loader library not found or too old: %ls (build XV2ModLoader first)\n", XV2_LOADER_LIBRARY);
        return 2;
    }
    std::filesystem::path outDir = argv[2];
    ReadGameData read = [&](const std::string& path, std::vector<uint8_t>& bytes) {
        std::string from;
        return ReadGameFile(path, bytes, from);
    };
    int failures = 0;
    for (int i = 3; i < argc; ++i) {
        std::string pair = argv[i];
        size_t colon = pair.find(':');
        if (colon == std::string::npos) { ++failures; continue; }
        std::string style = pair.substr(0, colon), race = pair.substr(colon + 1), ean = style;
        const size_t eanColon = race.find(':');
        if (eanColon != std::string::npos) {
            ean = race.substr(eanColon + 1);
            race = race.substr(0, eanColon);
        }
        std::string raceBcsPath = "chara/" + race + "/" + race + ".bcs";
        std::vector<uint8_t> raceBcs;
        std::vector<PropFile> files;
        std::string note;
        bool made = read(raceBcsPath, raceBcs) &&
                    FitPropsToRace(read, raceBcs, raceBcsPath, "chara/" + style + "/" + style + ".bcs",
                                   "chara/" + ean + "/" + ean + ".ean",
                                   "battle_styles/fit/" + race + "." + style + ".bcs", files, note);
        printf("%s -> %s: %s%s\n", style.c_str(), race.c_str(), made ? "" : "FAILED - ", note.c_str());
        failures += !made;
        for (const PropFile& file : files) {
            if (!Write(outDir / file.path, file.bytes)) {
                printf("  can't write %s\n", file.path.c_str());
                ++failures;
            } else {
                printf("  %s (%zu bytes)\n", file.path.c_str(), file.bytes.size());
            }
        }
    }
    return failures ? 1 : 0;
}

// Offline check of ean_fit: fits roster movesets to CaC races from the game's
// own files (loose data, then the cpks; read through the loader library, so
// build XV2ModLoader first) and writes the results for
// ClaudeTools\battle_styles\eantracks.py to inspect.
//
//   ean_fit_test.exe <game folder> <output folder> STYLE:RACE[:STYLE_SE:RACE_SE] ...
//
// Writes <output>\STYLE.RACE.ean, STYLE.cam.RACE.ean, STYLE_PLAYER.RACE.bdm and
// STYLE_PLAYER.RACE.bac, and prints each fit's note. With the two character
// SE banks (e.g. ZMS:NMC:CAR_BTL_ZMS_SE:CAR_BTL_NMC_SE) it also merges them
// (acb_merge.h) into <output>\STYLE_SE.RACE.acb and points the BAC's
// character sounds at the merged cues, as the mod does.
#include "../src/acb_merge.h"
#include "../src/game_files.h"
#include "../src/ean_fit.h"
#include <cstdio>
#include <string>
#include <vector>

static bool Write(const std::string& path, const std::vector<uint8_t>& bytes)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "wb") != 0) return false;
    bool ok = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    fclose(file);
    return ok;
}

// Fits one style file to the race's file of the same kind; prints the note.
static bool FitOne(const std::string& stylePath, const std::string& racePath,
                   const std::string& outPath, bool body, EanFit& fit)
{
    std::vector<uint8_t> style, race, out;
    std::string from, note;
    if (!ReadGameFile(stylePath, style, from) || !ReadGameFile(racePath, race, from)) {
        printf("  %s: not found\n", stylePath.c_str());
        return false;
    }
    // back props placed by the race's Poop Stick as the mod does (prop_fit.h
    // PropPlacement); the race BCS is chara/RACE/RACE.bcs next to its EAN
    PropPlacement back;
    if (body) {
        ReadGameData read = [&](const std::string& path, std::vector<uint8_t>& bytes) {
            std::string source;
            return ReadGameFile(path, bytes, source);
        };
        back = MeasurePropPlacement(read, racePath.substr(0, racePath.size() - 4) + ".bcs", stylePath);
        if (back.backKnown) printf("  back props %+.3f deep\n", back.shift);
    }
    bool made = FitEanToRace(style.data(), style.size(), race.data(), race.size(), body,
                             body ? kRaceBaseAnimations : kRaceBaseCameras, out, fit, note, back);
    printf("  %s: %s%s\n", stylePath.c_str(), made ? "" : "FAILED - ", note.c_str());
    return made && Write(outPath, out);
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        printf("usage: ean_fit_test <game folder> <output folder> STYLE:RACE ...\n");
        return 2;
    }
    std::string gameNarrow = argv[1], outDir = argv[2];
    std::wstring game(gameNarrow.begin(), gameNarrow.end());
    if (!UseGameFilesLibrary(XV2_LOADER_LIBRARY, game)) {
        printf("loader library not found or too old: %ls (build XV2ModLoader first)\n", XV2_LOADER_LIBRARY);
        return 2;
    }
    int failures = 0;
    for (int i = 3; i < argc; ++i) {
        std::string pair = argv[i];
        size_t colon = pair.find(':');
        if (colon == std::string::npos) { ++failures; continue; }
        std::string style = pair.substr(0, colon), race = pair.substr(colon + 1), styleSe, raceSe;
        size_t banks = race.find(':');
        if (banks != std::string::npos) {
            std::string names = race.substr(banks + 1);
            race.resize(banks);
            size_t split = names.find(':');
            if (split == std::string::npos) { ++failures; continue; }
            styleSe = names.substr(0, split);
            raceSe = names.substr(split + 1);
        }
        printf("%s -> %s\n", style.c_str(), race.c_str());
        std::string styleDir = "chara/" + style + "/" + style, raceDir = "chara/" + race + "/" + race;
        EanFit body, camera;
        bool bodyMade = FitOne(styleDir + ".ean", raceDir + ".ean", outDir + "/" + style + "." + race + ".ean",
                               true, body);
        bool cameraMade = FitOne(styleDir + ".cam.ean", raceDir + ".cam.ean",
                                 outDir + "/" + style + ".cam." + race + ".ean", false, camera);
        failures += !bodyMade + !cameraMade;
        BacFit bacFit;
        bacFit.bodyShift = bodyMade ? body.shift : 0;
        bacFit.cameraShift = cameraMade ? camera.shift : 0;
        bacFit.characterSe = BacFit::CharacterSe::Drop;
        std::vector<uint8_t> styleBdm, raceBdm, bdmOut, bac, bacOut;
        std::string from, note;
        if (!styleSe.empty()) {
            std::vector<uint8_t> styleAcb, raceAcb, acbOut;
            const std::string folder = "sound/SE/Battle/Chara/";
            bool merged = ReadGameFile(folder + styleSe + ".acb", styleAcb, from) &&
                          ReadGameFile(folder + raceSe + ".acb", raceAcb, from) &&
                          MergeAcb(raceAcb.data(), raceAcb.size(), styleAcb.data(), styleAcb.size(), acbOut,
                                   bacFit.characterSeCues, note);
            printf("  %s + %s: %s%s\n", raceSe.c_str(), styleSe.c_str(), merged ? "" : "FAILED - ", note.c_str());
            failures += !(merged && Write(outDir + "/" + styleSe + "." + race + ".acb", acbOut));
            if (merged) bacFit.characterSe = BacFit::CharacterSe::Remap;
        }
        if (ReadGameFile(styleDir + "_PLAYER.bdm", styleBdm, from) &&
            ReadGameFile(raceDir + "_PLAYER.bdm", raceBdm, from)) {
            bool merged = FitBdmToRace(styleBdm.data(), styleBdm.size(), raceBdm.data(), raceBdm.size(), &bdmOut,
                                       bacFit.bdmIds, note);
            printf("  %s_PLAYER.bdm: %s%s\n", styleDir.c_str(), merged ? "" : "FAILED - ", note.c_str());
            failures += !(merged && Write(outDir + "/" + style + "_PLAYER." + race + ".bdm", bdmOut));
            if (!merged) bacFit.bdmIds.clear();
        } else {
            printf("  BDM not found\n");
        }
        if (!ReadGameFile(styleDir + "_PLAYER.bac", bac, from)) {
            printf("  BAC not found\n");
            ++failures;
            continue;
        }
        std::vector<uint8_t> raceBac;
        if (bodyMade && ReadGameFile(raceDir + "_PLAYER.bac", raceBac, from)) {
            bacFit.raceBac = raceBac.data();
            bacFit.raceBacSize = raceBac.size();
        }
        bool fitted = FitBacToRace(bac.data(), bac.size(), bacFit, bacOut, note);
        printf("  %s_PLAYER.bac: %s%s\n", styleDir.c_str(), fitted ? "" : "FAILED - ", note.c_str());
        failures += !(fitted && Write(outDir + "/" + style + "_PLAYER." + race + ".bac", bacOut));
    }
    return failures ? 1 : 0;
}

// Offline check of the HUD stage choice (hud_stages.cpp) on the installed
// x2m transformations: hudtest <game>\data. Prints each x2m awoken skill's
// stages (full HP, plenty of Ki) and checks New Transformations' Super
// Saiyan All (ID2 1594, AL4R) against its BCM: base names stage 1, stage 9
// (KK X20) names Deactivate at full HP and stage 10 (UI -Sign-) at 60 % or
// less; and that an aura of another skill, a CUS stage count that differs
// or a vanilla skill leaves the game's choice.
#include "../src/hud_stages.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

static std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

// Same lookup as mod.cpp LoadSkillChain: skill\MET\<ID2>_*\<folder>[_PLAYER].bcm/.bac.
static bool LoadChain(const std::filesystem::path& data, int id2, std::vector<ChainEntry>& chain)
{
    char prefix[8];
    snprintf(prefix, sizeof prefix, "%04d_", id2);
    std::error_code error;
    for (const auto& folder : std::filesystem::directory_iterator(data / "skill" / "MET", error)) {
        std::string name = folder.path().filename().string();
        if (!folder.is_directory() || name.rfind(prefix, 0) != 0) continue;
        auto pick = [&](const char* extension) {
            auto player = folder.path() / (name + "_PLAYER" + extension);
            return ReadFile(std::filesystem::exists(player) ? player : folder.path() / (name + extension));
        };
        return ParseBcmChain(pick(".bcm"), pick(".bac"), chain) && !chain.empty();
    }
    return false;
}

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage: hudtest <game>\\data\n"); return 2; }
    std::filesystem::path data = argv[1];
    AuraStages stages;
    int entries = ParseAuraStages(ReadFile(data / "pre-baked.xml"), stages);
    for (const auto& skill : stages.stageCount) {
        std::vector<ChainEntry> chain;
        if (LoadChain(data, skill.first, chain)) stages.chain[skill.first] = chain;
    }
    printf("%d CusAuraData entries, %d skills, %d BCM chains\n", entries, (int)stages.stageCount.size(),
           (int)stages.chain.size());
    int failures = 0;
    auto expect = [&](bool ok, const char* what, int skill, int aura) {
        if (!ok) { printf("FAIL %s: skill %d aura 0x%x\n", what, skill, aura); ++failures; }
    };
    const CharacterState rested{false, 0, 10000.0f, 1.0f};
    int al4rAura[12] = {};
    for (const auto& [owner, count] : stages.stageCount) {
        int skill = kAwokenSkillBase + owner, shown = -2;
        CharacterState character = rested;
        expect(NextStageToName(stages, skill, count, character, shown), "untransformed", skill, 0);
        printf("ID2 %4d: %2d stages%s: base->%d", owner, count, stages.chain.count(owner) ? "" : " (no BCM)", shown);
        character.transformed = true;
        for (const auto& [aura, stage] : stages.byAura) {
            if (stage.ownerId2 != owner) continue;
            if (owner == 1594 && stage.index < 12) al4rAura[stage.index] = aura;
            character.aura = aura;
            bool ok = NextStageToName(stages, skill, count, character, shown);
            expect(ok && shown < count && (shown == -1 || shown > stage.index), "next stage", skill, aura);
            printf(" %d->%d", stage.index, shown);
            expect(!NextStageToName(stages, skill, count + 1, character, shown), "stage count differs", skill, aura);
            expect(!NextStageToName(stages, skill + 1, count, character, shown), "other skill's aura", skill + 1, aura);
        }
        printf("\n");
    }
    if (stages.chain.count(1594) && stages.stageCount[1594] == 12) {
        int skill = kAwokenSkillBase + 1594, shown = -2;
        CharacterState character = rested;
        expect(NextStageToName(stages, skill, 12, character, shown) && shown == 1, "AL4R base -> SSJ", skill, 0);
        character.transformed = true;
        character.aura = al4rAura[9];
        expect(NextStageToName(stages, skill, 12, character, shown) && shown == -1, "AL4R KK X20 full HP", skill,
               character.aura);
        character.healthRatio = 0.5f;
        expect(NextStageToName(stages, skill, 12, character, shown) && shown == 10, "AL4R KK X20 50% HP", skill,
               character.aura);
        character.aura = al4rAura[11];
        character.healthRatio = 0.1f;
        expect(NextStageToName(stages, skill, 12, character, shown) && shown == -1, "AL4R UI last", skill,
               character.aura);
    } else {
        printf("(AL4R not installed - its checks skipped)\n");
    }
    int shown = 7;
    CharacterState vanilla{true, 0x14, 10000.0f, 1.0f};
    expect(!NextStageToName(stages, 25000, 3, vanilla, shown), "vanilla skill", 25000, 0x14);
    expect(!NextStageToName(stages, 100, 3, vanilla, shown), "not awoken", 100, 0x14);
    printf(failures ? "%d failures\n" : "all good\n", failures);
    return failures ? 1 : 0;
}

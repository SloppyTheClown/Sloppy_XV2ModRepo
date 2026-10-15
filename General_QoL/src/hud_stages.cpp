#include "hud_stages.h"
#include <cstdlib>
#include <cstring>

// pre-baked.xml writes ids as "0x4a" or "1594".
static bool ParseNumber(const std::string& text, int& value)
{
    if (text.empty()) return false;
    bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    char* end = nullptr;
    long parsed = strtol(text.c_str() + (hex ? 2 : 0), &end, hex ? 16 : 10);
    if (!end || *end) return false;
    value = (int)parsed;
    return true;
}

// The quoted value after `key` (e.g. `cus_aura_id="`) between from and to.
static bool AttributeIn(const std::string& xml, size_t from, size_t to, const char* key, int& value)
{
    size_t at = xml.find(key, from);
    if (at == std::string::npos || at >= to) return false;
    at += strlen(key);
    size_t quote = xml.find('"', at);
    if (quote == std::string::npos || quote >= to) return false;
    return ParseNumber(xml.substr(at, quote - at), value);
}

int ParseAuraStages(const std::string& xml, AuraStages& out)
{
    out = AuraStages();
    int entries = 0;
    const char kOpen[] = "<CusAuraData ", kClose[] = "</CusAuraData>";
    for (size_t at = xml.find(kOpen); at != std::string::npos; at = xml.find(kOpen, at + 1)) {
        size_t end = xml.find(kClose, at);
        if (end == std::string::npos) break;
        int auraId = 0, owner = 0;
        if (!AttributeIn(xml, at, end, "cus_aura_id=\"", auraId) ||
            !AttributeIn(xml, at, end, "<CUS_ID2_OWNER value=\"", owner))
            continue;
        ++entries;
        int& count = out.stageCount[owner];
        out.byAura[auraId] = {owner, count};
        ++count;
    }
    return entries;
}

// Little-endian reads that fail instead of running off the buffer.
template <typename T> static bool ReadAt(const std::string& data, size_t at, T& value)
{
    if (at + sizeof(T) > data.size() || at + sizeof(T) < at) return false;
    memcpy(&value, data.data() + at, sizeof(T));
    return true;
}

// Whether BAC entry `index` has a type 15 (Function) row with function 0xd
// (Activate Transformation). BAC layout (Xv2CoreLib BAC/Parser.cs): "#BAC",
// entry count @8, entry table offset @16; entries 16 bytes (type count i16
// @4, type list offset i32 @8); type list rows 16 bytes (type i16, row count
// i16 @2, rows offset i32 @8); type 15 rows 32 bytes (function i32 @8).
static bool BacEntryTransforms(const std::string& bac, int index)
{
    int32_t count = 0, table = 0;
    if (!ReadAt(bac, 8, count) || !ReadAt(bac, 16, table) || index < 0 || index >= count) return false;
    size_t entry = (size_t)table + (size_t)index * 16;
    int16_t typeCount = 0;
    int32_t typeList = 0;
    if (!ReadAt(bac, entry + 4, typeCount) || !ReadAt(bac, entry + 8, typeList) || !typeList) return false;
    for (int type = 0; type < typeCount; ++type) {
        size_t row = (size_t)typeList + (size_t)type * 16;
        int16_t kind = 0, rows = 0;
        int32_t rowsAt = 0;
        if (!ReadAt(bac, row, kind) || !ReadAt(bac, row + 2, rows) || !ReadAt(bac, row + 8, rowsAt)) return false;
        if (kind != 15) continue;
        for (int i = 0; i < rows; ++i) {
            int32_t function = 0;
            if (!ReadAt(bac, (size_t)rowsAt + (size_t)i * 32 + 8, function)) return false;
            if (function == 0xd) return true;
        }
    }
    return false;
}

bool ParseBcmChain(const std::string& bcm, const std::string& bac, std::vector<ChainEntry>& out)
{
    out.clear();
    if (bcm.compare(0, 4, "#BCM") != 0 || bac.compare(0, 4, "#BAC") != 0) return false;
    // BCM layout (Xv2CoreLib BCM/Parser.cs): first entry offset @12; entries
    // 112 bytes: primary activator conditions u32 @24, BAC entry i16 @32,
    // sibling offset @48, child offset @52, KiRequired u32 @92,
    // HealthRequired f32 @96, TransStage i16 @100.
    const uint32_t kInBaseForm = 0x80, kInTransformedState = 0x100;
    const size_t kEntrySize = 112;
    int32_t root = 0;
    if (!ReadAt(bcm, 12, root)) return false;
    // Depth 0 = the root entry and its siblings, depth 1 = their children;
    // the steps limit stops a looping sibling chain in a broken file.
    size_t steps = bcm.size() / kEntrySize + 1;
    struct Level { int32_t at; int depth; };
    std::vector<Level> pending{{root, 0}};
    while (!pending.empty()) {
        Level level = pending.back();
        pending.pop_back();
        for (int32_t at = level.at; at > 0 && steps; --steps) {
            uint32_t conditions = 0, kiRequired = 0;
            int16_t bacEntry = 0, stage = 0;
            int32_t sibling = 0, child = 0;
            float health = 0;
            if (!ReadAt(bcm, at + 24, conditions) || !ReadAt(bcm, at + 32, bacEntry) ||
                !ReadAt(bcm, at + 48, sibling) || !ReadAt(bcm, at + 52, child) ||
                !ReadAt(bcm, at + 92, kiRequired) || !ReadAt(bcm, at + 96, health) ||
                !ReadAt(bcm, at + 100, stage))
                return false;
            if ((conditions & (kInBaseForm | kInTransformedState)) && stage >= 0 &&
                BacEntryTransforms(bac, bacEntry))
                out.push_back({(conditions & kInBaseForm) != 0, (conditions & kInTransformedState) != 0, stage,
                               kiRequired, health});
            // Children come right after their parent in file order.
            if (child > 0 && level.depth < 1) {
                pending.push_back({sibling, level.depth});
                pending.push_back({child, level.depth + 1});
                break;
            }
            at = sibling;
        }
    }
    return true;
}

// Whether the character passes an entry's gates. HealthRequired < 0 = at
// most -x of max HP, > 0 = above x (NT's stats.txt and AL4R's UI stages:
// -0.6 / -0.4).
static bool GatesPass(const ChainEntry& entry, const CharacterState& character)
{
    if (character.ki < (float)entry.kiRequired) return false;
    if (entry.healthRequired < 0 && character.healthRatio > -entry.healthRequired) return false;
    if (entry.healthRequired > 0 && character.healthRatio <= entry.healthRequired) return false;
    return true;
}

bool NextStageToName(const AuraStages& stages, int skillId, int stageCount, const CharacterState& character,
                     int& shown)
{
    if (skillId < kAwokenSkillBase) return false;
    int id2 = skillId % kAwokenSkillBase;
    auto count = stages.stageCount.find(id2);
    if (count == stages.stageCount.end() || count->second != stageCount) return false;
    auto chain = stages.chain.find(id2);
    bool hasChain = chain != stages.chain.end() && !chain->second.empty();

    if (!character.transformed) {
        shown = 0;
        if (!hasChain) return true;
        const ChainEntry* cheapest = nullptr;
        for (const ChainEntry& entry : chain->second) {
            if (!entry.fromBase) continue;
            if (GatesPass(entry, character)) {
                shown = entry.stage;
                return true;
            }
            if (!cheapest || entry.kiRequired < cheapest->kiRequired) cheapest = &entry;
        }
        if (cheapest) shown = cheapest->stage;
        return true;
    }

    auto stage = stages.byAura.find(character.aura);
    if (stage == stages.byAura.end() || stage->second.ownerId2 != id2) return false;
    int current = stage->second.index;
    if (!hasChain) {
        shown = current + 1 < count->second ? current + 1 : -1;
        return true;
    }
    shown = -1;
    for (const ChainEntry& entry : chain->second) {
        if (entry.fromTransformed && entry.stage > current && GatesPass(entry, character)) {
            shown = entry.stage;
            break;
        }
    }
    return true;
}

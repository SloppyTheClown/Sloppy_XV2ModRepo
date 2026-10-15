// Battle HUD awoken-skill stage name: which stage of an x2m transformation
// the skill panel names. The exe hook that asks lives in mod.cpp
// (HudStageNames).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// One top-level BCM entry of an awoken skill that transforms: the skill
// button goes to `stage` from base form (`fromBase`) or from a lower stage
// (`fromTransformed`), when the character has `kiRequired` Ki and passes the
// health gate.
struct ChainEntry {
    bool fromBase;        // PrimaryActivatorConditions InBaseForm (0x80)
    bool fromTransformed; // PrimaryActivatorConditions InTransformedState (0x100)
    int stage;            // BCM TransStage: the CUS stage it transforms into
    uint32_t kiRequired;  // BCM KiRequired, the same units as the character's Ki (100 = one bar)
    float healthRequired; // BCM HealthRequired: < 0 = at most -x of max HP, > 0 = above x, 0 = none
};

// The stages of every x2m awoken skill, from data\pre-baked.xml (written by
// xv2ins, read by XV2Patcher). Each <CusAuraData cus_aura_id=".."> is one
// stage; <CUS_ID2_OWNER> is the skill's ID2. A skill's entries appear in
// stage order (xv2ins writes the x2m's CUS_AUR list in one go: AL4R
// 0x4a..0x55 = stages 0..11, matching their TRANS_STAGE).
struct AuraStages {
    struct Stage { int ownerId2; int index; };
    std::map<int, Stage> byAura;      // cus aura id -> owner skill, stage index
    std::map<int, int> stageCount;    // owner ID2 -> number of stages
    std::map<int, std::vector<ChainEntry>> chain;  // owner ID2 -> its BCM's transform entries, file order
};

// Parses pre-baked.xml's text. Returns the number of CusAuraData entries read.
int ParseAuraStages(const std::string& xml, AuraStages& out);

// The transform entries of an awoken skill's BCM (<ID2>_<CODE>_PLAYER.bcm),
// in file order: entries at depth 0 and 1 (the button's own choices; deeper
// ones are NT's multi-choice follow-ups) with InBaseForm or
// InTransformedState whose BAC entry activates a transformation (BAC type 15
// function 0xd; the revert entries' BAC only deactivates). Returns false if
// either file is not a BCM/BAC. Layouts: Xv2CoreLib BCM/Parser.cs and
// BAC/Parser.cs (ClaudeTools\XV2-Tools).
bool ParseBcmChain(const std::string& bcm, const std::string& bac, std::vector<ChainEntry>& out);

// Awoken skill ids are 25000 + ID2 (the HUD's name lookup 0x1c1cc0 takes
// id % 25000 for BHD_MET_<ID2>_<stage>).
const int kAwokenSkillBase = 25000;

// What the HUD hook reads off the character.
struct CharacterState {
    bool transformed;
    int aura;            // current cus aura id
    float ki;            // current Ki
    float healthRatio;   // current / max HP
};

// The stage the HUD should name for skill `skillId`: the stage the skill
// button goes to now, or -1 ("Deactivate") when it reverts.
// * With a BCM chain: the first entry in file order that applies (from base
//   form when untransformed, else to a stage above the current one) and
//   whose Ki and health gates pass. Untransformed with none affordable, the
//   cheapest base entry is named (vanilla names a stage there too).
// * Without one: the next stage index (stage 0 untransformed).
// Returns false - leave the game's choice - when the skill's pre-baked
// stages are not all of its `stageCount` CUS stages (vanilla skills, or a
// vanilla skill with one aura edited), or the current aura is not one of them.
bool NextStageToName(const AuraStages& stages, int skillId, int stageCount, const CharacterState& character,
                     int& shown);

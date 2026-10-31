// ---------------------------------------------------------------------------
//  Menu scripts: show the combined supers in the game's UI
//
//  * AVATAR_FACULTY.iggy (Customize > Attributes): the AvatarStatus script
//    shared with General_QoL (General_QoL\as3\AvatarStatus.as, built by
//    ClaudeTools\iggy\menu_abc.py qol_faculty into avatar_faculty_abc.h).
//    It holds "XV2FACULTY:" + one flag per mod: [0] General_QoL's free
//    points, [1] ours ('a' additive, 'h' highest, '0' off). We set ours.
//  * CMN_CHART.iggy (the stat radar chart): parked - as3\_CMN_CHART.as
//    (chain link + "SUP" label) looked bad in game, so it is not served.
//
//  It goes through the loader's ReplaceIggyScript, which swaps a script only
//  while the menu still has the one it was built from. General_QoL swaps the
//  Attributes script in its own file hook, so the order of the two is not
//  fixed; each order ends with both flags set:
//    loader first:  vanilla -> ours ('0' + mode); General_QoL then finds the
//                   marker and sets its '1'.
//    QoL first:     vanilla -> General_QoL's ('1' + '0'); we register that
//                   one as a second original -> '1' + mode.
//  (Research_Notes\ui-menu-screens.md "Stat radar chart")
// ---------------------------------------------------------------------------
#include "menu_scripts.h"
#include "xv2_files.h"
#include "avatar_faculty_abc.h"
#include <cstring>
#include <vector>

namespace {

const char kOwner[] = "CombinedSupers";
const char kFacultyPath[] = "ui/iggy/AVATAR_FACULTY.iggy";

// The shared script's feature flags (General_QoL\as3\AvatarStatus.as
// FeatureFlags): the marker, then one character per mod.
const char kFacultyMarker[] = "XV2FACULTY:";
const size_t kFreePointsFlag = 0;  // General_QoL's: '1' on, '0' off
const size_t kSupersFlag = 1;      // ours

uint64_t Fnv64(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
    return hash;
}

// Offset of the first flag character in the embedded shared script, or 0.
size_t FacultyFlagsOffset()
{
    const size_t markerSize = sizeof(kFacultyMarker) - 1;
    for (size_t at = 0; at + markerSize + 2 <= sizeof(kAvatarFacultyAbc); ++at)
        if (memcmp(kAvatarFacultyAbc + at, kFacultyMarker, markerSize) == 0) return at + markerSize;
    return 0;
}

// The shared script with the two flags set.
std::vector<uint8_t> FacultyScript(size_t flags, char freePoints, char supers)
{
    std::vector<uint8_t> abc(kAvatarFacultyAbc, kAvatarFacultyAbc + sizeof(kAvatarFacultyAbc));
    abc[flags + kFreePointsFlag] = (uint8_t)freePoints;
    abc[flags + kSupersFlag] = (uint8_t)supers;
    return abc;
}

char g_applied = 0;  // the mode last applied (0 = none yet)

}  // namespace

std::string MenuScripts_Apply(char mode)
{
    if (mode == g_applied) return "";
    XV2Files files;
    if (!files.Load()) {
        g_applied = mode;
        return "menu scripts: no XV2 Mod Loader file API (loader missing or too old) - Attributes unchanged";
    }
    if (mode == '0') {
        files.Remove(kOwner, kFacultyPath);
        g_applied = mode;
        return "menu scripts: removed (supers not combined)";
    }
    const size_t flags = FacultyFlagsOffset();
    if (!flags) return "menu scripts: the built-in Attributes script has no feature flags (rebuild it)";

    char reason[160] = "";
    std::string result = "menu scripts:";
    // Over the vanilla menu: ours alone.
    std::vector<uint8_t> alone = FacultyScript(flags, '0', mode);
    // Over General_QoL's (free points on, ours off): both.
    std::vector<uint8_t> qolOnly = FacultyScript(flags, '1', '0');
    std::vector<uint8_t> both = FacultyScript(flags, '1', mode);
    bool faculty = files.ReplaceIggyScript(kOwner, kFacultyPath, alone.data(), alone.size(),
                                           kVanillaAvatarFacultyAbcSize, kVanillaAvatarFacultyAbcHash, reason,
                                           sizeof(reason)) &&
                   files.ReplaceIggyScript(kOwner, kFacultyPath, both.data(), both.size(), (uint32_t)qolOnly.size(),
                                           Fnv64(qolOnly.data(), qolOnly.size()), reason, sizeof(reason));
    result += faculty ? " Attributes box on" : std::string(" Attributes box refused (") + reason + ")";
    result += mode == 'h' ? " (total = the higher one)" : " (total = sum)";
    g_applied = mode;
    return result;
}

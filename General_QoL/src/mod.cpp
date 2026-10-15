// ---------------------------------------------------------------------------
//  General QoL - mod core
//
//  * Transfer shop: every Time Machine destination (save flags
//    UNLOCK_TELEPORT_POINT_01..10) is switched on for the loaded character,
//    and the robots no longer say they are locked before the prologue.
//  * Flight: the lobby player may always fly (the game's "can fly" check
//    always says yes); the save's flight flag is left untouched. Optional
//    lobby flight speed multiplier (never in battle).
//  * Story requirements: quests (parallel, expert, training, ...) no longer
//    wait for the story, nor do mentors, shops and desks (quests, battles,
//    advancement tests, radio, photo mode, multiplayer, festival lobby, ...)
//    or the expert mission rifts, not even for the prologue or the parallel
//    quest tutorial; shop items (skills, super souls, clothes, accessories)
//    are sold from the start.
//  * Parallel quests: none waits for another parallel quest.
//  * TP medal shop: every seasonal item is sold in every season.
//  * Replay NPC: one NPC in Dastar's spot in the Time Nest (Hercule by
//    default, the "DLC Master") lists the DLC story quests this character
//    can play; Brashy, Spots and Culina are hidden.
//  * Boot: no logos, no intro movie, and the intro and title screen play at
//    the volume settings the save had last time instead of full volume.
//  * Automatic flag fixer: saves whose flags disagree with their quest
//    progress (e.g. after a save editor's flag tool) no longer break the
//    game - a flag that says episodes 1 and 2 are cleared before story quest
//    CTP_99_01 is no longer reloads Conton City forever (Conton TV check).
//  * Camera zoom: + and - move the camera closer or further away, one
//    setting each for battle, lobby, menus and cutscenes, kept between
//    sessions in general_qol_saved.txt.
//  * Map: the lobby map shows each mentor's name next to their icon.
//  * Raid characters: the Master Raid bosses (Supreme Kai of Time,
//    Corrupted Fused Zamasu, Demigra, Final Form Mira, raid Super Buu) are
//    unlocked without the TP shop gifts or the tutorial; still raid-only.
//  * Transformation HUD: an x2m awoken skill's battle HUD names the next
//    stage instead of always the top one (every x2m aura needs 0 Ki).
//  * Dragon Balls: the game always counts all seven as held, so Shenron can
//    be summoned at any time; a wish never uses them up.
//
//  How:
//  * Flags: each character's 2048 system flags are a bitset at the start of
//    its save block in the quest data manager; a background check every 2 s
//    sets any of the loaded character's that are off.
//  * Script: every data file the game loads is read through one request
//    class. Its "get data" / "get size" methods are redirected; for
//    data/event/lobby/setup_pre_positioning.tsr (starts the scene) and
//    lobby_event_manage.tsd (shows the Supreme Kai of Time the scene talks
//    to, the mentors, desks and transfer robots), the quest files and the
//    item files they return an edited copy owned by the mod; so they do for
//    the replay NPC's lobby entry, lines, greeting voice and quest menu
//    header.
//  * Replay menu: the quest list Dastar opens (-5) and Brashy's (-3) are
//    each built by one function; for those lists it is replaced by one that
//    looks up the configured quests by name.
//    Other mods may hook the same methods: whatever is already there
//    is chained to.
//
//  Addresses are for game 1.26.01; on any other version the mod logs it and
//  changes nothing.
// ---------------------------------------------------------------------------
#include "mod.h"
#include "json.h"
#include "tsr.h"
#include "lobby_npcs.h"
#include "data.h"
#include "replay_npc.h"
#include "map_names.h"
#include "hud_stages.h"
#include "default_config.h"
#include <intrin.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

// ---------------------------------------------------------------- config ---
// Defaults here are what a setting missing from the config means.
struct Config {
    bool enabled = true;
    bool teleports = false;
    bool flight = false;
    float lobbyFlightSpeed = 1.0f;  // lobby_flight_speed (1 = the game's speed)
    bool storyFlags = false;  // clear_story_flags
    bool parallelQuests = false;
    bool expertMissions = false;
    bool levelCap = false;
    bool tpShop = false;
    bool skipLogos = false;
    bool skipIntro = false;
    bool titleVolume = false;  // title_volume
    // automatic_flag_fixer (true/false): game-side fixes for saves whose flags
    // disagree with their quest progress. Older configs' object form
    // {enabled, conton_tv_loop} is still read.
    bool fixTvLobbyLoop = false;  // the Conton City TV reload loop fix
    bool revealConditions = false;  // reveal_hidden_conditions
    bool raidCharacters = false;  // raid_characters
    bool waitForAllReady = false;  // wait_for_all_ready (15 s countdown: PQ and most rooms)
    bool waitForAllReadyRaid = false;  // wait_for_all_ready_raid (60 s countdown: room modes 4/5/9/12)
    bool hudNextStage = false;  // transform_hud_next_stage
    bool allDragonBalls = false;  // all_dragon_balls
    bool cameraZoom = false;
    std::vector<int> zoomInKeys{VK_OEM_PLUS, VK_ADD}, zoomOutKeys{VK_OEM_MINUS, VK_SUBTRACT}, zoomResetKeys{VK_BACK};
    float zoomSpeed = 1.8f;  // zoom factor per second while a key is held
    float zoomMin = 0.5f, zoomMax = 4.0f;
    bool mentorNames = false;  // mentor_names_on_map
    bool logging = false;
    bool logFiles = false;  // debug: log every file path the game loads
    ReplayNpc npc;
    std::vector<std::string> quests;  // the replay menu, by quest name
};

// Every DLC story quest, in story order: the replay menu when the config has
// no "quests" list.
static const char* const kDefaultQuests[] = {
    "CTP_14_00", "CTP_14_01", "CTP_14_02",
    "CTP_15_00", "CTP_15_01", "CTP_15_02", "CTP_15_03", "CTP_15_04",
    "CTP_16_00", "CTP_16_10",
    "CTP_17_00", "CTP_17_10", "CTP_17_20", "CTP_17_30",
    "CTP_18_00", "CTP_18_10",
    "CTP_19_01", "CTP_19_02", "CTP_19_00", "CTP_19_10",
    "CTP_20_00", "CTP_20_01", "CTP_20_02", "CTP_20_03", "CTP_20_99",
    "CTP_21_99",
    "CTP_22_00", "CTP_22_01", "CTP_22_02", "CTP_22_03",
    "CTP_23_00", "CTP_23_01", "CTP_23_02", "CTP_23_03",
    "CTP_24_00", "CTP_24_01", "CTP_24_02", "CTP_24_03", "CTP_24_04",
    "CTP_25_00", "CTP_25_01", "CTP_25_02", "CTP_25_03",
    "CTP_26_00", "CTP_26_01", "CTP_26_02", "CTP_26_03", "CTP_26_04", "CTP_26_05",
    "CTP_27_00", "CTP_27_01", "CTP_27_02", "CTP_27_03", "CTP_27_04",
    "CTP_27_05", "CTP_27_06", "CTP_27_07", "CTP_27_08",
    "CTP_26_99", "CTP_27_99",
};
static Config  g_cfg;
static wchar_t g_dir[MAX_PATH];     // the mod's folder: config and log
static std::vector<std::string> g_configErrors;

static void Log(const char* fmt, ...)
{
    if (!g_cfg.logging) return;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\general_qol.log", g_dir);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") || !f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "%02d:%02d:%02d.%03d  ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

// First run: write the shipped default config. CREATE_NEW - an existing file,
// even a broken one, is never touched.
static void EnsureConfig()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\general_qol.json", g_dir);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(h, kDefaultConfig, (DWORD)(sizeof(kDefaultConfig) - 1), &wrote, nullptr);
    CloseHandle(h);
}

static bool ReadWhole(const wchar_t* path, std::string& out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    out.resize((size_t)min(sz.QuadPart, (LONGLONG)(1 << 20)));
    DWORD got = 0;
    BOOL ok = ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr);
    CloseHandle(h);
    out.resize(got);
    return ok != FALSE;
}

// A key name from the config -> virtual-key code (0 = unknown): one letter or
// digit, a name below, F1..F24, or a code like "0x6B".
static int KeyCode(const std::string& name)
{
    std::string n;
    for (char ch : name) n += (char)tolower((unsigned char)ch);
    if (n.size() == 1 && isalnum((unsigned char)n[0])) return toupper((unsigned char)n[0]);
    static const struct { const char* name; int vk; } kNames[] = {
        {"+", VK_OEM_PLUS}, {"=", VK_OEM_PLUS}, {"-", VK_OEM_MINUS}, {"num+", VK_ADD}, {"num-", VK_SUBTRACT},
        {"num*", VK_MULTIPLY}, {"num/", VK_DIVIDE}, {"num0", VK_NUMPAD0}, {"num1", VK_NUMPAD1},
        {"num2", VK_NUMPAD2}, {"num3", VK_NUMPAD3}, {"num4", VK_NUMPAD4}, {"num5", VK_NUMPAD5},
        {"num6", VK_NUMPAD6}, {"num7", VK_NUMPAD7}, {"num8", VK_NUMPAD8}, {"num9", VK_NUMPAD9},
        {"backspace", VK_BACK}, {"tab", VK_TAB}, {"enter", VK_RETURN}, {"space", VK_SPACE},
        {"insert", VK_INSERT}, {"delete", VK_DELETE}, {"home", VK_HOME}, {"end", VK_END},
        {"pageup", VK_PRIOR}, {"pagedown", VK_NEXT}, {"up", VK_UP}, {"down", VK_DOWN}, {"left", VK_LEFT},
        {"right", VK_RIGHT}, {"[", VK_OEM_4}, {"]", VK_OEM_6}, {",", VK_OEM_COMMA}, {".", VK_OEM_PERIOD},
        {"/", VK_OEM_2}, {";", VK_OEM_1}, {"'", VK_OEM_7}, {"\\", VK_OEM_5}, {"`", VK_OEM_3},
        {"mouse4", VK_XBUTTON1}, {"mouse5", VK_XBUTTON2}, {"middlemouse", VK_MBUTTON},
    };
    for (const auto& k : kNames)
        if (n == k.name) return k.vk;
    if (n.size() >= 2 && n[0] == 'f' && isdigit((unsigned char)n[1])) {
        int f = atoi(n.c_str() + 1);
        if (f >= 1 && f <= 24) return VK_F1 + f - 1;
    }
    if (n.size() > 2 && n[0] == '0' && n[1] == 'x') {
        long v = strtol(n.c_str() + 2, nullptr, 16);
        if (v > 0 && v < 0xff) return (int)v;
    }
    return 0;
}

// Settings the player's file leaves out keep the Config defaults above (the
// unlocks are off unless the file turns them on). A file that cannot be read
// or parsed gets the shipped general_qol.json instead.
static void LoadConfig()
{
    JVal j;
    std::string err;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\general_qol.json", g_dir);
    std::string text;
    if (!(ReadWhole(path, text) && JsonParse(text, j, err))) {
        g_configErrors.push_back(err.empty() ? "general_qol.json could not be read - using the shipped settings"
                                             : "general_qol.json: " + err + " - using the shipped settings");
        JsonParse(kDefaultConfig, j, err);
    }
    Config c;
    c.enabled = j.boolOr("enabled", c.enabled);
    c.teleports = j.boolOr("unlock_teleports", c.teleports);
    // Called unlock_flight_after_prologue before flight came right away.
    c.flight = j.boolOr("unlock_flight", j.boolOr("unlock_flight_after_prologue", c.flight));
    c.lobbyFlightSpeed = (float)j.numOr("lobby_flight_speed", c.lobbyFlightSpeed);
    if (!(c.lobbyFlightSpeed >= 0.1f && c.lobbyFlightSpeed <= 10.0f)) {
        g_configErrors.push_back("lobby_flight_speed must be 0.1..10 - using 1");
        c.lobbyFlightSpeed = 1.0f;
    }
    c.storyFlags = j.boolOr("clear_story_flags", c.storyFlags);
    c.parallelQuests = j.boolOr("unlock_all_parallel_quests", c.parallelQuests);
    c.expertMissions = j.boolOr("unlock_all_expert_missions", c.expertMissions);
    c.levelCap = j.boolOr("unlock_level_cap", c.levelCap);
    c.tpShop = j.boolOr("tp_shop_all_seasons", c.tpShop);
    c.skipLogos = j.boolOr("skip_logos", c.skipLogos);
    c.skipIntro = j.boolOr("skip_intro", c.skipIntro);
    // Called mute_title_music when the title only went silent.
    c.titleVolume = j.boolOr("title_volume", j.boolOr("mute_title_music", c.titleVolume));
    if (const JVal* f = j.get("automatic_flag_fixer"); f && f->type == JVal::Bool) {
        c.fixTvLobbyLoop = f->b;
    } else if (f && f->type == JVal::Obj) {
        bool on = f->boolOr("enabled", true);
        c.fixTvLobbyLoop = on && f->boolOr("conton_tv_loop", true);
    }
    if (const JVal* z = j.get("camera_zoom"); z && z->type == JVal::Bool) {
        c.cameraZoom = z->b;
    } else if (z && z->type == JVal::Obj) {
        c.cameraZoom = z->boolOr("enabled", true);
        auto keys = [&](const char* name, std::vector<int>& out) {
            const JVal* k = z->get(name);
            if (!k || k->type != JVal::Arr) return;
            out.clear();
            for (const JVal& v : k->a) {
                int vk = v.type == JVal::Str ? KeyCode(v.s) : 0;
                if (vk) out.push_back(vk);
                else g_configErrors.push_back(std::string("camera_zoom.") + name + ": unknown key \"" + v.s + "\"");
            }
        };
        keys("zoom_in_keys", c.zoomInKeys);
        keys("zoom_out_keys", c.zoomOutKeys);
        keys("reset_keys", c.zoomResetKeys);
        c.zoomSpeed = (float)z->numOr("speed", c.zoomSpeed);
        c.zoomMin = (float)z->numOr("min", c.zoomMin);
        c.zoomMax = (float)z->numOr("max", c.zoomMax);
        if (!(c.zoomSpeed > 1.0f && c.zoomSpeed <= 100.0f)) {
            g_configErrors.push_back("camera_zoom.speed must be above 1 - using 1.8");
            c.zoomSpeed = 1.8f;
        }
        if (!(c.zoomMin > 0.05f && c.zoomMin <= 1.0f && c.zoomMax >= 1.0f && c.zoomMax <= 20.0f)) {
            g_configErrors.push_back("camera_zoom.min must be 0.05..1 and max 1..20 - using 0.5 and 4");
            c.zoomMin = 0.5f;
            c.zoomMax = 4.0f;
        }
    }
    c.mentorNames = j.boolOr("mentor_names_on_map", c.mentorNames);
    c.revealConditions = j.boolOr("reveal_hidden_conditions", c.revealConditions);
    c.raidCharacters = j.boolOr("raid_characters", c.raidCharacters);
    c.waitForAllReady = j.boolOr("wait_for_all_ready", c.waitForAllReady);
    c.waitForAllReadyRaid = j.boolOr("wait_for_all_ready_raid", c.waitForAllReadyRaid);
    c.hudNextStage = j.boolOr("transform_hud_next_stage", c.hudNextStage);
    c.allDragonBalls = j.boolOr("all_dragon_balls", c.allDragonBalls);
    c.logging = j.boolOr("logging", c.logging);
    c.logFiles = j.boolOr("log_files", c.logFiles);
    c.quests.assign(std::begin(kDefaultQuests), std::end(kDefaultQuests));
    if (const JVal* n = j.get("replay_npc"); n && n->type == JVal::Obj) {
        ReplayNpc& e = c.npc;
        e.enabled = n->boolOr("enabled", e.enabled);
        std::string model = n->strOr("model", e.model.c_str());
        if (model.size() >= 1 && model.size() <= 4) e.model = model;
        else g_configErrors.push_back("replay_npc.model must be a 3-letter character code like STN");
        e.costume = (int)n->numOr("costume", e.costume);
        e.idleAction = (int)n->numOr("idle_action", e.idleAction);
        e.talkMotion = n->strOr("talk_motion", e.talkMotion.c_str());
        e.name = n->strOr("name", e.name.c_str());
        e.greeting = n->strOr("greeting", e.greeting.c_str());
        e.option = n->strOr("option", e.option.c_str());
        e.menuLine = n->strOr("menu_line", e.menuLine.c_str());
        e.farewellGo = n->strOr("farewell_go", e.farewellGo.c_str());
        e.farewellStay = n->strOr("farewell_stay", e.farewellStay.c_str());
        e.menuTitle = n->strOr("menu_title", e.menuTitle.c_str());
        e.talkVoice = n->strOr("talk_voice", e.talkVoice.c_str());
        if (const JVal* h = n->get("hide_npcs"); h && h->type == JVal::Arr) {
            e.hideNpcs.clear();
            for (const JVal& v : h->a)
                if (v.type == JVal::Num) e.hideNpcs.push_back((int)v.n);
        }
        if (const JVal* q = n->get("quests"); q && q->type == JVal::Arr) {
            c.quests.clear();
            for (const JVal& v : q->a)
                if (v.type == JVal::Str && !v.s.empty()) c.quests.push_back(v.s);
        }
    }
    g_cfg = c;
}

// ----------------------------------------------------------- saved state ---
// What the mod remembers between sessions, in general_qol_saved.txt: the
// camera zoom per mode and the save's last volume options. Its own file, not
// the config: it changes while playing and belongs to each machine (like the
// logs), so tools that copy configs between machines can leave it out.
enum CamMode { kModeNone = -1, kModeBattle, kModeLobby, kModeMenu, kModeCutscene, kModes };
static const char* const kModeNames[kModes] = {"battle", "lobby", "menu", "cutscene"};
static const int kVolumeOptions[] = {3, 4, 5};  // option ids; see ApplyTitleVolume

struct Saved {
    float zoom[kModes] = {1.0f, 1.0f, 1.0f, 1.0f};
    int volume[3] = {-1, -1, -1};  // option levels 0..5, -1 = not seen yet
};
static Saved   g_saved;
static SRWLOCK g_savedLock = SRWLOCK_INIT;

static void LoadSaved()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\general_qol_saved.txt", g_dir);
    std::string text;
    if (!ReadWhole(path, text)) return;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        std::string key = line.substr(0, eq);
        double v = atof(line.c_str() + eq + 1);
        for (int m = 0; m < kModes; ++m)
            if (key == std::string("zoom_") + kModeNames[m] && v >= 0.1 && v <= 10.0) g_saved.zoom[m] = (float)v;
        for (int i = 0; i < 3; ++i)
            if (key == "volume_" + std::to_string(kVolumeOptions[i]) && v >= 0 && v <= 5) g_saved.volume[i] = (int)v;
    }
}

// Rewrites the whole file (a few lines); called only when something changed.
// Both the camera thread and the flag thread write it, one at a time.
static void WriteSaved()
{
    static SRWLOCK fileLock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&fileLock);
    AcquireSRWLockShared(&g_savedLock);
    Saved s = g_saved;
    ReleaseSRWLockShared(&g_savedLock);
    std::string text = "# General QoL remembers these between sessions. Delete this file to reset them.\n";
    char line[64];
    for (int m = 0; m < kModes; ++m) {
        sprintf_s(line, "zoom_%s=%.3f\n", kModeNames[m], s.zoom[m]);
        text += line;
    }
    text += "# The save's volume options (0-5) last time a character was loaded, for the title screen.\n";
    for (int i = 0; i < 3; ++i) {
        sprintf_s(line, "volume_%d=%d\n", kVolumeOptions[i], s.volume[i]);
        text += line;
    }
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\general_qol_saved.txt", g_dir);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        WriteFile(h, text.data(), (DWORD)text.size(), &wrote, nullptr);
        CloseHandle(h);
    }
    ReleaseSRWLockExclusive(&fileLock);
}

// ------------------------------------------------------------ addresses ---
// Game 1.26.01 (DBXV2.exe).
static uintptr_t g_base = 0;
static const uintptr_t kRequestVtable = 0xf9b270;  // file request class
static const uintptr_t kQuestData = 0x15545b0;     // global: quest data manager
// System flags (script SYSFLAG/SETSYSFLAG) are per character: 2048 bits at
// the start of the loaded character's save block,
// manager + 0xe700 + slot * 0xe880 (0x81ad00), slot = int at manager+8+0x44
// (0x433e0). Verified in game: the flight scene's flag 801 and a robot's
// first-talk flag 114 appear there as they are set.
static const uintptr_t kSlot = 8 + 0x44;
static const uintptr_t kFirstBlock = 0xe700, kBlockSize = 0xe880;
static const int kSlots = 8;
static const int kFirstTeleport = 60, kLastTeleport = 69;  // UNLOCK_TELEPORT_POINT_01..10
// The shops' and desks' own unlock flags (their triggers and scripts check
// these, not the story). The lobby script sets them on load once the
// prologue (SHOP, BATTLE, QUEST, ONLINE_BATTLE, LEADERBOARD), the parallel
// quest tutorial TMQ_1400 (ONLINE_QUEST), episodes 1 and 2 (ONLINE_LOBBY,
// ODEKAKE, PARTY, TENKAICHI, PRESENT) or episode 13 (XENLON) are cleared,
// and ONBATTLETOUR after its scene. Numbers from the event table's constants.
static const int kDeskUnlocks[] = {0, 1, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 5213};
// HLQ_BUNCH_EVENT_CLEAR: set by the scene that gathers the expert missions
// into one rift (object 266, every expert mission in a list) once HLQ_1100
// is cleared. With it on, that rift is there and the scene never plays.
static const int kExpertListRift = 2400;
// Account flags (script ACCFLAG/SETACCFLAG) are save-wide: 288 bits at
// manager + 8 + 0xcc (ACCFLAG handler 0x3c9988 -> 0x4b9b0, getter 0x826ba0).
// The maximum level (0x848dd0) is 199, lowered to 180/160/140/120/99 while
// ACCFLAG_MAXIMUM_LV_STAGE9..5 (34..30, set by Whis's level cap talks) are
// off and to 95/90/85/80 while STAGE4..1 (9..6, set by the Elder Kai
// events) are off. Only the Lite version caps it lower.
static const uintptr_t kAccountFlags = 8 + 0xcc;
static const int kLevelCapStages[] = {6, 7, 8, 9, 30, 31, 32, 33, 34};
// Raid bosses: the character select's table (0x5e4140) shows a raid boss
// slot only while its account flag is on: DMG 20, MRN 21, ZMD 22
// (ACCFLAG_ORE_RAID_UNLOCK_*, set when Fu gets the boss's TP shop gift,
// npc_talk_present_fuu.tsr), STD and SBM costume 6 24
// (ACCFLAG_PRB_FIRST_UNLOCK, set after the Master Raid tutorial,
// setup_pre_positioning.tsr). The raid key itself still keeps these slots to
// raids.
static const int kRaidBossFlags[] = {20, 21, 22, 24};

// The lobby file the mentor and desk unlocks edit (see Classify).
static const char kLobbyEvents[] = "data/event/lobby/lobby_event_manage.tsd";
// The lobby NPCs these edit: see lobby_npcs.h.

// DBXV2.exe 1.26.01, from its PE header (readable before Steam's DRM has
// decrypted the code).
static bool IsGame12601()
{
    const uint8_t* b = (const uint8_t*)g_base;
    uint32_t pe = *(const uint32_t*)(b + 0x3C);
    return *(const uint32_t*)(b + pe + 8) == 0x6A63732E &&        // TimeDateStamp
           *(const uint32_t*)(b + pe + 24 + 56) == 0x1A1F000;     // SizeOfImage
}

// ------------------------------------------------------------ file hook ---
typedef void*    (__fastcall* GetDataFn)(void* request);
typedef uint64_t (__fastcall* GetSizeFn)(void* request);
static GetDataFn g_origData18 = nullptr, g_origData20 = nullptr;
static GetSizeFn g_origSize30 = nullptr, g_origSize38 = nullptr;

struct Served {
    bool edited = false;
    std::vector<uint8_t> bytes;
};
// Key: the file's path, the game's data pointer and size for one load.
// Entries are kept for the whole session - the game may hold on to a buffer
// it was given.
static SRWLOCK g_lock = SRWLOCK_INIT;
static std::map<std::tuple<std::string, uintptr_t, uint64_t>, Served*> g_served;

// The last edit made per path, to reuse when the same content loads again
// (the game reloads some), so reloads do not pile up copies.
struct LastEdit {
    Served*  served = nullptr;
    uint64_t srcHash = 0, srcSize = 0, outHash = 0;
};
static std::map<std::string, LastEdit> g_lastByPath;

// FNV-1a: a quick fingerprint to tell whether a file's bytes changed.
static uint64_t Fnv64(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
    return hash;
}

enum class FileKind { None, LobbyEvents, Quests, Items, TpShop, NpcFile, LobbyHud };

// Cheap first filter on the raw path: only .tsr .tsd .qxd .idb .est .tnl .msg
// .acb .iggytex .iggy matter.
static bool MaybeOurs(void* request)
{
    const char* path = (const char*)request + 8;  // path stored inline
    size_t len = strnlen(path, 0x118);
    if (len >= 8 && !_stricmp(path + len - 8, ".iggytex")) return true;
    if (len >= 5 && !_stricmp(path + len - 5, ".iggy")) return true;
    if (len < 4 || path[len - 4] != '.') return false;
    // The 3-letter extension, lower-cased.
    char e1 = (char)(path[len - 3] | 0x20), e2 = (char)(path[len - 2] | 0x20), e3 = (char)(path[len - 1] | 0x20);
    return (e1 == 't' && e2 == 's' && (e3 == 'r' || e3 == 'd')) || (e1 == 'q' && e2 == 'x' && e3 == 'd') ||
           (e1 == 'i' && e2 == 'd' && e3 == 'b') || (e1 == 'e' && e2 == 's' && e3 == 't') ||
           (e1 == 't' && e2 == 'n' && e3 == 'l') || (e1 == 'm' && e2 == 's' && e3 == 'g') ||
           (e1 == 'a' && e2 == 'c' && e3 == 'b');
}

// Lower case, '/' separators.
static std::string RequestPath(void* request)
{
    const char* raw = (const char*)request + 8;
    std::string path(raw, strnlen(raw, 0x118));
    for (char& ch : path) {
        if (ch == '\\') ch = '/';
        else if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
    }
    return path;
}

// The path may carry a prefix, so only its tail is compared.
static FileKind Classify(const std::string& path)
{
    auto ends = [&](const char* tail) {
        size_t tailLen = strlen(tail);
        return path.size() >= tailLen && path.compare(path.size() - tailLen, tailLen, tail) == 0 &&
               (path.size() == tailLen || path[path.size() - tailLen - 1] == '/');
    };
    if (ends(kLobbyEvents)) return FileKind::LobbyEvents;
    // Every quest list but the story itself (tpq), which stays in order.
    if (path.find("data/quest/") != std::string::npos && path.size() > 9 &&
        !path.compare(path.size() - 9, 9, "_data.qxd") &&
        !ends("tpq_data.qxd"))
        return FileKind::Quests;
    if (ends("data/system/item/skill_item.idb") || ends("data/system/item/talisman_item.idb") ||
        ends("data/system/item/accessory_item.idb") || ends("data/system/item/costume_top_item.idb") ||
        ends("data/system/item/costume_bottom_item.idb") || ends("data/system/item/costume_gloves_item.idb") ||
        ends("data/system/item/costume_shoes_item.idb"))
        return FileKind::Items;
    if (ends("data/system/eventshoptbl.est") || ends("data/system/eventshoptbl_freemium.est"))
        return FileKind::TpShop;
    if (ClassifyNpcFile(path) != NpcFile::None) return FileKind::NpcFile;
    if (ends("data/ui/iggy/lobyhud.iggy")) return FileKind::LobbyHud;
    return FileKind::None;
}

// Debug (log_files): each distinct path once, in load order.
static std::set<std::string> g_seen;
static void LogFile(void* request, uint64_t size)
{
    const char* raw = (const char*)request + 8;
    std::string path(raw, strnlen(raw, 0x118));
    AcquireSRWLockExclusive(&g_lock);
    bool fresh = g_seen.insert(path).second;
    ReleaseSRWLockExclusive(&g_lock);
    if (fresh) Log("load %s (%llu bytes)", path.c_str(), (unsigned long long)size);
}

// The story edit makes the story requirements of the mentors and desks
// always true, the "locked" edits make the transfer robots' and the radio's
// locked trigger never true. Every edit keeps the file's size and layout.

static bool EditLobbyEvents(const uint8_t* src, size_t srcSize, std::vector<uint8_t>& out)
{
    std::vector<uint8_t> scratch;
    bool edited = false;
    // Each edit applies on top of the ones before it: `cur`/`n` is the file so
    // far (the source until the first edit changes something).
    const uint8_t* cur = src;
    size_t n = srcSize;
    auto apply = [&](const char* what, auto edit) {
        int triggersChanged = edit(cur, scratch);
        if (triggersChanged < 0) Log("%s: unexpected layout - %s left as is", kLobbyEvents, what);
        else Log("%s: %s: %d triggers changed", kLobbyEvents, what, triggersChanged);
        if (triggersChanged > 0) {
            out.swap(scratch);
            edited = true;
            cur = out.data();
            n = out.size();
        }
    };
    if (g_cfg.storyFlags) {
        apply("mentors and desks, story requirements", [&](const uint8_t* cur, std::vector<uint8_t>& o) {
            return TsdStoryAlways(cur, n, kStoryNpcs, o);
        });
        apply("desks, parallel quest tutorial requirement", [&](const uint8_t* cur, std::vector<uint8_t>& o) {
            return TsdReplaceForNpcs(cur, n, kPqTutorialNpcs, kPqTutorialDone, kAlwaysPqTutorial, o);
        });
        apply("radio, locked before the prologue", [&](const uint8_t* cur, std::vector<uint8_t>& o) {
            return TsdReplaceForNpcs(cur, n, kStoryLockedNpcs, kPrologueLocked, kNeverPrologue, o);
        });
    }
    if (g_cfg.teleports) {
        apply("transfer robots, locked before the prologue", [&](const uint8_t* cur, std::vector<uint8_t>& o) {
            return TsdReplaceForNpcs(cur, n, kTeleportLockedNpcs, kPrologueLocked, kNeverPrologue, o);
        });
    }
    // Last, as it rebuilds the file (a new length).
    if (g_cfg.npc.enabled) {
        std::string note;
        if (EditNpcFile(NpcFile::LobbyEvents, g_cfg.npc, cur, n, scratch, note)) {
            out.swap(scratch);
            edited = true;
        }
        Log("%s: replay NPC: %s", kLobbyEvents, note.c_str());
    }
    return edited;
}

// Each Edit* below gets the file as loaded (data, size), fills `out` with the
// edited copy and returns whether it changed anything.
static bool EditQuests(const std::string& path, const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (!g_cfg.storyFlags && !g_cfg.parallelQuests && !g_cfg.expertMissions) return false;
    int cleared = QxdClearRequirements(data, size, g_cfg.storyFlags, g_cfg.parallelQuests, g_cfg.expertMissions, out);
    Log("%s (%llu bytes): %s", path.c_str(), (unsigned long long)size,
        cleared < 0 ? "unexpected layout - left as is"
                    : (std::to_string(cleared) + " story / parallel quest / expert mission requirements cleared").c_str());
    if (cleared < 0) return false;
    // The parallel quest list also needs every quest unlocked from the start
    // (see QxdUnlockFromStart), not only its requirements cleared.
    int unlocked = 0;
    const char kParallelQuests[] = "tmq_data.qxd";
    size_t tailLen = sizeof(kParallelQuests) - 1;
    if (g_cfg.parallelQuests && path.size() >= tailLen &&
        path.compare(path.size() - tailLen, tailLen, kParallelQuests) == 0) {
        unlocked = QxdUnlockFromStart(out);
        Log("%s: %d parallel quests unlocked from the start", path.c_str(), unlocked);
    }
    return cleared > 0 || unlocked > 0;
}

static bool EditItems(const std::string& path, const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (!g_cfg.storyFlags) return false;
    int unlocked = IdbClearStoryPoint(data, size, out);
    Log("%s (%llu bytes): %s", path.c_str(), (unsigned long long)size,
        unlocked < 0 ? "unexpected layout - left as is"
                     : (std::to_string(unlocked) + " items sold from the start").c_str());
    return unlocked > 0;
}

// The only edit that changes a file's size: every season lists every item.
static bool EditTpShop(const std::string& path, const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (!g_cfg.tpShop) return false;
    int items = EstAllSeasons(data, size, out);
    Log("%s (%llu bytes): %s", path.c_str(), (unsigned long long)size,
        items < 0 ? "unexpected layout - left as is"
                  : (std::to_string(items) + " items in every season (" + std::to_string(out.size()) + " bytes)").c_str());
    return items > 0;
}

static bool EditNpc(const std::string& path, const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (!g_cfg.npc.enabled) return false;
    std::string note;
    bool edited = EditNpcFile(ClassifyNpcFile(path), g_cfg.npc, data, size, out, note);
    Log("%s (%llu bytes): %s%s", path.c_str(), (unsigned long long)size, note.c_str(), edited ? "" : " - left as is");
    return edited;
}

// The lobby map's script, with the mentor name labels (map_names.cpp).
static bool EditLobbyHud(const std::string& path, const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (!g_cfg.mentorNames) return false;
    std::string note;
    bool edited = PatchLobbyHud(data, size, out, note);
    Log("%s (%llu bytes): %s%s", path.c_str(), (unsigned long long)size, note.c_str(), edited ? "" : " - left as is");
    return edited;
}

static bool EditFile(FileKind kind, const std::string& path, const uint8_t* data, size_t size,
                     std::vector<uint8_t>& out)
{
    switch (kind) {
    case FileKind::NpcFile: return EditNpc(path, data, size, out);
    case FileKind::TpShop: return EditTpShop(path, data, size, out);
    case FileKind::LobbyEvents: return EditLobbyEvents(data, size, out);
    case FileKind::Quests: return EditQuests(path, data, size, out);
    case FileKind::Items: return EditItems(path, data, size, out);
    case FileKind::LobbyHud: return EditLobbyHud(path, data, size, out);
    default: return false;
    }
}

// The edited copy to hand the game for this load, or null to let the
// original through.
static Served* Lookup(void* request, void* data, uint64_t size)
{
    if (g_cfg.logFiles) LogFile(request, size);
    if (!MaybeOurs(request)) return nullptr;
    std::string path = RequestPath(request);
    FileKind kind = Classify(path);
    if (kind == FileKind::None) return nullptr;
    auto key = std::make_tuple(path, (uintptr_t)data, size);
    AcquireSRWLockExclusive(&g_lock);
    Served*& served = g_served[key];
    if (!served) {
        // Same file loaded again: hand out the earlier edited copy if the
        // game has not written to it.
        const uint8_t* src = (const uint8_t*)data;
        uint64_t srcHash = Fnv64(src, (size_t)size);
        LastEdit& last = g_lastByPath[path];
        if (last.served && last.srcHash == srcHash && last.srcSize == size &&
            (!last.served->edited || Fnv64(last.served->bytes.data(), last.served->bytes.size()) == last.outHash)) {
            served = last.served;
        } else {
            served = new Served;
            served->edited = EditFile(kind, path, src, (size_t)size, served->bytes);
            if (!served->edited) served->bytes.clear();
            last = {served, srcHash, size, served->edited ? Fnv64(served->bytes.data(), served->bytes.size()) : 0};
        }
    }
    Served* result = served;
    ReleaseSRWLockExclusive(&g_lock);
    return result->edited ? result : nullptr;
}

// Whether a request needs Lookup: a file this mod may edit, or every file
// while log_files lists them.
static bool WantsRequest(void* request) { return g_cfg.logFiles || MaybeOurs(request); }

// Every mod in the file-request chain calls the next one's data and size;
// fetching them for files this mod never edits multiplied the calls ~600x
// down a six-mod chain (host hitches online), so the path is checked first.
static void* Serve(void* request, void* data)
{
    if (!data || !WantsRequest(request)) return data;
    Served* served = Lookup(request, data, g_origSize38(request));
    return served ? served->bytes.data() : data;
}
static uint64_t ServeSize(void* request, uint64_t size)
{
    if (!WantsRequest(request)) return size;
    void* data = g_origData20(request);
    if (!data) return size;
    Served* served = Lookup(request, data, g_origSize38(request));
    return served ? served->bytes.size() : size;
}
// The four hooked vtable slots, named after their byte offset in the table.
static void*    __fastcall HkData18(void* request) { return Serve(request, g_origData18(request)); }
static void*    __fastcall HkData20(void* request) { return Serve(request, g_origData20(request)); }
static uint64_t __fastcall HkSize30(void* request) { return ServeSize(request, g_origSize30(request)); }
static uint64_t __fastcall HkSize38(void* request) { return ServeSize(request, g_origSize38(request)); }

// Points the request class's data/size methods at the functions above,
// chaining to whatever is in the table, under a lock every such mod shares.
static bool InstallFileHook()
{
    if (!IsGame12601()) return false;
    HANDLE lock = CreateMutexW(nullptr, FALSE, L"XV2Mods_FileRequestHook");
    if (lock) WaitForSingleObject(lock, 10000);
    uintptr_t* vtable = (uintptr_t*)(g_base + kRequestVtable);
    g_origData18 = (GetDataFn)vtable[3];
    g_origData20 = (GetDataFn)vtable[4];
    g_origSize30 = (GetSizeFn)vtable[6];
    g_origSize38 = (GetSizeFn)vtable[7];
    DWORD oldProtect;
    bool ok = VirtualProtect(&vtable[3], 5 * sizeof(uintptr_t), PAGE_READWRITE, &oldProtect) != FALSE;
    if (ok) {
        vtable[3] = (uintptr_t)&HkData18;
        vtable[4] = (uintptr_t)&HkData20;
        vtable[6] = (uintptr_t)&HkSize30;
        vtable[7] = (uintptr_t)&HkSize38;
        VirtualProtect(&vtable[3], 5 * sizeof(uintptr_t), oldProtect, &oldProtect);
    }
    if (lock) { ReleaseMutex(lock); CloseHandle(lock); }
    return ok;
}

static double MsSinceStart();

// ---------------------------------------------------------- replay menu ---
// Quest-list menus (QUEST_MENU "TPQ", -1, <list>): Brashy's -3 (TPQ_ADD) and
// Dastar's -5 (TPQ_FUTURE) are built by 0x599ba0 and 0x5996c0 from the save's
// story progress. For those two lists the mod builds the list from the config
// instead: 0x81ac70([0x15545b0], -1) -> ctx; 0x834600(ctx, 0x59a5a0([menu+
// 0xa0]), name) -> quest; appended with the game's 0x3def0.
static const uintptr_t kBuildAddMenu = 0x599ba0;    // builds quest list -3 (TPQ_ADD, Brashy)
static const uintptr_t kBuildFutureMenu = 0x5996c0; // builds quest list -5 (TPQ_FUTURE, Dastar)
static const uintptr_t kQuestCtx = 0x81ac70;        // (questData, -1) -> quest context
static const uintptr_t kQuestByName = 0x834600;     // (ctx, typeIndex, name) -> quest
static const uintptr_t kVecInsert = 0x3def0;        // vector<void*> grow-and-insert
static const uintptr_t kTypeIndex = 0x59a5a0;       // menu quest type -> type index
static const uintptr_t kQuestRecord = 0x7fb090;     // (tables, type, index) -> quest's QXD record
static const uintptr_t kDlcAllowed = 0x59b570;      // (menu, DLC_Flag) -> nonzero if every DLC bit passes
static const uintptr_t kProgressMgr = 0x1554458;    // global: progress manager; +0x928 quest tables

struct Sig { uintptr_t rva; const char* hex; };
static const Sig kMenuSigs[] = {
    {kBuildAddMenu, "48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D9"},
    {kBuildFutureMenu, "48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D9 48 81 EC 90 00 00 00"},
    {kQuestCtx, "40 53 48 83 EC 20 48 8B D9 83 FA FF 75 0E 48 83"},
    {kQuestByName, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 63 DA 48 8B"},
    {kVecInsert, "48 89 4C 24 08 53 56 57 41 54 41 55 41 56 41 57 48 83 EC 30"},
    {kTypeIndex, "4C 8B DC 48 81 EC A8 00 00 00"},
    {kQuestRecord, "40 53 48 83 EC 20 49 63 D8 85 D2 78 49 45 85 C0 78 44 83 FA"},
    {kDlcAllowed, "40 53 48 83 EC 20 8B DA F6 C2 01 74 15 BA 01 00 00 00 8D 4A"},
};

static bool Matches(const Sig& sig)
{
    const uint8_t* p = (const uint8_t*)(g_base + sig.rva);
    for (const char* h = sig.hex; *h;) {
        if (*h == ' ') { ++h; continue; }
        unsigned v = 0;
        if (sscanf_s(h, "%2x", &v) != 1) return false;
        if (*p++ != (uint8_t)v) return false;
        h += 2;
    }
    return true;
}
static bool MenuCodeMatches()
{
    for (const Sig& sig : kMenuSigs)
        if (!Matches(sig)) return false;
    return true;
}

typedef void  (__fastcall* BuildFn)(void* menu, void** vec);
typedef void* (__fastcall* QuestCtxFn)(void* questData, int mode);
typedef void* (__fastcall* QuestByNameFn)(void* ctx, int typeIndex, const char* name);
typedef void* (__fastcall* VecInsertFn)(void** vec, void** where, void** value);
typedef int   (__fastcall* TypeIndexFn)(int questType);
typedef uint8_t* (__fastcall* QuestRecordFn)(void* tables, int type, int index);
typedef int   (__fastcall* DlcAllowedFn)(void* menu, uint32_t dlcFlag);
static BuildFn g_origAdd = nullptr;     // trampolines to the originals
static BuildFn g_origFuture = nullptr;
static bool    g_loggedMissing = false;

// Fills the menu's quest vector with the configured quests this character
// can play: each must pass the game's own DLC_Flag check, as every other
// quest list does (the original builder: QXD record +0x10c -> 0x59b570), and
// one with no DLC_Flag, which that check never holds back, must be reached:
// state (quest vt+0x18, as the original builder reads it) past QUEST_LOCK (0).
static void BuildReplayList(void* menu, void** vec, BuildFn orig)
{
    auto questCtx = (QuestCtxFn)(g_base + kQuestCtx);
    auto byName = (QuestByNameFn)(g_base + kQuestByName);
    auto insert = (VecInsertFn)(g_base + kVecInsert);
    auto typeIndex = (TypeIndexFn)(g_base + kTypeIndex);
    auto record = (QuestRecordFn)(g_base + kQuestRecord);
    auto dlcAllowed = (DlcAllowedFn)(g_base + kDlcAllowed);

    void* questData = *(void**)(g_base + kQuestData);
    if (!questData) { orig(menu, vec); return; }
    int type = typeIndex(*(int*)((uint8_t*)menu + 0xa0));
    void* ctx = questCtx(questData, -1);
    uint8_t* progress = *(uint8_t**)(g_base + kProgressMgr);
    int listed = 0, locked = 0, unreached = 0;
    std::string missing, notReached;
    for (const std::string& name : g_cfg.quests) {
        void* quest = byName(ctx, type, name.c_str());
        if (!quest) { missing += " " + name; continue; }
        {
            // Quest object: +8 type, +0xc index into the quest tables.
            uint8_t* qxdRecord = progress ? record(*(void**)(progress + 0x928), *(int*)((uint8_t*)quest + 8),
                                                   *(int*)((uint8_t*)quest + 0xc)) : nullptr;
            uint32_t dlcFlag = qxdRecord ? *(uint32_t*)(qxdRecord + 0x10c) : 0;
            if (!qxdRecord || !dlcAllowed(menu, dlcFlag)) { ++locked; continue; }
            if (dlcFlag == 0) {
                auto getState = (int (__fastcall*)(void*))(*(void***)quest)[3];
                if (getState(quest) < 1) { ++unreached; notReached += " " + name; continue; }
            }
        }
        // vec is a std::vector<void*>: [0] begin, [1] end, [2] capacity end.
        bool alreadyListed = false;
        for (void** it = (void**)vec[0]; it != (void**)vec[1]; ++it) alreadyListed |= *it == quest;
        if (alreadyListed) continue;
        void** end = (void**)vec[1];
        if (end == (void**)vec[2]) {
            insert(vec, end, &quest);  // the game's own grow-and-insert
        } else {
            *end = quest;
            vec[1] = end + 1;
        }
        ++listed;
    }
    Log("replay menu: %d quests listed, %d still DLC-locked, %d not reached yet%s", listed, locked, unreached,
        notReached.c_str());
    if (!missing.empty() && !g_loggedMissing) {
        Log("  not found in the game's quest data:%s", missing.c_str());
        g_loggedMissing = true;
    }
}

// Each builder is replaced only for its own list; a menu for one quest ID
// (0 or more at +0xa4) goes to the original.
static void Build(void* menu, void** vec, int wantList, BuildFn orig)
{
    int list = *(int*)((uint8_t*)menu + 0xa4);  // quest ID, or a negative list number
    if (list != wantList || g_cfg.quests.empty()) {
        orig(menu, vec);
        return;
    }
    BuildReplayList(menu, vec, orig);
}
static void __fastcall HkBuildAddMenu(void* menu, void** vec) { Build(menu, vec, -3, g_origAdd); }
static void __fastcall HkBuildFutureMenu(void* menu, void** vec) { Build(menu, vec, -5, g_origFuture); }

// Executable memory within +/-2GB of `target`, for a 5-byte relative jmp.
static uint8_t* AllocNear(uintptr_t target, size_t size)
{
    SYSTEM_INFO si; GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity;
    for (uintptr_t delta = gran; delta < 0x40000000; delta += gran) {
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t addr = dir ? target + delta : target - delta;
            addr -= addr % gran;
            void* p = VirtualAlloc((void*)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (p) return (uint8_t*)p;
        }
    }
    return nullptr;
}

static void AbsJmp(uint8_t* at, uintptr_t to)  // jmp [rip+0]; dq to   (14 bytes)
{
    at[0] = 0xFF; at[1] = 0x25; memset(at + 2, 0, 4);
    memcpy(at + 6, &to, 8);
}

// Function entry -> cave -> hook. The first `n` (5..8) bytes of the target
// must be whole, position-independent instructions (callers check the bytes);
// they are replayed in the trampoline returned, which calls the original.
// The jmp goes in with one 8-byte write, so a target 8-byte aligned is safe
// to patch while other threads run it.
static void* HookCode(uint8_t* target, size_t n, void* hook)
{
    // Cave layout: +0 jmp to the hook; +16 the trampoline (the n replayed
    // bytes, then a jmp back to target + n).
    uint8_t* cave = AllocNear((uintptr_t)target, 64);
    if (!cave) return nullptr;
    AbsJmp(cave, (uintptr_t)hook);
    memcpy(cave + 16, target, n);
    AbsJmp(cave + 16 + n, (uintptr_t)target + n);
    uint8_t code[8];  // target's first 8 bytes with the jmp written over them
    memcpy(code, target, 8);
    code[0] = 0xE9;
    int32_t rel = (int32_t)((intptr_t)cave - ((intptr_t)target + 5));
    memcpy(code + 1, &rel, 4);
    DWORD old;
    if (!VirtualProtect(target, 8, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    if (((uintptr_t)target & 7) == 0) InterlockedExchange64((volatile LONG64*)target, *(LONG64*)code);
    else memcpy(target, code, 5);
    VirtualProtect(target, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 8);
    return cave + 16;
}

// Both list builders start with a 5-byte instruction (checked by the signatures).
static bool InstallMenuHook(uintptr_t rva, void* hook, BuildFn* orig)
{
    *orig = (BuildFn)HookCode((uint8_t*)(g_base + rva), 5, hook);
    return *orig != nullptr;
}

// The menu hook patches code: waits (up to 3 minutes) until Steam's DRM has
// decrypted it and it matches.
static void InstallReplayMenu()
{
    bool matched = false, ok = false;
    for (int i = 0; i < 18000 && !matched; ++i) {
        matched = MenuCodeMatches();
        if (!matched) Sleep(10);
    }
    if (matched)  // tried once: a hook must never go in twice
        ok = InstallMenuHook(kBuildAddMenu, (void*)&HkBuildAddMenu, &g_origAdd) &&
             InstallMenuHook(kBuildFutureMenu, (void*)&HkBuildFutureMenu, &g_origFuture);
    Log(ok ? "replay menu hook installed at %.0f ms (%d quests configured)"
           : "replay menu hook NOT installed (%.0f ms): code does not match game 1.26.01",
        MsSinceStart(), (int)g_cfg.quests.size());
}

// ---------------------------------------------------------- map names ---
// The lobby map (LOBYHUD.iggy) gets one text line per icon from the game,
// "Event:id=<icon>,x=<%>,y=<%>". 0x3e9890 walks the lobby NPCs and calls
// 0x5767c0(map, [npc+0x1c], icon type [npc+0x34c], &position = npc+0x2d0)
// for each; that one starts the line with "Event:", prints
// "id=%d,x=%d,y=%d" and adds it with std::string::append(text, count)
// (0x7a740) at 0x576904. The call there is pointed at a cave that passes rdi
// - still the NPC's position pointer (0x5767e7 mov rdi, r9) - on as a 4th
// argument; HkMapEventAppend adds the NPC's name (the wide string at
// npc+0x42 the game shows with %ls, getter 0x4aa1d0) as ",c0=..,c1=..".
// The edited map script (as3\LobbyMap.as, served by EditLobbyHud) shows
// the name next to the instructor icons.
static const uintptr_t kMapEventAppendCall = 0x576904;  // e8 rel32 -> 0x7a740
static const uintptr_t kStringAppend = 0x7a740;         // std::string::append(this, text, count)
static const uintptr_t kNpcPosition = 0x2d0;            // getter 0x4aa1c0
static const uintptr_t kNpcName = 0x42;                 // getter 0x4aa1d0(npc, 0); the next string is at +0x142
static const size_t kNpcNameMaxChars = 64;              // also LobbyMap.as eventName's limit

static const Sig kMapNameSigs[] = {
    {0x5767e7, "49 8B F9 41 8B E8 8B DA 48 8B"},                // mov rdi, r9 (position) ...
    {0x5768fa, "48 8D 54 24 70 48 8D 4C 24 48 E8 37 3E B0 FF"},  // ... append(line, "id=..", count)
    {0x3e994f, "E8 6C 08 0C 00 4C 8B E0"},                      // r12 = position getter(npc)
    {0x3e99be, "4D 8B CC 44 8B C3 41 8B D7 49 8B 8E 68 01 00 00"},  // r9 = r12, then call 0x5767c0
    {0x4aa1c0, "48 8D 81 D0 02 00 00 C3"},                      // position = npc + 0x2d0
    {0x4aa1d0, "48 8D 81 42 01 00 00 85 D2 75 04 48 8D 41 42 C3"},  // name = npc + 0x42 / 0x142
};

typedef void* (__fastcall* StringAppendFn)(void* line, const char* text, size_t count);
static StringAppendFn g_stringAppend = nullptr;

// Debug log: each name sent to the map, once.
static std::set<std::wstring> g_namesLogged;

static void* __fastcall HkMapEventAppend(void* line, const char* fields, size_t count, const uint8_t* npcPosition)
{
    std::string text(fields, count);
    const wchar_t* name = (const wchar_t*)(npcPosition - kNpcPosition + kNpcName);
    bool sent = AppendNameFields(name, kNpcNameMaxChars, text);
    if (sent && g_cfg.logging && g_namesLogged.size() < 200 && g_namesLogged.insert(name).second) {
        char utf8[3 * kNpcNameMaxChars + 1];
        int bytes = WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, sizeof(utf8), nullptr, nullptr);
        Log("map: %s ... %s", bytes > 0 ? utf8 : "?", text.c_str());
    }
    return g_stringAppend(line, text.data(), text.size());
}

// Waits (up to 3 minutes) for Steam's DRM to decrypt the code, then points
// the append call at a cave: mov r9, rdi; jmp HkMapEventAppend. Installed
// at boot, long before the lobby map runs that code.
static void InstallMapNames()
{
    bool matched = false, ok = false;
    for (int i = 0; i < 18000 && !matched; ++i) {
        matched = true;
        for (const Sig& sig : kMapNameSigs) matched = matched && Matches(sig);
        if (!matched) Sleep(10);
    }
    uint8_t* call = (uint8_t*)(g_base + kMapEventAppendCall);
    uint8_t* cave = matched ? AllocNear((uintptr_t)call, 32) : nullptr;
    if (cave) {
        g_stringAppend = (StringAppendFn)(g_base + kStringAppend);
        const uint8_t movR9Rdi[] = {0x49, 0x89, 0xF9};
        memcpy(cave, movR9Rdi, sizeof(movR9Rdi));
        AbsJmp(cave + sizeof(movR9Rdi), (uintptr_t)&HkMapEventAppend);
        int32_t rel = (int32_t)((intptr_t)cave - ((intptr_t)call + 5));
        DWORD old;
        if (VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy(call + 1, &rel, 4);
            VirtualProtect(call, 5, old, &old);
            FlushInstructionCache(GetCurrentProcess(), call, 5);
            ok = true;
        }
    }
    Log(ok ? "map names hook installed at %.0f ms"
           : "map names hook NOT installed (%.0f ms): code does not match game 1.26.01",
        MsSinceStart());
}

// ----------------------------------------------------------- save flags ---
static volatile LONG* LoadedCharacterFlags()
{
    uint8_t* mgr = *(uint8_t**)(g_base + kQuestData);
    if (!mgr) return nullptr;
    int slot = *(volatile int*)(mgr + kSlot);
    if (slot < 0 || slot >= kSlots) return nullptr;
    return (volatile LONG*)(mgr + kFirstBlock + slot * kBlockSize);
}
static bool FlagOn(volatile LONG* flags, int i) { return (flags[i >> 5] >> (i & 31)) & 1; }
// Returns whether the flag was off.
static bool SetFlag(volatile LONG* flags, int i)
{
    if (FlagOn(flags, i)) return false;
    InterlockedOr(&flags[i >> 5], (LONG)(1u << (i & 31)));
    return true;
}

// Sets any flags of the loaded character (and the level cap's account
// flags) that are off; at most 35 dword
// reads per call.
static void UpdateFlags()
{
    volatile LONG* flags = LoadedCharacterFlags();
    if (!flags) return;
    if (g_cfg.teleports) {
        int set = 0;
        for (int i = kFirstTeleport; i <= kLastTeleport; ++i) set += SetFlag(flags, i);
        if (set) Log("transfer shop: %d destinations unlocked", set);
    }
    if (g_cfg.storyFlags) {
        int set = 0;
        for (int i : kDeskUnlocks) set += SetFlag(flags, i);
        if (set) Log("shops and desks: %d unlock flags set", set);
    }
    if (g_cfg.expertMissions && SetFlag(flags, kExpertListRift)) Log("expert missions: list rift unlocked");
    // Only once a character is loaded, so the save's own flags are in place.
    uint8_t* mgr = *(uint8_t**)(g_base + kQuestData);
    volatile LONG* account = (volatile LONG*)(mgr + kAccountFlags);
    if (g_cfg.levelCap) {
        int set = 0;
        for (int i : kLevelCapStages) set += SetFlag(account, i);
        if (set) Log("level cap: %d stages unlocked (save-wide)", set);
    }
    if (g_cfg.raidCharacters) {
        int set = 0;
        for (int i : kRaidBossFlags) set += SetFlag(account, i);
        if (set) Log("raid characters: %d raid boss unlock flags set (save-wide)", set);
    }
}

// ----------------------------------------------------------------- boot ---

// Logos: the Logo step (0x57e620) updates the logo slideshow (Advertise,
// 8 slides, ~19 s) and waits while it is not done:
//   call IsDone ; test eax,eax ; je wait ; mov rcx,[rbx+0x60]
// Removing the je makes it finish on its first frame; the game then resets
// the slideshow and moves on as usual.
static const uintptr_t kLogoWait = 0x57e63b;
static const uint8_t kLogoWaitCode[] = {0x85, 0xC0, 0x74, 0x51, 0x48, 0x8B, 0x4B, 0x60};

// Intro movie: Menu::WaitInstall plays demo A00_01 (the intro) before the
// title. Its Start (0x6284e0, called through Event::WaitInstall's vtable
// 0xfcd808 slot 1) creates the demo player and runs load -> play -> stop;
// IsDone (slot 3) reads [obj+0x60], which only Stop (0x628180, slot 4) sets.
// Start jumps straight to Stop: done at once, nothing loaded - what the
// game does when the intro is skipped with a button, minus the movie.
static const uintptr_t kIntroStart = 0x6284e0, kIntroStop = 0x628180;
static const uint8_t kIntroStartCode[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x70, 0x48, 0xC7, 0x44, 0x24, 0x30};
static const uint8_t kIntroStopCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x40};

// Title volume: volume options are only read from the save after Start, so
// until then (intro, title screen) the sound system plays everything at full
// volume. The game applies them in 0x8479e0: category 0 <- option 3, 2 <- 4,
// 1 <- 5, each level 0..5 through the table at 0x10d3178 (0.0, 0.2 .. 1.0),
// with SetCategoryVolume (0x363d10) on Sound::System (global 0x1554490),
// which it calls only when [snd+0x10] != 0 and [snd+0x18bc8] == 0 (0x433d0,
// 0x362fa0). The mod does the same once, with the levels remembered from the
// last time a character was loaded (RememberVolume); before it has seen any,
// it mutes category 0 (music). The save's own settings replace it on Start.
static const uintptr_t kSound = 0x1554490, kSetCategoryVolume = 0x363d10, kVolumeTable = 0x10d3178;
static const uint8_t kSetCategoryVolumeCode[] = {0x40, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83, 0xEC, 0x40};
static const float kVolumeLevels[6] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
static const int kVolumeCategory[3] = {0, 2, 1};  // for options 3, 4, 5
typedef void(__fastcall* SetCategoryVolumeFn)(void* sound, int category, float volume);

// Conton TV lobby loop: "is Conton TV unlocked" (0x3d5220) is true when
// SYSFLAG 34 CTP_EPISODE_01_AND_02_CLEARED, SYSFLAG 80 PUT_CONTONTOTV or
// story quest CTP_99_01 is set/cleared. While the TV is not placed (80 off)
// and it says unlocked, the lobby (0x3e5909, 0x3e5a08) sets the TV up, which
// reloads the lobby - but the scene that places it (CTV_UNLOCK_EVENT,
// setup_pre_positioning.tsr) only runs once CTP_99_01 is cleared. A save with
// flag 34 on and CTP_99_01 not cleared (Save Editor 3.5.0 "Validate Flags"
// does this) reloads Conton City forever. The game only sets 34 when
// CTP_99_01 is cleared, so the flag-34 test is redundant for a consistent
// save: it is changed to test flag 80 instead (the instruction
// mov dword [rsp+0x38], 34 at 0x3d5230, the flag id byte at +4). Found by
// clearing/setting flags live during the loop (Research_Notes,
// save-flags-and-unlocks.md); confirmed in game.
static const uintptr_t kTvUnlockFlagId = 0x3d5230;
static const uint8_t kTvUnlockFlagIdCode[] = {0xC7, 0x44, 0x24, 0x38, 0x22, 0x00, 0x00, 0x00};
static const uint8_t kTvPlacedFlag = 80;  // PUT_CONTONTOTV

// Parallel quest details (quest detail builder 0x59e660, TMQ branch) show
// condition lines 2 and 3 (quest msg kinds 4 and 5) only when the quest
// object's vt+0x58 value is >= 2 / >= 3, else msg QUE_STR_10_000 ("???").
// The two jl's that pick "???" (cmp eax,2 / jl at 0x59f397, cmp eax,3 / jl
// at 0x59f406) are NOPed, so the real text always shows. Found by reverse
// engineering the game (Research_Notes, lobby-quests-and-shops.md).
static const uintptr_t kConditionChecks[] = {0x59f394, 0x59f403};
static const uint8_t kConditionCheckCode[2][5] = {{0x83, 0xF8, 0x02, 0x7C, 0x34}, {0x83, 0xF8, 0x03, 0x7C, 0x44}};

// Online room (ROOM.iggy, Menu::RoomUI) countdown: the room's event handler
// 0x5cf900 starts the 60 s room timer (0x5dc5b0, seconds in edx) on events 1
// and 3 - event 3 is queued (0x475cb4) when a member's ready flag
// (member+0x149) is set - in room modes 4/5/9/12 with 2+ members. When it
// runs out (0x5dc580) everyone is sent on. The host sends everyone on by
// itself once all are ready (0x5cf7d0 -> all-ready check 0x472610), so the
// timer is never started instead: the jb at 0x5cfa2c that skips the start for
// fewer than 2 members becomes a jmp to the same target (0x5cfb79; nop + e9,
// the rel32 0x147 stays). Found by reverse engineering the game
// (Research_Notes, lobby-quests-and-shops.md "Online room countdown").
static const uintptr_t kRoomTimerCheck = 0x5cfa29;
static const uint8_t kRoomTimerCheckCode[] = {0x83, 0xF8, 0x02, 0x0F, 0x82, 0x47, 0x01, 0x00, 0x00,
                                              0xBA, 0x3C, 0x00, 0x00, 0x00};  // cmp eax,2 / jb / mov edx,60
// The 15 s countdown of the other room modes (1/2/6-8/11/15/18-22, e.g.
// Parallel Quests): network message 6 (P2P handler 0x475640, case at
// 0x475b65) queues event 5, whose case picks the mode through a byte table
// (0x5cfc78) and reaches "mov edx,15" at 0x5cfb6c before the shared Start
// call. That mov becomes "jmp 0x5cfb79" (eb 0b + 3 nops), past the call.
// Only the byte table jumps to 0x5cfb6c; the 60 s path enters at 0x5cfb71.
static const uintptr_t kRoomShortTimerStart = 0x5cfb6c;
static const uint8_t kRoomShortTimerStartCode[] = {0xBA, 0x0F, 0x00, 0x00, 0x00,   // mov edx,15
                                                   0x48, 0x8B, 0xCE,               // mov rcx,rsi
                                                   0xE8, 0x37, 0xCA, 0x00, 0x00};  // call Start 0x5dc5b0

static bool CodeIs(uintptr_t rva, const uint8_t* code, size_t n)
{
    return memcmp((const void*)(g_base + rva), code, n) == 0;
}

// Returns true once done (or impossible).
static bool SkipLogos()
{
    if (!CodeIs(kLogoWait, kLogoWaitCode, sizeof(kLogoWaitCode))) return false;  // not decrypted yet
    uint8_t* je = (uint8_t*)(g_base + kLogoWait + 2);
    DWORD old;
    if (!VirtualProtect(je, 2, PAGE_EXECUTE_READWRITE, &old)) {
        Log("logos: could not patch");
        return true;
    }
    je[0] = 0x90;
    je[1] = 0x90;
    VirtualProtect(je, 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), je, 2);
    Log("logos: skipped (%.0f ms after start)", MsSinceStart());
    return true;
}

static bool SkipIntro()
{
    if (!CodeIs(kIntroStart, kIntroStartCode, sizeof(kIntroStartCode)) ||
        !CodeIs(kIntroStop, kIntroStopCode, sizeof(kIntroStopCode)))
        return false;  // not decrypted yet
    uint8_t* at = (uint8_t*)(g_base + kIntroStart);
    DWORD old;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old)) {
        Log("intro: could not patch");
        return true;
    }
    int32_t rel = (int32_t)((intptr_t)kIntroStop - (intptr_t)(kIntroStart + 5));
    at[0] = 0xE9;  // jmp Stop
    memcpy(at + 1, &rel, 4);
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    Log("intro: skipped (%.0f ms after start)", MsSinceStart());
    return true;
}

static bool FixTvLobbyLoop()
{
    if (!CodeIs(kTvUnlockFlagId, kTvUnlockFlagIdCode, sizeof(kTvUnlockFlagIdCode))) return false;  // not decrypted yet
    uint8_t* flagId = (uint8_t*)(g_base + kTvUnlockFlagId + 4);
    DWORD old;
    if (!VirtualProtect(flagId, 1, PAGE_EXECUTE_READWRITE, &old)) {
        Log("flag fixer, Conton TV lobby loop: could not patch");
        return true;
    }
    *flagId = kTvPlacedFlag;
    VirtualProtect(flagId, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), flagId, 1);
    Log("flag fixer, Conton TV lobby loop: fixed (%.0f ms after start)", MsSinceStart());
    return true;
}

static bool RevealConditions()
{
    for (int i = 0; i < 2; ++i)
        if (!CodeIs(kConditionChecks[i], kConditionCheckCode[i], sizeof(kConditionCheckCode[i])))
            return false;  // not decrypted yet
    for (uintptr_t check : kConditionChecks) {
        uint8_t* jump = (uint8_t*)(g_base + check + 3);  // the 2-byte jl after the cmp
        DWORD old;
        if (!VirtualProtect(jump, 2, PAGE_EXECUTE_READWRITE, &old)) {
            Log("hidden quest conditions: could not patch");
            return true;
        }
        jump[0] = jump[1] = 0x90;
        VirtualProtect(jump, 2, old, &old);
        FlushInstructionCache(GetCurrentProcess(), jump, 2);
    }
    Log("hidden quest conditions: revealed (%.0f ms after start)", MsSinceStart());
    return true;
}

// Writes code bytes over game code; false if the page can't be made writable.
static bool PatchCode(uintptr_t rva, const uint8_t* code, size_t size)
{
    uint8_t* at = (uint8_t*)(g_base + rva);
    DWORD old;
    if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(at, code, size);
    VirtualProtect(at, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, size);
    return true;
}

// wait_for_all_ready turns off the 15 s countdown, wait_for_all_ready_raid
// the 60 s one (each only when its key is on).
static bool WaitForAllReady()
{
    if (!CodeIs(kRoomTimerCheck, kRoomTimerCheckCode, sizeof(kRoomTimerCheckCode)) ||
        !CodeIs(kRoomShortTimerStart, kRoomShortTimerStartCode, sizeof(kRoomShortTimerStartCode)))
        return false;  // not decrypted yet
    if (g_cfg.waitForAllReady) {
        static const uint8_t skipStart[] = {0xEB, 0x0B, 0x90, 0x90, 0x90};  // jmp 0x5cfb79 (past the Start call)
        if (PatchCode(kRoomShortTimerStart, skipStart, sizeof(skipStart)))
            Log("room countdown (15 s): off, rooms wait until everyone is ready (%.0f ms after start)",
                MsSinceStart());
        else
            Log("room countdown (15 s): could not patch");
    }
    if (g_cfg.waitForAllReadyRaid) {
        // nop + jmp rel32: the jb's own rel32, one byte later, keeps the same target
        static const uint8_t alwaysSkip[] = {0x90, 0xE9};
        if (PatchCode(kRoomTimerCheck + 3, alwaysSkip, sizeof(alwaysSkip)))  // the 6-byte jb after the cmp
            Log("room countdown (60 s): off, rooms wait until everyone is ready (%.0f ms after start)",
                MsSinceStart());
        else
            Log("room countdown (60 s): could not patch");
    }
    return true;
}

// ------------------------------------------------- HUD transform stages ---
// The battle HUD's awoken skill panel names one stage (BHD_MET_<ID2>_<n>).
// fn 0x15e230 picks it for slot 8 at 0x160370-0x160436: the highest stage
// whose Ki requirement the character can afford now. Every x2m aura has
// KI_REQ 0, so an x2m transformation always names its top stage (New
// Transformations' Super Saiyan All: "Ultra Instinct" on every stage). The
// result (edi; -1 = nothing above the current stage) meets at 0x160436 on
// every path; a cave there replaces it, for skills whose CUS stages are all
// pre-baked (x2m), with the stage the skill button goes to now: the first
// transform entry of the skill's BCM (file order) above the current stage
// whose Ki/HP gates pass, -1 if none (hud_stages.h NextStageToName).
// Vanilla skills keep the game's choice. XV2Patcher
// patches 0x1603a1/0x1603b0 and 0x11ffc0, none of the bytes used here.
// Found by reverse engineering the game (Research_Notes, new-transformations.md "Which stage the HUD
// names").
static const uintptr_t kHudStagePicked = 0x160436;
static const uint8_t kHudStagePickedCode[] = {0xF6, 0x86, 0xB0, 0x00, 0x00, 0x00, 0x10,  // test byte [rsi+0xb0],0x10
                                              0x74, 0x1A,                                // je 0x160459
                                              0x41, 0x81, 0xFD, 0xAD, 0x61, 0x00, 0x00,  // cmp r13d,25005
                                              0x75, 0x11};                               // jne 0x160459
static const uintptr_t kHudStageAfterTest = 0x160459, kHudStageAfterCmp = 0x160446;
static const Sig kHudStageSigs[] = {
    {0x16004a, "44 0F BF 6B 08"},                 // r13d = the slot's skill id
    {0x160370, "0F BF 5B 3C 89 5D B0"},           // [rbp-0x50] = CUS Num_Of_Transforms
    {0x160377, "48 8B CE E8 41 FC FB FF"},        // rsi = the character: stage getter 0x11ffc0(rsi)
    {0xc8bb0, "8B 81 EC 22 00 00 C3"},            // current aura = [character+0x22ec]
};
static const uintptr_t kCharTransformFlags = 0xb0;  // byte; 0x10 = transformed (0x11ffc0, 0x160436)
static const uint8_t kCharTransformed = 0x10;
static const uintptr_t kCharAura = 0x22ec;          // cus aura id (getter 0xc8bb0; XV2Patcher's pre-baked ids)
// Float Ki and HP: 0xc66a0 compares a Ki cost <= [char+0x10c]; 0xcfdf0 /
// 0xcfe30 read current HP [+0x100] against max HP [+0x104]. Not yet
// confirmed on the HUD's character - the "hud stage" log line prints them.
static const uintptr_t kCharHealth = 0x100, kCharMaxHealth = 0x104, kCharKi = 0x10c;

static AuraStages g_auraStages;  // filled before the cave goes in, read-only after
static SRWLOCK g_hudLogLock = SRWLOCK_INIT;
static std::set<std::tuple<int, int, int>> g_hudLogged;

// Called from the cave on the battle thread for the awoken slot each frame:
// returns the stage to name (the game's own when not ours to change).
static int __fastcall HkHudStage(const uint8_t* character, int skillId, int gameStage, int stageCount)
{
    CharacterState state;
    state.transformed = (character[kCharTransformFlags] & kCharTransformed) != 0;
    state.aura = *(const int*)(character + kCharAura);
    state.ki = *(const float*)(character + kCharKi);
    float health = *(const float*)(character + kCharHealth), maxHealth = *(const float*)(character + kCharMaxHealth);
    // An unreadable ratio counts as full health (the HP-gated stages hidden).
    state.healthRatio = maxHealth > 0 && health >= 0 && health <= maxHealth ? health / maxHealth : 1.0f;
    int shown = gameStage;
    if (!NextStageToName(g_auraStages, skillId, stageCount, state, shown)) return gameStage;
    if (g_cfg.logging) {
        AcquireSRWLockExclusive(&g_hudLogLock);
        bool fresh = g_hudLogged.size() < 200 &&
                     g_hudLogged.insert({skillId, state.transformed ? state.aura : -1, shown}).second;
        ReleaseSRWLockExclusive(&g_hudLogLock);
        if (fresh) {
            char where[32] = "untransformed";
            if (state.transformed) sprintf_s(where, "aura 0x%x", state.aura);
            Log("hud stage: skill %d, %s, ki %.0f, hp %.0f/%.0f -> names stage %d (game picked %d)", skillId, where,
                state.ki, health, maxHealth, shown, gameStage);
        }
    }
    return shown;
}

// The transform entries of x2m awoken skill `id2`'s BCM, from its loose
// files: data\skill\MET\<ID2 4 digits>_<CHAR>_<CODE>\ (xv2ins installs x2m
// skills there) <folder>_PLAYER.bcm or <folder>.bcm, and the BAC beside it.
static bool LoadSkillChain(const std::wstring& data, int id2, std::vector<ChainEntry>& chain)
{
    wchar_t pattern[64];
    swprintf_s(pattern, L"\\skill\\MET\\%04d_*", id2);
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW((data + pattern).c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    std::wstring name;
    do {
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) name = found.cFileName;
    } while (name.empty() && FindNextFileW(search, &found));
    FindClose(search);
    if (name.empty()) return false;
    std::wstring base = data + L"\\skill\\MET\\" + name + L"\\" + name;
    std::string bcm, bac;
    if (!ReadWhole((base + L"_PLAYER.bcm").c_str(), bcm) && !ReadWhole((base + L".bcm").c_str(), bcm)) return false;
    if (!ReadWhole((base + L"_PLAYER.bac").c_str(), bac) && !ReadWhole((base + L".bac").c_str(), bac)) return false;
    return ParseBcmChain(bcm, bac, chain) && !chain.empty();
}

// pre-baked.xml from <game>\data (the exe is <game>\bin\DBXV2.exe), then
// each x2m awoken skill's BCM chain (a skill without one names stage + 1).
static bool LoadAuraStages()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    for (int i = 0; i < 2; ++i)
        if (wchar_t* slash = wcsrchr(path, L'\\')) *slash = 0;
    std::wstring data = std::wstring(path) + L"\\data";
    std::string xml;
    if (!ReadWhole((data + L"\\pre-baked.xml").c_str(), xml)) return false;
    int entries = ParseAuraStages(xml, g_auraStages);
    int chains = 0;
    for (const auto& skill : g_auraStages.stageCount) {
        std::vector<ChainEntry> chain;
        if (!LoadSkillChain(data, skill.first, chain)) continue;
        g_auraStages.chain[skill.first] = chain;
        ++chains;
    }
    Log("hud stage names: pre-baked.xml has %d stages of %d x2m awoken skills, %d with a BCM chain", entries,
        (int)g_auraStages.stageCount.size(), chains);
    return entries > 0;
}

// Returns true once done (or impossible).
static bool HudStageNames()
{
    if (!CodeIs(kHudStagePicked, kHudStagePickedCode, sizeof(kHudStagePickedCode))) return false;
    for (const Sig& sig : kHudStageSigs)
        if (!Matches(sig)) return false;  // not decrypted yet
    if (!LoadAuraStages()) {
        Log("hud stage names: no pre-baked.xml stages (no x2m transformations) - left as is");
        return true;
    }
    uint8_t* site = (uint8_t*)(g_base + kHudStagePicked);
    uint8_t* cave = AllocNear((uintptr_t)site, 128);
    if (!cave) {
        Log("hud stage names: no memory for the cave");
        return true;
    }
    // Volatile registers saved (7 pushes + 0x28 keep rsp 16-byte aligned),
    // edi = HkHudStage(rsi, r13d, edi, [rbp-0x50]), then the replaced test/je/cmp and
    // jumps back.
    uint8_t* at = cave;
    auto emit = [&](std::initializer_list<uint8_t> bytes) { for (uint8_t b : bytes) *at++ = b; };
    emit({0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});  // push rax rcx rdx r8-r11
    emit({0x48, 0x83, 0xEC, 0x28});                                            // sub rsp,0x28
    emit({0x48, 0x89, 0xF1});                                                  // mov rcx,rsi
    emit({0x44, 0x89, 0xEA});                                                  // mov edx,r13d
    emit({0x41, 0x89, 0xF8});                                                  // mov r8d,edi
    emit({0x44, 0x8B, 0x4D, 0xB0});                                            // mov r9d,[rbp-0x50]
    emit({0x48, 0xB8});                                                        // mov rax,HkHudStage
    uintptr_t hook = (uintptr_t)&HkHudStage;
    memcpy(at, &hook, 8); at += 8;
    emit({0xFF, 0xD0});                                                        // call rax
    emit({0x48, 0x83, 0xC4, 0x28});                                            // add rsp,0x28
    emit({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59});        // pop r11-r8 rdx rcx
    emit({0x89, 0xC7});                                                        // mov edi,eax
    emit({0x58});                                                              // pop rax
    emit({0xF6, 0x86, 0xB0, 0x00, 0x00, 0x00, 0x10});                          // test byte [rsi+0xb0],0x10
    emit({0x75, 0x0E});                                                        // jne (past the next jmp)
    AbsJmp(at, g_base + kHudStageAfterTest); at += 14;
    emit({0x41, 0x81, 0xFD, 0xAD, 0x61, 0x00, 0x00});                          // cmp r13d,25005
    AbsJmp(at, g_base + kHudStageAfterCmp);
    // Site: jmp cave, then nops up to 0x160446 (never reached).
    uint8_t code[16];
    memset(code, 0x90, sizeof(code));
    code[0] = 0xE9;
    int32_t rel = (int32_t)((intptr_t)cave - ((intptr_t)site + 5));
    memcpy(code + 1, &rel, 4);
    if (PatchCode(kHudStagePicked, code, sizeof(code)))
        Log("hud stage names: the HUD names the next stage (%.0f ms after start)", MsSinceStart());
    else
        Log("hud stage names: could not patch");
    return true;
}

// ---------------------------------------------------------------- flight ---
// 0x84b380 = "can fly": SYSFLAG 16 UNLOCK_Buku || 0x826bd0(account manager,
// 2). Its only caller (0x4b046c, lobby player update 0x4b03a0) sets bit
// 0x1000 "flight locked" in [player+0x3c8] when it returns 0. It becomes
// "mov eax,1; ret", so the save's flags stay as they are. Found by reverse
// engineering the game (Research_Notes, save-flags-and-unlocks.md "Flight").
static const uintptr_t kCanFly = 0x84b380;
static const uint8_t kCanFlyCode[] = {0x48, 0x89, 0x5C, 0x24, 0x10,  // mov [rsp+0x10],rbx
                                      0x57,                          // push rdi
                                      0x48, 0x83, 0xEC, 0x20};       // sub rsp,0x20

// Returns true once done (or impossible).
static bool FlightAlwaysAllowed()
{
    if (!CodeIs(kCanFly, kCanFlyCode, sizeof(kCanFlyCode))) return false;  // not decrypted yet
    static const uint8_t yes[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};  // mov eax,1; ret
    if (PatchCode(kCanFly, yes, sizeof(yes)))
        Log("flight: always allowed (%.0f ms after start)", MsSinceStart());
    else
        Log("flight: could not patch");
    return true;
}

// ---------------------------------------------------------- dragon balls ---
// 0x848bf0(type, id, flags) = how many of an item the loaded character holds
// (types 0..9: the inventory list's count, 0x82d340). The Shenron script
// (dragon_ball.tsr CALL_DRAGON: ITEM(7,1..7)>=1, expression handler call at
// 0x3c9d54) and the wish (0x4f1db0) both ask it. The wish counts the balls
// held, reports 7 - held to the network/telemetry side as balls bought with
// STP medals (event 0x31, 0x6a2730 -> 0x72b1c0), then takes balls 1..7
// (0x847d30 -> remove 0x82cf70, which just returns 0 for a ball not held).
// So the count, not the inventory, is changed: balls 1..7 (type 7
// "important", one each) always count 1 - the wish reports 0 bought, like a
// normal wish, and nothing is given or written to the save. Found by reverse
// engineering the game (Research_Notes, save-flags-and-unlocks.md "Dragon
// Balls").
static const uintptr_t kItemCount = 0x848bf0;
static const uint8_t kItemCountCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08,   // mov [rsp+8],rbx
                                         0x48, 0x89, 0x74, 0x24, 0x10};  // mov [rsp+0x10],rsi
static const int kImportantItemType = 7, kFirstDragonBall = 1, kLastDragonBall = 7;
typedef int(__fastcall* ItemCountFn)(int type, int id, int flags);
static ItemCountFn g_origItemCount;

static int __fastcall HkItemCount(int type, int id, int flags)
{
    if (type == kImportantItemType && id >= kFirstDragonBall && id <= kLastDragonBall) return 1;
    return g_origItemCount(type, id, flags);
}

// Returns true once done (or impossible).
static bool AllDragonBalls()
{
    if (!CodeIs(kItemCount, kItemCountCode, sizeof(kItemCountCode))) return false;  // not decrypted yet
    // The first instruction (5 bytes) is replayed in the trampoline.
    g_origItemCount = (ItemCountFn)HookCode((uint8_t*)(g_base + kItemCount), 5, (void*)&HkItemCount);
    Log(g_origItemCount ? "dragon balls: all seven always held (%.0f ms after start)"
                        : "dragon balls: could not hook (%.0f ms after start)", MsSinceStart());
    return true;
}

// Lobby flight speed. PhysChar@Lobby's step (0x4b3c60, rbx = the PhysChar,
// [rbx+0x10] = its character) turns the step's move delta into a velocity:
// delta * (1/dt) * xmm10 (0x4b3e84). xmm10 is 1, or 0.025 when |delta|^2 >
// 1.5 (a teleport guard, 0x4b3d51); every path meets at 0x4b3d63. A cave
// there multiplies xmm10 for the local lobby player only ([exe+0x15712f8] =
// CLobbyPlayer, +0x5b0 = its PhysChar) while airborne (character +0x3b4
// bits 0xc, set by 0x4868b0 above 0.5 height). Scaling after the guard keeps
// the guard from seeing the faster speed (Revamp's x1.4 lobby.bac boost
// stalls every few seconds). Battle uses another class. Found by reverse
// engineering the game (Research_Notes, lobby-physics-and-debug-menu.md
// "Flight speed").
static const uintptr_t kLobbyStepJoin = 0x4b3d63, kLobbyStepAfterJoin = 0x4b3d6c;
static const uint8_t kLobbyStepJoinCode[] = {0x48, 0x8B, 0x4B, 0x10,         // mov rcx,[rbx+0x10]
                                             0xBF, 0x01, 0x00, 0x00, 0x00};  // mov edi,1
static const uintptr_t kLobbyPlayer = 0x15712f8;   // CLobbyPlayer*
static const int32_t kLobbyPlayerPhysics = 0x5b0;  // CLobbyPlayer +0x5b0 = PhysChar@Lobby*
static const Sig kLobbyStepSigs[] = {
    {0x4b3d5a, "F3 44 0F 10 15"},  // movss xmm10,[0.025] (the guard)
    {0x4b3d76, "8B 83 F0 00 00 00"},  // mov eax,[rbx+0xf0]: rax is free in the cave
};

// Returns true once done (or impossible).
static bool LobbyFlightSpeed()
{
    if (!CodeIs(kLobbyStepJoin, kLobbyStepJoinCode, sizeof(kLobbyStepJoinCode))) return false;
    for (const Sig& sig : kLobbyStepSigs)
        if (!Matches(sig)) return false;  // not decrypted yet
    uint8_t* site = (uint8_t*)(g_base + kLobbyStepJoin);
    uint8_t* cave = AllocNear((uintptr_t)site, 96);
    if (!cave) {
        Log("lobby flight speed: no memory for the cave");
        return true;
    }
    // Jump distances below are counted from the instruction lengths.
    uint8_t* at = cave;
    auto emit = [&](std::initializer_list<uint8_t> bytes) { for (uint8_t b : bytes) *at++ = b; };
    emit({0x48, 0xB8});                                          // mov rax,&CLobbyPlayer*
    uintptr_t player = g_base + kLobbyPlayer;
    memcpy(at, &player, 8); at += 8;
    emit({0x48, 0x8B, 0x00});                                    // mov rax,[rax]
    emit({0x48, 0x85, 0xC0});                                    // test rax,rax
    emit({0x74, 31});                                            // je skip
    emit({0x48, 0x3B, 0x98});                                    // cmp rbx,[rax+0x5b0]
    memcpy(at, &kLobbyPlayerPhysics, 4); at += 4;
    emit({0x75, 22});                                            // jne skip
    emit({0x48, 0x8B, 0x4B, 0x10});                              // mov rcx,[rbx+0x10]
    emit({0xF6, 0x81, 0xB4, 0x03, 0x00, 0x00, 0x0C});            // test byte [rcx+0x3b4],0xc (airborne)
    emit({0x74, 9});                                             // je skip
    emit({0xF3, 0x44, 0x0F, 0x59, 0x15, 23, 0x00, 0x00, 0x00});  // mulss xmm10,[multiplier]
    // skip: the replaced instructions, then back.
    emit({0x48, 0x8B, 0x4B, 0x10});                              // mov rcx,[rbx+0x10]
    emit({0xBF, 0x01, 0x00, 0x00, 0x00});                        // mov edi,1
    AbsJmp(at, g_base + kLobbyStepAfterJoin); at += 14;
    memcpy(at, &g_cfg.lobbyFlightSpeed, 4);                      // multiplier
    // Site: jmp cave + 4 nops (never reached).
    uint8_t code[9] = {0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90};
    int32_t rel = (int32_t)((intptr_t)cave - ((intptr_t)site + 5));
    memcpy(code + 1, &rel, 4);
    if (PatchCode(kLobbyStepJoin, code, sizeof(code)))
        Log("lobby flight speed: x%.2f (%.0f ms after start)", g_cfg.lobbyFlightSpeed, MsSinceStart());
    else
        Log("lobby flight speed: could not patch");
    return true;
}

static bool ApplyTitleVolume()
{
    uint8_t* snd = *(uint8_t**)(g_base + kSound);
    if (!snd || !*(volatile int*)(snd + 0x10) || *(void* volatile*)(snd + 0x18bc8)) return false;
    if (!CodeIs(kSetCategoryVolume, kSetCategoryVolumeCode, sizeof(kSetCategoryVolumeCode)) ||
        memcmp((const void*)(g_base + kVolumeTable), kVolumeLevels, sizeof(kVolumeLevels))) {
        Log("title volume: unexpected code - left as is");
        return true;
    }
    auto setVolume = (SetCategoryVolumeFn)(g_base + kSetCategoryVolume);
    AcquireSRWLockShared(&g_savedLock);
    Saved s = g_saved;
    ReleaseSRWLockShared(&g_savedLock);
    if (s.volume[0] < 0) {
        setVolume(snd, kVolumeCategory[0], 0.0f);
        Log("title volume: the save's volume options are not known yet (they are remembered once a character "
            "loads) - music muted (%.0f ms after start)", MsSinceStart());
        return true;
    }
    for (int i = 0; i < 3; ++i)
        if (s.volume[i] >= 0) setVolume(snd, kVolumeCategory[i], kVolumeLevels[s.volume[i]]);
    Log("title volume: options %d %d %d from last time (%.0f ms after start)", s.volume[0], s.volume[1],
        s.volume[2], MsSinceStart());
    return true;
}

// Polls every 10 ms, for at most 60 s, until all are done.
static void BootTweaks()
{
    bool logos = !g_cfg.skipLogos, intro = !g_cfg.skipIntro, volume = !g_cfg.titleVolume, tv = !g_cfg.fixTvLobbyLoop;
    bool conditions = !g_cfg.revealConditions, room = !g_cfg.waitForAllReady && !g_cfg.waitForAllReadyRaid;
    bool hud = !g_cfg.hudNextStage, fly = !g_cfg.flight, flySpeed = g_cfg.lobbyFlightSpeed == 1.0f;
    bool dragonBalls = !g_cfg.allDragonBalls;
    auto allDone = [&] { return logos && intro && volume && tv && conditions && room && hud && fly && flySpeed && dragonBalls; };
    for (int i = 0; i < 6000 && !allDone(); ++i) {
        if (!logos) logos = SkipLogos();
        if (!intro) intro = SkipIntro();
        if (!volume) volume = ApplyTitleVolume();
        if (!tv) tv = FixTvLobbyLoop();
        if (!conditions) conditions = RevealConditions();
        if (!room) room = WaitForAllReady();
        if (!hud) hud = HudStageNames();
        if (!fly) fly = FlightAlwaysAllowed();
        if (!flySpeed) flySpeed = LobbyFlightSpeed();
        if (!dragonBalls) dragonBalls = AllDragonBalls();
        if (!allDone()) Sleep(10);
    }
    if (!logos) Log("logos: code never matched - left as is");
    if (!intro) Log("intro: code never matched - left as is");
    if (!volume) Log("title volume: sound system never became ready - left as is");
    if (!tv) Log("flag fixer, Conton TV lobby loop: code never matched - left as is");
    if (!conditions) Log("hidden quest conditions: code never matched - left as is");
    if (!room) Log("room countdown: code never matched - left as is");
    if (!hud) Log("hud stage names: code never matched - left as is");
    if (!fly) Log("flight: code never matched - left as is");
    if (!flySpeed) Log("lobby flight speed: code never matched - left as is");
    if (!dragonBalls) Log("dragon balls: code never matched - left as is");
}

// Options are bytes at manager + 8 + 0x8c + 4 + id (getter 0x8247f0 ->
// 0x829750); the volume ones are levels 0..5. At the title they hold the
// defaults (5) and the character slot already reads as valid (seen in game),
// so they count as the save's only once the sound system plays at exactly
// those levels: the game has applied them (on Start), and the title's own
// volumes (ApplyTitleVolume) are gone. Remembered for the next title screen.
static const uintptr_t kOptions = 8 + 0x8c + 4;
static const uintptr_t kCategoryVolumes = 0x1a404;  // floats in Sound::System, one per category

static void RememberVolume()
{
    uint8_t* mgr = *(uint8_t**)(g_base + kQuestData);
    uint8_t* snd = *(uint8_t**)(g_base + kSound);
    if (!mgr || !snd) return;
    const uint8_t* opt = mgr + kOptions;
    const volatile float* cat = (const volatile float*)(snd + kCategoryVolumes);
    for (int i = 0; i < 3; ++i) {
        int v = opt[kVolumeOptions[i]];
        if (v > 5 || cat[kVolumeCategory[i]] != kVolumeLevels[v]) return;
    }
    bool changed = false;
    AcquireSRWLockExclusive(&g_savedLock);
    for (int i = 0; i < 3; ++i) {
        int v = opt[kVolumeOptions[i]];
        if (v != g_saved.volume[i]) {
            g_saved.volume[i] = v;
            changed = true;
        }
    }
    Saved s = g_saved;
    ReleaseSRWLockExclusive(&g_savedLock);
    if (changed) {
        Log("volume options from the save: %d %d %d (remembered for the title screen)", s.volume[0], s.volume[1],
            s.volume[2]);
        WriteSaved();
    }
}

// --------------------------------------------------------------- camera ---
// The renderer (render_pass_x64.dll, Scene) draws with its own camera object.
// The game hands its current camera to Scene::SetMainCamera, which copies
// position, interest, up, roll, fov, clip and aspect into the scene's camera
// and calculates it (0x3f6f0 in the DLL); Scene::SetMainPrevCamera does the
// same for the previous frame's camera (motion blur). The mod hooks both in
// DBXV2's import table and, after the copy, moves the scene camera's position
// along the line to its interest point by the mode's zoom.
//
// The game itself also looks through the camera it handed over: the lobby
// culls characters with that camera's view and projection (0x3c3070), battle
// places the name plates and the lock-on marker with them (0xba15e). So that
// camera's view matrix is zoomed too: xgcore's EmCamera::calcViewMatrix is
// hooked and, for that camera only, computes the view from a zoomed copy of
// its position. Its stored position stays as the game set it, so no camera
// logic that reads it back ever sees the zoom (nothing feeds back).
//
// The mode is the call site (return address):
//   battle    0x15d650 / 0x166470 / 0x16cf10 (Battle::CameraManager's current
//             camera, 0x156b408), 0xe4080 (Mob)
//   lobby     0x3f4db0 (Lobby JobCallbackUpdateTail) and 0x457250
//             (MatchingLobby): Lobby::CameraManager (0x15712e8) +0x20
//   menu      0x665bf0 (Menu::Camera: character select / creation)
//   cutscene  0x2b05a0 .. 0x2b2370 and 0x3759f0 / 0x377540 (demo playback)
// Not zoomed: the loading screens (LoadQuest, LoadVs, LoadVsGBB,
// TtlLoadVsCore) and Menu::Camera's restore of a saved scene camera (0x6648c0).
struct CallSite { uintptr_t ret; CamMode mode; };
static const CallSite kMainSites[] = {
    {0x15d73c, kModeBattle}, {0x1664d9, kModeBattle}, {0x16cf6f, kModeBattle}, {0xe41d1, kModeBattle},
    {0x3f4f05, kModeLobby}, {0x4573a9, kModeLobby},
    {0x665c9a, kModeMenu},
    {0x2b0bb3, kModeCutscene}, {0x2b14b4, kModeCutscene}, {0x2b1f64, kModeCutscene}, {0x2b25d4, kModeCutscene},
    {0x375aa3, kModeCutscene}, {0x37758a, kModeCutscene},
};
static const CallSite kPrevSites[] = {
    {0x15d753, kModeBattle}, {0x1664ef, kModeBattle}, {0x3f4eb4, kModeLobby}, {0x457354, kModeLobby},
    {0x665c86, kModeMenu},
};
static const uintptr_t kIatSetMain = 0xf4d650, kIatSetPrev = 0xf4d700;  // DBXV2's import slots
// xgcore.dll EmCamera::calcViewMatrix (0xf650): LookAt(position +0x90,
// interest +0xa0, up +0xb0 or a default) + roll +0xc0 -> view matrix +0x10.
static const char kCalcViewName[] = "?calcViewMatrix@EmCamera@Eva@XG@@IEBAXXZ";
static const uint8_t kCalcViewCode[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x4C, 0x8D, 0x81, 0xA0, 0x00, 0x00, 0x00};

typedef void(__fastcall* SetSceneCamFn)(void* scene, const void* cam);
typedef void*(__fastcall* GetSceneCamFn)(const void* scene);
typedef int(__fastcall* SceneFlagFn)(const void* scene);
typedef const float*(__fastcall* CamVecFn)(const void* cam);
typedef void(__fastcall* CamSetVecFn)(void* cam, const float* v);
typedef void(__fastcall* CamFn)(void* cam);
static SetSceneCamFn g_origSetMain = nullptr, g_origSetPrev = nullptr;
static GetSceneCamFn g_sceneMain = nullptr, g_scenePrev = nullptr;
static SceneFlagFn   g_prevManaged = nullptr;
static CamVecFn      g_camPos = nullptr, g_camInterest = nullptr;
static CamSetVecFn   g_camSetPos = nullptr;
static CamFn         g_camCalculate = nullptr;
static CamFn         g_origCalcView = nullptr;
static volatile LONG      g_camMode = kModeNone;   // mode of the last SetMainCamera call
static volatile ULONGLONG g_camModeTick = 0;       // and when
static void* volatile     g_camSource = nullptr;   // the game camera it was given

static CamMode ModeOf(const CallSite* sites, size_t n, uintptr_t ret)
{
    for (size_t i = 0; i < n; ++i)
        if (sites[i].ret == ret) return sites[i].mode;
    return kModeNone;
}

// Position = interest + (position - interest) * zoom, then recalculated.
static void Zoom(void* cam, float zoom)
{
    if (zoom > 0.999f && zoom < 1.001f) return;
    const float* pos = g_camPos(cam);
    const float* at = g_camInterest(cam);
    alignas(16) float p[4];
    for (int i = 0; i < 3; ++i) p[i] = at[i] + (pos[i] - at[i]) * zoom;
    p[3] = pos[3];
    g_camSetPos(cam, p);
    g_camCalculate(cam);
}

// The view matrix of the game's camera, from a copy of the camera whose
// position is zoomed: the camera object itself is only given the matrix.
static void __fastcall HkCalcViewMatrix(void* cam)
{
    LONG mode = g_camMode;
    float zoom = mode != kModeNone && cam == g_camSource ? g_saved.zoom[mode] : 1.0f;
    if (zoom > 0.999f && zoom < 1.001f) {
        g_origCalcView(cam);
        return;
    }
    alignas(16) uint8_t copy[0xd0];  // everything calcViewMatrix reads (up to +0xc4)
    memcpy(copy, cam, sizeof(copy));
    float* pos = (float*)(copy + 0x90);
    const float* at = (const float*)(copy + 0xa0);
    for (int i = 0; i < 3; ++i) pos[i] = at[i] + (pos[i] - at[i]) * zoom;
    g_origCalcView(copy);
    memcpy((uint8_t*)cam + 0x10, copy + 0x10, 0x40);
}

// Map object culling, run with the game's camera, also reads its position:
// * 0x33b610 (once a frame, battle and lobby) builds world-space planes from
//   the position, frustum and view, so with only the view zoomed it culled
//   props near the screen edges;
// * 0x31dd70 (each draw list, from 0x338e20) hides objects behind the plane
//   through the position, facing the interest - props near the camera at
//   the screen edges are behind the game's position but in front of the
//   zoomed one.
// Given the zoomed camera, both get a copy of it with the position zoomed
// instead. Neither keeps a pointer to the camera.
static const uintptr_t kCull = 0x33b610, kCullBehind = 0x31dd70;
static const uint8_t kCullCode[] = {0x48, 0x8B, 0xC4, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
static const uint8_t kCullBehindCode[] = {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20, 0x41, 0x56};
typedef uint64_t(__fastcall* CullFn)(void* cam, float f, void* r8, void* r9);
typedef uint64_t(__fastcall* CullBehindFn)(void* list, void* cam, float f, int64_t r9);
static CullFn       g_origCull = nullptr;
static CullBehindFn g_origCullBehind = nullptr;

// Fills `copy` (an EmCamera is 0x100 bytes, see Instantiate) with the camera,
// position zoomed, if it is the zoomed one; false otherwise.
static bool ZoomedCopy(const void* cam, uint8_t (&copy)[0x100])
{
    LONG mode = g_camMode;
    float zoom = mode != kModeNone && cam && cam == g_camSource ? g_saved.zoom[mode] : 1.0f;
    if (zoom > 0.999f && zoom < 1.001f) return false;
    memcpy(copy, cam, sizeof(copy));
    float* pos = (float*)(copy + 0x90);
    const float* at = (const float*)(copy + 0xa0);
    for (int i = 0; i < 3; ++i) pos[i] = at[i] + (pos[i] - at[i]) * zoom;
    *(uint32_t*)(copy + 0xec) |= 1;  // view recalculated from that position
    return true;
}

static uint64_t __fastcall HkCull(void* cam, float f, void* r8, void* r9)
{
    alignas(16) uint8_t copy[0x100];
    return g_origCull(ZoomedCopy(cam, copy) ? copy : cam, f, r8, r9);
}
static uint64_t __fastcall HkCullBehind(void* list, void* cam, float f, int64_t r9)
{
    alignas(16) uint8_t copy[0x100];
    return g_origCullBehind(list, ZoomedCopy(cam, copy) ? copy : cam, f, r9);
}
// A camera copied onto itself (the menu restoring one) is never zoomed again.
static void __fastcall HkSetMainCamera(void* scene, const void* cam)
{
    uintptr_t ret = (uintptr_t)_ReturnAddress() - g_base;
    CamMode mode = ModeOf(kMainSites, _countof(kMainSites), ret);
    void* own = g_sceneMain(scene);
    g_origSetMain(scene, cam);
    // No log here: the lobby and menu cameras alternate every frame.
    g_camSource = mode != kModeNone ? (void*)cam : nullptr;
    g_camMode = mode;
    g_camModeTick = GetTickCount64();
    if (mode != kModeNone && cam && own && own != cam) Zoom(own, g_saved.zoom[mode]);
}

static void __fastcall HkSetMainPrevCamera(void* scene, const void* cam)
{
    CamMode mode = ModeOf(kPrevSites, _countof(kPrevSites), (uintptr_t)_ReturnAddress() - g_base);
    void* own = g_scenePrev(scene);
    bool copies = !g_prevManaged(scene);  // otherwise the scene keeps its own previous camera
    g_origSetPrev(scene, cam);
    if (mode != kModeNone && copies && cam && own && own != cam) Zoom(own, g_saved.zoom[mode]);
}

// Replaces one import slot, once the loader (after Steam's DRM) has filled it.
static bool HookImport(uintptr_t slotRva, void* expect, void* hook, void** orig)
{
    void* volatile* slot = (void* volatile*)(g_base + slotRva);
    for (int i = 0; i < 18000 && *slot != expect; ++i) Sleep(10);
    if (*slot != expect) return false;
    DWORD old;
    if (!VirtualProtect((void*)slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = expect;
    InterlockedExchangePointer((void* volatile*)slot, hook);
    VirtualProtect((void*)slot, sizeof(void*), old, &old);
    return true;
}

// Every call site is an indirect call through its import slot (false until
// Steam's DRM has decrypted the code).
static bool CallSitesMatch()
{
    auto calls = [](const CallSite* sites, size_t n, uintptr_t slot) {
        for (size_t i = 0; i < n; ++i) {
            const uint8_t* p = (const uint8_t*)(g_base + sites[i].ret - 6);
            if (p[0] != 0xFF || p[1] != 0x15 || sites[i].ret + *(const int32_t*)(p + 2) != slot) return false;
        }
        return true;
    };
    return calls(kMainSites, _countof(kMainSites), kIatSetMain) && calls(kPrevSites, _countof(kPrevSites), kIatSetPrev);
}

static bool InstallCameraHooks()
{
    HMODULE rp = GetModuleHandleW(L"render_pass_x64.dll"), xg = GetModuleHandleW(L"xgcore.dll");
    if (!rp || !xg) return false;
    void* setMain = (void*)GetProcAddress(rp, "?SetMainCamera@Scene@Common@Game@XG@@QEAAXPEBVEmCamera@Eva@4@@Z");
    void* setPrev = (void*)GetProcAddress(rp, "?SetMainPrevCamera@Scene@Common@Game@XG@@QEAAXPEBVEmCamera@Eva@4@@Z");
    g_sceneMain = (GetSceneCamFn)GetProcAddress(rp, "?GetMainCamera@Scene@Common@Game@XG@@QEBAPEBVEmCamera@Eva@4@XZ");
    g_scenePrev = (GetSceneCamFn)GetProcAddress(rp, "?GetMainPrevCamera@Scene@Common@Game@XG@@QEBAPEBVEmCamera@Eva@4@XZ");
    g_prevManaged = (SceneFlagFn)GetProcAddress(rp, "?IsPrevCameraManaged@Scene@Common@Game@XG@@QEBAHXZ");
    g_camPos = (CamVecFn)GetProcAddress(xg, "?GetPosition@EmCamera@Eva@XG@@QEBAAEBUEmVector@23@XZ");
    g_camInterest = (CamVecFn)GetProcAddress(xg, "?GetInterest@EmCamera@Eva@XG@@QEBAAEBUEmVector@23@XZ");
    g_camSetPos = (CamSetVecFn)GetProcAddress(xg, "?SetPosition@EmCamera@Eva@XG@@QEAAXAEBUEmVector@23@@Z");
    g_camCalculate = (CamFn)GetProcAddress(xg, "?Calculate@EmCamera@Eva@XG@@QEAAXXZ");
    uint8_t* calcView = (uint8_t*)GetProcAddress(xg, kCalcViewName);
    if (!setMain || !setPrev || !g_sceneMain || !g_scenePrev || !g_prevManaged || !g_camPos || !g_camInterest ||
        !g_camSetPos || !g_camCalculate || !calcView || ((uintptr_t)calcView & 7) ||
        memcmp(calcView, kCalcViewCode, sizeof(kCalcViewCode)) || !CodeIs(kCull, kCullCode, sizeof(kCullCode)) ||
        !CodeIs(kCullBehind, kCullBehindCode, sizeof(kCullBehindCode)))
        return false;
    // The trampoline replays push rbx; sub rsp,0x40 (6 bytes).
    g_origCalcView = (CamFn)HookCode(calcView, 6, (void*)&HkCalcViewMatrix);
    if (!g_origCalcView) return false;
    // The map object culling: mov rax,rsp; push rbp; push r12 (6 bytes).
    g_origCull = (CullFn)HookCode((uint8_t*)(g_base + kCull), 6, (void*)&HkCull);
    if (!g_origCull) return false;
    // The behind-the-camera test: mov [rsp+0x18],rbx (5 bytes).
    g_origCullBehind = (CullBehindFn)HookCode((uint8_t*)(g_base + kCullBehind), 5, (void*)&HkCullBehind);
    if (!g_origCullBehind) return false;    // The previous camera first: zooming only the current one would look like
    // a jump to the motion blur.
    return HookImport(kIatSetPrev, setPrev, (void*)&HkSetMainPrevCamera, (void**)&g_origSetPrev) &&
           HookImport(kIatSetMain, setMain, (void*)&HkSetMainCamera, (void**)&g_origSetMain);
}

static bool GameInFront()
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}
static bool AnyDown(const std::vector<int>& keys)
{
    for (int vk : keys)
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    return false;
}

// The zoom-in keys (default + on the main keyboard or keypad) bring the camera
// closer, the zoom-out keys (-) move it away, the reset keys (Backspace) put
// it back to 1, for the mode on screen (one that set the camera in the last
// half second). Checks the keys every 16 ms; the new zoom is written to the
// saved file when the keys are let go.
static DWORD WINAPI CameraThread(LPVOID)
{
    bool sites = false;
    for (int i = 0; i < 18000 && !sites; ++i) {  // up to 3 minutes
        sites = CallSitesMatch();
        if (!sites) Sleep(10);
    }
    if (!sites || !InstallCameraHooks()) {
        Log("camera zoom: NOT installed (%.0f ms): %s", MsSinceStart(),
            sites ? "render_pass_x64.dll / xgcore.dll not as expected" : "code does not match game 1.26.01");
        return 0;
    }
    AcquireSRWLockExclusive(&g_savedLock);
    for (float& z : g_saved.zoom) z = min(max(z, g_cfg.zoomMin), g_cfg.zoomMax);
    ReleaseSRWLockExclusive(&g_savedLock);
    Log("camera zoom: installed at %.0f ms (zoom battle %.2f, lobby %.2f, menu %.2f, cutscene %.2f)", MsSinceStart(),
        g_saved.zoom[0], g_saved.zoom[1], g_saved.zoom[2], g_saved.zoom[3]);
    const float rate = logf(g_cfg.zoomSpeed);
    ULONGLONG last = GetTickCount64();
    int changed = kModeNone;
    for (;;) {
        Sleep(16);
        ULONGLONG now = GetTickCount64();
        float dt = min((now - last) / 1000.0f, 0.1f);
        last = now;
        int dir = 0;
        bool reset = false;
        if (GameInFront()) {

            if (AnyDown(g_cfg.zoomInKeys)) --dir;
            if (AnyDown(g_cfg.zoomOutKeys)) ++dir;
            reset = AnyDown(g_cfg.zoomResetKeys);
        }
        LONG mode = g_camMode;
        if ((dir || reset) && mode != kModeNone && now - g_camModeTick < 500) {
            AcquireSRWLockExclusive(&g_savedLock);
            float z = reset ? 1.0f : g_saved.zoom[mode] * expf(rate * dt * dir);
            g_saved.zoom[mode] = min(max(z, g_cfg.zoomMin), g_cfg.zoomMax);
            ReleaseSRWLockExclusive(&g_savedLock);
            changed = mode;
        } else if (!dir && !reset && changed != kModeNone) {
            Log("camera zoom: %s %.2f", kModeNames[changed], g_saved.zoom[changed]);
            WriteSaved();
            changed = kModeNone;
        }
    }
}

// --------------------------------------------------------------- thread ---
static double MsSinceStart()
{
    FILETIME c, e, k, u, now;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER a{c.dwLowDateTime, c.dwHighDateTime}, b{now.dwLowDateTime, now.dwHighDateTime};
    return (double)(b.QuadPart - a.QuadPart) / 10000.0;
}

static DWORD WINAPI ThreadMain(LPVOID)
{
    EnsureConfig();
    LoadConfig();
    Log("---- General QoL v" MOD_VERSION " starting,%.0f ms after the game started", MsSinceStart());
    for (const std::string& e : g_configErrors) Log("config: %s", e.c_str());
    if (!g_cfg.enabled) { Log("disabled in the config"); return 0; }
    LoadSaved();
    g_base = (uintptr_t)GetModuleHandleW(nullptr);

    bool known = false;
    for (int i = 0; i < 2000 && !known; ++i) {
        known = IsGame12601();
        if (!known) Sleep(5);
    }
    if (!known) { Log("unknown game version - nothing is changed"); return 0; }

    if (g_cfg.teleports || g_cfg.storyFlags || g_cfg.parallelQuests || g_cfg.expertMissions ||
        g_cfg.tpShop || g_cfg.logFiles || g_cfg.npc.enabled || g_cfg.mentorNames) {
        bool ok = InstallFileHook();
        Log(ok ? "file hook installed at %.0f ms" : "file hook NOT installed (%.0f ms)", MsSinceStart());
    }
    if (g_cfg.cameraZoom)
        if (HANDLE t = CreateThread(nullptr, 0, CameraThread, nullptr, 0, nullptr)) CloseHandle(t);
    if (g_cfg.skipLogos || g_cfg.skipIntro || g_cfg.titleVolume || g_cfg.fixTvLobbyLoop ||
        g_cfg.revealConditions || g_cfg.waitForAllReady || g_cfg.waitForAllReadyRaid || g_cfg.hudNextStage ||
        g_cfg.flight || g_cfg.lobbyFlightSpeed != 1.0f || g_cfg.allDragonBalls)
        BootTweaks();
    if (g_cfg.npc.enabled) InstallReplayMenu();
    if (g_cfg.mentorNames) InstallMapNames();
    bool flags = g_cfg.teleports || g_cfg.storyFlags || g_cfg.expertMissions || g_cfg.levelCap ||
                 g_cfg.raidCharacters;
    if (flags || g_cfg.titleVolume) {
        Log("flag / volume option check on (every 2 s)");
        for (;;) {
            if (flags) UpdateFlags();
            if (g_cfg.titleVolume) RememberVolume();
            Sleep(2000);
        }
    }
    return 0;
}

void Mod_Start(HMODULE self)
{
    GetModuleFileNameW(self, g_dir, MAX_PATH);  // ...\mods\General_QoL\General_QoL.dll
    if (wchar_t* slash = wcsrchr(g_dir, L'\\')) *slash = 0;
    if (HANDLE t = CreateThread(nullptr, 0, ThreadMain, nullptr, 0, nullptr)) CloseHandle(t);
}

void Mod_Stop()
{
    // Nothing to undo: this only runs as the game exits.
}

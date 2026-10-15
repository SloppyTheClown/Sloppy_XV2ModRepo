// ---------------------------------------------------------------------------
//  Battle Styles - mod core
//
//  Phase 1 (test build): every created character (CaC, chara ids 100..107)
//  borrows the basic moveset - animations, BAC, BCM, BDM, camera - of one
//  roster character named in the config ("test_style": its 3-letter CMS
//  code, e.g. "GOK"). Skills are untouched: they live in their own files.
//
//  How: the game asks one function, the CMS path getter (0x2c5510), for every
//  character file path ("chara/HUM/HUM.ean", "chara/HUM/HUM_PLAYER.bac", ...).
//  For a CaC and one of the moveset fields it is asked again with the style
//  character's id instead.
//
//  Phase 3/4: the style comes from the "Battle Style" equip slot
//  (style_menu.cpp), stored per save character slot. In battle each player's
//  CaC wears the style of the save slot it came from (the battle player
//  loader hook below says which player a request is for), and so does the
//  character select preview (its own hook); the lobby and other menus, where
//  no player is known, show the loaded character's. Config
//  "apply_to_all_cacs" (testing) puts the loaded character's style on every
//  CaC instead, and "test_style" overrides both.
//
//  Phase 2 (online): each player's machine writes its style's CMS code into
//  the setup record the character select sends (unused tail of the name);
//  every machine reads it back from the record, never from its own save.
//
//  Later phases (see Research_Notes\battle-styles.md): style XMLs installed
//  through the XV2Patcher ecosystem.
// ---------------------------------------------------------------------------
#include "mod.h"
#include "acb_merge.h"
#include "game_files.h"
#include "ean_fit.h"
#include "json.h"
#include "prop_fit.h"
#include "style_menu.h"
#include "default_config.h"
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// ----------------------------------------------------------------- config ---
// CMS string fields that make up a moveset (CMS entry: string offsets at
// +0x20 + field * 4; layout from Xv2CoreLib CMS/Parser.cs).
struct MovesetField { int field; const char* name; };
static const MovesetField kMovesetFields[] = {
    {1,  "ean"},      // body animations (idle, run, basic attacks, throw)
    {6,  "cam_ean"},  // camera animations of those moves
    {7,  "bac"},      // what each basic move does (game appends _PLAYER)
    {8,  "bcm"},      // the button -> move combo tree (game appends _PLAYER)
    {12, "bdm"},      // damage of the basic moves (game appends _PLAYER)
};

// Whether a style replaces CMS field `field` (one of kMovesetFields).
static bool IsMovesetField(int field)
{
    for (const MovesetField& part : kMovesetFields)
        if (part.field == field) return true;
    return false;
}

struct Config {
    bool enabled = true;
    bool logging = false;
    bool styleSearch = true;            // the loader's search box filters the style list
    std::vector<std::string> excludeStyles = {"fake", "giants"};  // groups / CMS codes never offered
    StyleChoice styleChoice = StyleChoice::Roster;    // config "styles"; List = `styles`
    std::vector<std::string> styles;    // codes offered in the equip list, for StyleChoice::List
    // config "style_names": CMS code -> list name, over the automatic one.
    // Defaults: movesets with no character select name (battle-styles.md
    // "Style names"); MAP is Majin Purification's awoken swap character,
    // D00-D02 are Revamp's loose characters (data\chara, no msg name).
    std::map<std::string, std::wstring> styleNames = {
        {"MAP", L"Majin (Purification)"}, {"D00", L"D00 (Revamp)"}, {"D01", L"D01 (Revamp)"}, {"D02", L"D02 (Revamp)"}};
};

static Config  g_cfg;
wchar_t g_dir[MAX_PATH];     // the mod's folder: config and log
static std::vector<std::string> g_configErrors;

void Log(const char* fmt, ...)
{
    if (!g_cfg.logging) return;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\battle_styles.log", g_dir);
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
    swprintf_s(path, L"%s\\battle_styles.json", g_dir);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(file, kDefaultConfig, (DWORD)(sizeof(kDefaultConfig) - 1), &wrote, nullptr);
    CloseHandle(file);
}

bool ReadWhole(const wchar_t* path, std::string& out)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    out.resize((size_t)min(size.QuadPart, (LONGLONG)(1 << 20)));
    DWORD got = 0;
    BOOL ok = ReadFile(file, out.data(), (DWORD)out.size(), &got, nullptr);
    CloseHandle(file);
    out.resize(got);
    return ok != FALSE;
}

static void LoadConfig()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\battle_styles.json", g_dir);
    std::string text, err;
    JVal root;
    if (!ReadWhole(path, text)) { g_configErrors.push_back("battle_styles.json not readable - defaults used"); return; }
    if (!JsonParse(text, root, err) || root.type != JVal::Obj) {
        g_configErrors.push_back("battle_styles.json: " + (err.empty() ? std::string("not an object") : err) + " - defaults used");
        return;
    }
    g_cfg.enabled = root.boolOr("enabled", true);
    g_cfg.logging = root.boolOr("logging", false);
    g_cfg.styleSearch = root.boolOr("style_search", true);
    // "styles": "all", "valid" (no fake or giant movesets), "roster" (valid,
    // and only characters unlocked in the character select; the default),
    // "unlocked" (valid, every character select character as if unlocked), or
    // a list of CMS codes in the order to show them. Every setting leaves out
    // characters the character select hides for DLC (style_menu.cpp).
    const JVal* styles = root.get("styles");
    if (styles && styles->type == JVal::Arr) {
        g_cfg.styleChoice = StyleChoice::List;
        for (const JVal& code : styles->a)
            if (code.type == JVal::Str && !code.s.empty()) g_cfg.styles.push_back(code.s);
    } else if (styles && styles->type == JVal::Str && styles->s == "all") {
        g_cfg.styleChoice = StyleChoice::All;
    } else if (styles && styles->type == JVal::Str && styles->s == "valid") {
        g_cfg.styleChoice = StyleChoice::Valid;
    } else if (styles && styles->type == JVal::Str && styles->s == "unlocked") {
        g_cfg.styleChoice = StyleChoice::Unlocked;
    } else if (styles && !(styles->type == JVal::Str && styles->s == "roster")) {
        g_configErrors.push_back("styles: expected \"all\", \"valid\", \"roster\", \"unlocked\" or a list of codes - \"roster\" used");
    }
    // "exclude_styles": groups ("fake", "giants", "race") and CMS codes; missing = "fake" + "giants" (race movesets are listed as "<race> Default").
    if (const JVal* excluded = root.get("exclude_styles"); excluded && excluded->type == JVal::Arr) {
        g_cfg.excludeStyles.clear();
        for (const JVal& item : excluded->a)
            if (item.type == JVal::Str && !item.s.empty()) g_cfg.excludeStyles.push_back(item.s);
    } else if (excluded) {
        g_configErrors.push_back("exclude_styles: expected a list - [\"fake\", \"giants\"] used");
    }
    // "style_names": {"<code>": "<name>"}; replaces the default names when present.
    if (const JVal* named = root.get("style_names"); named && named->type == JVal::Obj) {
        g_cfg.styleNames.clear();
        for (const auto& [code, name] : named->o) {
            if (code.empty() || name.type != JVal::Str || name.s.empty()) continue;
            int chars = MultiByteToWideChar(CP_UTF8, 0, name.s.data(), (int)name.s.size(), nullptr, 0);
            std::wstring wide(chars, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, name.s.data(), (int)name.s.size(), wide.data(), chars);
            g_cfg.styleNames[code] = wide;
        }
    } else if (named) {
        g_configErrors.push_back("style_names: expected {\"CODE\": \"name\"} - default names used");
    }
}

// ------------------------------------------------------------ game code ---
uintptr_t g_base = 0;

// DBXV2.exe 1.26.01, from its PE header (readable before Steam's DRM has
// decrypted the code).
static bool IsGame12601()
{
    const uint8_t* image = (const uint8_t*)g_base;
    uint32_t pe = *(const uint32_t*)(image + 0x3C);
    return *(const uint32_t*)(image + pe + 8) == 0x6A63732E &&        // TimeDateStamp
           *(const uint32_t*)(image + pe + 24 + 56) == 0x1A1F000;     // SizeOfImage
}

// Executable memory within +/-2GB of `target`, for a 5-byte relative jmp.
static uint8_t* AllocNear(uintptr_t target, size_t size)
{
    SYSTEM_INFO info; GetSystemInfo(&info);
    uintptr_t granularity = info.dwAllocationGranularity;
    for (uintptr_t delta = granularity; delta < 0x40000000; delta += granularity) {
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t addr = dir ? target + delta : target - delta;
            addr -= addr % granularity;
            void* memory = VirtualAlloc((void*)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (memory) return (uint8_t*)memory;
        }
    }
    return nullptr;
}

static void AbsJmp(uint8_t* at, uintptr_t to)  // jmp [rip+0]; dq to   (14 bytes)
{
    at[0] = 0xFF; at[1] = 0x25; memset(at + 2, 0, 4);
    memcpy(at + 6, &to, 8);
}

// Function entry -> cave -> hook. The first `size` (5..8) bytes of the target
// must be whole, position-independent instructions (callers check the bytes);
// they are replayed in the trampoline returned, which calls the original.
// The jmp goes in with one 8-byte write, so a target 8-byte aligned is safe
// to patch while other threads run it.
void* HookCode(uint8_t* target, size_t size, void* hook)
{
    // Cave layout: +0 jmp to the hook; +16 the trampoline (the replayed
    // bytes, then a jmp back to target + size).
    uint8_t* cave = AllocNear((uintptr_t)target, 64);
    if (!cave) return nullptr;
    AbsJmp(cave, (uintptr_t)hook);
    memcpy(cave + 16, target, size);
    AbsJmp(cave + 16 + size, (uintptr_t)target + size);
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

bool CodeIs(uintptr_t rva, const uint8_t* code, size_t size)
{
    return memcmp((const void*)(g_base + rva), code, size) == 0;
}

// Waits (up to 3 minutes) until Steam's DRM has decrypted the page holding
// this code: pages decrypt one by one, so one hook's page being ready says
// nothing about another's (seen in game: two missed at 643 ms).
bool WaitForCode(uintptr_t rva, const uint8_t* expected, size_t size)
{
    for (int i = 0; i < 18000; ++i) {
        if (CodeIs(rva, expected, size)) return true;
        Sleep(10);
    }
    return false;
}

// ------------------------------------------------------- CMS path getter ---
// char* GetCmsPath(cms, charaId, field, out[0x80]): finds the CMS entry with
// that id and writes "chara/<dir>/<name>[_PLAYER].<ext>" into out, or returns
// null. Its only caller, 0x2c54b0, prepends "data/" (12 callers from
// character loading, lobby and menus).
static const uintptr_t kGetCmsPath = 0x2c5510;
// push rbx; push rbp; push rsi; push rdi; sub rsp, 0xd8 - the hook takes the
// first 5 bytes (the four pushes; 0x2c5510 is 8-byte aligned).
static const uint8_t kGetCmsPathCode[] = {0x40, 0x53, 0x55, 0x56, 0x57, 0x48, 0x81, 0xEC, 0xD8, 0x00, 0x00, 0x00};

// CMS file layout (Xv2CoreLib CMS/Parser.cs; walked the same way at 0x2c553e):
// entry count at +8, offset of the entry table at +0xc, 0x54 bytes per entry
// with the chara id at +0 and the 3-letter code at +4.
static const size_t kCmsCount = 8, kCmsTable = 0xc, kCmsEntrySize = 0x54, kCmsCode = 4;

// Created characters: HUM, HUF, SYM, SYF, NMC, FRI, MAM, MAF (CMS ids 100..107).
static const int kFirstCac = 100, kLastCac = 107;

typedef char* (__fastcall* GetCmsPathFn)(void* cms, int charaId, int field, char* out);
static GetCmsPathFn g_origGetCmsPath = nullptr;

// The CMS id of `code` in this CMS file, or -1.
static int FindCharaId(const uint8_t* cms, const std::string& code)
{
    int32_t count = *(const int32_t*)(cms + kCmsCount);
    uint32_t table = *(const uint32_t*)(cms + kCmsTable);
    if (count <= 0 || table < 0x10) return -1;  // same sanity check as the game (0x2c554e)
    const uint8_t* entry = cms + table;
    for (int32_t i = 0; i < count; ++i, entry += kCmsEntrySize)
        if (_strnicmp((const char*)entry + kCmsCode, code.c_str(), 4) == 0) return *(const int32_t*)entry;
    return -1;
}

static SRWLOCK g_logLock = SRWLOCK_INIT;
static std::set<std::string> g_logged;  // lines already logged this session

// Logs a line only the first time it comes up: the getter runs for every
// load of every character and the answer rarely changes.
static void LogOnce(const char* fmt, ...)
{
    if (!g_cfg.logging) return;
    char line[512];
    va_list ap; va_start(ap, fmt); vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
    AcquireSRWLockExclusive(&g_logLock);
    bool first = g_logged.insert(line).second;
    ReleaseSRWLockExclusive(&g_logLock);
    if (first) Log("%s", line);
}

// The 3-letter code of CMS entry `charaId` ("FRI"), or empty.
static std::string CharaCode(const uint8_t* cms, int charaId)
{
    int32_t count = *(const int32_t*)(cms + kCmsCount);
    uint32_t table = *(const uint32_t*)(cms + kCmsTable);
    if (count <= 0 || table < 0x10) return {};
    const uint8_t* entry = cms + table;
    for (int32_t i = 0; i < count; ++i, entry += kCmsEntrySize)
        if (*(const int32_t*)entry == charaId) return std::string((const char*)entry + kCmsCode, strnlen((const char*)entry + kCmsCode, 4));
    return {};
}

// CMS path fields fitted to the race (numbers as in kMovesetFields).
static const int kBcsField = 0;     // the body BCS: swapped for the prop BCS (StyleBcsPath)
static const int kEanField = 1;
static const int kCamEanField = 6;
static const int kBacField = 7;
static const int kBdmField = 12;
static const size_t kPathBuffer = 0x80;  // GetCmsPath's out: 0x2c54b0's stack buffer [rsp+0x20, rsp+0xa0)
// Fitted copies, relative to <game>\data. A path of their own is required:
// the game loads a file once per path and shares it between characters, and
// resolves "..", so "chara/FRI/../GOK/GOK.ean" became plain GOK.ean (log,
// 2026-10-05).
static const char kFitFolder[] = "battle_styles/fit";

std::wstring g_gameDir;  // <game>, from the exe path (<game>\bin\DBXV2.exe)
static SRWLOCK g_fitLock = SRWLOCK_INIT;
struct FittedFile {
    bool usable = false;
    int shift = 0;                          // EANs: how far the style's own ids were moved (EanFit::shift)
    std::map<uint16_t, uint16_t> bdmIds;    // BDM: the style's ids in the merged file (FitBdmToRace)
    std::map<uint16_t, uint16_t> cueIds;    // SE bank: the style's cue ids in the merged bank (MergeAcb)
};
static std::map<std::string, FittedFile> g_fitted;  // fitted path -> result, this session

// Writes `bytes` to <game>\data\<path> unless it already holds exactly that,
// creating folders on the way.
static bool WriteDataFile(const std::string& path, const std::vector<uint8_t>& bytes)
{
    std::wstring full = g_gameDir + L"\\data\\" + std::wstring(path.begin(), path.end());
    std::replace(full.begin(), full.end(), L'/', L'\\');
    std::string existing;
    if (ReadWhole(full.c_str(), existing) && existing.size() == bytes.size() &&
        memcmp(existing.data(), bytes.data(), bytes.size()) == 0)
        return true;
    for (size_t slash = full.find(L'\\', g_gameDir.size() + 1); slash != std::wstring::npos; slash = full.find(L'\\', slash + 1))
        CreateDirectoryW(full.substr(0, slash).c_str(), nullptr);
    HANDLE file = CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, nullptr) && written == bytes.size();
    CloseHandle(file);
    return ok;
}

// "chara/GOK/GOK.ean" for a FRI CaC -> "battle_styles/fit/GOK.FRI.ean".
static std::string FittedPath(const std::string& source, const std::string& race)
{
    std::string stem = source.substr(source.find_last_of('/') + 1);
    size_t extension = stem.find_last_of('.');
    std::string suffix = extension == std::string::npos ? std::string() : stem.substr(extension);
    if (extension != std::string::npos) stem.resize(extension);
    return std::string(kFitFolder) + "/" + stem + "." + race + suffix;
}

// Makes (once per game session) the copy of a style file fitted to the race
// and returns how it went; `make` builds the bytes and fills in the result's
// details (shift, bdmIds). Same inputs give the same
// bytes on every machine.
template <class Make>
static FittedFile FitOnce(const std::string& source, const std::string& fitted, Make make)
{
    if (fitted.size() >= kPathBuffer) return {};
    AcquireSRWLockExclusive(&g_fitLock);
    auto known = g_fitted.find(fitted);
    FittedFile result;
    if (known != g_fitted.end()) {
        result = known->second;
    } else {
        std::vector<uint8_t> original, fittedBytes;
        std::string from, note;
        if (!ReadGameFile(source, original, from)) note = "source not found";
        else if (make(original, fittedBytes, result, note))
            result.usable = WriteDataFile(fitted, fittedBytes) || (note += "; could not write", false);
        Log("%s (%s) -> %s: %s%s", source.c_str(), from.c_str(), fitted.c_str(), result.usable ? "" : "NOT used - ",
            note.c_str());
        g_fitted[fitted] = result;
    }
    ReleaseSRWLockExclusive(&g_fitLock);
    return result;
}

// A style EAN (body or camera) fitted to the race (ean_fit.h). `racePath` is
// the CaC's own file for the same field (e.g. "chara/FRI/FRI.ean"): its
// skeleton and transformation animations go into the copy.
// Reads one game file for prop_fit (loose data first, then the cpks).
static bool ReadGameDataFile(const std::string& path, std::vector<uint8_t>& bytes)
{
    std::string from;
    return ReadGameFile(path, bytes, from);
}

// `back`: where props carried on the back go on this race (prop_fit.h
// PropPlacement); the prop BCS's skeleton places them the same way.
static FittedFile FitEan(const std::string& source, const std::string& race, const std::string& racePath, bool body,
                         const PropPlacement& back = {})
{
    return FitOnce(source, FittedPath(source, race),
                   [&](const std::vector<uint8_t>& style, std::vector<uint8_t>& out, FittedFile& result, std::string& note) {
                       std::vector<uint8_t> raceEan;
                       std::string raceFrom;
                       if (racePath.empty() || !ReadGameFile(racePath, raceEan, raceFrom)) {
                           note = "race file not found";
                           return false;
                       }
                       EanFit fit;
                       bool made = FitEanToRace(style.data(), style.size(), raceEan.data(), raceEan.size(), body,
                                                body ? kRaceBaseAnimations : kRaceBaseCameras, out, fit, note, back);
                       result.shift = fit.shift;
                       return made;
                   });
}

// The style's and the CaC's paths for one CMS field; false when that field
// isn't one a style swaps.
static bool FieldPaths(void* cms, int charaId, int styleId, int field, char (&stylePath)[kPathBuffer],
                       char (&racePath)[kPathBuffer])
{
    return IsMovesetField(field) && g_origGetCmsPath(cms, styleId, field, stylePath) &&
           g_origGetCmsPath(cms, charaId, field, racePath);
}

// The style EAN of one field fitted to the race. Unused (empty) when that
// field isn't swapped.
static FittedFile FitField(void* cms, int charaId, int styleId, int field, const std::string& race)
{
    char stylePath[kPathBuffer] = {}, racePath[kPathBuffer] = {};
    if (!FieldPaths(cms, charaId, styleId, field, stylePath, racePath)) return {};
    if (field != kEanField) return FitEan(stylePath, race, racePath, false);
    // the race's BCS (field 0, not the prop BCS StyleBcsPath swaps in): its
    // folder holds the Poop Stick the back depth is measured on
    char raceBcs[kPathBuffer] = {};
    PropPlacement back;
    if (g_origGetCmsPath(cms, charaId, kBcsField, raceBcs)) back = MeasurePropPlacement(ReadGameDataFile, raceBcs, stylePath);
    return FitEan(stylePath, race, racePath, true, back);
}

// The style BDM merged with the race's (ean_fit.h FitBdmToRace). Unused
// (empty) when the BDM isn't swapped.
static FittedFile FitBdm(void* cms, int charaId, int styleId, const std::string& race)
{
    char stylePath[kPathBuffer] = {}, racePath[kPathBuffer] = {};
    if (!FieldPaths(cms, charaId, styleId, kBdmField, stylePath, racePath)) return {};
    return FitOnce(stylePath, FittedPath(stylePath, race),
                   [&](const std::vector<uint8_t>& style, std::vector<uint8_t>& out, FittedFile& result,
                       std::string& note) {
                       std::vector<uint8_t> raceBdm;
                       std::string raceFrom;
                       if (!ReadGameFile(racePath, raceBdm, raceFrom)) {
                           note = "race file not found";
                           return false;
                       }
                       return FitBdmToRace(style.data(), style.size(), raceBdm.data(), raceBdm.size(), &out,
                                           result.bdmIds, note);
                   });
}

// ------------------------------------------------- character SE bank ---
// A CaC loads its race's character SE bank (system/chara_sound.cso), the
// style BAC plays cues of the style character's bank: the race's bank gets
// the style's cues appended (acb_merge.h) and the CaC loads that copy
// instead (HkLoadCharaSe). Under data\sound: the bank path the game builds
// is "data/sound/" + path + ".acb", and a path of our own keeps the shared
// race bank untouched for other characters.
static const char kCsoPath[] = "system/chara_sound.cso";
static const char kCharaSeFolder[] = "SE/Battle/Chara/";  // under data/sound, as 0x298c70 builds it
static const char kFitSeFolder[] = "SE/Battle/Chara/BattleStyles/";
// Set once the bank load is redirected (HkLoadCharaSe): without it the game
// loads the race bank, which lacks the merged cue ids, so sounds are dropped.
static volatile bool g_charaSeHooked = false;

// The game's CSO, read once (loose data or the cpks).
static const std::vector<uint8_t>& CsoFile()
{
    static std::vector<uint8_t> cso;
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL {
        std::string from;
        if (!ReadGameFile(kCsoPath, cso, from)) Log("%s not found: style sounds are dropped", kCsoPath);
        return TRUE;
    }, nullptr, nullptr);
    return cso;
}

static std::string CharaSeBankOf(int charaId)
{
    const std::vector<uint8_t>& cso = CsoFile();
    return cso.empty() ? std::string() : CharaSeBank(cso.data(), cso.size(), charaId);
}

// The style character's bank. A custom style's moveset (tools\moveset_to_style.py)
// has no CSO entry - LB Mod Installer can't bind a CSO chara id to the CMS id
// it picks - so its bank is found by name: CAR_BTL_<code>_SE, when present.
static std::string StyleSeBankOf(const uint8_t* cms, int styleId)
{
    std::string bank = CharaSeBankOf(styleId);
    if (!bank.empty() || !cms) return bank;
    const std::string code = CharaCode(cms, styleId);
    if (code.empty()) return bank;
    const std::string byName = "CAR_BTL_" + code + "_SE";
    return GameFileExists(std::string("sound/") + kCharaSeFolder + byName + ".acb") ? byName : bank;
}

// The merged bank's path as the game asks for it (no "data/sound/", no
// ".acb"): "SE/Battle/Chara/BattleStyles/CAR_BTL_ZMS_SE.NMC".
static std::string FittedSePath(const std::string& styleBank, const std::string& race)
{
    return kFitSeFolder + styleBank + "." + race;
}

// The race's bank with the style's cues appended. Unused (empty) when either
// has no bank or they are the same one.
static FittedFile FitCharacterSe(const uint8_t* cms, int charaId, int styleId, const std::string& race)
{
    const std::string raceBank = CharaSeBankOf(charaId), styleBank = StyleSeBankOf(cms, styleId);
    if (raceBank.empty() || styleBank.empty() || _stricmp(raceBank.c_str(), styleBank.c_str()) == 0) {
        LogOnce("chara %d SE bank \"%s\", style %d \"%s\": nothing to merge", charaId, raceBank.c_str(), styleId,
                styleBank.c_str());
        return {};
    }
    const std::string sound = std::string("sound/") + kCharaSeFolder;
    return FitOnce(sound + styleBank + ".acb", "sound/" + FittedSePath(styleBank, race) + ".acb",
                   [&](const std::vector<uint8_t>& style, std::vector<uint8_t>& out, FittedFile& result,
                       std::string& note) {
                       std::vector<uint8_t> raceAcb;
                       std::string raceFrom;
                       if (!ReadGameFile(sound + raceBank + ".acb", raceAcb, raceFrom)) {
                           note = "race bank not found";
                           return false;
                       }
                       return MergeAcb(raceAcb.data(), raceAcb.size(), style.data(), style.size(), out,
                                       result.cueIds, note);
                   });
}

// Points `path` (the style file the game asked for) at its fitted copy.
// EANs and the BDM are merged with the race's; the BAC follows what the
// merged files moved and points sounds on the character's own SE bank at
// the merged bank's cues (dropped when there is no merged bank).
static void UseFitted(char* path, void* cms, int charaId, int styleId, int field)
{
    std::string race = CharaCode((const uint8_t*)cms, charaId), source = path;
    if (race.empty()) return;
    FittedFile result;
    if (field == kEanField || field == kCamEanField) {
        result = FitField(cms, charaId, styleId, field, race);
    } else if (field == kBdmField) {
        result = FitBdm(cms, charaId, styleId, race);
    } else if (field == kBacField) {
        FittedFile body = FitField(cms, charaId, styleId, kEanField, race);
        FittedFile camera = FitField(cms, charaId, styleId, kCamEanField, race);
        BacFit fit;
        fit.bodyShift = body.usable ? body.shift : 0;
        fit.cameraShift = camera.usable ? camera.shift : 0;
        FittedFile damage = FitBdm(cms, charaId, styleId, race);
        if (damage.usable) fit.bdmIds = damage.bdmIds;
        FittedFile sounds = g_charaSeHooked ? FitCharacterSe((const uint8_t*)cms, charaId, styleId, race) : FittedFile{};
        fit.characterSe = sounds.usable ? BacFit::CharacterSe::Remap : BacFit::CharacterSe::Drop;
        if (sounds.usable) fit.characterSeCues = sounds.cueIds;
        result = FitOnce(source, FittedPath(source, race),
                         [&](const std::vector<uint8_t>& bac, std::vector<uint8_t>& out, FittedFile&, std::string& note) {
                             // The race's BAC, for the CaC-only entries the style lacks (they
                             // play race animation ids, which only the merged body EAN keeps).
                             char stylePath[kPathBuffer] = {}, racePath[kPathBuffer] = {};
                             std::vector<uint8_t> raceBac;
                             std::string raceFrom;
                             if (body.usable && FieldPaths(cms, charaId, styleId, kBacField, stylePath, racePath) &&
                                 ReadGameFile(racePath, raceBac, raceFrom)) {
                                 fit.raceBac = raceBac.data();
                                 fit.raceBacSize = raceBac.size();
                             }
                             return FitBacToRace(bac.data(), bac.size(), fit, out, note);
                         });
    }
    if (result.usable) strcpy_s(path, kPathBuffer, FittedPath(source, race).c_str());
}

// ------------------------------------------------- battle player loader ---
// void LoadBattlePlayer(loader, playerIndex 0..13, setup): loads one battle
// player's character (fn 0x16bfa0, only caller 0x169ede). Every moveset file
// of that player is asked for inside it, on the same thread: 0x113d40 (EAN
// field 1, then fields 6/7/8/9/12 in the loop at 0x1144d0) is called from it
// directly (0x16c484) and through 0x16c530 (0x16c7b3).
static const uintptr_t kLoadBattlePlayer = 0x16bfa0;
// mov rax, rsp; mov [rax+0x10], edx; mov [rax+8], rcx. The first 6 bytes go
// to the hook's trampoline: no rip-relative operands, nothing jumps into them.
static const uint8_t kLoadBattlePlayerCode[] = {0x48, 0x8B, 0xC4, 0x89, 0x50, 0x10, 0x48, 0x89, 0x48, 0x08};

// The loader's per-player entries start at loader + 0xe0, 0x1f0 bytes each
// (0x16bfcc). They are the battle setup list's player records from +0x70 on
// (the loader copies 0x70 bytes from its setup argument): the setup writer
// 0x652210 stores chara at +0x70, costume +0x74, save character slot +0x78,
// here +0, +4, +8. The save slot is the one the CaC came from: the story
// setup 0x3a8570 passes the save manager's loaded slot (0x81acc0 -> 0x433e0,
// [mgr+8]+0x44) as that argument.
static const size_t kPlayerEntries = 0xe0, kPlayerEntrySize = 0x1f0;
static const int kBattlePlayers = 14;  // setup list slots (0x652210: cmp edx, 0xd)
struct BattlePlayerEntry {
    int32_t charaId;
    int32_t costume;
    int32_t saveSlot;
};

// Per battle setup list index: the style its last record carried. An entry
// written without a record (story setup 0x3a8844) has fromRecord false and
// falls back to the local save slot.
struct SetupStyle {
    bool fromRecord = false;
    int charaId = -1;
    int saveSlot = -1;
    std::string style;
};
static SRWLOCK g_setupLock = SRWLOCK_INIT;
static SetupStyle g_setupStyle[kBattlePlayers];

static SetupStyle SetupStyleOf(int index)
{
    AcquireSRWLockShared(&g_setupLock);
    SetupStyle entry = g_setupStyle[index];
    ReleaseSRWLockShared(&g_setupLock);
    return entry;
}

// The battle player being loaded on this thread (entry null outside the
// loader). The entry is read when a path is asked for: the loader fills it
// from its setup argument first.
struct LoadingPlayer {
    int index;
    const BattlePlayerEntry* entry;
};
static thread_local LoadingPlayer t_loadingPlayer = {-1, nullptr};

typedef void (__fastcall* LoadBattlePlayerFn)(void* loader, int playerIndex, void* setup);
static LoadBattlePlayerFn g_origLoadBattlePlayer = nullptr;

static void ForgetStyledResources(int player);  // character effects, below

static void __fastcall HkLoadBattlePlayer(void* loader, int playerIndex, void* setup)
{
    LoadingPlayer outer = t_loadingPlayer;  // not seen nested; restored anyway
    t_loadingPlayer = {-1, nullptr};
    if (loader && playerIndex >= 0 && playerIndex < kBattlePlayers) {
        t_loadingPlayer = {playerIndex, (const BattlePlayerEntry*)((const uint8_t*)loader + kPlayerEntries +
                                                                 playerIndex * kPlayerEntrySize)};
        ForgetStyledResources(playerIndex);  // the slot's previous character
    }
    g_origLoadBattlePlayer(loader, playerIndex, setup);
    const BattlePlayerEntry* entry = t_loadingPlayer.entry;
    if (entry && entry->charaId >= 0) {  // at most 14 lines per battle; -1 = empty slot
        const SetupStyle record = SetupStyleOf(playerIndex);
        Log("battle player %d: chara %d costume %d save slot %d; setup record: %s chara %d save slot %d style \"%s\"",
            playerIndex, entry->charaId, entry->costume, entry->saveSlot, record.fromRecord ? "yes" : "no",
            record.charaId, record.saveSlot, record.style.c_str());
    }
    t_loadingPlayer = outer;
}

// ------------------------------------------------- loadout builder ---
// void BuildLoadout(character, out, preset): fills a battle setup record
// from loadout record `preset` of a save character (fn 0x64fa20; negative
// = record 0, the main loadout - cmovns at 0x64faf8; records 0x68 bytes at
// character + 0xf4). Every battle CaC and the character select preview go
// through it: story and most modes pass -1, the battle character select
// passes the preset picked there (per-player table at [0x81ad40(mgr)] +
// 0xec + player * 4, set by 0x842cb0 from 0x3ab660, read by 0x8424d0 at
// 0x38d8fe), the preview its r9 (0x5e6754).
static const uintptr_t kBuildLoadout = 0x64fa20;
// mov [rsp+8], rbx (5 bytes, taken by the hook); mov [rsp+0x10], rbp
static const uint8_t kBuildLoadoutCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10};
// Save characters: manager [0x15545b0] + 0x120c0 + slot * 0xe880 (0x81aba0).
static const uintptr_t kSaveManager = 0x15545b0;
static const size_t kSaveCharacters = 0x120c0, kSaveCharacterSize = 0xe880;
static const int kSaveSlots = 8;
static const int kLoadoutRecords = 8;  // 0 = main, 1..7 = saved presets

// The loadout record each save slot was last built from - for battle, the
// one it fights with (the battle player loader runs after the setup).
static volatile LONG g_slotPreset[kSaveSlots] = {};

typedef void (__fastcall* BuildLoadoutFn)(void* character, void* out, int preset);
static BuildLoadoutFn g_origBuildLoadout = nullptr;

// Save slot of a character pointer, or -1 (not one of the 8 save characters).
static int SaveSlotOf(const void* character)
{
    uintptr_t manager = *(const uintptr_t*)(g_base + kSaveManager);
    if (!manager || (uintptr_t)character < manager + kSaveCharacters) return -1;
    uintptr_t offset = (uintptr_t)character - (manager + kSaveCharacters);
    if (offset % kSaveCharacterSize) return -1;
    size_t slot = offset / kSaveCharacterSize;
    return slot < (size_t)kSaveSlots ? (int)slot : -1;
}

static void __fastcall HkBuildLoadout(void* character, void* out, int preset)
{
    int slot = SaveSlotOf(character);
    int record = preset < 0 || preset >= kLoadoutRecords ? 0 : preset;
    if (slot >= 0 && InterlockedExchange(&g_slotPreset[slot], record) != record)
        Log("save slot %d: loadout from preset %d", slot, record);  // only on a change
    g_origBuildLoadout(character, out, preset);
}

// ------------------------------------------ character select preview ---
// SelectCharacter's preview (fn 0x5e66d0(menu, chara, costume, r9), only
// caller 0x5e707a in its update 0x5e6ce0) loads the model under the cursor,
// paths included (0x657ff0 at 0x5e6974 asks the getter for fields 1/3/7).
// For a CaC it reads that character from the save with 0x81aba0(save
// manager [0x15545b0], menu +0x450) - 0x81aba0 returns save character
// `slot` (0x120c0 + slot * 0xe880; -1 = the loaded one), so +0x450 is the
// previewed CaC's save slot. Its r9 is the loadout record it shows (passed
// to 0x64fa20 at 0x5e6754): the preset picked in the character select.
static const uintptr_t kSelectCharaPreview = 0x5e66d0;
// mov rax, rsp; push rdi; push r12 (6 bytes, taken by the hook); push r13
static const uint8_t kSelectCharaPreviewCode[] = {0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54, 0x41, 0x55};
static const size_t kSelectCharaSaveSlot = 0x450;
static const int kNoPreview = -2;  // -1 is a valid value: the loaded slot

// The save slot whose CaC this thread's character select preview is loading,
// and the preset shown.
static thread_local int t_previewSlot = kNoPreview;
static thread_local int t_previewPreset = 0;

typedef int (__fastcall* SelectCharaPreviewFn)(void* menu, int charaId, int costume, int preset);
static SelectCharaPreviewFn g_origSelectCharaPreview = nullptr;

static int __fastcall HkSelectCharaPreview(void* menu, int charaId, int costume, int preset)
{
    int outerSlot = t_previewSlot, outerPreset = t_previewPreset;
    t_previewSlot = menu ? *(const int32_t*)((const uint8_t*)menu + kSelectCharaSaveSlot) : kNoPreview;
    t_previewPreset = preset < 0 || preset >= kLoadoutRecords ? 0 : preset;  // as 0x64fa20 reads it
    int result = g_origSelectCharaPreview(menu, charaId, costume, preset);
    t_previewSlot = outerSlot;
    t_previewPreset = outerPreset;
    return result;
}

// ------------------------------------------ setup records (online sync) ---
// The character select turns each local player into a 0x100-byte setup
// record (packer 0x666ce0) and sends it with 0x666af0(state, record, 0x100,
// 1): online to every lobby member, offline straight to the receiver
// (0x666c68). Every record - ours and the other players' - becomes a battle
// setup list entry through 0x668b30(state, record) -> 0x6674a0 -> 0x652210.
// Record layout (written 0x666d3d.., read 0x668b55..): +0 type word (0 =
// player), +8 setup list index byte, +0xc chara id, +0x10 costume byte (the
// packer zeroes it for a CaC at 0x6673bb), +0x12 save slot byte, +0xc0 the
// name: 64 bytes from strncpy (so zero padded), read back with strlen.
//
// The style travels as its CMS code in the name's last 4 bytes. A vanilla
// game always sends zeros there (= own moveset) and never reads past the
// name's terminator. So every machine fights every player with the style
// that player sent; the local equipped_styles.sav only decides what we send.
static const uintptr_t kPackLoadoutCall = 0x666f0d;  // in the packer: call 0x64fd80(character, record + 0x2c, preset)
static const uintptr_t kSendRecordCall = 0x6673e6;   // in the packer: call 0x666af0(state, record, 0x100, 1)
static const uintptr_t kWriteSetupCall = 0x667552;   // in 0x6674a0: call 0x652210 (the setup writer, above)
static const uint8_t kPackLoadoutCallCode[] = {0xE8, 0x6E, 0x8E, 0xFE, 0xFF};
static const uint8_t kSendRecordCallCode[] = {0xE8, 0x05, 0xF7, 0xFF, 0xFF};
static const uint8_t kWriteSetupCallCode[] = {0xE8, 0xB9, 0xAC, 0xFE, 0xFF};
static const uintptr_t kReceiveRecord = 0x668b30;
// mov r11, rsp; push rbp; push r15 (6 bytes, taken by the hook); sub rsp, 0xf8
static const uint8_t kReceiveRecordCode[] = {0x4C, 0x8B, 0xDC, 0x55, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xF8, 0x00, 0x00, 0x00};
static const int kPlayerRecord = 0;  // record type word (0x668aa0 dispatches 0 to 0x668b30)
static const size_t kRecordSize = 0x100, kRecordChara = 0xc, kRecordName = 0xc0, kRecordNameSize = 0x40;
static const size_t kRecordStyle = kRecordSize - 4;  // the name's last 4 bytes
static const size_t kStyleCodeSize = 4;              // CMS codes are up to 4 chars (entry +4, 4 bytes)

// Points the call instruction at `rva` (E8 rel32) at `hook`, through a cave
// within +/-2GB; returns the function it called, or null.
// `prefix` (up to 16 bytes, position-independent) runs in the cave before the
// jmp to the hook, e.g. to pass the hook a register the call doesn't.
static void* RedirectCall(uintptr_t rva, void* hook, const uint8_t* prefix = nullptr, size_t prefixSize = 0)
{
    uint8_t* call = (uint8_t*)(g_base + rva);
    if (call[0] != 0xE8 || prefixSize > 16) return nullptr;
    uintptr_t original = (uintptr_t)call + 5 + *(const int32_t*)(call + 1);
    uint8_t* cave = AllocNear((uintptr_t)call, 32);
    if (!cave) return nullptr;
    if (prefixSize) memcpy(cave, prefix, prefixSize);
    AbsJmp(cave + prefixSize, (uintptr_t)hook);
    int32_t rel = (int32_t)((intptr_t)cave - ((intptr_t)call + 5));
    DWORD old;
    if (!VirtualProtect(call + 1, 4, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    memcpy(call + 1, &rel, 4);
    VirtualProtect(call + 1, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), call, 5);
    return (void*)original;
}

static bool IsCac(int charaId) { return charaId >= kFirstCac && charaId <= kLastCac; }

// The style this machine sends for its save slot `slot`, loadout `preset`.
static std::string OwnStyle(int slot, int preset)
{
    return StyleMenu_PresetCode(slot, preset);
}

// The record the packer on this thread is building, when it is a save
// character's (only that path builds the loadout with 0x64fd80, 0x666f0d).
struct PackingRecord {
    bool saveCharacter;
    int saveSlot;
    int preset;
};
static thread_local PackingRecord t_packing = {false, -1, 0};

typedef void (__fastcall* PackLoadoutFn)(void* character, void* out, int preset);
static PackLoadoutFn g_origPackLoadout = nullptr;

static void __fastcall HkPackLoadout(void* character, void* out, int preset)
{
    g_origPackLoadout(character, out, preset);
    const int slot = SaveSlotOf(character);
    t_packing = {slot >= 0, slot, preset < 0 || preset >= kLoadoutRecords ? 0 : preset};  // as 0x64fe39 reads it
}

typedef uintptr_t (__fastcall* SendRecordFn)(void* state, uint8_t* record, int size, int flags);
static SendRecordFn g_origSendRecord = nullptr;

// Stamps our style into a save CaC's record just before it goes out.
static uintptr_t __fastcall HkSendRecord(void* state, uint8_t* record, int size, int flags)
{
    const PackingRecord packing = t_packing;
    t_packing = {false, -1, 0};
    if (record && size == (int)kRecordSize && *(const uint16_t*)record == kPlayerRecord && packing.saveCharacter &&
        IsCac(*(const int32_t*)(record + kRecordChara))) {
        const char* name = (const char*)record + kRecordName;
        const std::string style = OwnStyle(packing.saveSlot, packing.preset);
        if (strnlen(name, kRecordNameSize) < kRecordStyle - kRecordName) {  // keep the name's terminator
            memset(record + kRecordStyle, 0, kStyleCodeSize);
            memcpy(record + kRecordStyle, style.data(), min(style.size(), kStyleCodeSize));
            LogOnce("record sent: player %d, save slot %d preset %d, style \"%s\"", record[8], packing.saveSlot,
                    packing.preset, style.c_str());
        } else {
            LogOnce("record sent: player %d, name too long to carry the style", record[8]);
        }
    }
    return g_origSendRecord(state, record, size, flags);
}

// The style code in a received record ("" = own moveset). Anything but a
// terminated name followed by code characters counts as no style.
static std::string RecordStyle(const uint8_t* record)
{
    const char* name = (const char*)record + kRecordName;
    if (strnlen(name, kRecordStyle - kRecordName) >= kRecordStyle - kRecordName) return {};
    const char* code = (const char*)record + kRecordStyle;
    std::string style(code, strnlen(code, kStyleCodeSize));
    for (char c : style)
        if (!isalnum((unsigned char)c) && c != '_') return {};
    return style;
}

// The style of the record this thread's 0x668b30 is applying.
struct ReceivingRecord {
    bool active;
    std::string style;
};
static thread_local ReceivingRecord t_receiving = {false, {}};

typedef uintptr_t (__fastcall* ReceiveRecordFn)(void* state, const uint8_t* record);
static ReceiveRecordFn g_origReceiveRecord = nullptr;

static uintptr_t __fastcall HkReceiveRecord(void* state, const uint8_t* record)
{
    ReceivingRecord outer = t_receiving;
    t_receiving = {true, record ? RecordStyle(record) : std::string()};
    uintptr_t result = g_origReceiveRecord(state, record);
    t_receiving = outer;
    return result;
}

typedef void (__fastcall* WriteSetupFn)(void* list, int index, int charaId, int costume, int saveSlot, int field7c,
                                        int field84, int field88);
static WriteSetupFn g_origWriteSetup = nullptr;

static void __fastcall HkWriteSetup(void* list, int index, int charaId, int costume, int saveSlot, int field7c,
                                    int field84, int field88)
{
    g_origWriteSetup(list, index, charaId, costume, saveSlot, field7c, field84, field88);
    if (index < 0 || index >= kBattlePlayers) return;
    SetupStyle entry;
    entry.fromRecord = t_receiving.active;
    entry.charaId = charaId;
    entry.saveSlot = saveSlot;
    entry.style = t_receiving.active ? t_receiving.style : std::string();
    AcquireSRWLockExclusive(&g_setupLock);
    g_setupStyle[index] = entry;
    ReleaseSRWLockExclusive(&g_setupLock);
    if (entry.fromRecord && IsCac(charaId))
        LogOnce("record applied: setup player %d, chara %d save slot %d, style \"%s\"", index, charaId, saveSlot,
                entry.style.c_str());
}

// The equipped style (empty = own moveset) for a path request of CaC
// `charaId`; `source` gets where it came from, for the log.
static std::string EquippedStyleFor(int charaId, std::string& source)
{
    const int previewSlot = t_previewSlot;
    if (previewSlot != kNoPreview) {
        char note[64];
        sprintf_s(note, "character select, save slot %d preset %d", previewSlot, t_previewPreset);
        source = note;
        return StyleMenu_PresetCode(previewSlot, t_previewPreset);  // -1 = the loaded slot
    }
    const LoadingPlayer loading = t_loadingPlayer;
    if (!loading.entry) { source = "loaded character"; return StyleMenu_EquippedCode(); }
    const int saveSlot = loading.entry->saveSlot;
    char note[96];
    // From the character select: the style the player's own machine sent.
    // The chara/slot compare guards the assumption that the loader's player
    // index is the setup list index.
    const SetupStyle setup = SetupStyleOf(loading.index);
    if (setup.fromRecord && setup.charaId == loading.entry->charaId && setup.saveSlot == saveSlot) {
        sprintf_s(note, "player %d, save slot %d, from its setup record", loading.index, saveSlot);
        source = note;
        return loading.entry->charaId == charaId ? setup.style : std::string();
    }
    // Story and other setups without a record: our own save slot.
    const int preset = saveSlot >= 0 && saveSlot < kSaveSlots ? g_slotPreset[saveSlot] : 0;
    sprintf_s(note, "player %d, save slot %d preset %d, no setup record", loading.index, saveSlot, preset);
    source = note;
    // A file of another character than the player's own keeps its moveset.
    if (loading.entry->charaId != charaId || saveSlot < 0) return {};
    return StyleMenu_PresetCode(saveSlot, preset);  // empty for a slot above 7
}

// The CMS code whose moveset CaC `charaId` loads (empty = its own): the
// equipped style, or for a custom style the CMS entry its file names for
// the CaC's race (StyleMenu_MovesetFor). The equipped code itself is what
// is saved and sent online; each machine resolves it the same way.
static std::string StyleFor(int charaId, std::string& source)
{
    return StyleMenu_MovesetFor(EquippedStyleFor(charaId, source), charaId);
}

// The CMS the getter was last given (one per game session), for the hooks
// below that need a style's chara id.
static const uint8_t* volatile g_cms = nullptr;

// ------------------------------------------------------------ style props ---
// A style character's prop (Whis's cane) is drawn on the CaC by a BCS of its
// own: the race's BCS with the prop as a physics object on the prop's a_x_*
// bone, and the race's skeleton with that bone added (the fitted body EAN
// animates it, ean_fit.h). The game builds the skeleton path from the BCS
// path - "<bcs path minus .bcs>_000[_LOD01..03].esk" (0x298240, 0x2987f0) -
// so pointing CMS field 0 at "battle_styles/fit/HUM.WIS.bcs" also loads
// "battle_styles/fit/HUM.WIS_000.esk", and unstyled CaCs keep the race's.
// The files are made once per race and style (prop_fit.h) from the style's
// own BCS, skeleton and part EMDs; a style without a_x_* prop bones keeps
// the race's BCS. Character select preview and battle only (StyleBcsPath).
// The race's BCS (`racePath`) with the style's props (only those its
// moveset EAN `styleEanPath` animates), made once; the prop
// meshes go into the style's chara folder, the skeletons next to the BCS.
static FittedFile FitPropBcs(const std::string& racePath, const std::string& styleBcsPath,
                             const std::string& styleEanPath, const std::string& fitted)
{
    return FitOnce(racePath, fitted,
                   [&](const std::vector<uint8_t>& raceBcs, std::vector<uint8_t>& out, FittedFile&, std::string& note) {
                       std::vector<PropFile> files;
                       if (!FitPropsToRace(ReadGameDataFile, raceBcs, racePath, styleBcsPath, styleEanPath, fitted, files, note)) return false;
                       for (size_t i = 1; i < files.size(); ++i)
                           if (!WriteDataFile(files[i].path, files[i].bytes)) {
                               note += "; could not write " + files[i].path;
                               return false;
                           }
                       out = std::move(files[0].bytes);
                       return true;
                   });
}

// CMS field 0 of a CaC: the style's prop BCS when there is one, else the
// race's own ("chara/HUM/HUM.bcs").
static char* StyleBcsPath(void* cms, int charaId, char* out)
{
    char* path = g_origGetCmsPath(cms, charaId, kBcsField, out);
    if (!path) return path;
    // Only where the style's moves play: the character select preview and
    // battle. The lobby and other menus (no player known, StyleFor falls back
    // to the loaded character) pose the CaC neutrally, and keep the model
    // after the style is changed there (seen in game, test 4).
    if (t_previewSlot == kNoPreview && !t_loadingPlayer.entry) return path;
    std::string source;
    const std::string style = StyleFor(charaId, source);
    // The prop bones only move with the fitted body EAN (ean_fit.h keeps their tracks).
    if (style.empty()) return path;
    const int styleId = FindCharaId((const uint8_t*)cms, style);
    char styleBcs[kPathBuffer] = {}, styleEan[kPathBuffer] = {};
    if (styleId < 0 || !g_origGetCmsPath(cms, styleId, kBcsField, styleBcs) ||
        !g_origGetCmsPath(cms, styleId, kEanField, styleEan))
        return path;
    const std::string propBcs = FittedPath(path, style);  // "battle_styles/fit/HUM.WIS.bcs"
    if (FitPropBcs(path, styleBcs, styleEan, propBcs).usable) {
        LogOnce("chara %d BCS (%s) -> %s", charaId, source.c_str(), propBcs.c_str());
        strcpy_s(path, kPathBuffer, propBcs.c_str());
    } else {
        LogOnce("chara %d BCS (%s): style \"%s\" has no props", charaId, source.c_str(), style.c_str());
    }
    return path;
}

static char* __fastcall HkGetCmsPath(void* cms, int charaId, int field, char* out)
{
    if (cms) g_cms = (const uint8_t*)cms;
    if (cms && out && IsCac(charaId) && field == kBcsField) return StyleBcsPath(cms, charaId, out);
    if (cms && out && charaId >= kFirstCac && charaId <= kLastCac && IsMovesetField(field)) {
        std::string source;
        std::string style = StyleFor(charaId, source);
        if (style.empty()) {  // Default: own moveset
            LogOnce("chara %d field %d (%s): own moveset", charaId, field, source.c_str());
            return g_origGetCmsPath(cms, charaId, field, out);
        }
        int styleId = FindCharaId((const uint8_t*)cms, style);
        if (styleId >= 0) {
            if (char* path = g_origGetCmsPath(cms, styleId, field, out)) {
                UseFitted(path, cms, charaId, styleId, field);
                LogOnce("chara %d field %d (%s) -> %s", charaId, field, source.c_str(), path);
                return path;
            }
        }
        // Unknown code, or the style character has no file for this field:
        // the CaC keeps its own.
        LogOnce("chara %d field %d (%s): style \"%s\" has no file, CaC's own kept", charaId, field,
                source.c_str(), style.c_str());
    }
    return g_origGetCmsPath(cms, charaId, field, out);
}

// The style of the battle player loading CaC `charaId` on this thread, with
// its CMS id: false when it has none.
static bool LoadingStyle(int charaId, std::string& style, int& styleId, std::string& source)
{
    const uint8_t* cms = g_cms;
    if (!cms || !IsCac(charaId) || !t_loadingPlayer.entry) return false;
    style = StyleFor(charaId, source);
    styleId = style.empty() ? -1 : FindCharaId(cms, style);
    return styleId >= 0;
}

// ---------------------------------------------- character SE bank load ---
// In 0x298c70 (CharaResource: loads the character's SE bank; only caller
// 0x11405e in 0x113d40, so inside the battle player loader's thread):
// 0x298e0c xor r9d, r9d; mov edx, ebx; mov rcx, rbp; call 0x3630a0 at
// 0x298e14 = LoadSoundBank(sound manager [0x1554490], slot, r8 =
// "SE/Battle/Chara/<CSO SE name>", 0); rdi = the resource, chara id at
// +0xd0 (0x298c9a, 0xc2e90). The call is redirected through a cave that
// first passes rdi in r9 (mov r9, rdi); the hook calls on with r9 = 0.
static const uintptr_t kLoadCharaSeAt = 0x298e0c, kLoadCharaSeCall = 0x298e14;
static const uint8_t kLoadCharaSeCode[] = {0x45, 0x33, 0xC9, 0x8B, 0xD3, 0x48, 0x8B, 0xCD, 0xE8, 0x87, 0xA2, 0x0C, 0x00};
static const uint8_t kPassResourceInR9[] = {0x49, 0x89, 0xF9};  // mov r9, rdi
static const size_t kResourceChara = 0xd0;

typedef uintptr_t (__fastcall* LoadSoundBankFn)(void* sound, int slot, const char* path, void* zero);
static LoadSoundBankFn g_origLoadSoundBank = nullptr;

static uintptr_t __fastcall HkLoadCharaSe(void* sound, int slot, const char* path, const uint8_t* resource)
{
    std::string redirected;
    std::string style, source;
    int styleId = -1;
    const int charaId = resource ? *(const int32_t*)(resource + kResourceChara) : -1;
    if (path && LoadingStyle(charaId, style, styleId, source)) {
        const std::string race = CharaCode(g_cms, charaId), raceBank = CharaSeBankOf(charaId);
        const char* name = strrchr(path, '/');
        name = name ? name + 1 : path;
        // Only the race's own bank (not one a transformation brings).
        if (!race.empty() && !raceBank.empty() && _stricmp(name, raceBank.c_str()) == 0 &&
            FitCharacterSe(g_cms, charaId, styleId, race).usable)
            redirected = FittedSePath(StyleSeBankOf(g_cms, styleId), race);
        LogOnce("chara %d SE bank %s (%s, style %s) -> %s", charaId, path, source.c_str(), style.c_str(),
                redirected.empty() ? "kept" : redirected.c_str());
    }
    return g_origLoadSoundBank(sound, slot, redirected.empty() ? path : redirected.c_str(), nullptr);
}

// ------------------------------------------------ character EEPK + owner ---
// A style BAC spawns character-EEPK effects (BAC type 8, EepkType 2) by the
// style character's effect ids (ZMG's light combo: beam sword effects 1, 12,
// 10 of chara/ZMG/ZMG.eepk); a CaC has its race's EEPK registered. So the
// style's EEPK is registered too, and character effects spawned for a styled
// CaC's mob (SpawnEffect below, and its owner when no id is given) use the
// style's chara id.
//
// 0x113d40(holder, {chara, costume, ...}, 0) loads one character's files;
// the battle player loader calls it at 0x16c484 and 0x16c7b3 (in 0x16c530),
// each "xor r8d, r8d; lea rdx, [..]; mov rcx, ..; call". `holder` points at
// the resource it creates; a battle mob keeps that holder at +0x4d0
// (0xc2e9c) and the chara id from the resource at +0xbc (0xc2e96).
static const uintptr_t kLoadCharaFilesAt[] = {0x16c47a, 0x16c7a8};
static const uintptr_t kLoadCharaFilesCall[] = {0x16c484, 0x16c7b3};
static const uint8_t kLoadCharaFilesCode[][16] = {
    {0x45, 0x33, 0xC0, 0x48, 0x8D, 0x55, 0xC7, 0x48, 0x8B, 0xCB, 0xE8, 0xB7, 0x78, 0xFA, 0xFF},
    {0x45, 0x33, 0xC0, 0x48, 0x8D, 0x54, 0x24, 0x40, 0x49, 0x8B, 0xCE, 0xE8, 0x88, 0x75, 0xFA, 0xFF}};
static const size_t kLoadCharaFilesCodeSize[] = {15, 16};

// RegisterEffects(effect manager [0x1554608], type 2 = character EEPK, chara
// id, 0, 1, 1): as the loader registers each character's EEPK at 0x16c761
// (by id through vfx_spec.ers; a chara already registered is looked up
// first, 0x957090).
static const uintptr_t kRegisterEffects = 0x956320, kEffectManager = 0x1554608;
static const uint8_t kRegisterEffectsCode[] = {0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
static const int kCharacterEepk = 2;
typedef uintptr_t (__fastcall* RegisterEffectsFn)(void* manager, int type, int charaId, int zero, int one, int one2);

// Battle::MobVfxOwner vtable 0xf5deb0 slot 14 (rva 0xf5df20) = 0x121370:
// the owner's chara id for character effects: mob [owner+8] (its
// transformed body [mob+0xa20] when [mob+0x15c0] == 0x34), [mob+0xbc], -1
// without a mob. xv2patcher (VfxFix) patches that function's code; the
// slot is chained, not the code.
static const uintptr_t kVfxOwnerCharaSlot = 0xf5df20, kMobVfxOwnerVtable = 0xf5deb0;
static const size_t kOwnerMob = 8, kMobState = 0x15c0, kMobTransformed = 0xa20, kMobHolder = 0x4d0;
static const int32_t kMobStateTransformed = 0x34;
typedef int (__fastcall* VfxOwnerCharaFn)(void* owner);
static VfxOwnerCharaFn g_origVfxOwnerChara = nullptr;

// SpawnEffect 0x956ef0(manager, request, eepk type, id, flags, a6, a7): the
// effect's container is looked up by (type, id) at 0x956fc6 (0x957160). The
// owner (slot 14 above) is only asked when id < 0 (0x956f8d); a BAC effect
// passes the mob's chara id itself, so for a character EEPK of a styled
// CaC's mob the hook swaps that id for the style's. request +8 = the owner
// (0x956fa4). Entry: mov r11, rsp; mov [r11+8], rbx (7 bytes, replayed).
static const uintptr_t kSpawnEffect = 0x956ef0;
static const uint8_t kSpawnEffectCode[] = {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x08, 0x49, 0x89, 0x6B, 0x10};
static const size_t kSpawnEffectHookSize = 7;
static const size_t kRequestOwner = 8;
typedef uintptr_t (__fastcall* SpawnEffectFn)(void* manager, void* request, int type, int id, uintptr_t flags,
                                              uintptr_t arg6, uintptr_t arg7);
static SpawnEffectFn g_origSpawnEffect = nullptr;

typedef uintptr_t (__fastcall* LoadCharaFilesFn)(void* holder, const int32_t* chara, int flags);
static LoadCharaFilesFn g_origLoadCharaFiles[2] = {};

// Styled CaC resources of the battle players: at most a few loads each.
// Matched by the resource (*holder), not the holder: the loader copies the
// holder into the player's map (0x1696f0) and the mob keeps that copy at
// +0x4d0 (0xc2e9c), so only the resource it points at is the same (seen in
// game: mob holder 0x...64fd0, loaded holders 0x...64e90/654d0/656b0/66330).
struct StyledResource {
    const void* resource;
    int slot;  // resource +0xc8, see ResourceSlot
    int player;
    int raceChara;
    int styleId;
};
static const size_t kStyledResources = kBattlePlayers * 4;
static SRWLOCK g_styledLock = SRWLOCK_INIT;
static StyledResource g_styled[kStyledResources] = {};
static size_t g_styledCount = 0;

static void ForgetStyledResources(int player)
{
    AcquireSRWLockExclusive(&g_styledLock);
    size_t kept = 0;
    for (size_t i = 0; i < g_styledCount; ++i)
        if (g_styled[i].player != player) g_styled[kept++] = g_styled[i];
    g_styledCount = kept;
    ReleaseSRWLockExclusive(&g_styledLock);
}

static void* ReadPointer(const void* address)
{
    __try {
        return address ? *(void* const*)address : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// A CharaResource's slot, +0xc8: the SE bank load 0x298c70 turns it into the
// player's sound slot (slot * 2 + 3, 0x298cb2..0x298cf8); -1 when unreadable.
static const size_t kResourceSlot = 0xc8;

static int ResourceSlot(const void* resource)
{
    if (!resource) return -1;
    __try {
        return *(const int32_t*)((const uint8_t*)resource + kResourceSlot);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

static uintptr_t LoadCharaFiles(int site, void* holder, const int32_t* chara, int flags)
{
    const uintptr_t result = g_origLoadCharaFiles[site](holder, chara, flags);
    std::string style, source;
    int styleId = -1;
    if (!holder || !chara || !LoadingStyle(chara[0], style, styleId, source)) return result;
    const int player = t_loadingPlayer.index;
    bool registered = false;  // this player's style EEPK already registered in this load
    AcquireSRWLockExclusive(&g_styledLock);
    for (size_t i = 0; i < g_styledCount; ++i)
        registered |= g_styled[i].player == player && g_styled[i].styleId == styleId;
    const void* resource = ReadPointer(holder);
    if (resource && g_styledCount < kStyledResources)
        g_styled[g_styledCount++] = {resource, ResourceSlot(resource), player, chara[0], styleId};
    ReleaseSRWLockExclusive(&g_styledLock);
    if (!registered) {
        void* manager = *(void**)(g_base + kEffectManager);
        if (manager) ((RegisterEffectsFn)(g_base + kRegisterEffects))(manager, kCharacterEepk, styleId, 0, 1, 1);
        LogOnce("player %d chara %d (%s): character effects of style %s (chara %d) registered%s", player, chara[0],
                source.c_str(), style.c_str(), styleId, manager ? "" : " - NOT, no effect manager");
    }
    return result;
}

static uintptr_t __fastcall HkLoadCharaFiles0(void* holder, const int32_t* chara, int flags)
{
    return LoadCharaFiles(0, holder, chara, flags);
}

static uintptr_t __fastcall HkLoadCharaFiles1(void* holder, const int32_t* chara, int flags)
{
    return LoadCharaFiles(1, holder, chara, flags);
}

// The body `mob` shows: its transformed body when [mob+0x15c0] == 0x34 (as
// 0x121370 picks it); null when unreadable (no C++ objects here: __try).
static const uint8_t* BodyMob(const uint8_t* mob)
{
    __try {
        if (mob && *(const int32_t*)(mob + kMobState) == kMobStateTransformed)
            mob = *(const uint8_t* const*)(mob + kMobTransformed);
        return mob;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// The owner's mob as 0x121370 picks it; null when unset or unreadable.
static const uint8_t* OwnerMob(void* owner)
{
    __try {
        return BodyMob(*(const uint8_t* const*)((const uint8_t*)owner + kOwnerMob));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// The resource of `mob`: [[mob+0x4d0]] (the holder copy's first field, whose
// +0xd0 is the chara id: 0xc2e90); null when unset or unreadable.
static const void* MobResource(const uint8_t* mob)
{
    if (!mob) return nullptr;
    __try {
        const void* const* holder = *(const void* const* const*)(mob + kMobHolder);
        return holder ? *holder : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// The style's chara id when `mob` is a styled CaC of race `charaId`, else
// charaId.
static int StyleCharaOfMob(const uint8_t* mob, int charaId)
{
    if (!IsCac(charaId) || !mob) return charaId;
    const void* resource = MobResource(mob);
    if (!resource) return charaId;
    // The mob's resource is not one the loader's holders point at (seen in
    // game: same pool, 0x3c0 apart), so: the same resource, else the same
    // slot (= the battle player index, seen in game: players 0 and 3), else -
    // only when the mob's slot is unreadable - the race's only loaded style.
    const int slot = ResourceSlot(resource);
    int sameResource = -1, sameSlot = -1, onlyStyle = -1;
    bool severalStyles = false;
    AcquireSRWLockShared(&g_styledLock);
    for (size_t i = 0; i < g_styledCount; ++i) {
        const StyledResource& styled = g_styled[i];
        if (styled.raceChara != charaId) continue;
        if (styled.resource == resource) sameResource = styled.styleId;
        if (slot >= 0 && styled.slot == slot) sameSlot = styled.styleId;
        if (onlyStyle >= 0 && onlyStyle != styled.styleId) severalStyles = true;
        onlyStyle = styled.styleId;
    }
    ReleaseSRWLockShared(&g_styledLock);
    if (sameResource >= 0) return sameResource;
    if (sameSlot >= 0) return sameSlot;
    return slot < 0 && onlyStyle >= 0 && !severalStyles ? onlyStyle : charaId;
}

// The same for an effect owner's mob.
static int StyleCharaOfOwner(void* owner, int charaId)
{
    return owner ? StyleCharaOfMob(OwnerMob(owner), charaId) : charaId;
}

// The style id last logged per battle player slot: the owner hook runs for
// every effect a styled CaC spawns, so LogOnce's formatting + locked set
// lookup is kept off that path (it showed as stutter with logging on).
static volatile LONG g_ownerStyleLogged[kBattlePlayers] = {};

static int __fastcall HkVfxOwnerChara(void* owner)
{
    const int charaId = g_origVfxOwnerChara(owner);
    const int styleId = StyleCharaOfOwner(owner, charaId);
    if (styleId != charaId && g_cfg.logging) {
        const int slot = ResourceSlot(MobResource(OwnerMob(owner)));
        if (slot >= 0 && slot < (int)kBattlePlayers && InterlockedExchange(&g_ownerStyleLogged[slot], styleId + 1) != styleId + 1)  // +1: 0 = none (chara 0 is GOK)
            Log("slot %d: character effects of race chara %d from style chara %d's EEPK", slot, charaId, styleId);
    }
    return styleId;
}

// The request's owner when it is a mob's effect owner (Battle::MobVfxOwner).
static void* MobOwnerOf(const void* request)
{
    __try {
        void* owner = *(void* const*)((const uint8_t*)request + kRequestOwner);
        return owner && *(const uintptr_t*)owner == g_base + kMobVfxOwnerVtable ? owner : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static uintptr_t __fastcall HkSpawnEffect(void* manager, void* request, int type, int id, uintptr_t flags,
                                          uintptr_t arg6, uintptr_t arg7)
{
    if (type == kCharacterEepk && IsCac(id) && request && g_styledCount) {
        const int styleId = StyleCharaOfOwner(MobOwnerOf(request), id);
        if (styleId != id) {
            LogOnce("character effect of race chara %d spawned from style chara %d's EEPK", id, styleId);
            id = styleId;
        }
    }
    return g_origSpawnEffect(manager, request, type, id, flags, arg6, arg7);
}

// Sounds and effects of the style character (after the setup record hooks).
// Combo List (pause menu; Menu::PopupCombo, POPUP_COMBO.iggy) is no moveset
// read: the iggy has two fixed pictures, "var1" (the CaC/most characters'
// longer chains) and "var2" (shorter), picked by the battle pause handler
// 0x16fef0 from byte +0x14 of the local player's CMS entry ([mob+0x3f0], set
// at 0xd3534) and passed as the 5th argument of the setup 0x58c020 (call at
// 0x1701ef). Only DMG, DM2, MRN, CL2 and ZMD have it set, so a CaC styled as
// one of those gets the style's picture here (battle-styles-ui.md "Combo List").
static const uintptr_t kComboListLayoutRead = 0x1701a3;  // movzx edi, byte [rax+0x14]
static const uint8_t kComboListLayoutCode[] = {0x0F, 0xB6, 0x78, 0x14};
static const uintptr_t kComboListSetupCall = 0x1701ef;
static const uintptr_t kBattleManager = 0x1554460;      // global pointer, read at 0x92059
static const uintptr_t kLocalPlayerIndex = 0x92050;     // fn(battle manager + 0x7c) -> player index (0x170172)
static const size_t kBattleManagerPlayerArg = 0x7c, kBattleMobs = 0x3a58;  // mobs by player index (0x170180)
static const size_t kMobCmsEntry = 0x3f0, kCmsComboLayout = 0x14;

typedef uint32_t (__fastcall* LocalPlayerIndexFn)(void* playerArg);
typedef void* (__fastcall* ComboListSetupFn)(void* menu, void* text, void* unknown, int unknown2, int shortLayout);
static ComboListSetupFn g_origComboListSetup = nullptr;

// The CMS entry of `charaId` in `cms`, or null.
static const uint8_t* CmsEntry(const uint8_t* cms, int charaId)
{
    int32_t count = *(const int32_t*)(cms + kCmsCount);
    uint32_t table = *(const uint32_t*)(cms + kCmsTable);
    if (count <= 0 || table < 0x10) return nullptr;
    const uint8_t* entry = cms + table;
    for (int32_t i = 0; i < count; ++i, entry += kCmsEntrySize)
        if (*(const int32_t*)entry == charaId) return entry;
    return nullptr;
}

// The local player's battle mob and its CMS chara id, as 0x16fef0 finds them;
// false when there is none (no C++ objects here: __try).
static bool LocalBattleMob(const uint8_t*& mob, int& charaId)
{
    __try {
        uint8_t* manager = *(uint8_t**)(g_base + kBattleManager);
        if (!manager) return false;
        const uint32_t index = ((LocalPlayerIndexFn)(g_base + kLocalPlayerIndex))(manager + kBattleManagerPlayerArg);
        if (index >= (uint32_t)kBattlePlayers) return false;
        mob = *(const uint8_t* const*)(manager + kBattleMobs + index * sizeof(void*));
        const uint8_t* entry = mob ? *(const uint8_t* const*)(mob + kMobCmsEntry) : nullptr;
        if (!entry) return false;
        charaId = *(const int32_t*)entry;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void* __fastcall HkComboListSetup(void* menu, void* text, void* unknown, int unknown2, int shortLayout)
{
    const uint8_t* mob = nullptr;
    int charaId = -1;
    const uint8_t* cms = g_cms;
    if (cms && LocalBattleMob(mob, charaId)) {
        const int styleId = StyleCharaOfMob(mob, charaId);
        const uint8_t* styleEntry = styleId != charaId ? CmsEntry(cms, styleId) : nullptr;
        if (styleEntry) {
            shortLayout = styleEntry[kCmsComboLayout];
            LogOnce("combo list: chara %d styled as %s (chara %d) -> %s layout", charaId,
                    CharaCode(cms, styleId).c_str(), styleId, shortLayout ? "short (var2)" : "long (var1)");
        }
    }
    return g_origComboListSetup(menu, text, unknown, unknown2, shortLayout);
}

static void InstallStyleAssetHooks()
{
    if (WaitForCode(kLoadCharaSeAt, kLoadCharaSeCode, sizeof(kLoadCharaSeCode)))
        g_origLoadSoundBank = (LoadSoundBankFn)RedirectCall(kLoadCharaSeCall, (void*)&HkLoadCharaSe,
                                                             kPassResourceInR9, sizeof(kPassResourceInR9));
    Log(g_origLoadSoundBank ? "character SE bank hook installed at %.0f ms"
                            : "character SE bank hook NOT installed (%.0f ms): style sounds are dropped",
        MsSinceStart());
    g_charaSeHooked = g_origLoadSoundBank != nullptr;

    bool codeMatches = WaitForCode(kRegisterEffects, kRegisterEffectsCode, sizeof(kRegisterEffectsCode));
    for (int site = 0; site < 2 && codeMatches; ++site)
        codeMatches = WaitForCode(kLoadCharaFilesAt[site], kLoadCharaFilesCode[site], kLoadCharaFilesCodeSize[site]);
    void** slot = (void**)(g_base + kVfxOwnerCharaSlot);
    if (codeMatches && *slot) {
        DWORD old;
        if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
            g_origVfxOwnerChara = (VfxOwnerCharaFn)*slot;
            InterlockedExchangePointer(slot, (void*)&HkVfxOwnerChara);
            VirtualProtect(slot, sizeof(void*), old, &old);
        }
    }
    if (g_origVfxOwnerChara && WaitForCode(kSpawnEffect, kSpawnEffectCode, sizeof(kSpawnEffectCode)))
        g_origSpawnEffect = (SpawnEffectFn)HookCode((uint8_t*)(g_base + kSpawnEffect), kSpawnEffectHookSize,
                                                    (void*)&HkSpawnEffect);
    if (g_origSpawnEffect) {
        g_origLoadCharaFiles[0] = (LoadCharaFilesFn)RedirectCall(kLoadCharaFilesCall[0], (void*)&HkLoadCharaFiles0);
        g_origLoadCharaFiles[1] = (LoadCharaFilesFn)RedirectCall(kLoadCharaFilesCall[1], (void*)&HkLoadCharaFiles1);
    }
    Log(g_origLoadCharaFiles[0] && g_origLoadCharaFiles[1]
            ? "character effect hooks installed at %.0f ms"
            : "character effect hooks NOT (all) installed (%.0f ms): style effects missing",
        MsSinceStart());

    if (WaitForCode(kComboListLayoutRead, kComboListLayoutCode, sizeof(kComboListLayoutCode)))
        g_origComboListSetup = (ComboListSetupFn)RedirectCall(kComboListSetupCall, (void*)&HkComboListSetup);
    Log(g_origComboListSetup ? "combo list hook installed at %.0f ms"
                             : "combo list hook NOT installed (%.0f ms): the CaC's combo list shows",
        MsSinceStart());
}

double MsSinceStart()
{
    FILETIME created, exited, kernel, user, now;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER start{created.dwLowDateTime, created.dwHighDateTime}, current{now.dwLowDateTime, now.dwHighDateTime};
    return (double)(current.QuadPart - start.QuadPart) / 10000.0;
}

// Patches code: waits (up to 3 minutes) until Steam's DRM has decrypted it.
static void InstallCmsHook()
{
    bool matched = false;
    for (int i = 0; i < 18000 && !matched; ++i) {
        matched = CodeIs(kGetCmsPath, kGetCmsPathCode, sizeof(kGetCmsPathCode));
        if (!matched) Sleep(10);
    }
    if (matched)  // tried once: a hook must never go in twice
        g_origGetCmsPath = (GetCmsPathFn)HookCode((uint8_t*)(g_base + kGetCmsPath), 5, (void*)&HkGetCmsPath);
    Log(g_origGetCmsPath ? "CMS path hook installed at %.0f ms"
                         : "CMS path hook NOT installed (%.0f ms): code does not match game 1.26.01", MsSinceStart());
    if (!g_origGetCmsPath) return;

    // Their pages may be decrypted later than the getter's.
    if (WaitForCode(kLoadBattlePlayer, kLoadBattlePlayerCode, sizeof(kLoadBattlePlayerCode)))
        g_origLoadBattlePlayer = (LoadBattlePlayerFn)HookCode((uint8_t*)(g_base + kLoadBattlePlayer), 6,
                                                              (void*)&HkLoadBattlePlayer);
    Log(g_origLoadBattlePlayer
            ? "battle player hook installed at %.0f ms"
            : "battle player hook NOT installed (%.0f ms): every CaC gets the loaded character's style",
        MsSinceStart());
    if (WaitForCode(kBuildLoadout, kBuildLoadoutCode, sizeof(kBuildLoadoutCode)))
        g_origBuildLoadout = (BuildLoadoutFn)HookCode((uint8_t*)(g_base + kBuildLoadout), 5, (void*)&HkBuildLoadout);
    Log(g_origBuildLoadout ? "loadout hook installed at %.0f ms"
                           : "loadout hook NOT installed (%.0f ms): battles use the main preset's style",
        MsSinceStart());
    if (WaitForCode(kSelectCharaPreview, kSelectCharaPreviewCode, sizeof(kSelectCharaPreviewCode)))
        g_origSelectCharaPreview = (SelectCharaPreviewFn)HookCode((uint8_t*)(g_base + kSelectCharaPreview), 6,
                                                                  (void*)&HkSelectCharaPreview);
    Log(g_origSelectCharaPreview
            ? "character select preview hook installed at %.0f ms"
            : "character select preview hook NOT installed (%.0f ms): previews show the loaded character's style",
        MsSinceStart());

    // Online sync. Receive side first: if the send side then fails, records
    // carry zeros and every machine shows that player's own moveset.
    if (WaitForCode(kPackLoadoutCall, kPackLoadoutCallCode, sizeof(kPackLoadoutCallCode)) &&
        WaitForCode(kSendRecordCall, kSendRecordCallCode, sizeof(kSendRecordCallCode)) &&
        WaitForCode(kWriteSetupCall, kWriteSetupCallCode, sizeof(kWriteSetupCallCode)) &&
        WaitForCode(kReceiveRecord, kReceiveRecordCode, sizeof(kReceiveRecordCode))) {
        g_origWriteSetup = (WriteSetupFn)RedirectCall(kWriteSetupCall, (void*)&HkWriteSetup);
        if (g_origWriteSetup)
            g_origReceiveRecord = (ReceiveRecordFn)HookCode((uint8_t*)(g_base + kReceiveRecord), 6, (void*)&HkReceiveRecord);
        if (g_origReceiveRecord)
            g_origPackLoadout = (PackLoadoutFn)RedirectCall(kPackLoadoutCall, (void*)&HkPackLoadout);
        if (g_origPackLoadout)
            g_origSendRecord = (SendRecordFn)RedirectCall(kSendRecordCall, (void*)&HkSendRecord);
    }
    Log(g_origSendRecord ? "setup record hooks installed at %.0f ms (styles sync online)"
                         : "setup record hooks NOT installed (%.0f ms): online players get our local styles",
        MsSinceStart());
    InstallStyleAssetHooks();
}

static DWORD WINAPI ThreadMain(LPVOID)
{
    EnsureConfig();
    LoadConfig();
    Log("---- Battle Styles v" MOD_VERSION " starting,%.0f ms after the game started", MsSinceStart());
    for (const std::string& error : g_configErrors) Log("config: %s", error.c_str());
    if (!GameFilesAvailable()) Log("XV2 Mod Loader's library is missing or too old to read game files - update the loader");
    if (!g_cfg.enabled) { Log("disabled in the config"); return 0; }
    g_base = (uintptr_t)GetModuleHandleW(nullptr);

    bool known = false;
    for (int i = 0; i < 2000 && !known; ++i) {
        known = IsGame12601();
        if (!known) Sleep(5);
    }
    if (!known) { Log("unknown game version - nothing is changed"); return 0; }
    StyleMenu_InstallDataHooks();  // data only: before the menu file can load
    StyleMenu_Build(g_cfg.styleChoice, g_cfg.styles, g_cfg.excludeStyles, g_cfg.styleNames, g_cfg.styleSearch);
    InstallCmsHook();              // waits for the decrypted code
    if (g_origGetCmsPath) StyleMenu_InstallCodeHooks();
    return 0;
}

void Mod_Start(HMODULE self)
{
    GetModuleFileNameW(self, g_dir, MAX_PATH);  // ...\mods\BattleStyles\BattleStyles.dll
    if (wchar_t* slash = wcsrchr(g_dir, L'\\')) *slash = 0;
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);  // <game>\bin\DBXV2.exe
    g_gameDir = exe;
    for (int i = 0; i < 2; ++i) g_gameDir.resize(g_gameDir.find_last_of(L'\\'));
    if (HANDLE thread = CreateThread(nullptr, 0, ThreadMain, nullptr, 0, nullptr)) CloseHandle(thread);
}

void Mod_Stop()
{
    // Nothing to undo: this only runs as the game exits.
}

// ---------------------------------------------------------------------------
//  The "Battle Style" equip slot (phase 3) and where the choice is kept
//  (phase 4, per character slot and preset; mod.cpp gives each battle player
//  the style of the save slot it came from).
//
//  Customize -> equip tab gets an 8th button under Super Soul. It works like
//  the other equipment: it shows the equipped style, and opens the game's
//  own item list (AVATAR_LIST) to pick one. Research_Notes\battle-styles-ui.md
//  "Equip screen UI" has the layout this relies on.
//
//  Pieces:
//  * AVATAR.iggy is served with a rebuilt ActionScript (as3\Avatar.as ->
//    avatar_abc.h via ClaudeTools\iggy\menu_abc.py): the button reads user-data
//    slots 96 (label) and 97 (count) and sends 240 to open the list.
//  * CHARASELE.iggy (character select skill popup) and AVATAR_SET.iggy (the
//    lobby Presets panel) are rebuilt the same way with a "Battle Style"
//    row. Its text rides on a string the game already sends (the Super Soul
//    name / the last skill name) after a tab, which the new AS splits off.
//  * The menu object (EditChara) is made with 2 more user-data slots, and
//    with its AVATAR_LIST also in the Customize mode (3), which has none.
//  * Its AS callback: 240 opens the list with the styles; whenever the
//    game pushes the cloth slots, slots 96/97 get the equipped style.
//  * Its list poll: when the style list closes, the picked entry is saved.
//  * The style list's search: the XV2 Mod Loader's search box (the one Item
//    Search shows on the same screen) filters the entries; AVATAR_LIST.iggy
//    is served with a rebuilt ActionScript (as3\AvatarList.as) that redraws
//    the open list when the game sends it a new count.
//  * Chara Search (its own DLL) rebuilds CHARASELE.iggy's script too,
//    for the character select's search and favorites. Only one script can
//    replace the game's, so while it is active this mod serves one with both
//    (charasele_search_abc.h) over the game's or over Chara Search's.
//  * The choice goes to equipped_styles.sav in the mod's folder, never the
//    save file, per Steam account, character slot and loadout preset
//    (layout below). The preset menu (EditMyset) is hooked so saving or
//    loading a preset copies the style with the rest of the loadout.
// ---------------------------------------------------------------------------
#include "style_menu.h"
#include "avatar_abc.h"
#include "avatarlist_abc.h"
#include "charasele_abc.h"
#include "charasele_patcher_abc.h"
#include "charasele_search_abc.h"
#include "charasele_patcher_search_abc.h"
#include "charasearch_ids.h"
#include "myset_abc.h"
#include "game_files.h"
#include "iggy_abc.h"
#include "json.h"
#include "mod.h"
#include "xv2_search.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <tuple>
#include <utility>

// ------------------------------------------------------------ the styles ---
// A character select costume that uses a style's moveset.
struct SelectCostume {
    int charaId = 0;         // CMS id
    int costume = 0;
    uint32_t dlcKey1 = 1;    // the slot's DLC keys (CST DlcFlag1 / DlcFlag2); 1, 0 = base game
    uint32_t dlcKey2 = 0;
};

struct Style {
    std::string  code;       // CMS code; empty = Default (the character's own moveset)
    std::wstring name;       // shown in the list and on the button
    int raceOrder = -1;      // index in kRaceMovesets for a race's own moveset (always unlocked, listed last)
    std::vector<SelectCostume> selectCostumes;  // every select costume using it
};

// Built once by StyleMenu_Build, read-only afterwards (published through
// g_stylesReady).
static std::vector<Style> g_styles;
static std::set<std::string> g_leftOutStyles;  // CMS codes left out: files missing, fake or excluded in the config
static volatile LONG g_stylesReady = 0;
static StyleChoice g_styleChoice = StyleChoice::All;  // config "styles"
static bool StylesReady();

// CMS (Xv2CoreLib CMS/Parser.cs): count at +8, table offset at +0xc, 0x54
// bytes per entry, code at +4, string fields as file offsets at +0x20 +
// field * 4 (0 = none). A value "../X/Y" means chara/X/Y.*, anything else
// chara/<code>/<value>.* (the path getter 0x2c5510).
static const size_t kCmsCount = 8, kCmsTable = 0xc, kCmsEntrySize = 0x54, kCmsCode = 4, kCmsStrings = 0x20;
static const int kEanField = 1, kBacField = 7, kBcmField = 8;  // the parts that make a moveset
// CMS entry +0x10 (eternity CmsFile.h unk_10): a size class, 0..6 in 1.26.01.
// 5 = the Great Apes (OSV OSB OBB OSN) and Shenron (SIN XAS), 6 = Cell Max
// (SIM SIH); every fighter is 0..4 (Broly, Dodoria, Guru 4). Giant movesets
// leave a CaC lying on the ground. Fits the data (ClaudeTools\battle_styles\
// giantmovesets.py); not proven to be what the game reads for giants.
static const size_t kCmsId = 0, kCmsSizeClass = 0x10;
static const uint32_t kGiantSizeClass = 5;

static uint32_t Rd32(const std::vector<uint8_t>& bytes, size_t at)
{
    if (at + 4 > bytes.size()) return 0;
    uint32_t value; memcpy(&value, bytes.data() + at, 4);
    return value;
}

// The CaC races' own movesets (chara/<race>/<race>.ean). In the roster they
// show up under an NPC that borrows one (Elder Kai ROK = HUF), and on a CaC
// of that race they are the same as Default. Listed under the race's name
// instead of the borrowing NPC's.
struct RaceMoveset { const char* code; const wchar_t* name; };
static const RaceMoveset kRaceMovesets[] = {
    {"HUM", L"Human M Default"},  {"HUF", L"Human F Default"},  {"SYM", L"Saiyan M Default"},
    {"SYF", L"Saiyan F Default"}, {"NMC", L"Namekian Race Default"}, {"FRI", L"Frieza Race Default"},
    {"MAM", L"Majin M Default"},  {"MAF", L"Majin F Default"},
};

// The race whose own moveset `ean`/`bac`/`bcm` ("HUF/HUF" each) is, or null.
static const RaceMoveset* RaceMovesetOf(const std::string& ean, const std::string& bac, const std::string& bcm)
{
    auto directory = [](const std::string& path) { return path.substr(0, path.find('/')); };
    for (const RaceMoveset& race : kRaceMovesets)
        if (directory(ean) == race.code && directory(bac) == race.code && directory(bcm) == race.code) return &race;
    return nullptr;
}

// EAN (Xv2CoreLib Animation/EAN, see ean_fit.cpp): +18 u16 animation count,
// +28 offset of the name table: one u32 file offset per animation, the
// names after it at the end of the file.
static const size_t kEanHeaderSize = 32, kEanAnimationCount = 18, kEanNameTable = 28;
enum class EanAttacks { Has, None, Unreadable };

// Whether a moveset's EAN has attack animations (<CODE>_ATC_...). Every
// fighter's has (Fu 15, Goku 40+); fake movesets - NPCs that borrow a
// fighter's BAC/BCM - have only idle, walk or cutscene animations, and on a
// CaC they break it (Reception Robot HST: shop/walk, no head bone - the
// CaC's head grows without end; Tokitoki TKT, Cheelai XAC: cutscenes;
// Shenron XAS/SIN, Chamel CML, Robelu RBL: a stand pose).
// ClaudeTools\battle_styles\fakemovesets.py lists them offline. Only the names at
// the end of the file are read (ReadGameFileEnds): the EANs total ~1.2 GB.
static EanAttacks EanAttackAnimations(const std::string& path)
{
    std::vector<uint8_t> head, names;
    uint64_t namesFrom = 0, fileSize = 0;
    auto tailFrom = [&](const std::vector<uint8_t>& header, uint64_t size) {
        fileSize = size;
        return namesFrom = Rd32(header, kEanNameTable);
    };
    if (!ReadGameFileEnds(path, kEanHeaderSize, tailFrom, head, names) || head.size() < kEanHeaderSize)
        return EanAttacks::Unreadable;
    uint16_t count; memcpy(&count, head.data() + kEanAnimationCount, 2);
    for (size_t i = 0; i < count; ++i) {
        if (4 * i + 4 > names.size()) return EanAttacks::Unreadable;
        uint32_t nameAt = Rd32(names, 4 * i);
        if (nameAt < namesFrom || nameAt >= fileSize) return EanAttacks::Unreadable;
        size_t at = (size_t)(nameAt - namesFrom);
        std::string name((const char*)names.data() + at, strnlen((const char*)names.data() + at, names.size() - at));
        if (name.find("_ATC_") != std::string::npos) return EanAttacks::Has;
    }
    return EanAttacks::None;
}

// A CMS entry's string field as the file it names ("GOK/GOK"), upper case;
// empty when the entry has none.
static std::string CmsFile(const std::vector<uint8_t>& cms, size_t entry, const std::string& code, int field)
{
    uint32_t offset = Rd32(cms, entry + kCmsStrings + field * 4);
    if (!offset || offset >= cms.size()) return {};
    std::string value((const char*)cms.data() + offset, strnlen((const char*)cms.data() + offset, cms.size() - offset));
    if (value.empty()) return {};
    std::string file = value.compare(0, 3, "../") == 0 ? value.substr(3) : code + "/" + value;
    for (char& c : file) c = (char)toupper((unsigned char)c);
    return file;
}

// msg file (Xv2CoreLib MSG): "#MSG", then at +8 {count, names table, ids
// table, lines table}. Names: 16 bytes {offset, chars, bytes, -}, UTF-16
// when bytes = 2 x chars (+ terminator), else 8-bit. Lines: 8 bytes
// {count, offset} -> 16 bytes {offset, chars, bytes, -}, UTF-16.
static std::map<std::string, std::wstring> ReadMsgNames(const std::vector<uint8_t>& msg)
{
    std::map<std::string, std::wstring> names;
    if (msg.size() < 0x18 || memcmp(msg.data(), "#MSG", 4) != 0) return names;
    uint32_t count = Rd32(msg, 8), namesTable = Rd32(msg, 0xc), linesTable = Rd32(msg, 0x14);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t nameOffset = Rd32(msg, namesTable + 16 * i), nameChars = Rd32(msg, namesTable + 16 * i + 4);
        uint32_t nameBytes = Rd32(msg, namesTable + 16 * i + 8);
        uint32_t lineCount = Rd32(msg, linesTable + 8 * i), lineEntry = Rd32(msg, linesTable + 8 * i + 4);
        if (!lineCount || (uint64_t)nameOffset + nameBytes > msg.size()) continue;
        std::string name;
        bool wide = nameBytes >= 2 * nameChars && nameBytes % 2 == 0;
        for (uint32_t c = 0; c < (wide ? nameBytes / 2 : nameBytes); ++c) {
            uint8_t ch = msg[nameOffset + (wide ? 2 * c : c)];
            if (!ch) break;
            name += (char)ch;
        }
        uint32_t textOffset = Rd32(msg, lineEntry), textBytes = Rd32(msg, lineEntry + 8);
        if ((uint64_t)textOffset + textBytes > msg.size()) continue;
        std::wstring text;
        for (uint32_t c = 0; 2 * c + 1 < textBytes; ++c) {
            wchar_t ch = (wchar_t)(msg[textOffset + 2 * c] | (msg[textOffset + 2 * c + 1] << 8));
            if (!ch) break;
            text += ch;
        }
        names[name] = text;
    }
    return names;
}

// One costume of a character select slot.
struct SlotCostume {
    std::string code;  // CMS code
    int costume = 0;
    int preset = 0;
    uint32_t dlcKey1 = 1, dlcKey2 = 0;  // see SelectCostume
};
using SelectSlot = std::vector<SlotCostume>;  // costumes in select order; [0] = the slot's default

// The character select slots. With XV2Patcher: data\XV2P_SLOTS.x2s, text,
// one "{...}" per slot holding "[code,costume,preset,unlock,gk2,voice1,
// voice2,dlc low,dlc high,...]" per costume (eternity_common
// Xv2PatcherSlotsFile: dlc low/high = the CST's DlcFlag1/2). Without it:
// chara_select_table.cst (Xv2CoreLib CST_File.cs): count at 12, entries after
// the 0x10 header, each {slot costume index at 0 (0 = a new slot), code at 4,
// costume u16 at 8, preset u16 at 0xa, DlcFlag1 at 0x14, DlcFlag2 at 0x18}.
static std::vector<SelectSlot> ReadSelectSlots(std::string& source)
{
    std::vector<SelectSlot> slots;
    std::vector<uint8_t> file;
    if (ReadGameFile("XV2P_SLOTS.x2s", file, source)) {
        std::vector<std::string> fields;
        bool inSlot = false, inCostume = false;
        for (uint8_t byte : file) {
            char ch = (char)byte;
            if (ch == '{') { inSlot = true; slots.emplace_back(); }
            else if (ch == '}') inSlot = false;
            else if (!inSlot) continue;
            else if (ch == '[') { inCostume = true; fields.assign(1, std::string()); }
            else if (!inCostume) continue;
            else if (ch == ',') fields.emplace_back();
            else if (ch != ']') fields.back() += ch;
            else {
                inCostume = false;
                if (fields.size() < 3) continue;
                SlotCostume costume{fields[0], atoi(fields[1].c_str()), atoi(fields[2].c_str())};
                // Signed decimal text ("2147483648" or "-2147483648"): strtoll keeps every bit.
                if (fields.size() >= 8) costume.dlcKey1 = (uint32_t)strtoll(fields[7].c_str(), nullptr, 10);
                if (fields.size() >= 9) costume.dlcKey2 = (uint32_t)strtoll(fields[8].c_str(), nullptr, 10);
                slots.back().push_back(costume);
            }
        }
    } else if (ReadGameFile("system/chara_select_table.cst", file, source) && file.size() >= 0x10 &&
               !memcmp(file.data(), "#CST", 4)) {
        uint32_t count = Rd32(file, 12);
        size_t entrySize = count ? (file.size() - 0x10) / count : 0;
        for (uint32_t i = 0; i < count && entrySize >= 0xc; ++i) {
            size_t entry = 0x10 + i * entrySize;
            if (Rd32(file, entry) == 0 || slots.empty()) slots.emplace_back();
            SlotCostume costume;
            costume.code.assign((const char*)file.data() + entry + 4, strnlen((const char*)file.data() + entry + 4, 4));
            costume.costume = file[entry + 8] | (file[entry + 9] << 8);
            costume.preset = file[entry + 0xa] | (file[entry + 0xb] << 8);
            if (entrySize >= 0x1c) {
                costume.dlcKey1 = Rd32(file, entry + 0x14);
                costume.dlcKey2 = Rd32(file, entry + 0x18);
            }
            slots.back().push_back(costume);
        }
    }
    slots.erase(std::remove_if(slots.begin(), slots.end(), [](const SelectSlot& slot) { return slot.empty(); }), slots.end());
    return slots;
}

static std::string ThreeDigits(int value)
{
    char text[16];
    snprintf(text, sizeof text, "%03d", value);
    return text;
}

// The list name of a moveset (`codes` = every CMS code using it), after the
// character select: the moveset of a slot's first (default) costume is named
// like that slot ("Trunks (Future)"); one reached only through another
// costume gets the name the game shows for that costume, plus the costume's
// name when that is the slot's own ("Trunks (Future) (Capsule Corp. Clothes
// 3)" for TR1, but "Hit (Awoken)" stays); a moveset in no slot keeps
// chara_<CODE>_000. Empty when there is no name at all.
static std::wstring MovesetName(const std::vector<std::string>& codes, const std::vector<SelectSlot>& slots,
                                const std::map<std::string, std::wstring>& characters,
                                const std::map<std::string, std::wstring>& variations)
{
    auto lookup = [](const std::map<std::string, std::wstring>& names, const std::string& key) {
        auto found = names.find(key);
        return found != names.end() ? found->second : std::wstring();
    };
    // Most costumes have no chara_<CODE>_<costume> line; the select shows chara_<CODE>_000 then.
    auto shownName = [&](const SlotCostume& costume) {
        std::wstring name = lookup(characters, "chara_" + costume.code + "_" + ThreeDigits(costume.costume));
        return name.empty() ? lookup(characters, "chara_" + costume.code + "_000") : name;
    };
    auto uses = [&](const SlotCostume& costume) { return std::find(codes.begin(), codes.end(), costume.code) != codes.end(); };
    for (const SelectSlot& slot : slots)
        if (uses(slot[0]) && !shownName(slot[0]).empty()) return shownName(slot[0]);
    for (const SelectSlot& slot : slots)
        for (const SlotCostume& costume : slot) {
            std::wstring name = shownName(costume);
            if (!uses(costume) || name.empty()) continue;
            std::wstring costumeName = lookup(variations, "variation_" + costume.code + "_" + ThreeDigits(costume.costume) + "_" + ThreeDigits(costume.preset));
            if (name == shownName(slot[0]) && !costumeName.empty()) name += L" (" + costumeName + L")";
            return name;
        }
    return lookup(characters, "chara_" + codes[0] + "_000");
}

// MSVC std::wstring, as the game's list entries hold it: an 8-char inline
// buffer or a pointer, then size and capacity (capacity 7 = inline).
struct GameWString {
    union { wchar_t inlineText[8]; const wchar_t* text; };
    size_t size;
    size_t capacity;
};
// One AVATAR_LIST entry (0x28 bytes, list opener template 0x4fcac0).
struct ListEntry {
    GameWString name;
    int32_t valid;    // 1 = selectable (0x5038f0 ignores the rest)
    int32_t unknown;  // 0 in every vanilla list
};
static_assert(sizeof(ListEntry) == 0x28, "AVATAR_LIST entry");
// The list's contents as 0x503970 takes them: default entry, then a
// std::vector<ListEntry> it copies (0x503220; only begin/end are read).
struct ListParam {
    int32_t    defaultIndex;
    int32_t    padding;
    ListEntry* begin;
    ListEntry* end;
    ListEntry* capacityEnd;
};

static std::vector<ListEntry> g_listEntries;  // built with g_styles; names point into it
// The same entries for a favorite style: "<name>\t★" (a tab, then a star), which the rebuilt
// AvatarList.as shows as the name and a gold star (only used while it is served).
static std::vector<std::wstring> g_favoriteNames;
static std::vector<ListEntry> g_favoriteEntries;

// A list entry showing `name`; long names point into `name`, which must
// outlive the entry (the list copies it when it is handed over).
static ListEntry ListEntryFor(const std::wstring& name)
{
    ListEntry entry{};
    entry.name.size = name.size();
    if (name.size() < 8) {
        memcpy(entry.name.inlineText, name.c_str(), (name.size() + 1) * sizeof(wchar_t));
        entry.name.capacity = 7;
    } else {
        entry.name.text = name.c_str();  // only read: the list copies it
        entry.name.capacity = name.size();
    }
    entry.valid = 1;
    return entry;
}
// config "style_search" (set by StyleMenu_Build): without it AVATAR_LIST is
// left alone and the list is not filtered.
static volatile LONG g_searchWanted = 0;
static bool SearchWanted() { return InterlockedCompareExchange(&g_searchWanted, 0, 0) != 0; }

// ------------------------------------------------------- custom styles ---
// Phase 5: styles described by data\battle_styles\styles\<Name>.xml, made by
// tools\moveset_to_style.py from a moveset mod (whose files it installs as
// new CMS entries through the LB Mod Installer) or written by hand:
//   <BattleStyle code="9A0" name="Gun Fu Rush">
//     <Race race="HUM" moveset="9A0" />
//     <Race race="HUF" moveset="9A1" />
//   </BattleStyle>
// `code` is what is equipped, saved and sent online (3 characters, like a
// CMS code; the setup record carries 4 bytes). Each <Race> names the CMS
// entry whose moveset a CaC of that race gets; race "ANY" serves races
// without their own line. The moveset is then fitted to the race like any
// other style. Listed under every "styles" setting (they are in no select
// slot, so "roster" and the DLC gate have nothing to check).
struct CustomStyle {
    std::string  code;
    std::wstring name;
    std::vector<std::pair<std::string, std::string>> movesets;  // race code or "ANY" -> CMS code
};
static std::vector<CustomStyle> g_customStyles;  // built by StyleMenu_Build, read-only afterwards
static const char kAnyRace[] = "ANY";

// The value of attribute `name` in the XML tag text `tag` ("" when absent),
// with the five XML entities decoded.
static std::string XmlAttribute(const std::string& tag, const char* name)
{
    const std::string key = std::string(" ") + name + "=\"";
    size_t at = tag.find(key);
    if (at == std::string::npos) return {};
    at += key.size();
    size_t end = tag.find('"', at);
    if (end == std::string::npos) return {};
    std::string value = tag.substr(at, end - at), decoded;
    static const std::pair<const char*, char> kEntities[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
    for (size_t i = 0; i < value.size(); ++i) {
        auto entity = std::find_if(std::begin(kEntities), std::end(kEntities),
                                   [&](const auto& known) { return !value.compare(i, strlen(known.first), known.first); });
        if (value[i] == '&' && entity != std::end(kEntities)) {
            decoded += entity->second;
            i += strlen(entity->first) - 1;
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}

// Every tag `<name ...>` in `text`, as its text up to the closing '>'.
static std::vector<std::string> XmlTags(const std::string& text, const char* name)
{
    std::vector<std::string> tags;
    const std::string open = std::string("<") + name;
    for (size_t at = text.find(open); at != std::string::npos; at = text.find(open, at + 1)) {
        char after = at + open.size() < text.size() ? text[at + open.size()] : 0;
        if (after != ' ' && after != '\t' && after != '\r' && after != '\n' && after != '/' && after != '>') continue;
        size_t end = text.find('>', at);
        if (end == std::string::npos) break;
        std::string tag = text.substr(at, end - at);
        std::replace_if(tag.begin(), tag.end(), [](char c) { return c == '\t' || c == '\r' || c == '\n'; }, ' ');
        tags.push_back(tag);
    }
    return tags;
}

static std::string Upper(std::string text)
{
    for (char& c : text) c = (char)toupper((unsigned char)c);
    return text;
}

// Reads data\battle_styles\styles\*.xml (loose files only: the LB Mod
// Installer puts them there). Lines naming no moveset are skipped here;
// StyleMenu_Build drops the ones whose moveset isn't in the CMS.
static std::vector<CustomStyle> ReadCustomStyles()
{
    std::vector<CustomStyle> styles;
    const std::wstring folder = g_gameDir + L"\\data\\battle_styles\\styles\\";
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW((folder + L"*.xml").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return styles;
    do {
        std::string text, file(MAX_PATH * 3, '\0');  // the file name as UTF-8, for the log
        file.resize(WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, file.data(), (int)file.size(), nullptr, nullptr));
        if (!file.empty()) file.pop_back();  // the terminator counted by -1
        if (!ReadWhole((folder + found.cFileName).c_str(), text)) continue;
        std::vector<std::string> roots = XmlTags(text, "BattleStyle");
        if (roots.empty()) { Log("styles: %s - no <BattleStyle> - skipped", file.c_str()); continue; }
        CustomStyle style;
        style.code = Upper(XmlAttribute(roots[0], "code"));
        std::string name = XmlAttribute(roots[0], "name");
        int chars = MultiByteToWideChar(CP_UTF8, 0, name.data(), (int)name.size(), nullptr, 0);
        style.name.resize(chars);
        MultiByteToWideChar(CP_UTF8, 0, name.data(), (int)name.size(), style.name.data(), chars);
        if (style.code.empty() || style.code.size() > 3) {
            Log("styles: %s - code \"%s\" must be 1 to 3 characters - skipped", file.c_str(), style.code.c_str());
            continue;
        }
        for (const std::string& line : XmlTags(text, "Race")) {
            std::string race = Upper(XmlAttribute(line, "race")), moveset = Upper(XmlAttribute(line, "moveset"));
            if (!race.empty() && !moveset.empty()) style.movesets.push_back({race, moveset});
        }
        styles.push_back(style);
    } while (FindNextFileW(search, &found));
    FindClose(search);
    return styles;
}

// The CMS code whose moveset a CaC (`charaId` 100..107, kRaceMovesets
// order) gets for the equipped `code`: for a custom style its race's line,
// else "ANY", else a race of the same sex (in the file's order), else its
// first line; any other code is returned as it is. Before the list is built
// a custom style's code is returned too - the converter makes it one of the
// style's own CMS entries, so it still loads (fitted).
std::string StyleMenu_MovesetFor(const std::string& code, int charaId)
{
    if (code.empty() || charaId < 100 || charaId > 107 || !StylesReady()) return code;
    auto style = std::find_if(g_customStyles.begin(), g_customStyles.end(),
                              [&](const CustomStyle& custom) { return !_stricmp(custom.code.c_str(), code.c_str()); });
    if (style == g_customStyles.end() || style->movesets.empty()) return code;
    const std::string race = kRaceMovesets[charaId - 100].code;
    // The female CaC races; every other one is male (Namekians and the Frieza race included).
    auto female = [](const std::string& raceCode) { return raceCode == "HUF" || raceCode == "SYF" || raceCode == "MAF"; };
    for (const auto& line : style->movesets) if (line.first == race) return line.second;
    for (const auto& line : style->movesets) if (line.first == kAnyRace) return line.second;
    for (const auto& line : style->movesets)
        if (line.first != kAnyRace && female(line.first) == female(race)) return line.second;
    return style->movesets[0].second;
}

void StyleMenu_Build(StyleChoice choice, const std::vector<std::string>& codes, const std::vector<std::string>& configExcluded,
                     const std::map<std::string, std::wstring>& configNames, bool search)
{
    InterlockedExchange(&g_searchWanted, search ? 1 : 0);
    g_styleChoice = choice;
    std::vector<uint8_t> cms, msg;
    std::string cmsFrom, msgFrom;
    if (!ReadGameFile("system/char_model_spec.cms", cms, cmsFrom)) { Log("styles: CMS not found - no style list"); return; }
    if (!ReadGameFile("msg/proper_noun_character_name_en.msg", msg, msgFrom)) Log("styles: character names not found - codes shown");
    std::map<std::string, std::wstring> names = ReadMsgNames(msg);

    // Every CMS entry with a body animation, BAC and combo tree, the first of
    // each distinct (EAN, BAC, BCM) set - variants sharing a moveset show once.
    // Sets whose files exist nowhere are left out: e.g. Fortune Teller Baba
    // (URN) names chara/URN/URN.ean, which no cpk or loose file has (1.26.01:
    // URN, SLM, PGS, YRN, GNE - ClaudeTools\battle_styles\missingfiles.py). The game
    // appends _PLAYER to the BAC and BCM names (path getter 0x2c5510).
    std::vector<std::string> withFiles;
    std::map<std::string, std::string> eanOf;  // first code -> its EAN ("GOK/GOK")
    std::map<std::string, const RaceMoveset*> raceOf;  // first code -> the race whose own moveset it is
    std::map<std::tuple<std::string, std::string, std::string>, std::string> firstCode;  // moveset -> its first code
    std::map<std::string, std::vector<std::string>> sharingCodes;  // first code -> every code using that moveset
    std::map<std::string, uint32_t> sizeClassOf;  // first code -> the largest size class of its codes
    std::map<std::string, int> cmsIdOf;           // every code -> its CMS id
    uint32_t count = Rd32(cms, kCmsCount), table = Rd32(cms, kCmsTable);
    for (uint32_t i = 0; i < count && table >= 0x10; ++i) {
        size_t entry = table + i * kCmsEntrySize;
        if (entry + kCmsEntrySize > cms.size()) break;
        std::string code((const char*)cms.data() + entry + kCmsCode, strnlen((const char*)cms.data() + entry + kCmsCode, 4));
        std::string ean = CmsFile(cms, entry, code, kEanField), bac = CmsFile(cms, entry, code, kBacField);
        std::string bcm = CmsFile(cms, entry, code, kBcmField);
        if (code.empty() || ean.empty() || bac.empty() || bcm.empty()) continue;
        cmsIdOf[code] = (int)Rd32(cms, entry + kCmsId);
        auto known = firstCode.emplace(std::make_tuple(ean, bac, bcm), code);
        const std::string first = known.first->second;
        sharingCodes[first].push_back(code);
        sizeClassOf[first] = (std::max)(sizeClassOf[first], Rd32(cms, entry + kCmsSizeClass));
        if (!known.second) continue;
        const std::string files[] = {"chara/" + ean + ".ean", "chara/" + bac + "_PLAYER.bac", "chara/" + bcm + "_PLAYER.bcm"};
        auto missing = std::find_if(std::begin(files), std::end(files), [](const std::string& file) { return !GameFileExists(file); });
        if (missing != std::end(files)) {
            Log("styles: %s left out - %s not found", code.c_str(), missing->c_str());
            g_leftOutStyles.insert(code);
            continue;
        }
        withFiles.push_back(code);
        eanOf[code] = ean;
        if (const RaceMoveset* race = RaceMovesetOf(ean, bac, bcm)) raceOf[code] = race;
    }

    // Config "exclude_styles": groups and CMS codes (any code sharing the
    // moveset). "fake": an EAN without attack animations (EanAttackAnimations);
    // "giants": a giant's (kGiantSizeClass; "giant" read the same); "race": a
    // CaC race's own moveset (kRaceMovesets). "styles": "valid" / "roster" /
    // "unlocked" add "fake" and "giants".
    std::vector<std::string> excluded = configExcluded;
    for (std::string& item : excluded)
        if (!_stricmp(item.c_str(), "giant")) item = "giants";
    if (choice == StyleChoice::Valid || choice == StyleChoice::Roster || choice == StyleChoice::Unlocked) excluded.insert(excluded.end(), {"fake", "giants"});
    auto excludes = [&](const char* entry) {
        return std::any_of(excluded.begin(), excluded.end(), [&](const std::string& item) { return !_stricmp(item.c_str(), entry); });
    };
    // Custom styles (ReadCustomStyles): each line's moveset must be a CMS
    // entry with its files. A moveset only custom styles use (all codes
    // sharing it are theirs) is listed under the style, not on its own.
    std::vector<CustomStyle> customStyles = ReadCustomStyles();
    std::set<std::string> customMovesets;
    for (CustomStyle& style : customStyles) {
        if (cmsIdOf.count(style.code) &&
            std::none_of(style.movesets.begin(), style.movesets.end(), [&](const auto& line) { return line.second == style.code; })) {
            Log("styles: custom style %s left out - its code is a CMS character with another moveset", style.code.c_str());
            style.movesets.clear();
            continue;
        }
        auto unusable = [&](const std::pair<std::string, std::string>& line) {
            auto shared = std::find_if(sharingCodes.begin(), sharingCodes.end(), [&](const auto& set) {
                return std::find(set.second.begin(), set.second.end(), line.second) != set.second.end();
            });
            if (shared == sharingCodes.end() || std::find(withFiles.begin(), withFiles.end(), shared->first) == withFiles.end()) {
                Log("styles: custom style %s - %s line: \"%s\" is no CMS moveset with its files - skipped", style.code.c_str(),
                    line.first.c_str(), line.second.c_str());
                return true;
            }
            return false;
        };
        style.movesets.erase(std::remove_if(style.movesets.begin(), style.movesets.end(), unusable), style.movesets.end());
        for (const auto& line : style.movesets) customMovesets.insert(line.second);
        if (style.movesets.empty()) Log("styles: custom style %s left out - no usable moveset", style.code.c_str());
    }
    customStyles.erase(std::remove_if(customStyles.begin(), customStyles.end(), [](const CustomStyle& style) { return style.movesets.empty(); }),
                       customStyles.end());

    std::vector<std::string> roster;
    ULONGLONG checkStarted = GetTickCount64();
    for (const std::string& code : withFiles) {
        const std::string& ean = eanOf[code];
        const std::vector<std::string>& sharers = sharingCodes[code];
        if (std::all_of(sharers.begin(), sharers.end(), [&](const std::string& other) { return customMovesets.count(other) != 0; }))
            continue;  // listed as its custom style
        const char* reason = nullptr;
        auto sharer = std::find_if(sharingCodes[code].begin(), sharingCodes[code].end(), [&](const std::string& other) { return excludes(other.c_str()); });
        if (sharer != sharingCodes[code].end()) reason = "excluded in the config";
        else if (excludes("race") && raceOf.count(code))
            reason = "a race's own moveset";
        else if (excludes("giants") && sizeClassOf[code] >= kGiantSizeClass)
            reason = "a giant's (CMS size class 5+)";
        else if (excludes("fake")) {
            EanAttacks attacks = EanAttackAnimations("chara/" + ean + ".ean");
            if (attacks == EanAttacks::None) reason = "fake (no attack animations)";
            else if (attacks == EanAttacks::Unreadable) Log("styles: %s - chara/%s.ean unreadable, kept", code.c_str(), ean.c_str());
        }
        if (reason) {
            Log("styles: %s left out - %s", code.c_str(), reason);
            g_leftOutStyles.insert(code);
            continue;
        }
        roster.push_back(code);
    }
    Log("styles: %zu of %zu movesets left out by \"exclude_styles\"/\"styles\" (%llu ms)", withFiles.size() - roster.size(),
        withFiles.size(), GetTickCount64() - checkStarted);

    std::vector<uint8_t> variationMsg;
    std::string slotsFrom, variationsFrom;
    if (!ReadGameFile("msg/proper_noun_variation_name_en.msg", variationMsg, variationsFrom)) Log("styles: costume names not found");
    std::map<std::string, std::wstring> variations = ReadMsgNames(variationMsg);
    std::vector<SelectSlot> slots = ReadSelectSlots(slotsFrom);
    if (slots.empty()) Log("styles: character select slots not found - names from the CMS codes only");

    std::vector<std::string> offered;
    if (choice == StyleChoice::List) {
        for (const std::string& wanted : codes) {
            auto found = std::find_if(roster.begin(), roster.end(), [&](const std::string& code) { return !_stricmp(code.c_str(), wanted.c_str()); });
            if (found != roster.end()) offered.push_back(*found);
            else if (std::any_of(g_leftOutStyles.begin(), g_leftOutStyles.end(), [&](const std::string& code) { return !_stricmp(code.c_str(), wanted.c_str()); }))
                Log("styles: \"%s\" is left out (see above) - skipped", wanted.c_str());
            else Log("styles: \"%s\" is not a moveset in the CMS - skipped", wanted.c_str());
        }
    } else {
        offered = roster;
    }

    g_styles.push_back({"", L"Default"});
    std::map<std::wstring, int> nameUses;
    size_t notInSelect = 0;
    for (const std::string& code : offered) {
        Style style{code, raceOf.count(code) ? raceOf[code]->name : MovesetName(sharingCodes[code], slots, names, variations)};
        // Config "style_names" (case-insensitive code of any character sharing the moveset) wins over the automatic name.
        const std::vector<std::string>& sharerCodes = sharingCodes[code];
        auto configName = std::find_if(configNames.begin(), configNames.end(), [&](const auto& named) {
            return std::any_of(sharerCodes.begin(), sharerCodes.end(), [&](const std::string& other) { return !_stricmp(other.c_str(), named.first.c_str()); });
        });
        if (configName != configNames.end()) style.name = configName->second;
        if (style.name.empty()) {
            Log("styles: %s has no name - shown as its code (name it in \"style_names\")", code.c_str());
            style.name.assign(code.begin(), code.end());
        }
        if (raceOf.count(code)) style.raceOrder = (int)(raceOf[code] - kRaceMovesets);
        const std::vector<std::string>& sharers = sharingCodes[code];
        for (const SelectSlot& slot : slots)
            for (const SlotCostume& costume : slot)
                if (std::find(sharers.begin(), sharers.end(), costume.code) != sharers.end())
                    style.selectCostumes.push_back({cmsIdOf[costume.code], costume.costume, costume.dlcKey1, costume.dlcKey2});
        // "roster" / "unlocked": only characters the select shows.
        if ((choice == StyleChoice::Roster || choice == StyleChoice::Unlocked) && style.raceOrder < 0 && style.selectCostumes.empty()) {
            Log("styles: %s left out - in no character select slot", code.c_str());
            g_leftOutStyles.insert(code);
            ++notInSelect;
            continue;
        }
        ++nameUses[style.name];
        g_styles.push_back(style);
    }
    // Custom styles: under every setting; a config list ("styles": [...]) only those it names.
    for (const CustomStyle& custom : customStyles) {
        if (choice == StyleChoice::List &&
            std::none_of(codes.begin(), codes.end(), [&](const std::string& wanted) { return !_stricmp(wanted.c_str(), custom.code.c_str()); }))
            continue;
        Style style{custom.code, custom.name};
        auto configName = std::find_if(configNames.begin(), configNames.end(),
                                       [&](const auto& named) { return !_stricmp(named.first.c_str(), custom.code.c_str()); });
        if (configName != configNames.end()) style.name = configName->second;
        if (style.name.empty()) style.name.assign(custom.code.begin(), custom.code.end());
        std::string lines;
        for (const auto& line : custom.movesets) lines += " " + line.first + "=" + line.second;
        Log("styles: custom style %s:%s", custom.code.c_str(), lines.c_str());
        ++nameUses[style.name];
        g_styles.push_back(style);
    }
    g_customStyles = customStyles;
    // Same name twice (several Gokus): add the code so the entries differ.
    for (Style& style : g_styles)
        if (nameUses[style.name] > 1) style.name += L" (" + std::wstring(style.code.begin(), style.code.end()) + L")";
    // By name, the race movesets last in kRaceMovesets order; "Default" stays
    // first. A list from the config keeps its order.
    if (choice != StyleChoice::List)
        std::stable_sort(g_styles.begin() + 1, g_styles.end(), [](const Style& a, const Style& b) {
            if ((a.raceOrder >= 0) != (b.raceOrder >= 0)) return b.raceOrder >= 0;
            if (a.raceOrder >= 0) return a.raceOrder < b.raceOrder;
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });

    for (const Style& style : g_styles) g_favoriteNames.push_back(style.name + L"\t★");
    for (size_t i = 0; i < g_styles.size(); ++i) {  // both vectors are complete: no reallocation moves the names now
        g_listEntries.push_back(ListEntryFor(g_styles[i].name));
        g_favoriteEntries.push_back(ListEntryFor(g_favoriteNames[i]));
    }
    InterlockedExchange(&g_stylesReady, 1);
    Log("styles: %zu offered of %zu roster movesets + %zu custom (%zu in no select slot; CMS from %s, names from %s, %zu select slots from %s)",
        g_styles.size() - 1, roster.size(), customStyles.size(), notInSelect, cmsFrom.c_str(), msgFrom.empty() ? "-" : msgFrom.c_str(), slots.size(),
        slotsFrom.empty() ? "-" : slotsFrom.c_str());
}

static bool StylesReady() { return InterlockedCompareExchange(&g_stylesReady, 0, 0) != 0; }
// ------------------------------------- equipped, per account/slot/preset ---
// equipped_styles.sav (JSON text; .sav so a tool that shares the mod's .json
// configs between players treats it as save data and leaves each player's
// choices alone): Steam account -> save
// character slot -> preset ->
// CMS code. Presets are the game's 8 loadout records per character (0x68
// bytes each at character + 0xf4, copied by 0x5064a0): record 0 is the main
// (current) loadout everything uses, 1..7 the saved presets. The equip slot
// and battles use preset 0; saving or loading a preset copies the style
// along with the record (EditMyset hook below).
//   {"76561198000000000": {"3": {"0": "GNY", "2": "GOK"}}}
// Files from before presets ({"<slot>": "<code>"}) are read as preset 0 of
// the current account and rewritten in this form on the next change.

// The save manager: loaded character slot = int at [0x15545b0] + 8 + 0x44,
// 0..7, already set at the title screen (Research_Notes\save-and-flags.md).
static const uintptr_t kSaveManager = 0x15545b0, kLoadedSlot = 8 + 0x44;
static const int kCharacterSlots = 8;
static const int kPresets = 8;      // records per character (EditMyset, 0x5064a0)
static const int kMainPreset = 0;   // the loadout in use

typedef std::map<int, std::string> PresetStyles;    // preset -> CMS code
typedef std::map<int, PresetStyles> SlotStyles;     // character slot -> presets
static SRWLOCK g_equippedLock = SRWLOCK_INIT;
static std::map<std::string, SlotStyles> g_equipped;  // Steam account -> slots; every account in the file
static std::string g_account;                          // the playing account, once known
static bool g_equippedLoaded = false;

static int LoadedSlot()
{
    uintptr_t manager = *(const uintptr_t*)(g_base + kSaveManager);
    if (!manager) return -1;
    int slot = *(const int*)(manager + kLoadedSlot);
    return slot >= 0 && slot < kCharacterSlots ? slot : -1;
}

// The loaded save's decrypted image, kept whole by the save/system singleton
// [0x1554448] at +0x68 (1.26.01; matched byte for byte against the decrypted
// DBXV2.sav, ClaudeTools\save\steamid_probe.py). The image starts "#SAV" and holds
// the owning Steam id at +8, XORed with "thebecon" (Xv2CoreLib SAV_File.cs).
static const uintptr_t kSaveSystem = 0x1554448;
static const uintptr_t kSaveImage = 0x68;
static const uintptr_t kSaveImageSteamId = 8;
static const uint64_t kSteamIdXor = 0x7468656265636F6E;

// The save's Steam account (64-bit id as text), or empty while no save is
// loaded yet. Read from the save rather than Steam: saves are per account.
static std::string SteamAccount()
{
    uintptr_t saveSystem = *(const uintptr_t*)(g_base + kSaveSystem);
    if (!saveSystem) return {};
    const char* image = (const char*)(saveSystem + kSaveImage);
    if (memcmp(image, "#SAV", 4) != 0) return {};
    uint64_t id = *(const uint64_t*)(image + kSaveImageSteamId) ^ kSteamIdXor;
    return id ? std::to_string(id) : std::string();
}

static std::wstring EquippedPath() { return std::wstring(g_dir) + L"\\equipped_styles.sav"; }
// The file's name before 2026-10; renamed to the .sav once, if no .sav exists yet.
static std::wstring OldEquippedPath() { return std::wstring(g_dir) + L"\\equipped_styles.json"; }

// Reads one slot's presets: {"<preset>": "<code>"}.
static PresetStyles ReadPresets(const JVal& presets)
{
    PresetStyles styles;
    if (presets.type != JVal::Obj) return styles;
    for (const auto& [key, code] : presets.o) {
        int preset = atoi(key.c_str());
        if (code.type == JVal::Str && !code.s.empty() && preset >= 0 && preset < kPresets) styles[preset] = code.s;
    }
    return styles;
}

// Caller holds g_equippedLock exclusively and has set g_account.
static void LoadEquipped()
{
    g_equippedLoaded = true;
    std::string text, error;
    JVal root;
    if (MoveFileExW(OldEquippedPath().c_str(), EquippedPath().c_str(), 0))  // fails when there's no old file or a .sav already
        Log("equipped_styles.json renamed to equipped_styles.sav");
    if (!ReadWhole(EquippedPath().c_str(), text)) return;  // nothing equipped yet
    if (!JsonParse(text, root, error) || root.type != JVal::Obj) { Log("equipped_styles.sav unreadable (%s) - ignored", error.c_str()); return; }
    int legacy = 0;
    for (const auto& [key, value] : root.o) {
        if (value.type == JVal::Str) {  // before presets: "<slot>": "<code>"
            int slot = atoi(key.c_str());
            if (!value.s.empty() && slot >= 0 && slot < kCharacterSlots) {
                g_equipped[g_account][slot][kMainPreset] = value.s;
                ++legacy;
            }
        } else if (value.type == JVal::Obj) {  // "<account>": {"<slot>": {...}}
            for (const auto& [slotKey, presets] : value.o) {
                int slot = atoi(slotKey.c_str());
                PresetStyles styles = ReadPresets(presets);
                if (slot >= 0 && slot < kCharacterSlots && !styles.empty()) g_equipped[key][slot] = styles;
            }
        }
    }
    if (legacy) Log("equipped_styles.sav: %d slot(s) in the old form taken as account %s, main preset", legacy, g_account.c_str());
}

// Caller holds g_equippedLock exclusively. Written to a temporary file
// first so a crash never leaves half a file.
static void SaveEquipped()
{
    std::string text = "{";
    const char* accountComma = "";
    for (const auto& [account, slots] : g_equipped) {
        if (slots.empty()) continue;
        text += std::string(accountComma) + "\n    \"" + account + "\": {";
        accountComma = ",";
        const char* slotComma = "";
        for (const auto& [slot, presets] : slots) {
            text += std::string(slotComma) + "\n        \"" + std::to_string(slot) + "\": {";
            slotComma = ",";
            const char* presetComma = "";
            for (const auto& [preset, code] : presets) {
                text += std::string(presetComma) + "\"" + std::to_string(preset) + "\": \"" + code + "\"";
                presetComma = ", ";
            }
            text += "}";
        }
        text += "\n    }";
    }
    text += "\n}\n";
    std::wstring path = EquippedPath(), temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) { Log("equipped_styles.sav: cannot write"); return; }
    DWORD written = 0;
    bool ok = WriteFile(file, text.data(), (DWORD)text.size(), &written, nullptr) && written == text.size();
    CloseHandle(file);
    if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) Log("equipped_styles.sav: cannot write");
}

// Takes g_equippedLock and returns the playing account's slots, loading the
// file on first use; null (lock released) while the account is not known -
// nothing is read or written until it is, so no style lands on the wrong
// account. Release with ReleaseSRWLockExclusive(&g_equippedLock).
static SlotStyles* LockAccount()
{
    AcquireSRWLockExclusive(&g_equippedLock);
    if (g_account.empty()) {
        g_account = SteamAccount();
        if (g_account.empty()) {
            ReleaseSRWLockExclusive(&g_equippedLock);
            static volatile LONG warned = 0;
            if (!InterlockedExchange(&warned, 1)) Log("Steam account not known yet (no save loaded) - styles off until it is");
            return nullptr;
        }
        Log("Steam account %s (from the save)", g_account.c_str());
    }
    if (!g_equippedLoaded) LoadEquipped();
    return &g_equipped[g_account];
}

// The code in one slot's preset, or empty.
static std::string PresetCode(const SlotStyles& slots, int slot, int preset)
{
    auto presets = slots.find(slot);
    if (presets == slots.end()) return {};
    auto code = presets->second.find(preset);
    return code != presets->second.end() ? code->second : std::string();
}

// Sets (code non-empty) or clears one slot's preset; true when it changed.
static bool SetPresetCode(SlotStyles& slots, int slot, int preset, const std::string& code)
{
    if (PresetCode(slots, slot, preset) == code) return false;
    if (code.empty()) {
        slots[slot].erase(preset);
        if (slots[slot].empty()) slots.erase(slot);
    } else {
        slots[slot][preset] = code;
    }
    return true;
}

std::string StyleMenu_PresetCode(int slot, int preset)
{
    if (slot == -1) slot = LoadedSlot();
    if (slot < 0 || slot >= kCharacterSlots || preset < 0 || preset >= kPresets) return {};
    SlotStyles* slots = LockAccount();
    if (!slots) return {};
    std::string code = PresetCode(*slots, slot, preset);
    ReleaseSRWLockExclusive(&g_equippedLock);
    // Equipped before its files were found missing (e.g. Baba from an older
    // build): play the character's own moveset instead.
    if (StylesReady() && g_leftOutStyles.count(code)) return {};
    return code;
}

std::string StyleMenu_EquippedCode(int slot)
{
    if (slot < 0 || slot >= kCharacterSlots) return {};
    return StyleMenu_PresetCode(slot, kMainPreset);
}

std::string StyleMenu_EquippedCode()
{
    int slot = LoadedSlot();
    return StyleMenu_EquippedCode(slot);
}

// The name shown for a style code: "Default" for none, the code itself for
// one no longer offered (or before the list is built).
static std::wstring StyleName(const std::string& code)
{
    if (code.empty()) return L"Default";
    if (StylesReady())
        for (size_t i = 1; i < g_styles.size(); ++i)
            if (!_stricmp(g_styles[i].code.c_str(), code.c_str())) return g_styles[i].name;
    return std::wstring(code.begin(), code.end());
}

// Index of the loaded slot's style in g_styles (0 = Default, also for a
// code no longer offered).
static int EquippedIndex()
{
    std::string code = StyleMenu_EquippedCode();
    for (size_t i = 1; i < g_styles.size(); ++i)
        if (!_stricmp(g_styles[i].code.c_str(), code.c_str())) return (int)i;
    return 0;
}

// The equip slot's pick goes to the loaded character's main preset.
static void Equip(int index)
{
    int slot = LoadedSlot();
    if (slot < 0 || index < 0 || index >= (int)g_styles.size()) return;
    SlotStyles* slots = LockAccount();
    if (!slots) return;
    if (SetPresetCode(*slots, slot, kMainPreset, g_styles[index].code)) SaveEquipped();
    ReleaseSRWLockExclusive(&g_equippedLock);
    Log("slot %d: style %s", slot, index ? g_styles[index].code.c_str() : "Default");
}

// The game copied loadout record `from` over `to` for the loaded character
// (preset load: n -> 0, preset save: 0 -> n); the style goes with it.
static void CopyPreset(int from, int to)
{
    int slot = LoadedSlot();
    if (slot < 0 || from < 0 || from >= kPresets || to < 0 || to >= kPresets || from == to) return;
    SlotStyles* slots = LockAccount();
    if (!slots) return;
    std::string code = PresetCode(*slots, slot, from);
    if (SetPresetCode(*slots, slot, to, code)) SaveEquipped();
    ReleaseSRWLockExclusive(&g_equippedLock);
    Log("slot %d: preset %d -> %d, style %s", slot, from, to, code.empty() ? "Default" : code.c_str());
}

// ------------------------------------------------------- the menu iggys ---
// File request class (other mods use the same hook, chained under
// one mutex): vtable slots 3/4 return the file's data, 6/7 its size; the
// path is stored inline at +8 (up to 0x118 chars).
static const uintptr_t kRequestVtable = 0xf9b270;
static const size_t kRequestPath = 8, kRequestPathMax = 0x118;

// The menus served with a rebuilt ActionScript (ClaudeTools\iggy\menu_abc.py),
// each only over a script it was built from.
struct AbcVariant {
    const char*    source;      // whose script it is, for the log
    uint32_t       vanillaAbcSize;
    uint64_t       vanillaAbcHash;
    const uint8_t* abc;
    size_t         abcSize;
};
struct MenuIggy {
    const char*       pathSuffix;  // lower case, '/' (data/ui/iggy/<NAME>.iggy)
    const char*       fileName;    // for the log
    const char*       lostFeature; // logged when it cannot be patched
    const AbcVariant* variants;
    size_t            variantCount;
    const AbcVariant* searchVariants;      // instead, while Chara Search is active (null: none)
    size_t            searchVariantCount;
    volatile LONG     servedPatched;  // the last copy the game got was the rebuilt one
};
static const AbcVariant kAvatarVariants[] = {
    {"vanilla", kVanillaAvatarAbcSize, kVanillaAvatarAbcHash, kAvatarAbc, sizeof(kAvatarAbc)}};
// xv2ins 4.7 installs its own loose CHARASELE.iggy (its CharaSele calls an
// XV2Patcher class), so everyone with xv2patcher has that one, not the cpk's.
static const AbcVariant kCharaSeleVariants[] = {
    {"vanilla", kVanillaCharaSeleAbcSize, kVanillaCharaSeleAbcHash, kCharaSeleAbc, sizeof(kCharaSeleAbc)},
    {"xv2patcher 4.7", kVanillaCharaSelePatcherAbcSize, kVanillaCharaSelePatcherAbcHash, kCharaSelePatcherAbc,
     sizeof(kCharaSelePatcherAbc)}};
// With Chara Search's search in it, over the game's scripts or the ones
// Chara Search serves (its file hook may run before this one).
static const AbcVariant kCharaSeleSearchVariants[] = {
    {"vanilla", kVanillaCharaSeleSearchAbcSize, kVanillaCharaSeleSearchAbcHash, kCharaSeleSearchAbc,
     sizeof(kCharaSeleSearchAbc)},
    {"xv2patcher 4.7", kVanillaCharaSelePatcherSearchAbcSize, kVanillaCharaSelePatcherSearchAbcHash,
     kCharaSelePatcherSearchAbc, sizeof(kCharaSelePatcherSearchAbc)},
    {"Chara Search's", kCharaSearchCharaSeleAbcSize, kCharaSearchCharaSeleAbcHash, kCharaSeleSearchAbc,
     sizeof(kCharaSeleSearchAbc)},
    {"Chara Search's xv2patcher 4.7", kCharaSearchCharaSelePatcherAbcSize, kCharaSearchCharaSelePatcherAbcHash,
     kCharaSelePatcherSearchAbc, sizeof(kCharaSelePatcherSearchAbc)}};
static const AbcVariant kAvatarListVariants[] = {
    {"vanilla", kVanillaAvatarListAbcSize, kVanillaAvatarListAbcHash, kAvatarListAbc, sizeof(kAvatarListAbc)}};
static const AbcVariant kMysetVariants[] = {
    {"vanilla", kVanillaMysetAbcSize, kVanillaMysetAbcHash, kMysetAbc, sizeof(kMysetAbc)}};
static MenuIggy g_avatarIggy = {"ui/iggy/avatar.iggy", "AVATAR.iggy", "no Battle Style slot",
                                kAvatarVariants, _countof(kAvatarVariants), nullptr, 0, 0};
static MenuIggy g_charaSeleIggy = {"ui/iggy/charasele.iggy", "CHARASELE.iggy", "no Battle Style row in the skill popup",
                                   kCharaSeleVariants, _countof(kCharaSeleVariants),
                                   kCharaSeleSearchVariants, _countof(kCharaSeleSearchVariants), 0};
static MenuIggy g_mysetIggy = {"ui/iggy/avatar_set.iggy", "AVATAR_SET.iggy", "no Battle Style row in the Presets panel",
                               kMysetVariants, _countof(kMysetVariants), nullptr, 0, 0};
// AVATAR_LIST is also the game's own list in the other EditChara modes
// (character creation etc.); the rebuilt script behaves as the vanilla one
// unless Battle Styles sends a count again.
static MenuIggy g_avatarListIggy = {"ui/iggy/avatar_list.iggy", "AVATAR_LIST.iggy",
                                   "the style list shows search matches only when it is opened again",
                                   kAvatarListVariants, _countof(kAvatarListVariants), nullptr, 0, 0};
static MenuIggy* const g_menuIggys[] = {&g_avatarIggy, &g_charaSeleIggy, &g_mysetIggy, &g_avatarListIggy};

// Whether the menu's AS is ours, so it splits off a style appended to a
// string; the vanilla one would show the tab and the name.
static bool ServedPatched(const MenuIggy& iggy) { return InterlockedCompareExchange((volatile LONG*)&iggy.servedPatched, 0, 0) != 0; }

typedef void*    (__fastcall* GetDataFn)(void* request);
typedef uint64_t (__fastcall* GetSizeFn)(void* request);
static GetDataFn g_origData18 = nullptr, g_origData20 = nullptr;
static GetSizeFn g_origSize30 = nullptr, g_origSize38 = nullptr;

// Patched copies, per source content and script set. Kept for the whole
// session: the game may hold on to a buffer it was given.
static SRWLOCK g_servedLock = SRWLOCK_INIT;
static std::map<std::tuple<uint64_t, uint64_t, bool>, std::vector<uint8_t>*> g_served;  // (hash, size, search) -> copy or null

// Whether Chara Search is loaded and has its hooks in, asked each time a
// menu file is served: the Lua script (chara_search.lua) patches CHARASELE
// through the loader's library under its LUA_NAME "CharaSearch"
// (XV2Files_Owner); older versions were CharaSearch.dll (CharaSearch_Active).
static bool CharaSearchActive()
{
    if (HMODULE library = GetModuleHandleW(L"XV2ModLoaderLib.dll")) {
        auto owner = (int (*)(const char*, char*, int))GetProcAddress(library, "XV2Files_Owner");
        char name[64] = {};
        if (owner && owner("ui/iggy/charasele.iggy", name, sizeof(name)) > 0 && strcmp(name, "CharaSearch") == 0)
            return true;
    }
    HMODULE module = GetModuleHandleW(L"CharaSearch.dll");
    auto active = module ? (int (*)())GetProcAddress(module, "CharaSearch_Active") : nullptr;
    return active && active() != 0;
}

// The menu file a request is for, or null. The game's requests use
// backslashes (data\ui\iggy\AVATAR.iggy, seen in live request objects), so
// '/' and '\' compare equal, case-insensitively.
static MenuIggy* RequestedMenu(void* request)
{
    const char* path = (const char*)request + kRequestPath;
    size_t length = strnlen(path, kRequestPathMax);
    for (MenuIggy* iggy : g_menuIggys) {
        if (iggy == &g_avatarListIggy && !SearchWanted()) continue;
        size_t suffix = strlen(iggy->pathSuffix);
        if (length < suffix + 1) continue;
        const char* tail = path + length - suffix;
        bool same = tail[-1] == '/' || tail[-1] == '\\';
        for (size_t i = 0; same && i < suffix; ++i) {
            char have = tail[i] == '\\' ? '/' : (char)tolower((unsigned char)tail[i]);
            same = have == iggy->pathSuffix[i];
        }
        if (same) return iggy;
    }
    return nullptr;
}

static std::vector<uint8_t>* Lookup(void* request, void* data, uint64_t size)
{
    MenuIggy* iggy = data ? RequestedMenu(request) : nullptr;
    if (!iggy) return nullptr;
    uint64_t hash = Fnv64((const uint8_t*)data, (size_t)size);
    bool search = iggy->searchVariants && CharaSearchActive();
    const AbcVariant* variants = search ? iggy->searchVariants : iggy->variants;
    size_t variantCount = search ? iggy->searchVariantCount : iggy->variantCount;
    AcquireSRWLockExclusive(&g_servedLock);
    auto found = g_served.find({hash, size, search});
    std::vector<uint8_t>* served;
    if (found != g_served.end()) {
        served = found->second;
    } else {
        // Try each script the menu has a rebuild for; the notes say why each
        // one did not match.
        served = nullptr;
        std::string notes;
        for (size_t i = 0; i < variantCount && !served; ++i) {
            const AbcVariant& variant = variants[i];
            auto copy = new std::vector<uint8_t>;
            std::string note;
            if (ReplaceIggyAbc((const uint8_t*)data, (size_t)size, variant.vanillaAbcSize, variant.vanillaAbcHash,
                               variant.abc, variant.abcSize, *copy, note)) {
                served = copy;
                notes = std::string(variant.source) + ": " + note;
            } else {
                delete copy;
                notes += (notes.empty() ? "" : "; ") + std::string(variant.source) + ": " + note;
            }
        }
        if (served) Log("%s (%llu bytes): %s%s", iggy->fileName, (unsigned long long)size, notes.c_str(),
                        search ? " (with Chara Search's search)" : "");
        else Log("%s (%llu bytes): NOT patched - %s: %s", iggy->fileName, (unsigned long long)size, iggy->lostFeature, notes.c_str());
        g_served[{hash, size, search}] = served;
    }
    ReleaseSRWLockExclusive(&g_servedLock);
    InterlockedExchange(&iggy->servedPatched, served ? 1 : 0);
    return served;
}

// Every mod in the file-request chain calls the next one's data and size;
// fetching them for files this mod never edits multiplied the calls ~600x
// down a six-mod chain (host hitches online), so the path is checked first.
static void* Serve(void* request, void* data)
{
    if (!data || !RequestedMenu(request)) return data;
    std::vector<uint8_t>* served = Lookup(request, data, g_origSize38(request));
    return served ? served->data() : data;
}
static uint64_t ServeSize(void* request, uint64_t size)
{
    if (!RequestedMenu(request)) return size;
    void* data = g_origData20(request);
    std::vector<uint8_t>* served = Lookup(request, data, data ? g_origSize38(request) : 0);
    return served ? served->size() : size;
}
// The four hooked slots, named after their byte offset in the table.
static void*    __fastcall HkData18(void* request) { return Serve(request, g_origData18(request)); }
static void*    __fastcall HkData20(void* request) { return Serve(request, g_origData20(request)); }
static uint64_t __fastcall HkSize30(void* request) { return ServeSize(request, g_origSize30(request)); }
static uint64_t __fastcall HkSize38(void* request) { return ServeSize(request, g_origSize38(request)); }

static bool InstallFileHook()
{
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

// ------------------------------------------------------- the equip menu ---
// AS3 <-> exe numbers, matching as3\Avatar.as. The in-game Customize menu
// runs in mode 3 (seen live with ClaudeTools\battle_styles\editchara.py).
static const int kSendRequestBattleStyle = 240; // SendTypeRequestListBattleStyle (vanilla ones end at 230)
static const int kUserDataStyleLabel = 96;     // ReceiveTypeBattleStyleStr
static const int kUserDataStyleCount = 97;     // ReceiveTypeBattleStyleNum: > 0 = slot active

// OggyBase user data (vtable 0xf98020): bounds-checked setters; the string
// one keeps the POINTER, so it must stay alive (g_styles' names do).
typedef void (__fastcall* SetUserDataIntFn)(void* menu, int index, int value);
typedef void (__fastcall* SetUserDataStrFn)(void* menu, int index, const wchar_t* text);
static const uintptr_t kSetUserDataInt = 0x2c81a0, kSetUserDataStr = 0x2c8240;

// EditChara (the Customize menu, vtable 0x10a0098).
static const uintptr_t kEditCharaVtable = 0x10a0098;
static const int kUserCallbackSlot = 17;              // +0x88: AS CallbackUserData("user", sendType, value)
static const uintptr_t kUserCallback = 0x4f72c0;      // vanilla entry of that slot
static const size_t kCallArgCount = 0x10, kCallArg0 = 0x28;  // argc; arg 0 as a double (0x4f72c0)
static const size_t kEditList = 0x6c0;        // EditList* (AVATAR_LIST)
static const size_t kStateMachine = 0x6b8;    // 0x652680(sm, state, 0, 0) switches the menu's state
static const size_t kMenuMode = 0x1f4;        // 3 = Customize in game, 7 = the other Customize list
static const size_t kOpenListType = 0x2f8;    // sendType of the open list
static const int kNoListOpen = 0x192;         // +0x2f8 when none is (0x4fdbaa)

static const uintptr_t kSetListContents = 0x503970;  // (EditList*, ListParam*)
static const uintptr_t kSetMenuState = 0x652680;      // (state machine, state, 0, 0)
static const uintptr_t kListHasResult = 0x503950, kListGetResult = 0x503940;
typedef void (__fastcall* SetListContentsFn)(void* list, const ListParam* param);
typedef void (__fastcall* SetMenuStateFn)(void* stateMachine, int state, int, int);
typedef bool (__fastcall* ListHasResultFn)(void* list);
typedef int  (__fastcall* ListGetResultFn)(void* list);

typedef void (__fastcall* UserCallbackFn)(void* menu, const uint8_t* call);
static UserCallbackFn g_origUserCallback = nullptr;

template <typename Fn> static Fn Game(uintptr_t rva) { return (Fn)(g_base + rva); }
template <typename T> static T& Field(void* object, size_t offset) { return *(T*)((uint8_t*)object + offset); }

// Shows the equipped style on the button (the AS waits for slot 96 to be
// sent again before it leaves a list).
static void ShowEquipped(void* menu)
{
    int index = StylesReady() ? EquippedIndex() : 0;
    const wchar_t* label = StylesReady() ? g_styles[index].name.c_str() : L"Default";
    Game<SetUserDataStrFn>(kSetUserDataStr)(menu, kUserDataStyleLabel, label);
    Game<SetUserDataIntFn>(kSetUserDataInt)(menu, kUserDataStyleCount, 1);
}

// The menu state to show an open list in, by menu mode - copied from the
// opener template 0x4fcac0 (0x4fcdb5..0x4fce04). They are the modes' main
// edit states: the list poll's close tail (0x4fdc1b) goes back to the same
// ones, the list itself takes the input while open.
static int ListState(int mode, int listType)
{
    switch (mode) {
    case 3: return 0x27;
    case 1: case 5: return 0x21;
    case 2: case 6: return 0x28;
    case 8: return 0x35 + (listType != kNoListOpen);
    case 4: return 0x34;
    default: return 5;  // 7
    }
}

// ------------------------------------------------ the style list's search ---
// The loader's search box, the same one Item Search shows on this screen
// (item_search.lua BOX_ID, BOX_HINT, BOX_X/BOX_Y, BOX_WIDTH): with both mods
// installed it is one box with one query, and either keeps it up alone.
static const char* const kSearchId = "equipment_search";
static const char* const kSearchHint = "Search (Ctrl+F)";
static const float kSearchX = 0.22f, kSearchY = 0.17f, kSearchWidth = 0.30f;  // fractions of the picture
static const char* const kNoStyleMatches = "No style matches";

// EditList (AVATAR_LIST) fields and calls besides 0x503970:
// * +0x1b4 flags: 1 = exited (0x6690e0), 2 = cancelled (0x6690a0), 2|4 =
//   decided (0x6690c0); 0x503970 clears all three, so the list is only
//   refilled while none is set.
// * +0x210 the result: the entry of the last cursor move (the AS sends
//   every move, handler 0x5038f0 - no bounds check, so the entries must
//   always match the count the AS has), the default on cancel (0x5038a0);
//   -1 again after the list poll (g_shownCursor keeps the cursor).
// * user data 18 (ReceiveType_ListDefault): the X button's entry, sent by
//   0x503a50 (also kept at +0x214) from the game's list opener; the rebuilt
//   AvatarList.as reads it as the cursor after a refresh, then puts the X
//   entry back.
static const size_t kListFlags = 0x1b4, kListResult = 0x210;
static const uint32_t kListEnded = 1 | 2 | 4;
static const uintptr_t kSetListXEntry = 0x503a50;  // (EditList*, index)
static const int kListUserDataCursor = 18;
typedef void (__fastcall* SetListXEntryFn)(void* list, int index);

// All on the game thread (the AS callback and the list poll).
static XV2Search g_search;
static int g_searchLoaded = 0;                 // 0 = not tried yet, 1 = the loader has the box, -1 = no box
static std::vector<ListEntry> g_shownEntries;  // what the list was given
static std::vector<int> g_shownStyles;         // list index -> g_styles index
static unsigned g_shownChanges = 0;            // the box's change count the list was filtered for
static bool g_noStyleMatches = false;
// The list index under the cursor. The result +0x210 holds a cursor move
// only until the next list poll, which empties it again (-1, seen in game:
// every F press read -1), so the poll hook keeps the last one here.
static int g_shownCursor = 0;

static bool SearchReady()
{
    if (!g_searchLoaded) {
        g_searchLoaded = SearchWanted() && g_search.Load() ? 1 : -1;
        Log("style list search: %s", g_searchLoaded > 0 ? "the loader's search box"
                                     : SearchWanted() ? "none (XV2 Mod Loader missing or too old)" : "off (style_search)");
    }
    return g_searchLoaded > 0;
}

// ------------------------------------------------------------ favorites ---
// favorite_styles.sav (JSON text, .sav for the same reason as
// equipped_styles.sav): Steam account -> CMS codes, shared by every
// character and preset of that account, the same as Item Search's
// favorites.sav. Favorites lead the style list, right after Default.
//   {"76561198000000000": ["GNY", "GOK"]}
// F (no Ctrl) or the left stick click on the cursor's entry toggles it.
// Game thread only (the list poll and the AS callbacks).
struct FavoriteFile {
    const wchar_t* name;                                       // in the mod's folder
    std::map<std::string, std::set<std::string>> accounts;     // every account in the file
    bool loaded = false;
};
static FavoriteFile g_styleFavorites{L"favorite_styles.sav"};
static std::string g_favoritesAccount;  // the playing account, once known
static bool g_favoriteHeld = false;     // F / L3 down last poll: one toggle per press

static std::wstring FavoritesPath(const FavoriteFile& file) { return std::wstring(g_dir) + L"\\" + file.name; }

// Only plain keys are kept (codes): SaveFavorites writes them unescaped.
static bool PlainKey(const std::string& key)
{
    return !key.empty() && key.find_first_of("\"\\\r\n") == std::string::npos;
}

static void LoadFavorites(FavoriteFile& file)
{
    file.loaded = true;
    std::string text, error;
    JVal root;
    if (!ReadWhole(FavoritesPath(file).c_str(), text)) return;  // none yet
    if (!JsonParse(text, root, error) || root.type != JVal::Obj) { Log("%ls unreadable (%s) - ignored", file.name, error.c_str()); return; }
    for (const auto& [account, codes] : root.o) {
        if (codes.type != JVal::Arr) continue;
        for (const JVal& code : codes.a)
            if (code.type == JVal::Str && PlainKey(code.s)) file.accounts[account].insert(code.s);
    }
}

// Written to a temporary file first so a crash never leaves half a file.
static void SaveFavorites(const FavoriteFile& file)
{
    std::string text = "{";
    const char* accountComma = "";
    for (const auto& [account, codes] : file.accounts) {
        if (codes.empty()) continue;
        text += std::string(accountComma) + "\n    \"" + account + "\": [";
        accountComma = ",";
        const char* codeComma = "";
        for (const std::string& code : codes) {
            text += std::string(codeComma) + "\"" + code + "\"";
            codeComma = ", ";
        }
        text += "]";
    }
    text += "\n}\n";
    std::wstring path = FavoritesPath(file), temporary = path + L".tmp";
    HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { Log("%ls: cannot write", file.name); return; }
    DWORD written = 0;
    bool ok = WriteFile(handle, text.data(), (DWORD)text.size(), &written, nullptr) && written == text.size();
    CloseHandle(handle);
    if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) Log("%ls: cannot write", file.name);
}

// The playing account's favorites in `file`, loading it on first use; null
// while no save is loaded (nothing is read or written until the account is known).
static std::set<std::string>* AccountFavorites(FavoriteFile& file)
{
    if (g_favoritesAccount.empty()) g_favoritesAccount = SteamAccount();
    if (g_favoritesAccount.empty()) return nullptr;
    if (!file.loaded) LoadFavorites(file);
    return &file.accounts[g_favoritesAccount];
}

static bool IsFavorite(const std::set<std::string>* favorites, int style)
{
    return favorites && style > 0 && favorites->count(g_styles[style].code) != 0;
}

// XInput's left stick click (XINPUT_GAMEPAD_LEFT_THUMB, xinput.h), read from
// the xinput DLL the game already loaded (no new DLL is pulled in).
static const WORD kPadLeftThumb = 0x40;
struct PadState { DWORD packet; WORD buttons; BYTE leftTrigger, rightTrigger; SHORT thumbs[4]; };  // XINPUT_STATE
typedef DWORD (WINAPI* XInputGetStateFn)(DWORD user, PadState* state);

static bool PadLeftThumbHeld()
{
    static XInputGetStateFn getState = nullptr;
    static bool looked = false;
    if (!looked) {
        looked = true;
        for (const wchar_t* name : {L"xinput1_3.dll", L"xinput1_4.dll", L"xinput9_1_0.dll"}) {
            HMODULE module = GetModuleHandleW(name);
            if (module && (getState = (XInputGetStateFn)GetProcAddress(module, "XInputGetState"))) break;
        }
        Log("favorite toggle: %s", getState ? "F or the left stick click" : "F (no xinput DLL in the game)");
    }
    if (!getState) return false;
    // Only pad 0 unless it is missing: a poll of an empty slot is slow
    // (XInput enumerates devices), so the rest are tried only then.
    for (DWORD user = 0; user < 4; ++user) {
        PadState state{};
        DWORD error = getState(user, &state);
        if (error == ERROR_SUCCESS) return (state.buttons & kPadLeftThumb) != 0;
        if (error != ERROR_DEVICE_NOT_CONNECTED || user > 0) return false;
    }
    return false;
}

// True once per press of F (no Ctrl) or the left stick click, only while
// the game's window is in front.
static bool FavoritePressed()
{
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    bool held = ((GetAsyncKeyState('F') & 0x8000) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) || PadLeftThumbHeld();
    bool pressed = held && !g_favoriteHeld;
    g_favoriteHeld = held;
    if (pressed && foregroundProcess != GetCurrentProcessId()) {
        Log("favorite: press ignored (the game window is not in front)");
        return false;
    }
    return pressed;
}

// Favorites or unfavorites the style under the open list's cursor and saves
// the file. False (nothing changed) on Default or while no save is loaded.
static bool ToggleCursorFavorite()
{
    int cursor = g_shownCursor;
    if (cursor < 0 || cursor >= (int)g_shownStyles.size()) {
        Log("favorite: cursor %d is outside the %zu shown styles - ignored", cursor, g_shownStyles.size());
        return false;
    }
    int style = g_shownStyles[cursor];
    std::set<std::string>* favorites = AccountFavorites(g_styleFavorites);
    if (style <= 0) return false;  // Default
    if (!favorites) { Log("favorite: no save loaded - ignored"); return false; }
    const std::string& code = g_styles[style].code;
    bool added = favorites->insert(code).second;
    if (!added) favorites->erase(code);
    SaveFavorites(g_styleFavorites);
    Log("favorite style %s %s", code.c_str(), added ? "added" : "removed");
    return true;
}

// ------------------------------------------------- character unlocks ---
// "styles": "roster" shows a style only while one of its character select
// costumes is unlocked. The character select asks check_unlock(menu, CMS id,
// costume) (0x5e2560, nonzero = unlocked) at 0x5e4ae2: mov r8d, [rdi+0x43c];
// mov rcx, rdi; mov edx, [rdi+0x438]; call. XV2Patcher's unlock_chara_mods
// hooks that call (UnlockCharaMods, for added characters) and
// unlock_chara_all writes "mov r8d, 1" over it, so the call there is
// followed instead of calling 0x5e2560 directly. 0x5e2560 reads only three
// fields of the menu (+0x27e8 / +0x27e4 nonzero = skip the checks, +0x444 a
// CaC entry's path; its sub-checks 0x5e4140 raid ACCFLAGs, 0x5e3cc0 DLC,
// 0x5e3f70 quest unlocks get the menu but don't read it); the rest is the
// save (0x15545b0), CMS and DLC tables. So a zeroed stand-in menu asks the
// same question outside the character select. Found by reverse engineering
// 1.26.01 (battle-styles.md "Unlocked styles"). Uses the local save: only
// the list is filtered; the equipped style still goes online by code.
static const uintptr_t kCheckUnlockSite = 0x5e4ae2, kCheckUnlock = 0x5e2560;
static const uint8_t kCheckUnlockSetup[] = {0x44, 0x8B, 0x87, 0x3C, 0x04, 0x00, 0x00,   // mov r8d, [rdi+0x43c]
                                            0x48, 0x8B, 0xCF,                           // mov rcx, rdi
                                            0x8B, 0x97, 0x38, 0x04, 0x00, 0x00};        // mov edx, [rdi+0x438]
static const uint8_t kUnlockAllWrite[] = {0x41, 0xB8, 0x01, 0x00, 0x00, 0x00};  // unlock_chara_all: mov r8d, 1
typedef int (__fastcall* CheckUnlockFn)(void* menu, int charaId, int costume);
alignas(16) static uint8_t g_unlockMenu[0x3000];  // zeroed stand-in; past +0x27e8, the last field read
static std::vector<bool> g_styleUnlocked;  // per g_styles index, set when the list opens; empty = all shown

// One check, false when it faulted (then the list shows everything).
static bool AskCheckUnlock(CheckUnlockFn check, int charaId, int costume, int& unlocked)
{
    __try {
        unlocked = check(g_unlockMenu, charaId, costume);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The live target of the E8 call at `call`, or null when it isn't one.
static const uint8_t* CallTarget(const uint8_t* call)
{
    if (call[0] != 0xE8) return nullptr;
    int32_t relative;
    memcpy(&relative, call + 1, 4);
    return call + 5 + relative;
}

// check_unlock as the character select calls it, or null (logged) when it
// can't be asked; also null under XV2Patcher's unlock_chara_all (all unlocked).
static CheckUnlockFn FindCheckUnlock()
{
    const uint8_t* site = (const uint8_t*)(g_base + kCheckUnlockSite);
    const uint8_t* call = site + sizeof kCheckUnlockSetup;
    if (memcmp(site, kCheckUnlockSetup, sizeof kCheckUnlockSetup) != 0) { Log("styles: unlock check not found - not applied"); return nullptr; }
    if (memcmp(call, kUnlockAllWrite, sizeof kUnlockAllWrite) == 0) return nullptr;  // XV2Patcher unlock_chara_all
    CheckUnlockFn check = (CheckUnlockFn)CallTarget(call);  // 0x5e2560, or XV2Patcher's hook
    if (!check) Log("styles: unlock check call changed - not applied");
    return check;
}

// ------------------------------------------------------ DLC characters ---
// Every "styles" setting leaves out the movesets of characters the character
// select would hide for DLC, so the list never offers more than the select.
// The select's script (CharaSele.as, the "delete chara" loop) drops a
// costume when
//   (owned1 & DlcFlag1) == 0 && (owned2 & DlcFlag2) == 0 (&& two single-
//   character unlock keys miss - not read here: the x2s files have none).
// The game builds both masks in 0x5e5fb0 before sending them to the script:
// * owned1 = 0x5e3a60(menu) (call at 0x5e6105, iggy var 0x224a): pack
//   ownership 0x2b9c30(pack, 0) and update level 0x2b9850() bits, plus the
//   raid bosses' bit 0x1c only in a raid quest (type 0xf) or with menu
//   +0x27e4 set. Called through the live call target.
// * owned2 is built inline, 0x5e611a..0x5e623e (more 0x2b9c30 / 0x2b9850
//   bits in edi; then var 0x224b). That block is run where it is: a stub
//   enters it with the registers it reads (esi = 0, r14d = 1, set at
//   0x5e5fd6 / 0x5e6069) and a hook at its end (0x5e623e, mov r8d, edi; mov
//   edx, 0x224b) hands edi back instead of going on to the script.
// So whatever the game runs there (XV2Patcher's ownership hook, any mod's
// patch) is what the list follows. The raid bit is always added: raids are
// a game mode, not a purchase, and "roster" checks the raid bosses'
// ACCFLAGs anyway. Found by reverse engineering 1.26.01 (battle-styles.md
// "DLC characters"). Like the unlock check this only filters the local list.
static const uintptr_t kOwnedMask1Site = 0x5e6102;  // mov rcx, rbx; call 0x5e3a60
static const uint8_t kOwnedMask1Setup[] = {0x48, 0x8B, 0xCB};
static const uintptr_t kOwnedMask2Start = 0x5e611a; // first instruction of the owned2 block
static const uintptr_t kOwnedMask2End = 0x5e623e;   // mov r8d, edi; mov edx, 0x224b
static const uint8_t kOwnedMask2EndCode[] = {0x44, 0x8B, 0xC7, 0xBA, 0x4B, 0x22, 0x00, 0x00};
static const uint32_t kRaidBit = 0x10000000;        // eternity's "PRB" DLC key
typedef uint32_t (__fastcall* OwnedMaskFn)(void* menu);
typedef uint32_t (__fastcall* RunOwned2Fn)();

struct OwnedDlc { uint32_t mask1 = 0, mask2 = 0; };

// The stub and the end hook share this page: code, then the fields below.
struct Owned2Stub {
    uint64_t savedRsp = 0;     // the stub's stack pointer, restored by the end hook
    uint64_t trampoline = 0;   // the end's replayed instructions -> back to 0x5e6246
    uint8_t  running = 0;      // set while the stub runs the block
};
static Owned2Stub* g_owned2Data = nullptr;
static RunOwned2Fn g_runOwned2 = nullptr;
static bool g_owned2Tried = false;

static void Emit(std::vector<uint8_t>& code, std::initializer_list<uint8_t> bytes) { code.insert(code.end(), bytes); }
static void EmitMovRax(std::vector<uint8_t>& code, uint64_t value)  // mov rax, imm64
{
    Emit(code, {0x48, 0xB8});
    for (int i = 0; i < 8; ++i) code.push_back((uint8_t)(value >> (8 * i)));
}

// Builds the stub that runs the owned2 block and the hook at its end; null
// (logged) when the end's code isn't the expected one.
static RunOwned2Fn MakeOwned2Runner()
{
    if (!CodeIs(kOwnedMask2End, kOwnedMask2EndCode, sizeof kOwnedMask2EndCode)) {
        Log("styles: owned DLC block end changed - DLC not checked");
        return nullptr;
    }
    uint8_t* page = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!page) return nullptr;
    Owned2Stub* data = (Owned2Stub*)(page + 0x800);  // VirtualAlloc zero-fills it
    uint64_t savedRsp = (uint64_t)&data->savedRsp, running = (uint64_t)&data->running;

    // Entry, called like uint32_t f(): save the callee-saved registers, set
    // the block's inputs, jump in. The stack ends 16-aligned with 0x20 of
    // shadow space for the block's calls (8 pushes + 0x28 after the return
    // address).
    std::vector<uint8_t> entry;
    Emit(entry, {0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57});  // push rbx..r15
    Emit(entry, {0x48, 0x83, 0xEC, 0x28});                                                  // sub rsp, 0x28
    EmitMovRax(entry, savedRsp); Emit(entry, {0x48, 0x89, 0x20});                           // mov [savedRsp], rsp
    EmitMovRax(entry, running);  Emit(entry, {0xC6, 0x00, 0x01});                           // mov byte [running], 1
    Emit(entry, {0x31, 0xF6});                                                              // xor esi, esi
    Emit(entry, {0x41, 0xBE, 0x01, 0x00, 0x00, 0x00});                                      // mov r14d, 1
    EmitMovRax(entry, g_base + kOwnedMask2Start); Emit(entry, {0xFF, 0xE0});                // jmp block

    // End hook: from the stub, return edi to its caller; otherwise (the
    // select) replay the original instructions. rax and the flags are free
    // here: the next instructions overwrite them before reading.
    std::vector<uint8_t> end;
    EmitMovRax(end, running);
    Emit(end, {0x80, 0x38, 0x00});                    // cmp byte [running], 0
    Emit(end, {0x74, 0x23});                          // je select (skips the 0x23 bytes below)
    Emit(end, {0xC6, 0x00, 0x00});                    // mov byte [running], 0
    Emit(end, {0x89, 0xF8});                          // mov eax, edi
    Emit(end, {0x48, 0xB9});                          // mov rcx, &savedRsp
    for (int i = 0; i < 8; ++i) end.push_back((uint8_t)(savedRsp >> (8 * i)));
    Emit(end, {0x48, 0x8B, 0x21});                    // mov rsp, [rcx]
    Emit(end, {0x48, 0x83, 0xC4, 0x28});              // add rsp, 0x28
    Emit(end, {0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5D, 0x5B});  // pop r15..rbx
    Emit(end, {0xC3});                                // ret
    EmitMovRax(end, (uint64_t)&data->trampoline);     // select:
    Emit(end, {0x48, 0x8B, 0x00, 0xFF, 0xE0});        // mov rax, [rax]; jmp rax

    memcpy(page, entry.data(), entry.size());
    uint8_t* endHook = page + 0x100;
    memcpy(endHook, end.data(), end.size());
    FlushInstructionCache(GetCurrentProcess(), page, 0x800);
    void* trampoline = HookCode((uint8_t*)(g_base + kOwnedMask2End), sizeof kOwnedMask2EndCode, endHook);
    if (!trampoline) { Log("styles: owned DLC block hook failed - DLC not checked"); return nullptr; }
    data->trampoline = (uint64_t)trampoline;
    g_owned2Data = data;
    return (RunOwned2Fn)page;
}

static bool AskOwnedDlc(OwnedMaskFn ownedMask1, OwnedDlc& owned)
{
    __try {
        owned.mask1 = ownedMask1(g_unlockMenu) | kRaidBit;
        owned.mask2 = g_runOwned2();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (g_owned2Data) g_owned2Data->running = 0;
        return false;
    }
}

// The select's owned DLC masks, worked out by the select's own code; false
// (logged) when they can't be read.
static bool ReadOwnedDlc(OwnedDlc& owned)
{
    if (!g_owned2Tried) {  // the hook goes in once, at the first list open (code decrypted by then)
        g_owned2Tried = true;
        g_runOwned2 = MakeOwned2Runner();
    }
    const uint8_t* mask1Site = (const uint8_t*)(g_base + kOwnedMask1Site);
    auto ownedMask1 = memcmp(mask1Site, kOwnedMask1Setup, sizeof kOwnedMask1Setup) ? nullptr
                    : (OwnedMaskFn)CallTarget(mask1Site + sizeof kOwnedMask1Setup);
    if (!ownedMask1 || !g_runOwned2) { Log("styles: DLC check not found - DLC styles shown"); return false; }
    if (!AskOwnedDlc(ownedMask1, owned)) { Log("styles: DLC check faulted - DLC styles shown"); return false; }
    return true;
}

// The select shows this costume with these DLC masks. Key 0, 0 = an
// XV2Patcher added character (no DLC).
static bool CostumeOwned(const SelectCostume& costume, const OwnedDlc& owned)
{
    if (costume.dlcKey1 == 0 && costume.dlcKey2 == 0) return true;
    return (costume.dlcKey1 & owned.mask1) != 0 || (costume.dlcKey2 & owned.mask2) != 0;
}

// Fills g_styleUnlocked when the list opens: DLC owned (every setting) and,
// for "roster", unlocked in the select ("unlocked" skips that check). Race
// movesets always count; a style shows when any of its select costumes
// passes. Styles in no select slot stay ("roster" / "unlocked" left them
// out already).
static void UpdateUnlockedStyles()
{
    g_styleUnlocked.clear();
    ULONGLONG started = GetTickCount64();
    OwnedDlc owned;
    bool dlcGate = ReadOwnedDlc(owned);
    CheckUnlockFn check = g_styleChoice == StyleChoice::Roster ? FindCheckUnlock() : nullptr;
    if (!dlcGate && !check) return;
    std::vector<bool> unlocked(g_styles.size(), true);
    size_t locked = 0;
    for (size_t i = 1; i < g_styles.size(); ++i) {
        const Style& style = g_styles[i];
        if (style.raceOrder >= 0 || style.selectCostumes.empty()) continue;
        bool any = false;
        for (const SelectCostume& costume : style.selectCostumes) {
            if (dlcGate && !CostumeOwned(costume, owned)) continue;
            int result = 1;
            if (check && !AskCheckUnlock(check, costume.charaId, costume.costume, result)) {
                Log("styles: unlock check faulted (chara %d costume %d) - all shown", costume.charaId, costume.costume);
                return;
            }
            if (result) { any = true; break; }
        }
        unlocked[i] = any;
        locked += !any;
    }
    g_styleUnlocked = unlocked;
    Log("styles: %zu of %zu not shown (DLC %s owned 0x%08x 0x%08x, unlock check %s, %llu ms)", locked, g_styles.size() - 1,
        dlcGate ? "checked," : "not checked,", owned.mask1, owned.mask2, check ? "on" : "off", GetTickCount64() - started);
}

// Fills g_shownEntries / g_shownStyles with the styles matching the box:
// Default first, then the favorites, then the rest, each in list order
// (sorted by name). Default stays first (the X button and a lost cursor go
// to entry 0) and always shows, as does the equipped style: a cancel returns
// the list's default entry, which has to be the equipped one. Locked styles
// ("roster") don't show. When nothing matches, the whole list shows with
// a note. Favorites carry the star only while the rebuilt AvatarList.as is
// served (the vanilla one would show the tab).
static void FilterStyles(int equipped)
{
    bool searching = SearchReady();
    if (searching) g_shownChanges = g_search.Changes(kSearchId);
    auto offered = [&](size_t i) {
        return g_styleUnlocked.size() != g_styles.size() || g_styleUnlocked[i] || i == 0 || (int)i == equipped;
    };
    std::vector<bool> keep(g_styles.size());
    bool anyMatch = false;
    for (size_t i = 0; i < g_styles.size(); ++i) {
        keep[i] = !searching || g_search.MatchesW(kSearchId, g_styles[i].name.c_str());
        anyMatch = anyMatch || (keep[i] && offered(i));
    }
    g_noStyleMatches = !anyMatch;
    const std::set<std::string>* favorites = AccountFavorites(g_styleFavorites);
    bool stars = ServedPatched(g_avatarListIggy);
    g_shownEntries.clear();
    g_shownStyles.clear();
    for (int pass = 0; pass < 2; ++pass) {  // 0: Default and the favorites, 1: the rest
        for (size_t i = 0; i < g_styles.size(); ++i) {
            bool favorite = IsFavorite(favorites, (int)i);
            if ((pass == 0) != (i == 0 || favorite)) continue;
            if (!offered(i)) continue;
            if (anyMatch && !keep[i] && i != 0 && (int)i != equipped) continue;
            g_shownEntries.push_back(favorite && stars ? g_favoriteEntries[i] : g_listEntries[i]);
            g_shownStyles.push_back((int)i);
        }
    }
}

// The list index showing style `style`, -1 when it is filtered out.
static int ShownIndexOf(int style)
{
    auto found = std::find(g_shownStyles.begin(), g_shownStyles.end(), style);
    return found == g_shownStyles.end() ? -1 : (int)(found - g_shownStyles.begin());
}

// Hands g_shownEntries to the list, with the equipped style as its default.
static void SetStyleListContents(void* list, int equipped)
{
    ListParam param{};
    param.defaultIndex = (std::max)(ShownIndexOf(equipped), 0);
    param.begin = g_shownEntries.data();
    param.end = param.capacityEnd = g_shownEntries.data() + g_shownEntries.size();
    Game<SetListContentsFn>(kSetListContents)(list, &param);
}

// The query changed while the style list is open: filter again and send the
// new entries. The rebuilt AvatarList.as sees the count again and redraws,
// with the cursor kept on its style (or on the first entry after Default
// when that one is filtered out).
static void RefreshStyleList(void* list)
{
    int equipped = EquippedIndex();
    int cursorStyle = g_shownCursor >= 0 && g_shownCursor < (int)g_shownStyles.size() ? g_shownStyles[g_shownCursor] : equipped;
    FilterStyles(equipped);
    int cursor = ShownIndexOf(cursorStyle);
    if (cursor < 0) cursor = g_shownStyles.size() > 1 ? 1 : 0;
    SetStyleListContents(list, equipped);
    Game<SetUserDataIntFn>(kSetUserDataInt)(list, kListUserDataCursor, cursor);
    Field<int>(list, kListResult) = cursor;  // the AS sends it back too, as a cursor move
    g_shownCursor = cursor;
}

// Same steps as the game's own list openers (template 0x4fcac0): contents
// with the equipped entry as default, remember which list is open, switch
// the menu to its list state.
static void OpenStyleList(void* menu)
{
    // No list object: the ctor patch below did not go in (crashed in game
    // when the mode 3 menu had none).
    void* list = Field<void*>(menu, kEditList);
    if (!StylesReady() || g_listEntries.empty() || !list) { ShowEquipped(menu); return; }
    int equipped = EquippedIndex();
    UpdateUnlockedStyles();  // the save may have unlocked more since the last open
    FilterStyles(equipped);
    SetStyleListContents(list, equipped);
    g_shownCursor = (std::max)(ShownIndexOf(equipped), 0);  // the list opens on its default entry
    g_favoriteHeld = true;  // a key already down (the press that opened the list) toggles nothing
    // The X button jumps to Default (the game's opener sends its lists' X
    // entry the same way; otherwise the last list's would be left).
    Game<SetListXEntryFn>(kSetListXEntry)(list, 0);
    Field<int>(menu, kOpenListType) = kSendRequestBattleStyle;
    Game<SetMenuStateFn>(kSetMenuState)(Field<void*>(menu, kStateMachine),
                                        ListState(Field<int>(menu, kMenuMode), kSendRequestBattleStyle), 0, 0);
}

static void __fastcall HkUserCallback(void* menu, const uint8_t* call)
{
    int sendType = *(const int*)(call + kCallArgCount) >= 1 ? (int)*(const double*)(call + kCallArg0) : -1;
    if (sendType == kSendRequestBattleStyle) { OpenStyleList(menu); return; }
    g_origUserCallback(menu, call);
}

// The preset menu, EditMyset (CMN_PRESET_PLT.iggy, vtable 0x10a1098; made
// by 0x5057a0). Its AS callback (slot 17, 0x505da0) passes arg 0 (sendType)
// and arg 1 (+0x48, a double) to 0x506710, which for
//   sendType 2 (load preset n):  0x5064a0(menu, n, 0) - record n over the main
//   sendType 3 (save preset n):  0x5064a0(menu, 0, n) - the main over record n
// 0x5064a0(menu, from, to) copies the loaded character's 0x68-byte loadout
// record (character + 0xf4 + i * 0x68) when both are >= 0; it is the only
// code that copies those records (ClaudeTools\battle_styles\presetrefs.py), and
// these two calls its only callers.
static const uintptr_t kEditMysetVtable = 0x10a1098;
static const uintptr_t kMysetCallback = 0x505da0;   // vanilla entry of slot 17
static const size_t kCallArg1 = 0x48;               // arg 1 as a double (0x505da6)
//   sendType 0 (RequestEquipData, preset n): user data 2..8 = the record's
//     equipment names, 9..16 its skill names (16 = evasive skill, the last
//     row, AvatarMyset.as ReceiveType_SkillStrEnd), then 0x64fa20 builds it.
static const int kMysetSendEquipData = 0, kMysetSendLoad = 2, kMysetSendSave = 3;
static const int kMysetUserDataLastSkill = 16;
static UserCallbackFn g_origMysetCallback = nullptr;

// Sent by the rebuilt CharaSele.as / AvatarMyset.as once per menu with its
// layout as one string (SendType_StyleLayoutDump), for placing the row.
// Never passed on: the game has no such sendType.
static const int kSendLayoutDump = 9901;
static const size_t kLayoutDumpMax = 256 * 1024;

// The OggyBase user-data vector (+0x160 begin, +0x168 end): 16-byte
// entries {int set, int type, value}; SetUserDataStr (0x2c8240) stores
// type 6 and the wchar_t pointer.
static const size_t kUserDataBegin = 0x160, kUserDataEnd = 0x168, kUserDataEntrySize = 16;
static const size_t kUserDataType = 4, kUserDataValue = 8;
static const int kUserDataTypeWideString = 6;

// The string in user-data slot `index`, empty when it holds none.
static std::wstring UserDataString(void* menu, int index)
{
    uint8_t* begin = Field<uint8_t*>(menu, kUserDataBegin);
    uint8_t* end = Field<uint8_t*>(menu, kUserDataEnd);
    if (!begin || index < 0 || index >= (end - begin) / (ptrdiff_t)kUserDataEntrySize) return {};
    uint8_t* entry = begin + index * kUserDataEntrySize;
    const wchar_t* text = *(const wchar_t**)(entry + kUserDataValue);
    if (*(const int*)(entry + kUserDataType) != kUserDataTypeWideString || !text) return {};
    return std::wstring(text, wcsnlen(text, 1024));
}

// Strings handed to the menus. The game keeps the pointer, so they are
// never freed; each distinct text is kept once (skill names x styles - a
// few hundred at most).
static SRWLOCK g_menuTextLock = SRWLOCK_INIT;
static std::set<std::wstring> g_menuTexts;

// Appends "\t<style name>" to the string in user-data slot `index`; the
// rebuilt AS shows the part after the tab on its Battle Style row.
static void AppendStyle(void* menu, int index, const std::string& code)
{
    std::wstring text = UserDataString(menu, index) + L"\t" + StyleName(code);
    AcquireSRWLockExclusive(&g_menuTextLock);
    const wchar_t* kept = g_menuTexts.insert(text).first->c_str();
    ReleaseSRWLockExclusive(&g_menuTextLock);
    Game<SetUserDataStrFn>(kSetUserDataStr)(menu, index, kept);
}

// Copies at most `capacity` - 1 chars of a string the AS passed; false when
// the pointer cannot be read (no C++ objects here: __try needs that).
static bool CopyAsString(const char* text, char* out, size_t capacity)
{
    __try {
        size_t length = strnlen(text, capacity - 1);
        memcpy(out, text, length);
        out[length] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// True (and the dump logged line by line) when `call` is the layout dump.
// A string argument is a UTF-8 char* in place of arg 1's double.
static bool HandleLayoutDump(const uint8_t* call)
{
    if (*(const int*)(call + kCallArgCount) < 2 || (int)*(const double*)(call + kCallArg0) != kSendLayoutDump) return false;
    const char* text = *(const char* const*)(call + kCallArg1);
    std::vector<char> copy(kLayoutDumpMax);
    if (!text || !CopyAsString(text, copy.data(), copy.size())) { Log("layout dump: unreadable"); return true; }
    for (const char* line = copy.data(); *line;) {
        const char* lineEnd = strchr(line, '\n');
        size_t length = lineEnd ? (size_t)(lineEnd - line) : strlen(line);
        Log("  %.*s", (int)length, line);
        line += length + (lineEnd ? 1 : 0);
    }
    return true;
}

static void __fastcall HkMysetCallback(void* menu, const uint8_t* call)
{
    if (HandleLayoutDump(call)) return;
    g_origMysetCallback(menu, call);
    if (*(const int*)(call + kCallArgCount) < 1) return;  // the game ignores those too
    int sendType = (int)*(const double*)(call + kCallArg0), preset = (int)*(const double*)(call + kCallArg1);
    if (sendType == kMysetSendLoad) CopyPreset(preset, kMainPreset);
    else if (sendType == kMysetSendSave) CopyPreset(kMainPreset, preset);
    else if (sendType == kMysetSendEquipData && ServedPatched(g_mysetIggy) && preset >= 0 && preset < kPresets)
        AppendStyle(menu, kMysetUserDataLastSkill, StyleMenu_PresetCode(-1, preset));
}

// The character select (SelectCharacter, CHARASELE.iggy, vtable 0x10be328).
// Its AS callback (slot 17, 0x5e1630) only gets the layout dump from us;
// Chara Search hooks the same slot for its messages (9902/9904), and
// each mod passes on what isn't its own.
// 0x5e57a0(menu) (only caller 0x5e4988) fills the skill popup's user data
// for the character under the cursor, ending with the Super Soul name in
// slot 17 (CharaSele.as ReceiveType_TarismanNameStr); the style goes after it.
static const uintptr_t kSelectCharaVtable = 0x10be328;
static const uintptr_t kSelectCharaCallback = 0x5e1630;
static UserCallbackFn g_origSelectCharaCallback = nullptr;

static void __fastcall HkSelectCharaCallback(void* menu, const uint8_t* call)
{
    if (HandleLayoutDump(call)) return;
    g_origSelectCharaCallback(menu, call);
}

static const uintptr_t kCharaSeleInfo = 0x5e57a0;
// mov rax, rsp; push rbp; push r12 (6 bytes, taken by the hook); push r13..r15;
// lea rbp, [rax-0x358]
static const uint8_t kCharaSeleInfoCode[] = {0x48, 0x8B, 0xC4, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56,
                                             0x41, 0x57, 0x48, 0x8D, 0xA8, 0xA8, 0xFC, 0xFF, 0xFF};
static const size_t kCharaSeleCharaId = 0x438;   // CMS id under the cursor (< 0: none, 0x5e57e3)
static const size_t kCharaSelePreset = 0x440;    // preset picked for it (0x5e66d0 hook in mod.cpp)
static const size_t kCharaSeleSaveSlot = 0x450;  // a CaC's save slot (battle-styles-loading-and-online.md)
static const int kFirstCacId = 100, kLastCacId = 109;  // add ecx,-0x64; cmp ecx,9 (0x290000)
static const int kTarismanNameUserData = 17;
typedef uint64_t (__fastcall* CharaSeleInfoFn)(void* menu);
static CharaSeleInfoFn g_origCharaSeleInfo = nullptr;

static uint64_t __fastcall HkCharaSeleInfo(void* menu)
{
    uint64_t result = g_origCharaSeleInfo(menu);
    int charaId = Field<int>(menu, kCharaSeleCharaId), slot = Field<int>(menu, kCharaSeleSaveSlot);
    int preset = Field<int>(menu, kCharaSelePreset);
    if (charaId >= kFirstCacId && charaId <= kLastCacId && slot >= 0 && slot < kCharacterSlots && ServedPatched(g_charaSeleIggy))
        AppendStyle(menu, kTarismanNameUserData, StyleMenu_PresetCode(slot, preset >= 0 && preset < kPresets ? preset : kMainPreset));
    return result;
}

// Points vtable slot `index` of the class at `vtableRva` at `hook` (.rdata:
// patched at once, Steam's DRM does not touch it); returns the old entry,
// null when it could not be written. `vanilla`: whether it was the game's.
static UserCallbackFn HookVtableSlot(uintptr_t vtableRva, int index, UserCallbackFn hook, uintptr_t vanillaRva, bool& vanilla)
{
    uintptr_t* slot = (uintptr_t*)(g_base + vtableRva) + index;
    vanilla = *slot == g_base + vanillaRva;
    DWORD old;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) return nullptr;
    UserCallbackFn original = (UserCallbackFn)*slot;
    *slot = (uintptr_t)hook;
    VirtualProtect(slot, sizeof(*slot), old, &old);
    return original;
}

// EditChara's cloth push 0x4fb3f0(menu, customize): refreshes the cloth and
// Super Soul slots (206..212). Called when the menu starts (0x4f7bb0) and
// for the tab requests 103/107 - hooking only 103/107 left the slot grey
// until the tab was shown a second time (seen in game).
static const uintptr_t kClothPush = 0x4fb3f0;
// mov [rsp+8], rbx (5 bytes, taken by the hook); mov [rsp+0x10], rsi; push rdi
static const uint8_t kClothPushCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57};
typedef void (__fastcall* ClothPushFn)(void* menu, int customize);
static ClothPushFn g_origClothPush = nullptr;

static void __fastcall HkClothPush(void* menu, int customize)
{
    g_origClothPush(menu, customize);
    ShowEquipped(menu);
}

// EditChara's list poll, every frame while a list is open: when the list
// closes it resets +0x2f8 to 0x192 (tail 0x4fdbaa; an unknown type like
// 240 is otherwise ignored). The entry under the cursor is the list's
// result (sent on every move; a cancel sends the default back), read here
// before the game's poll empties it.
static const uintptr_t kListPoll = 0x4fd9d0;
// push rbx (40 53); sub rsp, 0x20; mov rbx, rcx; mov rcx, [rcx+0x6c0] - the
// hook takes the first 6 bytes (0x4fd9d0 is 16-byte aligned).
static const uint8_t kListPollCode[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x89, 0xC0};
typedef uint64_t (__fastcall* ListPollFn)(void* menu);
static ListPollFn g_origListPoll = nullptr;

static uint64_t __fastcall HkListPoll(void* menu)
{
    bool styleList = Field<int>(menu, kOpenListType) == kSendRequestBattleStyle;
    void* list = styleList ? Field<void*>(menu, kEditList) : nullptr;
    int picked = -1;  // a g_styles index
    if (list && Game<ListHasResultFn>(kListHasResult)(list)) {
        int shown = Game<ListGetResultFn>(kListGetResult)(list);
        if (shown >= 0 && shown < (int)g_shownStyles.size()) {
            picked = g_shownStyles[shown];
            g_shownCursor = shown;
        }
    }
    uint64_t result = g_origListPoll(menu);
    if (!styleList) return result;
    if (Field<int>(menu, kOpenListType) == kNoListOpen) {
        if (picked >= 0) Equip(picked);
        ShowEquipped(menu);
        // the note is ours only while the style list is in front
        if (SearchReady()) g_search.SetNote(kSearchId, "");
    } else if (list && SearchReady()) {
        // The box stays up while the list is open (it hides by itself once
        // Show stops). Without the rebuilt list script the matches show the
        // next time the list opens: a count the script never reads must not
        // change under it.
        unsigned changes = g_search.Show(kSearchId, kSearchHint, kSearchX, kSearchY, kSearchWidth);
        bool canRefresh = ServedPatched(g_avatarListIggy) && !(Field<uint32_t>(list, kListFlags) & kListEnded);
        // read every poll so a press held from before (or while typing) never toggles later
        bool favoritePressed = FavoritePressed();
        bool typing = g_search.Typing(kSearchId) != 0;
        if (favoritePressed && (!canRefresh || typing))  // once per press: why it did nothing
            Log("favorite: press ignored (%s)", typing ? "typing in the box"
                : !ServedPatched(g_avatarListIggy) ? "vanilla AVATAR_LIST script" : "list closing");
        if (favoritePressed && canRefresh && !typing && ToggleCursorFavorite())
            RefreshStyleList(list);
        else if (changes != g_shownChanges && canRefresh)
            RefreshStyleList(list);
        // every frame: Item Search sets its note only when it changes
        g_search.SetNote(kSearchId, g_noStyleMatches ? kNoStyleMatches : "");
    }
    return result;
}

// The ctor of EditChara (0x4f67a0) passes its user-data slot count as
// 2 + 0x5e (mov edx, 2; lea r8d, [rdx+0x5e] at 0x4f67cf). 0x60 gives the
// 98 slots Avatar.as now uses.
static const uintptr_t kSlotCountCode = 0x4f67cf, kSlotCountByte = 0x4f67d7;
static const uint8_t kSlotCountVanilla[] = {0xBA, 0x02, 0x00, 0x00, 0x00, 0x44, 0x8D, 0x42, 0x5E};
static const uint8_t kSlotCountPatched = 0x60;

// The same ctor makes the AVATAR_LIST (+0x6c0, EditList ctor 0x503700) only
// in modes 1, 2, 5, 6 and 8: lea eax, [rcx-1]; test eax, ~5; je create;
// cmp ecx, 8; jne skip (0x4f6ab6). The Customize menu (mode 3) has none, so
// the jne becomes two nops: every mode gets the list, which stays hidden
// until the style list opens. The dtor (0x4f6f10) frees it when present.
static const uintptr_t kListCtorCheck = 0x4f6ab6, kListCtorSkip = 0x4f6ac3;
static const uint8_t kListCtorCheckCode[] = {0x8D, 0x41, 0xFF, 0xA9, 0xFA, 0xFF, 0xFF, 0xFF, 0x74, 0x05, 0x83, 0xF9, 0x08, 0x75, 0x39};
static const uint8_t kListCtorAlways[] = {0x90, 0x90};

// Writes a few bytes of code (already checked to be the vanilla ones).
static bool PatchCode(uintptr_t rva, const uint8_t* bytes, size_t size)
{
    DWORD old;
    uint8_t* code = (uint8_t*)(g_base + rva);
    if (!VirtualProtect(code, size, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(code, bytes, size);
    VirtualProtect(code, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), code, size);
    return true;
}

void StyleMenu_InstallDataHooks()
{
    bool fileHook = InstallFileHook();
    bool vanilla = false, mysetVanilla = false;
    g_origUserCallback = HookVtableSlot(kEditCharaVtable, kUserCallbackSlot, &HkUserCallback, kUserCallback, vanilla);
    g_origMysetCallback = HookVtableSlot(kEditMysetVtable, kUserCallbackSlot, &HkMysetCallback, kMysetCallback, mysetVanilla);
    bool selectVanilla = false;
    // Chara Search hooks this slot under the same mutex
    HANDLE selectLock = CreateMutexW(nullptr, FALSE, L"XV2Mods_SelectCharaCallbackHook");
    if (selectLock) WaitForSingleObject(selectLock, 10000);
    g_origSelectCharaCallback = HookVtableSlot(kSelectCharaVtable, kUserCallbackSlot, &HkSelectCharaCallback,
                                               kSelectCharaCallback, selectVanilla);
    if (selectLock) { ReleaseMutex(selectLock); CloseHandle(selectLock); }
    Log("menu: file hook %s, AS callback %s%s, preset menu %s%s, character select %s%s", fileHook ? "installed" : "NOT installed",
        g_origUserCallback ? "hooked" : "NOT hooked", vanilla ? "" : " (slot was already changed by another mod)",
        g_origMysetCallback ? "hooked" : "NOT hooked - presets keep no style",
        mysetVanilla ? "" : " (slot was already changed by another mod)",
        g_origSelectCharaCallback ? "hooked" : "NOT hooked", selectVanilla ? "" : " (slot was already changed by another mod)");
}

void StyleMenu_InstallCodeHooks()
{
    bool slotsOk = WaitForCode(kSlotCountCode, kSlotCountVanilla, sizeof(kSlotCountVanilla))
                   && PatchCode(kSlotCountByte, &kSlotCountPatched, 1);
    bool listOk = WaitForCode(kListCtorCheck, kListCtorCheckCode, sizeof(kListCtorCheckCode))
                  && PatchCode(kListCtorSkip, kListCtorAlways, sizeof(kListCtorAlways));
    // Hooks are tried once: a hook must never go in twice.
    if (WaitForCode(kListPoll, kListPollCode, sizeof(kListPollCode)))
        g_origListPoll = (ListPollFn)HookCode((uint8_t*)(g_base + kListPoll), 6, (void*)&HkListPoll);
    if (WaitForCode(kClothPush, kClothPushCode, sizeof(kClothPushCode)))
        g_origClothPush = (ClothPushFn)HookCode((uint8_t*)(g_base + kClothPush), 5, (void*)&HkClothPush);
    if (WaitForCode(kCharaSeleInfo, kCharaSeleInfoCode, sizeof(kCharaSeleInfoCode)))
        g_origCharaSeleInfo = (CharaSeleInfoFn)HookCode((uint8_t*)(g_base + kCharaSeleInfo), 6, (void*)&HkCharaSeleInfo);
    Log("menu: user-data slots %s, list in every mode %s, list poll %s, cloth push %s, skill popup %s (%.0f ms)",
        slotsOk ? "98" : "NOT patched", listOk ? "yes" : "NOT patched", g_origListPoll ? "hooked" : "NOT hooked",
        g_origClothPush ? "hooked" : "NOT hooked", g_origCharaSeleInfo ? "hooked" : "NOT hooked", MsSinceStart());
}

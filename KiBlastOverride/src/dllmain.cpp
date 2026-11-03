// ---------------------------------------------------------------------------
//  KiBlastOverride.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\KiBlastOverride\
//
//  A CaC equips a charged ki blast on Customize > Skills (a "Ki Blast Override"
//  row under the skills) from every blast skill the loaded CUS has; the super
//  soul then no longer changes the blast. "Default" = vanilla, souls decide;
//  "Race Default" = the race's blast whatever the soul.
//  Game 1.27. Research_Notes\ki-blast-type.md.
//
//  * This file: the menu row and its list (EditChara hooks), the blast list
//    (CUS + CMS + MSG read through the loader), the saved picks.
//  * blast_sites.cpp: where the blast is chosen when a loadout is built or
//    sent online (the pick rides in the CSS record's loadout, +0xa2).
//  * as3\Avatar.as: the row itself, handed to the loader's UI library, which
//    merges it with other mods' AVATAR edits.
//
//  Picks: equipped_blasts.sav next to the DLL, per Steam account, save slot
//  and loadout preset. The Skills page always shows preset 0 (the one in
//  use); the Presets panel copies the pick along with the loadout record.
//
//  Reads ki_blast_override.json and equipped_blasts.sav next to the DLL;
//  writes equipped_blasts.sav, and ki_blast_override.log only when "logging"
//  is true. No network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <intrin.h>
#include "avatar_abc.h"
#include "avatarlist_abc.h"
#include "blast_sites.h"
#include "charasele_abc.h"
#include "charasele_patcher_abc.h"
#include "default_config.h"
#include "ean_fit.h"
#include "myset_abc.h"
#include "xv2_files.h"
#include "xv2_search.h"
#include "xv2_ui.h"
#include "x2d_read.h"
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

uintptr_t g_exe = 0; // the game's image base (extern in blast_sites.h)

namespace {

// --- Game addresses (static disassembly of 1.27) ------------------------
// Copied from Multi Accessory / Battle Styles (same exe, same menu), see the
// comments there for how each was found.

// Live save: CaC of slot n = *0x1568310 + 0x120d0 + n * 0xe880; current slot
// = 0x433e0(manager + 8) (Multi Accessory dllmain.cpp, from 0x828b40 and 0x653830).
constexpr uintptr_t kSaveManagerRva = 0x1568310;
constexpr uintptr_t kCurrentSlotRva = 0x433e0;
constexpr size_t kSaveSlots = 0x120d0, kSaveSlotSize = 0xe880;
constexpr int kSlotCount = 8;
constexpr int kPresetCount = 8; // loadout records per CaC: 0 = in use, 1..7 = saved presets
constexpr int kMainPreset = 0;  // the Skills page shows the loadout in use

// The loaded save's decrypted image is kept by the save/system singleton
// [0x15681a8] at +0x68: "#SAV", then the owning Steam id at +8 XORed with
// "thebecon" (Xv2CoreLib SAV_File.cs; same read as Battle Styles).
constexpr uintptr_t kSaveSystemRva = 0x15681a8;
constexpr size_t kSaveImage = 0x68, kSaveImageSteamId = 8;
constexpr uint64_t kSteamIdXor = 0x7468656265636F6E;

// The Presets panel, EditMyset (vtable 0x10af228, .rdata): its AS callback
// (slot 17) gets argc at +0x10, arg 0 (sendType) as a double at +0x28, arg 1
// (preset) at +0x48. sendType 2 = load preset n (record n over record 0),
// 3 = save preset n (record 0 over record n).
constexpr uintptr_t kEditMysetVtableRva = 0x10af228;
constexpr int kUserCallbackSlot = 17;
constexpr size_t kCallArgCount = 0x10, kCallArg0 = 0x28, kCallArg1 = 0x48;
// sendType 0 = fill the user data with preset n: 2..8 the equipment, 9..16 the skills.
constexpr int kMysetLoad = 2, kMysetSave = 3, kMysetEquipData = 0;
constexpr int kMysetFirstSkillUserData = 9; // ReceiveType_SkillStrStart

// The character select, SelectCharacter (CHARASELE.iggy, vtable 0x10cc558).
// 0x5e62d0(menu) fills the skill popup's user data; its only caller is the
// tail jump (e9) at 0x5e54b8. Fields seen in that function.
constexpr uintptr_t kSelectCharaVtableRva = 0x10cc558;
constexpr uintptr_t kFillSkillPopupRva = 0x5e62d0, kFillSkillPopupJumpRva = 0x5e54b8;
constexpr size_t kSelectCharaCmsId = 0x438, kSelectCharaPreset = 0x440, kSelectCharaSlot = 0x450;
constexpr int kFirstCacCmsId = 100, kLastCacCmsId = 109;
// ReceiveType_SkillNameStrStart: `lea edx,[rsi+0x2120]` at 0x5e6a79 in the fill's loop.
constexpr int kPopupFirstSkillUserData = 0x2120;

// The menus' AS sends this type with a line for the log (as3\ row helpers).
constexpr int kSendMenuNote = 9911;

// The Customize menu, EditChara (vtable 0x10ae228, .rdata; slot 17 = its AS
// callback; Battle Styles and Multi Accessory chain it too).
constexpr uintptr_t kEditCharaVtableRva = 0x10ae228;
constexpr size_t kEditList = 0x6c0;     // EditList* (AVATAR_LIST)
constexpr size_t kStateMachine = 0x6b8; // 0x656490(sm, state, 0, 0) switches the menu's state
constexpr size_t kMenuMode = 0x1f4;     // 3 = Customize in game
constexpr size_t kOpenListType = 0x2f8; // sendType of the open list
constexpr int kNoListOpen = 0x192;      // +0x2f8 when none is (0x4fe68a)

// AS <-> exe numbers, matching as3\Avatar.as. Battle Styles uses 240 and user
// data 96/97, Multi Accessory 98..101; ours are free.
constexpr int kSendRequestBlast = 241;
constexpr int kUserDataLabel = 102;
constexpr int kUserDataActive = 103; // > 0 = the row is usable

// OggyBase user-data setters (bounds-checked; the string one keeps the pointer).
constexpr uintptr_t kSetUserDataIntRva = 0x2c82a0, kSetUserDataStrRva = 0x2c8340;

// EditList (AVATAR_LIST): contents, result, X-button entry (Battle Styles).
constexpr uintptr_t kSetListContentsRva = 0x504450; // (EditList*, ListParam*)
constexpr uintptr_t kSetMenuStateRva = 0x656490;    // (state machine, state, 0, 0)
constexpr uintptr_t kListHasResultRva = 0x504430, kListGetResultRva = 0x504420;
constexpr uintptr_t kSetListXEntryRva = 0x504530;   // (EditList*, index)
// +0x1b4 flags: 1 = exited, 2 = cancelled, 2|4 = decided (0x66cfb0, 0x66cf70, 0x66cf90).
constexpr size_t kListFlags = 0x1b4;
constexpr uint32_t kListCancelled = 2, kListDecided = 2 | 4;
// Refilling an open list (Research_Notes\battle-styles-ui.md, Style list search):
// only while no end flag is set (exited 1, cancelled 2, decided 4). The result at
// +0x210 has no bounds check in the game, so it must match the entries sent. User
// data 18 is the X button's entry; the rebuilt AvatarList.as takes it as the cursor.
constexpr size_t kListResult = 0x210;
constexpr uint32_t kListEnded = 1 | 2 | 4;
constexpr int kListUserDataCursor = 18;

// Call sites (E8) of EditChara's skill push 0x4fc850(menu, customize) and list
// poll 0x4fe4b0(menu). Redirected at the call sites, never at the entries:
// Battle Styles hooks both entries (Research_Notes\ki-blast-type.md).
constexpr uintptr_t kCharaEanCreatedCheckRva = 0x260d83, kIsCreatedCharaRva = 0x2900c0; // PatchCharaEanLookup
// RegisterEffectsHook. A character's own effects: 0x16cee0, run per battle slot, and 0x16d560
// (reached through a vtable: a character replaced mid-battle). Both pass the slot's CMS id
// (+0xe0). 0x16c821 is NOT this: it registers an awoken skill's form and a plain battle skips it.
constexpr uintptr_t kRegisterEffectsCallRvas[] = {0x16cf26, 0x16dadb};
constexpr uintptr_t kRegisterEffectsRva = 0x9644b0;
constexpr int kEepkCharacter = 2;                         // RegisterEffects' type for a character's EEPK
constexpr int kEffectLogLines = 8;
constexpr uintptr_t kSkillPushRva = 0x4fc850;
constexpr uintptr_t kSkillPushCallRvas[] = {0x4f8851, 0x4f8903, 0x4fb3e1};
constexpr uintptr_t kListPollRva = 0x4fe4b0;
constexpr uintptr_t kListPollCallRva = 0x4fe103;

// EditChara's ctor 0x4f7280 passes its user-data slot count as 2 + 0x5e
// (mov edx, 2; lea r8d, [rdx+0x5e] at 0x4f72af). Raised to the most any mod
// needs (Battle Styles 0x60, Multi Accessory 0x64, us 0x66 = slots up to
// 103): each mod only ever raises the byte.
constexpr uintptr_t kSlotCountCodeRva = 0x4f72af, kSlotCountByteRva = 0x4f72b7;
constexpr uint8_t kSlotCountCode[] = {0xBA, 0x02, 0x00, 0x00, 0x00, 0x44, 0x8D, 0x42};
constexpr uint8_t kSlotCountVanilla = 0x5e, kSlotCountWanted = 0x66;

// The same ctor makes the AVATAR_LIST (+0x6c0) only in modes 1, 2, 5, 6 and 8:
// lea eax, [rcx-1]; test eax, ~5; je create; cmp ecx, 8; jne skip (0x4f7596).
// The Customize menu (mode 3) has none, so the jne (0x4f75a3) becomes two nops
// (as Battle Styles does; it may have patched it already).
constexpr uintptr_t kListCtorCheckRva = 0x4f7596, kListCtorSkipRva = 0x4f75a3;
constexpr uint8_t kListCtorCheckCode[] = {0x8D, 0x41, 0xFF, 0xA9, 0xFA, 0xFF, 0xFF, 0xFF, 0x74, 0x05, 0x83, 0xF9, 0x08, 0x75, 0x39};
constexpr uint8_t kNop2[] = {0x90, 0x90};

// Game files (read through the loader, so other mods' CUS/CMS edits count).
// CUS (Xv2CoreLib CUS_File.cs): counts at 16 (super) and 32 (blast), offsets
// at 40 (super), 44 (ultimate) and 56 (blast); an entry is
// (ultimate offset - super offset) / super count bytes: ShortName[4] at +0,
// ID1 u16 at +8, ID2 u16 at +10, race lock u8 at +12, files flags u16 at +14,
// EAN path offset u32 at +20, EEPK path offset u32 at +28.
constexpr const char *kCusPath = "system/custom_skill.cus";
constexpr size_t kCusSuperCount = 16, kCusBlastCount = 32, kCusSuperOffset = 40, kCusUltimateOffset = 44, kCusBlastOffset = 56;
constexpr size_t kCusEntryId1 = 8, kCusEntryId2 = 10, kCusEntryRaceLock = 12, kCusEntryFiles = 14, kCusEntryEanPath = 20, kCusEntryEepkPath = 28;
constexpr uint8_t kCusAllRaces = 0xff;   // race lock with every race's bit set
constexpr uint32_t kCusFilesEepk = 0x4;  // the skill loads an effect file (own, or from the EEPK path)
constexpr uint32_t kCusFilesCharaSpecificEan = 0x800; // the skill's EAN is <name>_<suffix>.ean (Xv2CoreLib CUS_File.cs)
constexpr int kBlastIdMin = 20000, kBlastIdMax = 24999; // blast skills' ID1
constexpr int kCacBlastIdMin = 21080, kCacBlastIdMax = 21099; // the CaC blasts (ki-blast-type.md)
constexpr int kCacStandardIdMax = 21084; // 21080..21084: the five standard types, named by the game
constexpr int kCommonBlastIdMax = 21091; // 21080..21091 are in CMN folders (the cpk's skill/BLT listing)
constexpr uint32_t kCusFilesMajinEan = 0x1;   // files flag: <name>_MAM.ean for a male Majin (0x260ba0)
constexpr size_t kCusEntryFlags = 13;         // byte; kCusFlagFemaleEan: <name>_F.ean for female races (0x260ba0)
constexpr uint8_t kCusFlagFemaleEan = 0x80;
constexpr uint8_t kCusFlagSkillEan = 0x40;    // without it 0x260ba0 returns at 0x260c27: the skill loads no EAN at all
// CUS skillsets (eternity_common CusFile.h CUSSkillSet): count at +8, table at +0xc,
// 32 bytes each: character (CMS id) at +0, skill ID1s u16 from +8; slot 7 = the blast.
constexpr size_t kCusSkillsetCount = 8, kCusSkillsetOffset = 0xc, kCusSkillsetSize = 32, kCusSkillsetBlast = 8 + 2 * 7;
// CMS (Xv2CoreLib CMS_File.cs): count at +8, table offset at +0xc, entries
// 0x54 bytes: id int32 at +0, code[4] at +4. A blast's ID2 / 10 = its owner's CMS id.
constexpr const char *kCmsPath = "system/char_model_spec.cms";
constexpr size_t kCmsEntrySize = 0x54;
// MSG ("#MSG", Xv2CoreLib MSG_File.cs, as Battle Styles reads it):
// ITM_STR_02_015..019 = Power/Rush/Bomb/Paralyze/Homing ki blast names.
constexpr const char *kMsgPath = "msg/menu_shop_text_en.msg";
// Keys chara_<CMS code>_<number>: the names shown on the roster.
constexpr const char *kCharaNamesPath = "msg/proper_noun_character_name_en.msg";
// Awoken skills are "met" in the game's files; names are met_skill_<ID2, 4 digits>.
constexpr const char *kAwokenNamesPath = "msg/proper_noun_skill_met_name_en.msg";
// CUS awoken table: count at 36, offset at 60 (Xv2CoreLib CUS Parser.cs). Entries are as
// long as the other skill tables'; CHANGE_SKILLSET 1..3 are i16s at +76.
constexpr size_t kCusAwokenCount = 36, kCusAwokenOffset = 60, kCusEntrySkillsetChange = 76;
constexpr int kCusSkillsetChanges = 3;
constexpr size_t kCusSkillsetPreset = 26; // the skillset's preset word, what CHANGE_SKILLSET names
// CMS 100..107 (HUM HUF SYM SYF NMC FRI MAM MAF) -> CUS race lock bit (Xv2CoreLib CusRaceLock).
constexpr int kCacCharaMin = 100;
constexpr uint8_t kCacCharaRaceBits[] = {0x01, 0x20, 0x02, 0x40, 0x04, 0x08, 0x10, 0x80};
constexpr char kDefaultLabel[] = "Default";
constexpr char kRaceDefaultLabel[] = "Race Default";
constexpr int kFixedEntries = 2; // list entries before the blasts: Default, Race Default

struct ListGameWString { // MSVC std::wstring as the game's list entries hold it
    union { wchar_t inlineText[8]; const wchar_t *text; };
    size_t size;
    size_t capacity; // 7 = the inline buffer
};
struct ListEntry { // one AVATAR_LIST entry (0x28 bytes, list opener template 0x4fd5a0)
    ListGameWString name;
    int32_t valid;   // 1 = selectable
    int32_t unknown; // 0 in every vanilla list
};
static_assert(sizeof(ListEntry) == 0x28, "AVATAR_LIST entry");
struct ListParam { // what 0x504450 takes: default entry, then a std::vector<ListEntry> (only begin/end read)
    int32_t defaultIndex;
    int32_t padding;
    ListEntry *begin, *end, *capacityEnd;
};

using CurrentSlotFn = int(__fastcall *)(void *managerPlus8);
using UserCallbackFn = void(__fastcall *)(void *menu, const uint8_t *call);
using SkillPushFn = void(__fastcall *)(void *menu, int customize);
using ListPollFn = uint64_t(__fastcall *)(void *menu);
using SetUserDataIntFn = void(__fastcall *)(void *menu, int index, int value);
using SetUserDataStrFn = void(__fastcall *)(void *menu, int index, const wchar_t *text);
using SetListContentsFn = void(__fastcall *)(void *list, const ListParam *param);
using SetMenuStateFn = void(__fastcall *)(void *stateMachine, int state, int, int);
using ListHasResultFn = bool(__fastcall *)(void *list);
using ListGetResultFn = int(__fastcall *)(void *list);
using SetListXEntryFn = void(__fastcall *)(void *list, int index);

std::wstring g_modDir;
bool g_logging = false;
UserCallbackFn g_mysetCallback = nullptr;     // EditMyset's slot 17 before our hook
UserCallbackFn g_editCharaCallback = nullptr; // EditChara's slot 17 before our hook

template <typename T> T &Field(void *base, size_t offset)
{
    return *reinterpret_cast<T *>(static_cast<char *>(base) + offset);
}
template <typename Fn> Fn Game(uintptr_t rva) { return reinterpret_cast<Fn>(g_exe + rva); }

bool g_searchWanted = true; // config "blast_search"

// --- Log ------------------------------------------------------------------------

std::mutex g_logLock;    // threads call in from the game and the loader
constexpr int kMaxLogLines = 600; // per session: callers log per event, never per frame

void VLog(const char *format, va_list args)
{
    if (!g_logging) return;
    std::lock_guard<std::mutex> lock(g_logLock);
    static int lines = 0;
    if (lines >= kMaxLogLines) return;
    FILE *file = nullptr;
    // One session per log: the first write of a run moves the last run's log to <name>.prev.log.
    static bool rotated = false;
    std::wstring path = g_modDir + L"ki_blast_override.log";
    if (!rotated) {
        rotated = true;
        MoveFileExW(path.c_str(), (g_modDir + L"ki_blast_override.prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    if (_wfopen_s(&file, path.c_str(), L"a") != 0 || !file) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    fprintf(file, "%02d:%02d:%02d ", now.wHour, now.wMinute, now.wSecond);
    vfprintf(file, format, args);
    fputc('\n', file);
    if (++lines == kMaxLogLines) fputs("(log line limit reached)\n", file);
    fclose(file);
}

void Log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    VLog(format, args);
    va_end(args);
}

// --- Config ------------------------------------------------------------------

// Reads ki_blast_override.json; a missing config is written from the default.
void ReadConfig()
{
    std::wstring path = g_modDir + L"ki_blast_override.json";
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) {
        if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file) {
            fputs(kDefaultConfig, file);
            fclose(file);
        }
        return;
    }
    std::string text(16384, '\0');
    text.resize(fread(&text[0], 1, text.size() - 1, file));
    fclose(file);
    // The text after "<key>":, null when the key is missing.
    auto valueOf = [&](const char *key) -> const char * {
        const char *found = strstr(text.c_str(), key);
        const char *colon = found ? strchr(found, ':') : nullptr;
        return colon ? colon + 1 + strspn(colon + 1, " \t\r\n") : nullptr;
    };
    const char *logging = valueOf("\"logging\"");
    g_logging = logging && strncmp(logging, "true", 4) == 0;
    // Added after the first releases, so a config without the key keeps the search on.
    const char *search = valueOf("\"blast_search\"");
    g_searchWanted = !search || strncmp(search, "false", 5) != 0;
    const char *transformations = valueOf("\"override_transformation_blasts\"");
    BlastSites_KeepThroughTransformations(transformations && strncmp(transformations, "true", 4) == 0);
}

// --- Game files -----------------------------------------------------------------

const XV2Files &Files()
{
    static XV2Files files = [] {
        XV2Files found;
        found.Load();
        return found;
    }();
    return files;
}

bool ReadGameFile(const char *path, std::vector<uint8_t> &out)
{
    const XV2Files &files = Files();
    if (!files.FindGameFile || !files.ReadGameFile) return false;
    char from[MAX_PATH] = {};
    long long size = files.FindGameFile(path, from, sizeof(from));
    if (size <= 0) return false;
    out.resize(static_cast<size_t>(size));
    return files.ReadGameFile(path, out.data(), out.size()) == size;
}

uint32_t Rd32(const std::vector<uint8_t> &data, size_t offset)
{
    uint32_t value = 0;
    if (offset + 4 <= data.size()) memcpy(&value, data.data() + offset, 4);
    return value;
}
uint32_t Rd16(const std::vector<uint8_t> &data, size_t offset)
{
    uint16_t value = 0;
    if (offset + 2 <= data.size()) memcpy(&value, data.data() + offset, 2);
    return value;
}

// Where a CUS keeps its blast skills; false with a reason when the layout is unexpected.
struct CusBlastTable { size_t offset; uint32_t count, stride; };
bool FindCusBlasts(const std::vector<uint8_t> &cus, CusBlastTable &table, const char *&why)
{
    uint32_t superCount = Rd32(cus, kCusSuperCount), superOffset = Rd32(cus, kCusSuperOffset);
    uint32_t ultimateOffset = Rd32(cus, kCusUltimateOffset);
    table = {Rd32(cus, kCusBlastOffset), Rd32(cus, kCusBlastCount), 0};
    if (!superCount || ultimateOffset <= superOffset || table.count > 4096) { why = "unexpected CUS layout"; return false; }
    table.stride = (ultimateOffset - superOffset) / superCount;
    if (table.stride < kCusEntryEepkPath + 4 || table.stride > 256) { why = "unexpected CUS entry size"; return false; }
    return true;
}

// Most roster blasts have race lock 0, and a CaC's blast word with such a skill
// reads 0xffff (no blast) by the battle setup, whatever the loadout said: Hit's
// 21260 and 21350 did, GUNN's 21590 (0xff) didn't. What blanks the word wasn't found,
// so the lock is the likely cause, not a proven one (ki-blast-type.md). Opens every
// blast to every race; the game's own skill lists never offer blasts.
// Also sets the per-character EAN flag on every blast: which ones get a port is known
// only after this file's read, and a character without a file falls through to the
// same lookup as without the flag (no vanilla blast lacking it has such a file,
// ClaudeTools\kiblast\blast_port_sources.py era check; 0x260d72 is its only reader).
// A ported blast also needs the skill EAN flag: 21085 and 21086 have no EAN, so vanilla
// never loads one for them. That is only known from the list, so this waits for it
// (a raw read of this file and of the BACs, no game code: nothing here can wait on us).
bool EnsureBlastList();
std::set<int> g_blastsCarryingAnimations; // by ID1, fixed once the list is built

int __cdecl PrepareCusBlasts(void *, const char *, const void *, size_t, const void *current, size_t currentSize,
                             XV2FileOutput *output)
{
    const bool listReady = EnsureBlastList();
    const auto *bytes = static_cast<const uint8_t *>(current);
    std::vector<uint8_t> cus(bytes, bytes + currentSize);
    CusBlastTable table{};
    const char *why = "";
    if (!FindCusBlasts(cus, table, why)) {
        strncpy_s(output->note, why, _TRUNCATE);
        return -1;
    }
    int opened = 0, flagged = 0, skillEans = 0;
    for (uint32_t i = 0; i < table.count; i++) {
        size_t entry = table.offset + static_cast<size_t>(i) * table.stride;
        if (entry + table.stride > cus.size()) break;
        int id1 = static_cast<int>(Rd16(cus, entry + kCusEntryId1));
        if (id1 < kBlastIdMin || id1 > kBlastIdMax) continue;
        if (cus[entry + kCusEntryRaceLock] != kCusAllRaces) {
            cus[entry + kCusEntryRaceLock] = kCusAllRaces;
            opened++;
        }
        uint32_t files = Rd16(cus, entry + kCusEntryFiles);
        if (!(files & kCusFilesCharaSpecificEan)) {
            uint16_t withFlag = static_cast<uint16_t>(files | kCusFilesCharaSpecificEan);
            memcpy(cus.data() + entry + kCusEntryFiles, &withFlag, sizeof(withFlag));
            flagged++;
        }
        if (listReady && g_blastsCarryingAnimations.count(id1) && !(cus[entry + kCusEntryFlags] & kCusFlagSkillEan)) {
            cus[entry + kCusEntryFlags] |= kCusFlagSkillEan;
            skillEans++;
        }
    }
    if (!opened && !flagged && !skillEans) return 0;
    sprintf_s(output->note, "%d blast race locks opened, %d per-character EAN flags set, %d skill EAN flags set", opened, flagged, skillEans);
    output->Set(output, cus.data(), cus.size());
    return 1;
}

std::string Code4(const std::vector<uint8_t> &data, size_t offset)
{
    std::string code;
    for (size_t i = 0; i < 4 && offset + i < data.size() && data[offset + i]; i++) code += static_cast<char>(data[offset + i]);
    return code;
}

// msg file: "#MSG", then at +8 {count, names table, ids table, lines table}.
// Names: 16 bytes {offset, chars, bytes, -}, UTF-16 when bytes = 2 x chars
// (+ terminator), else 8-bit. Lines: 8 bytes {count, offset} -> 16 bytes
// {offset, chars, bytes, -}, UTF-16 (Battle Styles style_menu.cpp ReadMsgNames).
std::map<std::string, std::wstring> ReadMsgNames(const std::vector<uint8_t> &msg)
{
    std::map<std::string, std::wstring> names;
    if (msg.size() < 0x18 || memcmp(msg.data(), "#MSG", 4) != 0) return names;
    uint32_t count = Rd32(msg, 8), namesTable = Rd32(msg, 0xc), linesTable = Rd32(msg, 0x14);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t nameOffset = Rd32(msg, namesTable + 16 * i), nameChars = Rd32(msg, namesTable + 16 * i + 4);
        uint32_t nameBytes = Rd32(msg, namesTable + 16 * i + 8);
        uint32_t lineCount = Rd32(msg, linesTable + 8 * i), lineEntry = Rd32(msg, linesTable + 8 * i + 4);
        if (!lineCount || static_cast<uint64_t>(nameOffset) + nameBytes > msg.size()) continue;
        std::string name;
        bool wide = nameBytes >= 2 * nameChars && nameBytes % 2 == 0;
        for (uint32_t c = 0; c < (wide ? nameBytes / 2 : nameBytes); ++c) {
            uint8_t ch = msg[nameOffset + (wide ? 2 * c : c)];
            if (!ch) break;
            name += static_cast<char>(ch);
        }
        uint32_t textOffset = Rd32(msg, lineEntry), textBytes = Rd32(msg, lineEntry + 8);
        if (static_cast<uint64_t>(textOffset) + textBytes > msg.size()) continue;
        std::wstring text;
        for (uint32_t c = 0; 2 * c + 1 < textBytes; ++c) {
            wchar_t ch = static_cast<wchar_t>(msg[textOffset + 2 * c] | (msg[textOffset + 2 * c + 1] << 8));
            if (!ch) break;
            text += ch;
        }
        names[name] = text;
    }
    return names;
}

// --- The blast list ---------------------------------------------------------------
// Default and Race Default first, then the CaC blasts by ID1, then every other loaded blast
// by ID1. Built once from the CUS the game loads; never on a tick.

struct Blast {
    int id1;            // or kPickDefault / kPickRaceDefault
    std::wstring label;
};
std::mutex g_listLock;
std::vector<Blast> g_blasts;     // fixed once g_listState is 1
std::map<int, int> g_cacPorts;   // hidden blast -> the game's CaC port of it; fixed once g_listState is 1
std::vector<ListEntry> g_entries; // the AVATAR_LIST entries; long names point into g_blasts' labels
std::atomic<int> g_listState{0}; // 0 = not built, 1 = ready, 2 = gave up
int g_listAttempts = 0;
ULONGLONG g_listLastAttempt = 0;
constexpr int kMaxListAttempts = 20;
constexpr ULONGLONG kListRetryMs = 5000; // the game's files may not be readable yet while it starts

ListEntry ListEntryFor(const std::wstring &name)
{
    ListEntry entry{};
    entry.name.size = name.size();
    if (name.size() < 8) {
        memcpy(entry.name.inlineText, name.c_str(), (name.size() + 1) * sizeof(wchar_t));
        entry.name.capacity = 7;
    } else {
        entry.name.text = name.c_str(); // only read: the list copies it
        entry.name.capacity = name.size();
    }
    entry.valid = 1;
    return entry;
}

std::wstring Widen(const std::string &text) { return std::wstring(text.begin(), text.end()); }

// The row text of a blast skill code: the game's own name for the five
// standard types, else the code. The text must name the type (a different
// message layout must not label a blast wrongly).
std::wstring TypeLabel(const std::string &code, const std::map<std::string, std::wstring> &messages)
{
    static const struct { const char *code, *message, *keyword; } types[] = {
        {"POW", "ITM_STR_02_015", "power"}, {"RSH", "ITM_STR_02_016", "rush"},      {"BOM", "ITM_STR_02_017", "bomb"},
        {"PAL", "ITM_STR_02_018", "paraly"}, {"HOM", "ITM_STR_02_019", "homing"},
    };
    for (const auto &type : types) {
        if (code != type.code) continue;
        auto found = messages.find(type.message);
        if (found == messages.end() || found->second.empty()) break;
        std::wstring lower = found->second;
        for (wchar_t &c : lower) c = static_cast<wchar_t>(towlower(c));
        if (lower.find(Widen(type.keyword)) != std::wstring::npos) return found->second;
        break;
    }
    return Widen(code);
}

// The CaC blasts that aren't one of the five types, named after who uses them
// in the vanilla data: the presets (CUS SkillsetChange / CMS) that select each
// file (ki-blast-type.md).
const wchar_t *CacSpecialLabel(const std::string &code)
{
    static const struct { const char *code; const wchar_t *label; } specials[] = {
        {"GBM", L"Golden Frieza - GBM"}, {"MAS", L"Kid Buu - MAS"}, {"HRS", L"Ribrianne - HRS"},   {"ISA", L"Hercule - ISA"},
        {"GOA", L"Gamma 1 - GOA"},       {"GTA", L"Gamma 2 - GTA"}, {"MGA", L"Ultra Instinct - MGA"},
    };
    for (const auto &special : specials)
        if (code == special.code) return special.label;
    return nullptr;
}

// The roster name of a CMS code: the first chara_<code>_ message, else empty.
std::wstring RosterName(const std::string &code, const std::map<std::string, std::wstring> &names)
{
    std::string prefix = "chara_" + code + "_";
    auto found = names.lower_bound(prefix);
    if (found != names.end() && found->first.compare(0, prefix.size(), prefix) == 0) return found->second;
    return L"";
}

// The name of the x2m a blast folder came from: the installer leaves its GUID in
// X2M_SKILL.ini and a zip per mod in InstallData (ki-blast-type.md, "Blast names").
// Empty for a blast it didn't install.
std::wstring ModNameOfFolder(const std::string &folder)
{
    std::vector<uint8_t> ini, x2d, xml;
    if (folder.empty() || !ReadGameFile(("skill/BLT/" + folder + "/X2M_SKILL.ini").c_str(), ini)) return L"";
    const std::string guid = X2mIniGuid(ini);
    if (guid.empty() || !ReadGameFile(("InstallData/" + guid + ".x2d").c_str(), x2d)) return L"";
    return ReadZipEntry(x2d, "x2m.xml", xml) ? X2mXmlValue(xml, "MOD_NAME") : L"";
}

// Per blast ID1, the name of the awoken skill whose transformation selects it: a CaC race's
// skillset with that blast and a preset the skill changes to, for a race the skill allows.
// How an installer package gives a transformation its own blast (no x2m can add a preset).
std::map<int, std::wstring> BlastsOfAwokenSkills(const std::vector<uint8_t> &cus, uint32_t stride, const std::map<std::string, std::wstring> &names)
{
    std::map<int, std::wstring> found;
    const uint32_t skillsets = Rd32(cus, kCusSkillsetCount), skillsetTable = Rd32(cus, kCusSkillsetOffset);
    const uint32_t awokens = Rd32(cus, kCusAwokenCount), awokenTable = Rd32(cus, kCusAwokenOffset);
    if (stride < kCusEntrySkillsetChange + 2 * kCusSkillsetChanges) return found;
    for (uint32_t i = 0; i < skillsets && skillsetTable + (static_cast<size_t>(i) + 1) * kCusSkillsetSize <= cus.size(); i++) {
        const size_t skillset = skillsetTable + static_cast<size_t>(i) * kCusSkillsetSize;
        const int raceIndex = static_cast<int>(Rd32(cus, skillset)) - kCacCharaMin;
        if (raceIndex < 0 || raceIndex >= static_cast<int>(sizeof(kCacCharaRaceBits))) continue;
        const int blast = Rd16(cus, skillset + kCusSkillsetBlast), preset = Rd16(cus, skillset + kCusSkillsetPreset);
        if (found.count(blast)) continue;
        for (uint32_t k = 0; k < awokens && awokenTable + (static_cast<size_t>(k) + 1) * stride <= cus.size(); k++) {
            const size_t awoken = awokenTable + static_cast<size_t>(k) * stride;
            if (!(cus[awoken + kCusEntryRaceLock] & kCacCharaRaceBits[raceIndex])) continue;
            bool selects = false;
            for (int change = 0; change < kCusSkillsetChanges; change++)
                selects |= static_cast<int>(Rd16(cus, awoken + kCusEntrySkillsetChange + 2 * static_cast<size_t>(change))) == preset;
            if (!selects) continue;
            char key[32];
            sprintf_s(key, "met_skill_%04d", static_cast<int>(Rd16(cus, awoken + kCusEntryId2)));
            auto name = names.find(key);
            if (name == names.end() || name->second.empty()) continue;
            found[blast] = name->second;
            break;
        }
    }
    return found;
}

// Who a blast comes from, for its row, best source first: the character that owns its ID2;
// the x2m that installed it (mod blasts share a nameless owner, XZT); the awoken skill that
// selects it; the first character preset using it (a moveset mod's character; last because a
// preset can point at an id the installer gave to an unrelated mod: GUNN's 21590 was named
// after Jiren that way); the owner's code. Empty when nothing is known.
std::wstring BlastOwner(int id1, int id2, const std::string &folder, const std::map<int, std::string> &ownerCodes,
                        const std::map<int, int> &firstUsers, const std::map<int, std::wstring> &awokenBlasts,
                        const std::map<std::string, std::wstring> &names)
{
    auto owner = ownerCodes.find(id2 / 10);
    if (owner != ownerCodes.end() && !owner->second.empty()) {
        std::wstring name = RosterName(owner->second, names);
        if (!name.empty()) return name;
    }
    std::wstring modName = ModNameOfFolder(folder);
    if (!modName.empty()) return modName;
    auto awoken = awokenBlasts.find(id1);
    if (awoken != awokenBlasts.end()) return awoken->second;
    auto user = firstUsers.find(id1);
    if (user != firstUsers.end()) {
        auto userCode = ownerCodes.find(user->second);
        if (userCode != ownerCodes.end() && !userCode->second.empty()) {
            std::wstring name = RosterName(userCode->second, names);
            if (!name.empty()) return name;
        }
    }
    return owner != ownerCodes.end() ? Widen(owner->second) : L"";
}

// Per blast ID1, the character (CMS id) of the first CUS skillset that selects it.
std::map<int, int> FirstBlastUsers(const std::vector<uint8_t> &cus)
{
    std::map<int, int> users;
    uint32_t count = Rd32(cus, kCusSkillsetCount), table = Rd32(cus, kCusSkillsetOffset);
    for (uint32_t i = 0; i < count && table + (static_cast<size_t>(i) + 1) * kCusSkillsetSize <= cus.size(); i++) {
        size_t skillset = table + static_cast<size_t>(i) * kCusSkillsetSize;
        users.emplace(Rd16(cus, skillset + kCusSkillsetBlast), static_cast<int>(Rd32(cus, skillset)));
    }
    return users;
}

// Per blast ID1, the characters (CMS ids, ascending) of every CUS skillset that selects it.
std::map<int, std::set<int>> AllBlastUsers(const std::vector<uint8_t> &cus)
{
    std::map<int, std::set<int>> users;
    uint32_t count = Rd32(cus, kCusSkillsetCount), table = Rd32(cus, kCusSkillsetOffset);
    for (uint32_t i = 0; i < count && table + (static_cast<size_t>(i) + 1) * kCusSkillsetSize <= cus.size(); i++) {
        size_t skillset = table + static_cast<size_t>(i) * kCusSkillsetSize;
        users[Rd16(cus, skillset + kCusSkillsetBlast)].insert(static_cast<int>(Rd32(cus, skillset)));
    }
    return users;
}

// A type label without " Ki Blast", for "<owner> - <type>".
std::wstring ShortTypeLabel(std::wstring label)
{
    const std::wstring suffix = L" Ki Blast";
    if (label.size() > suffix.size() && label.compare(label.size() - suffix.size(), suffix.size(), suffix) == 0)
        label.resize(label.size() - suffix.size());
    return label;
}

// A roster blast that is just one of the five standard types: the standard code and
// no effects of its own (none at all, or the EEPK path points at a CMN skill's).
// Blasts with own or BRL effects stay. ClaudeTools\kiblast\blast_cus.py shows the data.
bool IsStandardTypeCopy(const std::vector<uint8_t> &cus, size_t entry, const std::string &code)
{
    static const char *const standard[] = {"POW", "RSH", "BOM", "PAL", "HOM"};
    bool isStandardCode = false;
    for (const char *type : standard) isStandardCode |= code == type;
    if (!isStandardCode) return false;
    if (!(Rd16(cus, entry + kCusEntryFiles) & kCusFilesEepk)) return true;
    uint32_t path = Rd32(cus, entry + kCusEntryEepkPath);
    if (!path || path >= cus.size()) return false; // effects in its own folder
    const char *text = reinterpret_cast<const char *>(cus.data()) + path;
    return strnlen(text, cus.size() - path) < cus.size() - path && strstr(text, "_CMN_") != nullptr;
}

// The folder a blast takes its effects from when its EEPK path is "BLT/<folder>" or
// "BLT/<folder>/<file>"; empty otherwise.
std::string EepkPathFolder(const std::vector<uint8_t> &cus, size_t entry)
{
    uint32_t path = Rd32(cus, entry + kCusEntryEepkPath);
    if (!path || path >= cus.size() || strnlen(reinterpret_cast<const char *>(cus.data()) + path, cus.size() - path) >= cus.size() - path) return "";
    std::string text = reinterpret_cast<const char *>(cus.data()) + path;
    if (text.rfind("BLT/", 0) != 0) return "";
    text.erase(0, 4);
    return text.substr(0, text.find('/'));
}

// The CaC blasts 21080..21091 live in CMN folders (1086_CMN_MAS), whatever character
// their ID2 / 10 would name (108 is MAP).
bool IsCommonBlast(int id1) { return id1 >= kCacBlastIdMin && id1 <= kCommonBlastIdMax; }

// The skill folder of a blast (not padded: BLT/400_BRL_BOM); empty when its owner is unknown.
std::string BlastFolder(int id1, int id2, const std::string &code, const std::map<int, std::string> &ownerCodes)
{
    if (IsCommonBlast(id1)) return std::to_string(id2) + "_CMN_" + code;
    auto owner = ownerCodes.find(id2 / 10);
    if (owner == ownerCodes.end() || owner->second.empty()) return "";
    return std::to_string(id2) + "_" + owner->second + "_" + code;
}

bool GameFileExists(const std::string &path) { return Files().FindGameFile && Files().FindGameFile(path.c_str(), nullptr, 0) >= 0; }

// A blast whose animations exist only for its owner: it asks for a character's own
// EAN (<folder>_brl.ean), has no EAN path to borrow one and no plain <folder>.ean to
// fall back on. On a CaC its charged BAC entries play a missing animation and the
// character stays stuck firing (Broly's 20400, Gamma's 22060 and 22070 in game).
bool LacksCacEan(const std::vector<uint8_t> &cus, size_t entry, const std::string &folder)
{
    if (!(Rd16(cus, entry + kCusEntryFiles) & kCusFilesCharaSpecificEan) || Rd32(cus, entry + kCusEntryEanPath) != 0) return false;
    return !folder.empty() && !GameFileExists("skill/BLT/" + folder + "/" + folder + ".ean");
}

// <game>\data, from the mod folder <game>\XV2ModLoader\mods\KiBlastOverride\.
std::wstring GameDataDir()
{
    std::wstring dir = g_modDir;
    for (int up = 0; up < 3 && !dir.empty(); up++) { // each pass drops one folder and its slash
        dir.resize(dir.size() - 1);
        size_t slash = dir.find_last_of(L"\\/");
        dir.resize(slash == std::wstring::npos ? 0 : slash + 1);
    }
    return dir + L"data\\";
}

// The files listed in converted_eans.txt, read once.
std::set<std::string> &WrittenFiles()
{
    static std::set<std::string> written = [] {
        std::set<std::string> lines;
        FILE *list = nullptr;
        if (_wfopen_s(&list, (g_modDir + L"converted_eans.txt").c_str(), L"r") != 0 || !list) return lines;
        char line[MAX_PATH * 2];
        while (fgets(line, sizeof(line), list)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0]) lines.insert(line);
        }
        fclose(list);
        return lines;
    }();
    return written;
}

// Writes data\<path> (backslashes), creating folders, and notes it in
// converted_eans.txt so build.bat uninstall can take it out again.
bool WriteConvertedFile(const std::string &path, const std::vector<uint8_t> &bytes)
{
    const std::wstring root = GameDataDir(), full = root + Widen(path);
    for (size_t slash = full.find(L'\\', root.size()); slash != std::wstring::npos; slash = full.find(L'\\', slash + 1))
        CreateDirectoryW(full.substr(0, slash).c_str(), nullptr);
    HANDLE file = CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
    CloseHandle(file);
    if (!ok) return false;
    const std::wstring manifest = g_modDir + L"converted_eans.txt";
    HANDLE list = WrittenFiles().count(path) ? INVALID_HANDLE_VALUE // rewritten: already listed
                                             : CreateFileW(manifest.c_str(), FILE_APPEND_DATA, 0, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (list != INVALID_HANDLE_VALUE) {
        const std::string line = path + "\r\n";
        WriteFile(list, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        CloseHandle(list);
    }
    WrittenFiles().insert(path);
    return true;
}

// True for a file this mod wrote, in an earlier run or this one. `path` with / or \.
bool IsOurFile(std::string path)
{
    std::replace(path.begin(), path.end(), '/', '\\');
    return WrittenFiles().count(path) != 0;
}

// The character EAN to convert a blast from: the owner's, else any other
// <folder>_<suffix>.ean: loose ones are listed, cpk ones tried per CMS code
// (the game looks one up by the character's code; 630_TOW_PAL also has _GAL).
std::string BlastSourceEan(const std::string &folder, int id2, const std::map<int, std::string> &ownerCodes)
{
    const std::string base = "skill/BLT/" + folder + "/" + folder;
    auto owner = ownerCodes.find(id2 / 10);
    if (owner != ownerCodes.end() && GameFileExists(base + "_" + owner->second + ".ean")) return base + "_" + owner->second + ".ean";
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW((GameDataDir() + L"skill\\BLT\\" + Widen(folder) + L"\\*.ean").c_str(), &found);
    if (search != INVALID_HANDLE_VALUE) {
        std::string name;
        do {
            // file names here are ASCII folder codes; a conversion cut short can leave our _F, _MAM
            std::string candidate;
            for (const wchar_t *c = found.cFileName; *c; c++) candidate += static_cast<char>(*c);
            const bool ours = candidate == folder + "_F.ean" || candidate == folder + "_MAM.ean" || IsOurFile("skill\\BLT\\" + folder + "\\" + candidate);
            if (!ours && candidate.find(".cam.") == std::string::npos) name = candidate;
        } while (name.empty() && FindNextFileW(search, &found));
        FindClose(search);
        if (!name.empty()) return "skill/BLT/" + folder + "/" + name;
    }
    for (const auto &code : ownerCodes) {
        const std::string candidate = base + "_" + code.second + ".ean";
        if (!code.second.empty() && !IsOurFile(candidate) && GameFileExists(candidate)) return candidate;
    }
    return "";
}

// Gives a LacksCacEan blast the files a CaC blast has, as the game's own CMN ones:
// plain <folder>.ean, _F (female races) and _MAM, all on the skeleton of those races.
// The owner's own file is untouched, and the game prefers it for the owner (21080
// has .ean, _F, _MAM and _GOK with the same 0x800 flag). Animations are bone-remapped
// by name (Battle Styles' FitEanToRace). The plain file goes last, as it is what marks
// a blast as done. `races` caches the race files for this list build.
bool ConvertBlastForCac(const std::string &folder, int id2, const std::map<int, std::string> &ownerCodes,
                        std::map<std::string, std::vector<uint8_t>> &races)
{
    const std::string sourcePath = BlastSourceEan(folder, id2, ownerCodes);
    std::vector<uint8_t> source;
    if (sourcePath.empty() || !ReadGameFile(sourcePath.c_str(), source)) { Log("convert %s: no character EAN found", folder.c_str()); return false; }
    static const struct { const char *suffix, *race; } targets[] = {{"_F", "HUF"}, {"_MAM", "MAM"}, {"", "HUM"}};
    for (const auto &target : targets) {
        std::vector<uint8_t> &race = races[target.race];
        const std::string racePath = std::string("chara/") + target.race + "/" + target.race + ".ean";
        if (race.empty() && !ReadGameFile(racePath.c_str(), race)) { Log("convert %s: cannot read %s", folder.c_str(), racePath.c_str()); return false; }
        std::vector<uint8_t> fitted;
        EanFit fit;
        std::string note;
        if (!FitEanToRace(source.data(), source.size(), race.data(), race.size(), true, 0, fitted, fit, note, {}, true)) {
            Log("convert %s: %s: %s", folder.c_str(), target.race, note.c_str());
            return false;
        }
        const std::string path = "skill\\BLT\\" + folder + "\\" + folder + target.suffix + ".ean";
        if (!WriteConvertedFile(path, fitted)) { Log("convert %s: cannot write %s", folder.c_str(), path.c_str()); return false; }
        Log("convert %s: %s -> %s (%d bytes): %s", folder.c_str(), sourcePath.c_str(), path.c_str(), static_cast<int>(fitted.size()), note.c_str());
    }
    return true;
}

// ---- Carrying a character's blast to a CaC (PLAN.md, ki-blast-type.md "Porting a blast")

// A blast's BAC as it asks the character's files.
constexpr uint32_t kBacEanCharacter = 5, kBacEanSkill = 65534; // animation record, EAN type at +8, id at +10
constexpr uint32_t kBacEepkCharacter = 2;                      // effect record, EEPK type at +8
constexpr size_t kBacRecordEan = 8, kBacRecordAnimation = 10;
constexpr size_t kBacRecordEepk = 8, kBacRecordEffectOwner = 0xc; // dword read at 0xf9820, the id SpawnEffect gets
constexpr int kBacTypeAnimation = 0, kBacTypeEffect = 8, kBacTypeProjectile = 9;
constexpr size_t kBacAnimationSize = 36, kBacEffectSize = 48, kBacProjectileSize = 48; // Xv2CoreLib BAC_File.cs
// Projectile record: BSA skill id, BSA type, entry. Type 255 + skill 65535 is the common file.
constexpr size_t kBacRecordBsaSkill = 8, kBacRecordBsaType = 10, kBacRecordBsaEntry = 12;
constexpr uint32_t kBacBsaAnySkill = 0xFFFF, kBacBsaTypeCommon = 255;
// Character animation ids below this are the same base moves on every race
// (HUM_BAS_STAND_S / JRN_BAS_STAND_S), and stay the CaC's own; above it each
// character has its own (a Namekian's are the Giant form). blast_port_check.py.
constexpr int kFirstCarriedAnimation = 82;

struct PortedBlast {
    std::map<int, int> animationSlots; // character animation id -> its id in the skill EAN
    int effectOwner = -1;              // CMS id whose EEPK the blast's character effects use
    std::map<int, int> commonBsaSlots; // common BSA entry -> its copy naming effectOwner
};
std::map<std::string, PortedBlast> g_portedBlasts; // by folder; nodes stay put, the BAC stages point at them
std::set<int> g_effectOwners;                      // fixed once g_listState is 1

// visit(type, record offset) for the animation and effect records of a BAC.
// Layout: blast_needs.py bac_refs (Xv2CoreLib BAC_File.cs).
template <class Visit>
void ForEachBacRecord(const std::vector<uint8_t> &bac, Visit visit)
{
    constexpr uint32_t kEmptyEntry = 0x80000000;
    const uint32_t count = Rd32(bac, 8), table = Rd32(bac, 16);
    for (uint32_t index = 0; index < count && table + (static_cast<size_t>(index) + 1) * 16 <= bac.size(); index++) {
        const size_t entry = table + static_cast<size_t>(index) * 16;
        if (Rd32(bac, entry) & kEmptyEntry) continue;
        const uint32_t typeCount = Rd16(bac, entry + 4), types = Rd32(bac, entry + 8);
        for (uint32_t t = 0; t < typeCount && types + (static_cast<size_t>(t) + 1) * 16 <= bac.size(); t++) {
            const size_t head = types + static_cast<size_t>(t) * 16;
            const int type = static_cast<int16_t>(Rd16(bac, head));
            const size_t size = type == kBacTypeAnimation    ? kBacAnimationSize
                                : type == kBacTypeEffect     ? kBacEffectSize
                                : type == kBacTypeProjectile ? kBacProjectileSize
                                                             : 0;
            const uint32_t records = Rd16(bac, head + 2), at = Rd32(bac, head + 8);
            if (!size || !at) continue;
            for (uint32_t r = 0; r < records && at + (static_cast<size_t>(r) + 1) * size <= bac.size(); r++)
                visit(type, at + static_cast<size_t>(r) * size);
        }
    }
}

// A projectile's effects: the common BSA shows some from the spawner's own EEPK (MAS's entry
// 120, Kid Buu's effect 10 on the target's leg). The record's id reaches SpawnEffect as it is
// (0x258770, sign extended), and vanilla entries also name characters (180: 150), so a copy of
// the entry naming the owner is the same fix as the BAC's id. A copy, as other characters' moves
// use the original. Layout: Xv2CoreLib BSA/Parser.cs. Slot count u16 @18, table u32 @20 (0 = empty);
// entry: sub-types u16 +34, their headers' offset from the entry u16 +36; 16 byte header: type u16,
// records u16 +6, data offset i32 +12 from the header; type 6, 12 byte effects: EEPK type u16, id u16.
constexpr const char *kCommonBsaPath = "skill/CMN/CMN.bsa";
constexpr uint32_t kBsaTypeEffect = 6, kBsaSpawnerId = 0xFFFF;
constexpr size_t kBsaEffectSize = 12, kBsaEffectOwner = 2;

struct CommonBsaCopy { int entry, owner, slot; };
std::vector<CommonBsaCopy> g_commonBsaCopies; // fixed once g_listState is 1

size_t BsaSlotCount(const std::vector<uint8_t> &bsa) { return bsa.size() < 24 ? 0 : Rd16(bsa, 18); }

size_t BsaEntryOffset(const std::vector<uint8_t> &bsa, size_t slot)
{
    if (slot >= BsaSlotCount(bsa)) return 0;
    const size_t at = static_cast<size_t>(Rd32(bsa, 20)) + 4 * slot;
    return at + 4 <= bsa.size() ? Rd32(bsa, at) : 0;
}

// visit(record offset) for the effects of the entry at `entry` showing the spawner's own EEPK.
template <class Visit>
void ForEachBsaSpawnerEffect(const std::vector<uint8_t> &bsa, size_t entry, Visit visit)
{
    if (!entry || entry + 38 > bsa.size()) return;
    const uint32_t typeCount = Rd16(bsa, entry + 34), types = Rd16(bsa, entry + 36);
    for (uint32_t t = 0; t < typeCount; t++) {
        const size_t head = entry + types + static_cast<size_t>(t) * 16;
        if (head + 16 > bsa.size()) return;
        if (Rd16(bsa, head) != kBsaTypeEffect) continue;
        const size_t data = head + static_cast<size_t>(static_cast<int32_t>(Rd32(bsa, head + 12)));
        for (uint32_t r = 0, records = Rd16(bsa, head + 6); r < records; r++) {
            const size_t record = data + static_cast<size_t>(r) * kBsaEffectSize;
            if (record + kBsaEffectSize > bsa.size()) return;
            if (Rd16(bsa, record) == kBacEepkCharacter && Rd16(bsa, record + kBsaEffectOwner) == kBsaSpawnerId) visit(record);
        }
    }
}

const std::vector<uint8_t> &CommonBsa()
{
    static const std::vector<uint8_t> bsa = [] {
        std::vector<uint8_t> file;
        ReadGameFile(kCommonBsaPath, file);
        return file;
    }();
    return bsa;
}

// The slot of the entry's copy naming this owner: the free ones from the top down (mods add low).
int CommonBsaCopySlot(int entry, int owner)
{
    for (const CommonBsaCopy &copy : g_commonBsaCopies)
        if (copy.entry == entry && copy.owner == owner) return copy.slot;
    int slot = g_commonBsaCopies.empty() ? static_cast<int>(BsaSlotCount(CommonBsa())) : g_commonBsaCopies.back().slot;
    while (--slot >= 0 && BsaEntryOffset(CommonBsa(), slot)) {}
    if (slot >= 0) g_commonBsaCopies.push_back({entry, owner, slot});
    return slot;
}

// Adds the copies. An entry's offsets are all from itself, so its bytes move as they are.
int __cdecl AddCommonBsaCopies(void *, const char *, const void *, size_t, const void *current, size_t currentSize,
                               XV2FileOutput *output)
{
    const auto *bytes = static_cast<const uint8_t *>(current);
    const std::vector<uint8_t> original(bytes, bytes + currentSize);
    std::vector<uint8_t> bsa = original;
    const size_t table = BsaSlotCount(original) ? Rd32(original, 20) : 0;
    int copied = 0;
    for (const CommonBsaCopy &copy : g_commonBsaCopies) {
        const size_t at = BsaEntryOffset(original, copy.entry);
        if (!at || BsaEntryOffset(original, copy.slot)) {
            Log("common BSA: entry %d not copied to %d (no entry, or that slot is taken now)", copy.entry, copy.slot);
            continue;
        }
        size_t end = original.size();
        for (size_t slot = 0, count = BsaSlotCount(original); slot < count; slot++) {
            const size_t other = BsaEntryOffset(original, slot);
            if (other > at && other < end) end = other;
        }
        bsa.resize((bsa.size() + 15) & ~static_cast<size_t>(15));
        const uint32_t moved = static_cast<uint32_t>(bsa.size());
        bsa.insert(bsa.end(), original.begin() + at, original.begin() + end);
        const uint16_t owner = static_cast<uint16_t>(copy.owner);
        ForEachBsaSpawnerEffect(bsa, moved, [&](size_t record) { memcpy(bsa.data() + record + kBsaEffectOwner, &owner, sizeof(owner)); });
        memcpy(bsa.data() + table + 4 * static_cast<size_t>(copy.slot), &moved, sizeof(moved));
        copied++;
    }
    if (!copied) return 0;
    sprintf_s(output->note, "%d projectile entries copied", copied);
    output->Set(output, bsa.data(), bsa.size());
    return 1;
}

// The BAC of a ported blast: carried animations from the skill EAN, character effects
// from the owner's EEPK (the id the record passes makes the game skip asking the
// spawning character for its own, 0xf9820; vanilla records carry -1).
int __cdecl PortBlastBac(void *user, const char *, const void *, size_t, const void *current, size_t currentSize,
                         XV2FileOutput *output)
{
    const auto &port = *static_cast<const PortedBlast *>(user);
    const auto *bytes = static_cast<const uint8_t *>(current);
    std::vector<uint8_t> bac(bytes, bytes + currentSize);
    int animations = 0, effects = 0, projectiles = 0;
    ForEachBacRecord(bac, [&](int type, size_t record) {
        if (type == kBacTypeAnimation && Rd16(bac, record + kBacRecordEan) == kBacEanCharacter) {
            auto slot = port.animationSlots.find(static_cast<int>(Rd16(bac, record + kBacRecordAnimation)));
            if (slot == port.animationSlots.end()) return;
            const uint16_t ean = static_cast<uint16_t>(kBacEanSkill), id = static_cast<uint16_t>(slot->second);
            memcpy(bac.data() + record + kBacRecordEan, &ean, sizeof(ean));
            memcpy(bac.data() + record + kBacRecordAnimation, &id, sizeof(id));
            animations++;
        } else if (type == kBacTypeEffect && port.effectOwner >= 0 && Rd16(bac, record + kBacRecordEepk) == kBacEepkCharacter) {
            memcpy(bac.data() + record + kBacRecordEffectOwner, &port.effectOwner, sizeof(port.effectOwner));
            effects++;
        } else if (type == kBacTypeProjectile && bac[record + kBacRecordBsaType] == kBacBsaTypeCommon && Rd16(bac, record + kBacRecordBsaSkill) == kBacBsaAnySkill) {
            auto copy = port.commonBsaSlots.find(static_cast<int>(Rd16(bac, record + kBacRecordBsaEntry)));
            if (copy == port.commonBsaSlots.end()) return;
            const uint16_t slot = static_cast<uint16_t>(copy->second);
            memcpy(bac.data() + record + kBacRecordBsaEntry, &slot, sizeof(slot));
            projectiles++;
        }
    });
    if (!animations && !effects && !projectiles) return 0;
    sprintf_s(output->note, "%d animation, %d effect and %d projectile records carried", animations, effects, projectiles);
    output->Set(output, bac.data(), bac.size());
    return 1;
}

// An animation's name in an EAN (ean_fit.cpp ParseEan), false for an empty slot.
bool EanAnimationName(const std::vector<uint8_t> &ean, int id, std::string &name)
{
    if (ean.size() < 32 || id < 0 || id >= static_cast<int>(Rd16(ean, 18))) return false;
    if (!Rd32(ean, Rd32(ean, 24) + 4 * static_cast<size_t>(id))) return false;
    const uint32_t at = Rd32(ean, Rd32(ean, 28) + 4 * static_cast<size_t>(id));
    name.clear();
    if (at) for (size_t i = at; i < ean.size() && ean[i]; i++) name += static_cast<char>(ean[i]);
    return true;
}

// A copy of an EAN with only these animations: offset 0 in the table is an empty slot.
std::vector<uint8_t> KeepEanAnimations(const std::vector<uint8_t> &ean, const std::set<int> &ids)
{
    std::vector<uint8_t> kept = ean;
    const size_t table = Rd32(ean, 24);
    const int count = static_cast<int>(Rd16(ean, 18));
    const uint32_t empty = 0;
    for (int id = 0; id < count && table + 4 * (static_cast<size_t>(id) + 1) <= kept.size(); id++)
        if (!ids.count(id)) memcpy(kept.data() + table + 4 * static_cast<size_t>(id), &empty, sizeof(empty));
    return kept;
}

// The created characters. skeleton: whose EAN they animate on (Saiyans are built on the
// humans); lookup: the skill EAN variant the game gives them without a file of their code (0x260ba0).
struct CacRace { const char *code, *skeleton, *lookup; };
constexpr CacRace kCacRaces[] = {{"HUM", "HUM", ""}, {"HUF", "HUF", "_F"}, {"SYM", "HUM", ""},    {"SYF", "HUF", "_F"},
                                 {"NMC", "NMC", ""}, {"FRI", "FRI", ""},   {"MAM", "MAM", "_MAM"}, {"MAF", "MAF", "_F"}};

bool IsCacRaceCode(const std::string &code)
{
    for (const CacRace &race : kCacRaces)
        if (code == race.code) return true;
    return false;
}

// The character a blast's carried animations come from, of those having them all: the
// folder's owner, else its users, created characters last (whose ids may be another
// form: a Majin's 96 is its transformation, Kid Buu's its blast), and the one with the
// most named as a blast. Empty when nobody has them.
std::string ChooseAnimationSource(const std::vector<std::string> &candidates, const std::set<int> &ids,
                                  std::map<std::string, std::vector<uint8_t>> &charaEans)
{
    std::string best;
    int bestScore = -1;
    for (int createdRound = 0; createdRound < 2 && best.empty(); createdRound++) {
        for (const std::string &code : candidates) {
            if (IsCacRaceCode(code) != (createdRound == 1)) continue;
            std::vector<uint8_t> &ean = charaEans[code];
            if (ean.empty()) ReadGameFile(("chara/" + code + "/" + code + ".ean").c_str(), ean);
            int score = 0;
            bool all = true;
            for (int id : ids) {
                std::string name;
                all = all && EanAnimationName(ean, id, name);
                score += name.find("_BLT_") != std::string::npos ? 1 : 0;
            }
            if (all && score > bestScore) { best = code; bestScore = score; }
        }
    }
    return best;
}

// Makes <folder>_<target>.ean (no target: the plain <folder>.ean): the skill's animations for
// that character, then the carried ones after them. A file already there (a mod's, or a
// cpk's) is left alone on disk and served changed; a new one is written for good. One of ours
// is kept unless expectedAnimations (0: any) says its slots are from another build.
bool WritePortedEan(const std::string &folder, const std::string &target, const std::string &skeleton, const std::string &source,
                    const std::vector<std::string> &skillFiles, const std::set<int> &ids, int base, uint32_t expectedAnimations,
                    std::map<std::string, std::vector<uint8_t>> &charaEans)
{
    const std::string path = "skill/BLT/" + folder + "/" + folder + (target.empty() ? "" : "_" + target) + ".ean";
    const bool ours = IsOurFile(path); // an earlier run wrote it
    if (ours) {
        if (!expectedAnimations) return true;
        std::vector<uint8_t> head(32);
        if (Files().ReadGameFilePart && Files().ReadGameFilePart(path.c_str(), 0, head.data(), head.size()) >= 32 && Rd16(head, 18) == expectedAnimations)
            return true;
        Log("port %s: %s is from another build, rewriting", folder.c_str(), path.c_str());
    }
    auto charaEan = [&](const std::string &code) -> std::vector<uint8_t> & {
        std::vector<uint8_t> &ean = charaEans[code];
        if (ean.empty()) ReadGameFile(("chara/" + code + "/" + code + ".ean").c_str(), ean);
        return ean;
    };
    const std::vector<uint8_t> &onto = charaEan(skeleton);
    if (onto.empty()) { Log("port %s: no EAN for %s", folder.c_str(), skeleton.c_str()); return false; }
    const std::vector<uint8_t> kept = KeepEanAnimations(charaEan(source), ids);
    std::vector<uint8_t> carried, skillFitted, composed;
    EanFit fit;
    std::string note;
    if (!FitEanToRace(kept.data(), kept.size(), onto.data(), onto.size(), source != skeleton, 0, carried, fit, note, {}, true)) {
        Log("port %s: fitting %s to %s: %s", folder.c_str(), source.c_str(), skeleton.c_str(), note.c_str());
        return false;
    }
    Log("port %s: %s -> %s: %s", folder.c_str(), source.c_str(), target.c_str(), note.c_str());
    for (const std::string &file : skillFiles) { // the first this character would load
        std::vector<uint8_t> skill;
        if (!ReadGameFile(file.c_str(), skill)) continue;
        // no body fit: these already are this character's moves, only the bone numbers differ
        if (!FitEanToRace(skill.data(), skill.size(), onto.data(), onto.size(), false, 0, skillFitted, fit, note, {}, true)) {
            Log("port %s: skill EAN %s: %s", folder.c_str(), file.c_str(), note.c_str());
            return false;
        }
        break;
    }
    const std::vector<int> ordered(ids.begin(), ids.end());
    if (!ComposeEan(skillFitted.data(), skillFitted.size(), carried.data(), carried.size(), ordered, base, composed, note)) {
        Log("port %s: %s: %s", folder.c_str(), target.c_str(), note.c_str());
        return false;
    }
    if (!ours && GameFileExists(path)) {
        char reason[128] = {};
        if (!Files().ReplaceFile || !Files().ReplaceFile("KiBlastOverride", path.c_str(), composed.data(), composed.size(), reason, sizeof(reason))) {
            Log("port %s: cannot serve %s: %s", folder.c_str(), path.c_str(), reason);
            return false;
        }
        return true;
    }
    std::string written = path;
    std::replace(written.begin(), written.end(), '/', '\\');
    if (!WriteConvertedFile(written, composed)) { Log("port %s: cannot write %s", folder.c_str(), written.c_str()); return false; }
    return true;
}

// What a blast takes from the character using it.
struct BlastNeeds {
    std::set<int> animations;       // carried character animations
    std::set<int> commonBsaEntries; // projectiles showing the spawner's own effects
    bool bacEffects = false;        // effect records of the spawner's own EEPK
    bool Any() const { return !animations.empty() || !commonBsaEntries.empty() || bacEffects; }
};

BlastNeeds ReadBlastNeeds(const std::vector<uint8_t> &bac)
{
    BlastNeeds needs;
    ForEachBacRecord(bac, [&](int type, size_t record) {
        if (type == kBacTypeAnimation && Rd16(bac, record + kBacRecordEan) == kBacEanCharacter) {
            const int id = static_cast<int>(Rd16(bac, record + kBacRecordAnimation));
            if (id >= kFirstCarriedAnimation) needs.animations.insert(id);
        } else if (type == kBacTypeEffect && Rd16(bac, record + kBacRecordEepk) == kBacEepkCharacter) {
            needs.bacEffects = true;
        } else if (type == kBacTypeProjectile && bac[record + kBacRecordBsaType] == kBacBsaTypeCommon && Rd16(bac, record + kBacRecordBsaSkill) == kBacBsaAnySkill) {
            const int entry = static_cast<int>(Rd16(bac, record + kBacRecordBsaEntry));
            ForEachBsaSpawnerEffect(CommonBsa(), BsaEntryOffset(CommonBsa(), entry), [&](size_t) { needs.commonBsaEntries.insert(entry); });
        }
    });
    return needs;
}

// Gives a CaC what a blast takes from its character, as loading a Battle Style does.
// Nothing happens for a blast using only base animations and its own effects.
void PortBlast(int id1, int id2, const std::string &folder, const std::vector<uint8_t> &cus, size_t entry,
               const std::map<int, std::string> &codes, const std::set<int> &users,
               std::map<std::string, std::vector<uint8_t>> &charaEans)
{
    const std::string base = "skill/BLT/" + folder + "/" + folder;
    std::vector<uint8_t> bac;
    if (folder.empty() || !ReadGameFile((base + ".bac").c_str(), bac)) return;
    const BlastNeeds needs = ReadBlastNeeds(bac);
    if (!needs.Any()) return;
    const std::set<int> &ids = needs.animations;
    const bool effects = needs.bacEffects || !needs.commonBsaEntries.empty();

    std::vector<std::string> candidates;
    auto owner = IsCommonBlast(id1) ? codes.end() : codes.find(id2 / 10);
    if (owner != codes.end() && !owner->second.empty()) candidates.push_back(owner->second);
    for (int user : users) {
        auto code = codes.find(user);
        if (code != codes.end() && !code->second.empty() && std::find(candidates.begin(), candidates.end(), code->second) == candidates.end())
            candidates.push_back(code->second);
    }
    std::string source;
    if (!ids.empty()) {
        source = ChooseAnimationSource(candidates, ids, charaEans);
        if (source.empty()) { Log("port %s: no character has all its animations - left as it is", folder.c_str()); return; }
    }
    // Effects: the same character; a blast that only needs effects, its first non-created candidate.
    std::string effectCode = source;
    for (const std::string &code : candidates)
        if (effectCode.empty() && !IsCacRaceCode(code)) effectCode = code;
    if (effectCode.empty() && !candidates.empty()) effectCode = candidates[0];
    int effectOwner = -1;
    if (effects)
        for (const auto &code : codes)
            if (code.second == effectCode) effectOwner = code.first;
    if (effects && effectOwner < 0) Log("port %s: no owner for its character effects", folder.c_str());

    PortedBlast &port = g_portedBlasts[folder];
    port = {};
    port.effectOwner = effectOwner;
    if (effectOwner >= 0) {
        for (int bsaEntry : needs.commonBsaEntries) {
            const int slot = CommonBsaCopySlot(bsaEntry, effectOwner);
            if (slot >= 0) port.commonBsaSlots[bsaEntry] = slot;
            else Log("port %s: no free common BSA slot for entry %d", folder.c_str(), bsaEntry);
        }
    }
    if (!ids.empty()) {
        // Who gets a file: every created race, and each other user with these animations of
        // its own (one without them plays nothing today, and still does).
        std::vector<std::string> otherUsers;
        for (const std::string &code : candidates) {
            if (IsCacRaceCode(code)) continue;
            std::string skip;
            std::vector<uint8_t> &ean = charaEans[code];
            if (ean.empty()) ReadGameFile(("chara/" + code + "/" + code + ".ean").c_str(), ean);
            bool all = !ean.empty();
            for (int id : ids) all = all && EanAnimationName(ean, id, skip);
            if (all) otherUsers.push_back(code);
        }
        // The carried ones go after the most animations any skill EAN of the blast has.
        // Our own per-character files don't count: the slots must not change between runs
        // (_F, _MAM of a flagged blast are ConvertBlastForCac's, so those are skill animations).
        int slotBase = 0;
        const uint8_t flags = entry + kCusEntryFlags < cus.size() ? cus[entry + kCusEntryFlags] : 0;
        // Without the skill EAN flag vanilla never loaded a plain file, so one of ours is this
        // port's own fallback, not skill animations (ConvertBlastForCac's are, on flagged blasts).
        const std::string plain = base + ".ean";
        const bool skillEanFlag = (flags & kCusFlagSkillEan) != 0;
        const bool plainIsOurFallback = !skillEanFlag && IsOurFile(plain);
        auto countAnimations = [&](const std::string &path) {
            std::vector<uint8_t> head(32);
            if (Files().ReadGameFilePart && Files().ReadGameFilePart(path.c_str(), 0, head.data(), head.size()) >= 32)
                slotBase = (std::max)(slotBase, static_cast<int>(Rd16(head, 18)));
        };
        if (!plainIsOurFallback) countAnimations(plain);
        for (const char *suffix : {"_F", "_MAM"}) {
            const std::string path = base + suffix + ".ean";
            if (skillEanFlag || !IsOurFile(path)) countAnimations(path);
        }
        for (const CacRace &race : kCacRaces) {
            const std::string path = base + "_" + race.code + ".ean";
            if (!IsOurFile(path)) countAnimations(path);
        }
        for (const std::string &code : otherUsers) {
            const std::string path = base + "_" + code + ".ean";
            if (!IsOurFile(path)) countAnimations(path);
        }
        // A file of ours on an unflagged blast is only this port's: wrong slots (an older build's) get rewritten.
        const uint32_t expected = skillEanFlag ? 0 : static_cast<uint32_t>(slotBase) + static_cast<uint32_t>(ids.size());
        // Each character's skill animations: its own file, else what the game looks up (0x260ba0).
        const uint32_t files = Rd16(cus, entry + kCusEntryFiles);
        bool ok = true;
        for (const CacRace &race : kCacRaces) {
            std::vector<std::string> skillFiles;
            const std::string own = base + "_" + race.code + ".ean";
            if (!IsOurFile(own)) skillFiles.push_back(own);
            const std::string lookup = race.lookup;
            const std::string lookupFile = base + lookup + ".ean";
            if (((lookup == "_F" && (flags & kCusFlagFemaleEan)) || (lookup == "_MAM" && (files & kCusFilesMajinEan))) && (skillEanFlag || !IsOurFile(lookupFile)))
                skillFiles.push_back(lookupFile);
            if (!plainIsOurFallback) skillFiles.push_back(plain);
            ok = WritePortedEan(folder, race.code, race.skeleton, source, skillFiles, ids, slotBase, expected, charaEans) && ok;
        }
        for (const std::string &code : otherUsers) {
            std::vector<std::string> skillFiles;
            const std::string own = base + "_" + code + ".ean";
            if (!IsOurFile(own)) skillFiles.push_back(own);
            if (!plainIsOurFallback) skillFiles.push_back(plain);
            ok = WritePortedEan(folder, code, code, code, skillFiles, ids, slotBase, expected, charaEans) && ok;
        }
        // The flag turning the EAN on is per blast, so a character with no file above (a modded
        // user lacking these animations) now asks for the plain one: never let that be missing.
        if (!(flags & kCusFlagSkillEan) && (!GameFileExists(plain) || IsOurFile(plain)))
            ok = WritePortedEan(folder, "", source, source, {}, ids, slotBase, expected, charaEans) && ok;
        if (!ok) { // the BAC stays as it was: asking for animations that aren't there hangs the character
            Log("port %s: animations NOT carried", folder.c_str());
            if (effectOwner < 0) { g_portedBlasts.erase(folder); return; }
        } else {
            int slot = slotBase;
            for (int id : ids) port.animationSlots[id] = slot++;
            g_blastsCarryingAnimations.insert(id1);
            Log("port %s: %d animations from %s at skill %d..%d", folder.c_str(), static_cast<int>(ids.size()), source.c_str(), slotBase, slot - 1);
        }
    }
    if (port.animationSlots.empty() && effectOwner < 0) { g_portedBlasts.erase(folder); return; }
    if (effectOwner >= 0) {
        g_effectOwners.insert(effectOwner);
        Log("port %s: character effects from %s (%d)", folder.c_str(), effectCode.c_str(), effectOwner);
    }
    char reason[128] = {};
    if (!Files().AddStage || !Files().AddStage("KiBlastOverride", (base + ".bac").c_str(), 0, &PortBlastBac, &port, reason, sizeof(reason)))
        Log("port %s: BAC NOT patched: %s", folder.c_str(), reason);
    // one stage for all ports: registering again replaces it
    if (!port.commonBsaSlots.empty() && (!Files().AddStage || !Files().AddStage("KiBlastOverride", kCommonBsaPath, 0, &AddCommonBsaCopies, nullptr, reason, sizeof(reason))))
        Log("port %s: common BSA NOT patched: %s", folder.c_str(), reason);
}

// A blast of a standard type taking effects or animations from its character is its own
// blast, not a copy of that type (21110 1110_TE1_RSH, Android 21's).
bool UsesItsCharacter(const std::string &folder)
{
    std::vector<uint8_t> bac;
    return !folder.empty() && ReadGameFile(("skill/BLT/" + folder + "/" + folder + ".bac").c_str(), bac) && ReadBlastNeeds(bac).Any();
}

bool BuildBlastList()
{
    std::vector<uint8_t> cus, cms, msg, charaMsg;
    if (!ReadGameFile(kCusPath, cus)) { Log("blast list: cannot read %s", kCusPath); return false; }
    CusBlastTable table{};
    const char *layoutProblem = "";
    if (!FindCusBlasts(cus, table, layoutProblem)) { Log("blast list: %s", layoutProblem); return false; }

    std::map<int, std::string> ownerCodes; // CMS id -> code
    if (ReadGameFile(kCmsPath, cms)) {
        uint32_t count = Rd32(cms, 8), cmsEntries = Rd32(cms, 0xc);
        if (cmsEntries >= 0x10)
            for (uint32_t i = 0; i < count && cmsEntries + (i + 1) * kCmsEntrySize <= cms.size(); i++)
                ownerCodes[static_cast<int>(Rd32(cms, cmsEntries + i * kCmsEntrySize))] = Code4(cms, cmsEntries + i * kCmsEntrySize + 4);
    } else {
        Log("blast list: cannot read %s - blasts of other characters show no owner", kCmsPath);
    }
    std::map<std::string, std::wstring> messages;
    if (ReadGameFile(kMsgPath, msg)) messages = ReadMsgNames(msg);
    else Log("blast list: cannot read %s - blasts are labelled by their code", kMsgPath);
    std::map<std::string, std::wstring> charaNames;
    if (ReadGameFile(kCharaNamesPath, charaMsg)) charaNames = ReadMsgNames(charaMsg);
    else Log("blast list: cannot read %s - owners show as their code", kCharaNamesPath);
    std::vector<uint8_t> awokenMsg;
    std::map<std::string, std::wstring> awokenNames;
    if (ReadGameFile(kAwokenNamesPath, awokenMsg)) awokenNames = ReadMsgNames(awokenMsg);
    else Log("blast list: cannot read %s - a transformation's blast isn't named after it", kAwokenNamesPath);
    const std::map<int, std::wstring> awokenBlasts = BlastsOfAwokenSkills(cus, table.stride, awokenNames);

    struct Raw { int id1, id2; std::string code, folder; };
    std::vector<Raw> raw;
    auto isCac = [](int id1) { return id1 >= kCacBlastIdMin && id1 <= kCacBlastIdMax; };
    const std::map<int, int> firstUsers = FirstBlastUsers(cus);
    const std::map<int, std::set<int>> allUsers = AllBlastUsers(cus);
    std::map<std::string, std::vector<uint8_t>> raceEans;
    int hidden = 0;
    // The game's own CaC ports: a CaC blast taking its effects from another blast's folder
    // (21089 GOA from 2060_FIO_GLO, 21090 GTA from 2070_FIT_GLT). The port has the same
    // projectiles plus a spawn offset for a CaC's hand and its own animations; the original
    // shows a CaC's plain shot poses with its gun misplaced, which fitting an EAN can't fix.
    // Details: ki-blast-type.md, "CaC animations".
    std::map<std::string, int> cacPortOfFolder;
    for (uint32_t i = 0; i < table.count; i++) {
        size_t entry = table.offset + static_cast<size_t>(i) * table.stride;
        if (entry + table.stride > cus.size()) break;
        int id1 = static_cast<int>(Rd16(cus, entry + kCusEntryId1));
        const std::string source = isCac(id1) ? EepkPathFolder(cus, entry) : "";
        if (!source.empty()) cacPortOfFolder.emplace(source, id1);
    }
    for (uint32_t i = 0; i < table.count; i++) {
        size_t entry = table.offset + static_cast<size_t>(i) * table.stride;
        if (entry + table.stride > cus.size()) break;
        int id1 = static_cast<int>(Rd16(cus, entry + kCusEntryId1));
        if (id1 < kBlastIdMin || id1 > kBlastIdMax) continue;
        std::string code = Code4(cus, entry);
        int id2 = static_cast<int>(Rd16(cus, entry + kCusEntryId2));
        const std::string folder = BlastFolder(id1, id2, code, ownerCodes);
        // a mod's blast always shows, even one that copies a standard type (the installer leaves this ini)
        const bool fromMod = !folder.empty() && GameFileExists("skill/BLT/" + folder + "/X2M_SKILL.ini");
        if (!isCac(id1) && !fromMod && IsStandardTypeCopy(cus, entry, code) && !UsesItsCharacter(folder)) { hidden++; continue; }
        const auto port = cacPortOfFolder.find(folder);
        if (!isCac(id1) && !fromMod && port != cacPortOfFolder.end()) {
            Log("blast list: %d %s hidden, the game's CaC port of it is %d", id1, folder.c_str(), port->second);
            g_cacPorts[id1] = port->second;
            continue;
        }
        bool duplicate = false;
        for (const Raw &earlier : raw) duplicate |= earlier.id1 == id1;
        if (duplicate) continue;
        if (LacksCacEan(cus, entry, folder)) ConvertBlastForCac(folder, id2, ownerCodes, raceEans);
        const auto users = allUsers.find(id1);
        PortBlast(id1, id2, folder, cus, entry, ownerCodes, users != allUsers.end() ? users->second : std::set<int>{}, raceEans);
        raw.push_back({id1, id2, code, folder});
    }
    std::sort(raw.begin(), raw.end(), [&](const Raw &a, const Raw &b) {
        if (isCac(a.id1) != isCac(b.id1)) return isCac(a.id1);
        return a.id1 < b.id1;
    });

    std::vector<Blast> blasts;
    blasts.push_back({kPickDefault, Widen(kDefaultLabel)});
    blasts.push_back({kPickRaceDefault, Widen(kRaceDefaultLabel)});
    for (const Raw &blast : raw) {
        std::wstring label = TypeLabel(blast.code, messages);
        const wchar_t *special = isCac(blast.id1) ? CacSpecialLabel(blast.code) : nullptr;
        if (special) {
            label = special;
        } else if (!isCac(blast.id1) || blast.id1 > kCacStandardIdMax) { // roster blasts have lower ids too (20340)
            std::wstring owner = BlastOwner(blast.id1, blast.id2, blast.folder, ownerCodes, firstUsers, awokenBlasts, charaNames);
            if (!owner.empty()) label = owner + L" - " + ShortTypeLabel(label);
        }
        blasts.push_back({blast.id1, label});
    }
    // identical labels (the CaC blasts of each race) would be indistinguishable: add the ID1
    for (Blast &blast : blasts) {
        if (blast.id1 < 0) continue;
        int same = 0;
        for (const Blast &other : blasts) same += other.label == blast.label;
        if (same > 1) blast.label += L" #" + std::to_wstring(blast.id1);
    }
    if (blasts.size() <= kFixedEntries) { Log("blast list: the CUS has no blast skills"); return false; }

    std::vector<ListEntry> entries;
    entries.reserve(blasts.size());
    g_blasts = std::move(blasts); // labels settle here: entries point into them
    for (const Blast &blast : g_blasts) entries.push_back(ListEntryFor(blast.label));
    g_entries = std::move(entries);
    Log("blast list: %d blast skills (%d entries incl. Default and Race Default), %d copies of the standard types hidden",
        static_cast<int>(g_blasts.size()) - kFixedEntries, static_cast<int>(g_blasts.size()), hidden);
    char cusStatus[256] = {};
    if (Files().Status) Files().Status(kCusPath, cusStatus, sizeof(cusStatus));
    Log("custom_skill.cus: %s", cusStatus);
    for (const Blast &blast : g_blasts) Log("  %d  %ls", blast.id1, blast.label.c_str());
    return true;
}

// True once the list is built; tries (rarely, never per frame) while it is not.
bool EnsureBlastList()
{
    int state = g_listState.load();
    if (state != 0) return state == 1;
    std::lock_guard<std::mutex> lock(g_listLock);
    if (g_listState.load() != 0) return g_listState.load() == 1;
    ULONGLONG now = GetTickCount64();
    if (g_listAttempts > 0 && now - g_listLastAttempt < kListRetryMs) return false;
    g_listLastAttempt = now;
    if (BuildBlastList()) { g_listState = 1; return true; }
    if (++g_listAttempts >= kMaxListAttempts) {
        Log("blast list: giving up after %d attempts - the row stays inactive", g_listAttempts);
        g_listState = 2;
    }
    return false;
}

// Index in g_blasts of a pick (blast ID1, kPickDefault or kPickRaceDefault); -1 when unknown.
int IndexOfBlast(int id1)
{
    if (!EnsureBlastList()) return -1;
    for (size_t i = 0; i < g_blasts.size(); i++)
        if (g_blasts[i].id1 == id1) return static_cast<int>(i);
    return -1;
}

// --- Which CaC ---------------------------------------------------------------------

char *SaveManager() { return *reinterpret_cast<char **>(g_exe + kSaveManagerRva); }

int CurrentSlot()
{
    char *manager = SaveManager();
    return manager ? Game<CurrentSlotFn>(kCurrentSlotRva)(manager + 8) : -1;
}

// --- Saved picks ---------------------------------------------------------------
// equipped_blasts.sav (JSON text; .sav so tools sharing the mod's .json
// configs leave each player's picks alone, as Multi Accessory's file): Steam
// account -> save slot (0..7) -> preset -> blast ID1, or -2 = Race Default.
// No entry = Default.
//   {"76561198000000000": {"3": {"0": 21082, "2": -2}}}

using PresetPicks = std::map<int, int>;       // preset -> blast ID1
using SlotPicks = std::map<int, PresetPicks>; // save slot -> presets
std::mutex g_savedLock;
std::map<std::string, SlotPicks> g_saved; // every account in the file
std::string g_account;                    // the playing account, once a save is loaded
bool g_savedLoaded = false;

std::wstring SavedPath() { return g_modDir + L"equipped_blasts.sav"; }

// The loaded save's Steam account (64-bit id as text), empty before one is
// loaded. From the save, not Steam: saves are per account.
std::string SteamAccount()
{
    char *saveSystem = *reinterpret_cast<char **>(g_exe + kSaveSystemRva);
    if (!saveSystem || memcmp(saveSystem + kSaveImage, "#SAV", 4) != 0) return {};
    uint64_t id = Field<uint64_t>(saveSystem, kSaveImage + kSaveImageSteamId) ^ kSteamIdXor;
    return id ? std::to_string(id) : std::string();
}

// Just enough JSON for the file's one shape.
struct JsonReader {
    const char *at;
    bool Eat(char c)
    {
        at += strspn(at, " \t\r\n");
        if (*at != c) return false;
        at++;
        return true;
    }
    bool Key(std::string &out) // "text" followed by ':'
    {
        if (!Eat('"')) return false;
        const char *end = strchr(at, '"');
        if (!end) return false;
        out.assign(at, end);
        at = end + 1;
        return Eat(':');
    }
    bool Int(int &out)
    {
        at += strspn(at, " \t\r\n");
        char *end = nullptr;
        long value = strtol(at, &end, 10);
        if (end == at) return false;
        out = static_cast<int>(value);
        at = end;
        return true;
    }
    // {"key": value, ...}, calling item(key) to read each value.
    template <typename Item> bool Object(Item item)
    {
        if (!Eat('{')) return false;
        if (Eat('}')) return true;
        do {
            std::string key;
            if (!Key(key) || !item(key)) return false;
        } while (Eat(','));
        return Eat('}');
    }
};

// Caller holds g_savedLock.
void LoadSaved()
{
    g_savedLoaded = true;
    FILE *file = nullptr;
    if (_wfopen_s(&file, SavedPath().c_str(), L"rb") != 0 || !file) return; // nothing equipped yet
    std::string text;
    char chunk[4096];
    for (size_t read; (read = fread(chunk, 1, sizeof chunk, file)) > 0;) text.append(chunk, read);
    fclose(file);
    std::map<std::string, SlotPicks> saved;
    JsonReader reader{text.c_str()};
    bool ok = reader.Object([&](const std::string &account) {
        return reader.Object([&](const std::string &slotKey) {
            int slot = atoi(slotKey.c_str());
            return reader.Object([&](const std::string &presetKey) {
                int preset = atoi(presetKey.c_str()), id1 = -1;
                if (!reader.Int(id1)) return false;
                if (slot >= 0 && slot < kSlotCount && preset >= 0 && preset < kPresetCount && (id1 >= 0 || id1 == kPickRaceDefault))
                    saved[account][slot][preset] = id1;
                return true;
            });
        });
    });
    if (ok) g_saved = saved;
    else Log("equipped_blasts.sav unreadable - ignored until a pick changes");
}

// Caller holds g_savedLock. Written to a temporary file first so a crash
// never leaves half a file.
void WriteSaved()
{
    std::string text = "{";
    const char *accountComma = "";
    for (const auto &[account, slots] : g_saved) {
        bool any = false;
        for (const auto &slot : slots) any |= !slot.second.empty();
        if (!any) continue;
        text += std::string(accountComma) + "\n    \"" + account + "\": {";
        accountComma = ",";
        const char *slotComma = "";
        for (const auto &[slot, presets] : slots) {
            if (presets.empty()) continue;
            text += std::string(slotComma) + "\n        \"" + std::to_string(slot) + "\": {";
            slotComma = ",";
            const char *presetComma = "";
            for (const auto &[preset, id1] : presets) {
                text += std::string(presetComma) + "\"" + std::to_string(preset) + "\": " + std::to_string(id1);
                presetComma = ", ";
            }
            text += "}";
        }
        text += "\n    }";
    }
    text += "\n}\n";
    std::wstring path = SavedPath(), temporary = path + L".tmp";
    FILE *file = nullptr;
    bool ok = _wfopen_s(&file, temporary.c_str(), L"wb") == 0 && file;
    if (ok) {
        ok = fwrite(text.data(), 1, text.size(), file) == text.size();
        ok = fclose(file) == 0 && ok;
    }
    if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        Log("equipped_blasts.sav: cannot write");
}

// The playing account's picks, loading the file on first use; null while no
// save is loaded (nothing is read or written then, so no pick lands on the
// wrong account). Caller holds g_savedLock.
SlotPicks *AccountPicks()
{
    if (g_account.empty()) {
        g_account = SteamAccount();
        if (g_account.empty()) return nullptr;
        Log("Steam account %s (from the save)", g_account.c_str());
    }
    if (!g_savedLoaded) LoadSaved();
    return &g_saved[g_account];
}

// The saved pick of a slot's preset, kPickDefault = none (caller holds g_savedLock).
int SavedPick(int slot, int preset)
{
    SlotPicks *picks = AccountPicks();
    if (!picks) return kPickDefault;
    auto presets = picks->find(slot);
    if (presets == picks->end()) return kPickDefault;
    auto pick = presets->second.find(preset);
    if (pick == presets->second.end()) return kPickDefault;
    // a pick saved before its blast was hidden in favour of the game's CaC port
    if (g_listState.load() == 1) {
        auto port = g_cacPorts.find(pick->second);
        if (port != g_cacPorts.end()) return port->second;
    }
    return pick->second;
}

// Sets a pick (kPickDefault clears it) and writes the file when it changed.
void SetPick(int slot, int preset, int id1)
{
    if (slot < 0 || slot >= kSlotCount || preset < 0 || preset >= kPresetCount) return;
    std::lock_guard<std::mutex> lock(g_savedLock);
    SlotPicks *picks = AccountPicks();
    if (!picks) return;
    if (SavedPick(slot, preset) == id1) return;
    if (id1 != kPickDefault) (*picks)[slot][preset] = id1;
    else (*picks)[slot].erase(preset);
    Log("slot %d preset %d: blast %d", slot + 1, preset, id1);
    WriteSaved();
}

// Presets panel: after the game loads or saves a preset record, copy the pick.
void CopyPreset(int from, int to)
{
    if (from < 0 || from >= kPresetCount || to < 0 || to >= kPresetCount || from == to) return;
    int slot = CurrentSlot();
    if (slot < 0 || slot >= kSlotCount) return;
    int id1;
    {
        std::lock_guard<std::mutex> lock(g_savedLock);
        id1 = SavedPick(slot, from);
    }
    SetPick(slot, to, id1);
}

// --- Menu row --------------------------------------------------------------------

// Strings handed to the menu. The game keeps the pointer, so they are never
// freed; each distinct text is kept once.
SRWLOCK g_menuTextLock = SRWLOCK_INIT;
std::set<std::wstring> g_menuTexts;

// Shows the equipped blast on the row (the AS waits for slot 102 to be sent
// again before it leaves a list).
void ShowEquipped(void *menu)
{
    std::wstring label = Widen(kDefaultLabel);
    int active = 0;
    if (EnsureBlastList()) {
        active = 1;
        int index = IndexOfBlast(KiBlast_Equipped(CurrentSlot(), kMainPreset));
        label = g_blasts[index < 0 ? 0 : index].label;
    }
    AcquireSRWLockExclusive(&g_menuTextLock);
    const wchar_t *kept = g_menuTexts.insert(label).first->c_str();
    ReleaseSRWLockExclusive(&g_menuTextLock);
    Game<SetUserDataStrFn>(kSetUserDataStrRva)(menu, kUserDataLabel, kept);
    Game<SetUserDataIntFn>(kSetUserDataIntRva)(menu, kUserDataActive, active);
    // The skill push repeats this a lot: only changes reach the log.
    static const wchar_t *lastLogged = nullptr;
    if (kept != lastLogged) {
        lastLogged = kept;
        Log("row: sent '%ls' active %d (user data slots %d)", kept, active,
            2 + *reinterpret_cast<const uint8_t *>(g_exe + kSlotCountByteRva));
    }
}

// The menu state to show an open list in, by menu mode - copied from the
// opener template 0x4fd5a0 (0x4fd895..0x4fd8e4) via Battle Styles ListState.
int ListState(int mode, int listType)
{
    switch (mode) {
    case 3: return 0x27;
    case 1: case 5: return 0x21;
    case 2: case 6: return 0x28;
    case 8: return 0x35 + (listType != kNoListOpen);
    case 4: return 0x34;
    default: return 5; // 7
    }
}

int g_cursor = -1; // the list index under the cursor (game thread): the result is emptied by the poll

// --- The list's search ---------------------------------------------------------------
// The loader's search box, the same one Item Search and Battle Styles show on this
// screen (item_search.lua BOX_ID, BOX_HINT, BOX_X/BOX_Y, BOX_WIDTH): with them
// installed it is one box with one query. All on the game thread.
const char *const kSearchId = "equipment_search";
const char *const kSearchHint = "Search (Ctrl+F)";
constexpr float kSearchX = 0.22f, kSearchY = 0.17f, kSearchWidth = 0.30f; // fractions of the picture
const char *const kNoBlastMatches = "No ki blast matches";
const char *const kAvatarListMenuPath = "ui/iggy/AVATAR_LIST.iggy";

XV2Search g_search;
int g_searchLoaded = 0;              // 0 = not tried yet, 1 = the loader has the box, -1 = no box
std::vector<ListEntry> g_shownEntries; // what the list was given
std::vector<int> g_shownBlasts;      // list index -> g_blasts index
unsigned g_shownChanges = 0;         // the box's change count the list was filtered for
bool g_noBlastMatches = false;

bool MenuEdited(const char *path);

bool SearchReady()
{
    if (!g_searchLoaded) {
        g_searchLoaded = g_searchWanted && g_search.Load() ? 1 : -1;
        Log("blast list search: %s", g_searchLoaded > 0 ? "the loader's search box"
                                     : g_searchWanted ? "none (XV2 Mod Loader missing or too old)" : "off (blast_search)");
    }
    return g_searchLoaded > 0;
}

// Fills g_shownEntries with the blasts matching the box. Default and Race Default
// always show, and so does the equipped blast: a cancel returns the list's default
// entry, which has to be the equipped one. No match shows everything, with a note.
void FilterBlasts(int equipped)
{
    bool searching = SearchReady();
    if (searching) g_shownChanges = g_search.Changes(kSearchId);
    std::vector<bool> matches(g_blasts.size(), true);
    bool anyMatch = false;
    for (size_t i = 0; i < g_blasts.size(); i++) {
        if (searching) matches[i] = g_search.MatchesW(kSearchId, g_blasts[i].label.c_str()) != 0;
        anyMatch |= matches[i];
    }
    g_noBlastMatches = !anyMatch;
    g_shownEntries.clear();
    g_shownBlasts.clear();
    for (size_t i = 0; i < g_blasts.size(); i++) {
        if (anyMatch && !matches[i] && i >= kFixedEntries && static_cast<int>(i) != equipped) continue;
        g_shownEntries.push_back(g_entries[i]);
        g_shownBlasts.push_back(static_cast<int>(i));
    }
}

// The list index showing g_blasts[blast], -1 when it is filtered out.
int ShownIndexOf(int blast)
{
    auto found = std::find(g_shownBlasts.begin(), g_shownBlasts.end(), blast);
    return found == g_shownBlasts.end() ? -1 : static_cast<int>(found - g_shownBlasts.begin());
}

// Hands g_shownEntries to the list, with the equipped blast as its default.
void SetBlastListContents(void *list, int equipped)
{
    ListParam param{};
    param.defaultIndex = (std::max)(ShownIndexOf(equipped), 0);
    param.begin = g_shownEntries.data();
    param.end = param.capacityEnd = g_shownEntries.data() + g_shownEntries.size();
    Game<SetListContentsFn>(kSetListContentsRva)(list, &param);
}

int EquippedBlastIndex() { return (std::max)(IndexOfBlast(KiBlast_Equipped(CurrentSlot(), kMainPreset)), 0); }

// The query changed while the list is open: filter again and send the new entries.
// The rebuilt AvatarList.as sees the count again and redraws, with the cursor kept
// on its blast (or on the first blast when that one is filtered out).
void RefreshBlastList(void *list)
{
    int equipped = EquippedBlastIndex();
    int cursorBlast = g_cursor >= 0 && g_cursor < static_cast<int>(g_shownBlasts.size()) ? g_shownBlasts[g_cursor] : equipped;
    FilterBlasts(equipped);
    int cursor = ShownIndexOf(cursorBlast);
    if (cursor < 0) cursor = static_cast<int>(g_shownBlasts.size()) > kFixedEntries ? kFixedEntries : 0;
    SetBlastListContents(list, equipped);
    Game<SetUserDataIntFn>(kSetUserDataIntRva)(list, kListUserDataCursor, cursor);
    Field<int>(list, kListResult) = cursor; // the AS sends it back too, as a cursor move
    g_cursor = cursor;
}

// Same steps as the game's own list openers (template 0x4fd5a0): contents
// with the equipped entry as default, remember which list is open, switch the
// menu to its list state.
void OpenBlastList(void *menu)
{
    void *list = Field<void *>(menu, kEditList);
    // No list object: the ctor patch did not go in.
    if (!list || !EnsureBlastList() || g_entries.empty()) { ShowEquipped(menu); return; }
    int equipped = EquippedBlastIndex();
    FilterBlasts(equipped);
    SetBlastListContents(list, equipped);
    g_cursor = (std::max)(ShownIndexOf(equipped), 0);
    // The X button jumps to the first entry, Default (the game's opener sends its lists' X entry the same way).
    Game<SetListXEntryFn>(kSetListXEntryRva)(list, 0);
    Field<int>(menu, kOpenListType) = kSendRequestBlast;
    Game<SetMenuStateFn>(kSetMenuStateRva)(Field<void *>(menu, kStateMachine),
                                           ListState(Field<int>(menu, kMenuMode), kSendRequestBlast), 0, 0);
}

void __fastcall EditCharaCallbackHook(void *menu, const uint8_t *call)
{
    int sendType = Field<int>(const_cast<uint8_t *>(call), kCallArgCount) >= 1
                       ? static_cast<int>(Field<double>(const_cast<uint8_t *>(call), kCallArg0)) : -1;
    if (sendType == kSendRequestBlast) { OpenBlastList(menu); return; }
    g_editCharaCallback(menu, call);
}

// Skill push 0x4fc850 (call sites 0x4f8851, 0x4f8903, 0x4fb3e1): refreshes the
// skill slots; the row follows. Also called while our list is open (the Skills
// page re-pushes then): the AS list wait ends as soon as slot 102 is re-sent,
// so the poll hook sends it once the list closes.
void __fastcall SkillPushHook(void *menu, int customize)
{
    Game<SkillPushFn>(kSkillPushRva)(menu, customize);
    if (Field<int>(menu, kOpenListType) == kSendRequestBlast) return;
    ShowEquipped(menu);
}

// List poll 0x4fe4b0 (call site 0x4fe103), every frame while a list is open:
// when the list closes it resets +0x2f8 to 0x192. The entry under the cursor
// is the list's result (sent on every move; a cancel sends the default back),
// read here before the game's poll empties it.
uint64_t __fastcall ListPollHook(void *menu)
{
    bool ours = Field<int>(menu, kOpenListType) == kSendRequestBlast;
    void *list = ours ? Field<void *>(menu, kEditList) : nullptr;
    int picked = -1;
    bool cancelled = false;
    if (list) {
        uint32_t flags = Field<uint32_t>(list, kListFlags);
        cancelled = (flags & kListDecided) != kListDecided && (flags & kListCancelled) != 0;
        if (Game<ListHasResultFn>(kListHasResultRva)(list)) {
            int shown = Game<ListGetResultFn>(kListGetResultRva)(list);
            if (shown >= 0 && shown < static_cast<int>(g_shownBlasts.size())) { picked = shown; g_cursor = shown; }
        } else if ((flags & kListDecided) == kListDecided) {
            picked = g_cursor; // the poll emptied an earlier cursor move
        }
    }
    uint64_t result = Game<ListPollFn>(kListPollRva)(menu);
    if (!ours) return result;
    if (Field<int>(menu, kOpenListType) == kNoListOpen) {
        if (picked >= 0 && picked < static_cast<int>(g_shownBlasts.size()) && !cancelled)
            SetPick(CurrentSlot(), kMainPreset, g_blasts[g_shownBlasts[picked]].id1);
        ShowEquipped(menu);
        // the note is ours only while this list is in front
        if (SearchReady()) g_search.SetNote(kSearchId, "");
    } else if (list && SearchReady()) {
        // The box stays up while the list is open (it hides by itself once Show stops).
        // Without the rebuilt list script the matches show the next time the list
        // opens: a count the script never reads must not change under it.
        unsigned changes = g_search.Show(kSearchId, kSearchHint, kSearchX, kSearchY, kSearchWidth);
        if (changes != g_shownChanges && MenuEdited(kAvatarListMenuPath) && !(Field<uint32_t>(list, kListFlags) & kListEnded))
            RefreshBlastList(list);
        // every frame: Item Search sets its note only when it changes
        g_search.SetNote(kSearchId, g_noBlastMatches ? kNoBlastMatches : "");
    }
    return result;
}

// --- Rows in the Presets panel and the skill popup -------------------------------

const char *const kMysetMenuPath = "ui/iggy/AVATAR_SET.iggy";
const char *const kCharaSeleMenuPath = "ui/iggy/CHARASELE.iggy";
const char *const kUiOwner = "KiBlastOverride";

std::string CopiedPatchText(int (*query)(const char *, char *, int), const char *path)
{
    char text[1024] = {};
    if (query) query(path, text, sizeof(text));
    return text;
}

// Whether the copy of the menu the game last read has our edit in it, so its
// script splits off what we append (the vanilla one would show the tab and the
// name). The loader's status for a file is "served: ..." with one patch, or
// "owner: outcome; owner: outcome" with several. "unchanged: no changes" means
// an earlier stage already served the same edit (Battle Styles registers the
// same AVATAR_LIST script), so the served copy has ours in it too.
bool MenuEdited(const char *path)
{
    std::string status = CopiedPatchText(Files().Status, path);
    if (status.compare(0, 7, "served:") == 0) return CopiedPatchText(Files().Owner, path) == kUiOwner;
    auto hasOutcome = [&status](const std::string &outcome) {
        size_t at = status.find(outcome);
        return at != std::string::npos && (at == 0 || (at >= 2 && status.compare(at - 2, 2, "; ") == 0));
    };
    if (hasOutcome(std::string(kUiOwner) + ": served:")) return true;
    return status.find(": served:") != std::string::npos && hasOutcome(std::string(kUiOwner) + ": unchanged: no changes");
}

// A user data string as the menu holds it. Vector at +0x160/+0x168 of 16-byte
// entries: type at +4 (6 = wide string), the pointer at +8 (SetUserDataStr 0x2c8340).
std::wstring UserDataString(void *menu, int index)
{
    uint8_t *begin = Field<uint8_t *>(menu, 0x160), *end = Field<uint8_t *>(menu, 0x168);
    if (!begin || index < 0 || index >= (end - begin) / 16) return {};
    uint8_t *entry = begin + static_cast<size_t>(index) * 16;
    const wchar_t *text = *reinterpret_cast<const wchar_t **>(entry + 8);
    if (*reinterpret_cast<const int *>(entry + 4) != 6 || !text) return {};
    return std::wstring(text, wcsnlen(text, 1024));
}

// Appends "\t<the preset's blast>" to a skill name the game just set; an earlier
// append (the game didn't set it again) is replaced.
void AppendBlastLabel(void *menu, int index, int slot, int preset)
{
    if (!EnsureBlastList()) return;
    std::wstring text = UserDataString(menu, index);
    text.resize((std::min)(text.size(), text.find(L'\t')));
    int blast = IndexOfBlast(KiBlast_Equipped(slot, preset));
    text += L"\t" + g_blasts[blast < 0 ? 0 : blast].label;
    AcquireSRWLockExclusive(&g_menuTextLock);
    const wchar_t *kept = g_menuTexts.insert(text).first->c_str();
    ReleaseSRWLockExclusive(&g_menuTextLock);
    Game<SetUserDataStrFn>(kSetUserDataStrRva)(menu, index, kept);
}

// Fault-safe: the pointer comes from the menu's call.
bool CopyMenuNote(const char *text, char *out, size_t capacity)
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

// A call from our row code carrying a line for the log (a string argument is a
// UTF-8 pointer where a number would be its double). True = it was one.
bool HandleMenuNote(const uint8_t *call)
{
    void *args = const_cast<uint8_t *>(call);
    if (Field<int>(args, kCallArgCount) < 2 || static_cast<int>(Field<double>(args, kCallArg0)) != kSendMenuNote) return false;
    char note[256];
    const char *text = Field<const char *>(args, kCallArg1);
    if (text && CopyMenuNote(text, note, sizeof(note))) Log("%s", note);
    return true;
}

// Presets panel: after the game loads or saves a preset, copy the pick; after it
// fills the panel, add the preset's blast for the row.
void __fastcall MysetCallbackHook(void *menu, const uint8_t *call)
{
    if (HandleMenuNote(call)) return;
    g_mysetCallback(menu, call);
    void *args = const_cast<uint8_t *>(call);
    if (Field<int>(args, kCallArgCount) < 2) return;
    int sendType = static_cast<int>(Field<double>(args, kCallArg0));
    int preset = static_cast<int>(Field<double>(args, kCallArg1));
    if (sendType == kMysetLoad) CopyPreset(preset, kMainPreset);
    else if (sendType == kMysetSave) CopyPreset(kMainPreset, preset);
    else if (sendType == kMysetEquipData && preset >= 0 && preset < kPresetCount && MenuEdited(kMysetMenuPath))
        AppendBlastLabel(menu, kMysetFirstSkillUserData, CurrentSlot(), preset);
}

UserCallbackFn g_selectCharaCallback = nullptr; // SelectCharacter's slot 17 before our hook

void __fastcall SelectCharaCallbackHook(void *menu, const uint8_t *call)
{
    if (!HandleMenuNote(call)) g_selectCharaCallback(menu, call);
}

// Character select: after the game (and Battle Styles, whose hook is on the
// function's entry) fills the skill popup, add a CaC's blast for the row.
uint64_t __fastcall FillSkillPopupHook(void *menu)
{
    uint64_t result = Game<uint64_t(__fastcall *)(void *)>(kFillSkillPopupRva)(menu);
    int cmsId = Field<int>(menu, kSelectCharaCmsId), preset = Field<int>(menu, kSelectCharaPreset), slot = Field<int>(menu, kSelectCharaSlot);
    if (cmsId >= kFirstCacCmsId && cmsId <= kLastCacCmsId && slot >= 0 && slot < kSlotCount && MenuEdited(kCharaSeleMenuPath))
        AppendBlastLabel(menu, kPopupFirstSkillUserData, slot, preset >= 0 && preset < kPresetCount ? preset : kMainPreset);
    return result;
}

// --- Menu edits ------------------------------------------------------------------

// Hands as3\AvatarMyset.as and CharaSele*.as (the rows) to the UI library, which
// merges them with other mods' edits of those menus. Without them the DLL appends
// nothing (MenuEdited) and the menus stay vanilla.
void RegisterRowEdits()
{
    XV2UI ui;
    if (!ui.Load()) return; // RegisterAvatarEdit logs it
    char reason[256] = {};
    if (!ui.EditScript(kUiOwner, kMysetMenuPath, kKboMysetAbc, sizeof(kKboMysetAbc), kKboMysetBaseAbc, sizeof(kKboMysetBaseAbc),
                       kKboMysetGameAbc, sizeof(kKboMysetGameAbc), 0, reason, sizeof(reason)))
        Log("%s: NOT patched - no row in the Presets panel: %s", kMysetMenuPath, reason);
    // one script per CHARASELE in use: the game's and xv2patcher 4.7's
    reason[0] = 0;
    int charaSele = ui.EditScript(kUiOwner, kCharaSeleMenuPath, kKboCharaSeleAbc, sizeof(kKboCharaSeleAbc), kKboCharaSeleBaseAbc,
                                  sizeof(kKboCharaSeleBaseAbc), kKboCharaSeleGameAbc, sizeof(kKboCharaSeleGameAbc), 0, reason, sizeof(reason));
    if (!charaSele) Log("%s: the vanilla script was refused: %s", kCharaSeleMenuPath, reason);
    reason[0] = 0;
    int charaSelePatcher = ui.EditScript(kUiOwner, kCharaSeleMenuPath, kKboCharaSelePatcherAbc, sizeof(kKboCharaSelePatcherAbc),
                                         kKboCharaSelePatcherBaseAbc, sizeof(kKboCharaSelePatcherBaseAbc), kKboCharaSelePatcherGameAbc,
                                         sizeof(kKboCharaSelePatcherGameAbc), 0, reason, sizeof(reason));
    if (!charaSelePatcher) Log("%s: the xv2patcher 4.7 script was refused: %s", kCharaSeleMenuPath, reason);
    if (!charaSele && !charaSelePatcher) Log("%s: NOT patched - no row in the skill popup", kCharaSeleMenuPath);
    // the same rebuilt script Battle Styles registers: the list redraws when the search changes
    reason[0] = 0;
    if (g_searchWanted && !ui.EditScript(kUiOwner, kAvatarListMenuPath, kAvatarListAbc, sizeof(kAvatarListAbc), kAvatarListBaseAbc,
                                         sizeof(kAvatarListBaseAbc), kAvatarListGameAbc, sizeof(kAvatarListGameAbc), 0, reason, sizeof(reason)))
        Log("%s: NOT patched - the search filters when the list next opens: %s", kAvatarListMenuPath, reason);
}

// Hands as3\Avatar.as (the row) to the loader's UI library, which merges it
// with other mods' AVATAR edits (Battle Styles' style slot, Multi Accessory).
bool RegisterAvatarEdit()
{
    XV2UI ui;
    if (!ui.Load()) {
        Log("AVATAR.iggy: NOT patched - no Ki Blast Override row: the mod loader's UI library "
            "(XV2UserInterfaceLib.dll) is missing; update XV2 Mod Loader");
        return false;
    }
    char reason[256] = {};
    bool scripted = ui.EditScript("KiBlastOverride", "ui/iggy/AVATAR.iggy", kKboAvatarAbc, sizeof(kKboAvatarAbc), kKboAvatarBaseAbc,
                                  sizeof(kKboAvatarBaseAbc), kKboAvatarGameAbc, sizeof(kKboAvatarGameAbc), 0, reason, sizeof(reason)) != 0;
    if (!scripted) Log("AVATAR.iggy: NOT patched - no Ki Blast Override row: %s", reason);

    // The tenth skill button and one more row of box (Battle Styles adds its
    // own row of box the same way). Optional: an older UI library has no display
    // edits (the row then lands on the last vanilla button, and the script copes
    // with no btn_10).
    if (!ui.EditDisplay) {
        Log("AVATAR.iggy: display edit unavailable (older UI library) - no tenth skill button");
        return scripted;
    }
    // Why +=0.06641: frame 5 (Skills page, ModeFrameInfoList) scales the 512 px
    // _CMN_M_B_plt_win01 by 0.82224 (421 px); one row is 34 px = 34/512 of it.
    const char *commands = "clone edit_menu btn_9 btn_10 0 34\n"
                           "matrix editmenu @1 5 * * * +=0.06641 * *\n";
    reason[0] = 0;
    if (!ui.EditDisplay("KiBlastOverride", "ui/iggy/AVATAR.iggy", commands, 0, reason, sizeof(reason)))
        Log("AVATAR.iggy: display edit refused: %s", reason);
    else
        Log("AVATAR.iggy: display edit registered (tenth button + one row of box)");
    return scripted;
}

// --- Install -------------------------------------------------------------------

// Every RVA in this file is for game 1.27 (PE TimeDateStamp + SizeOfImage);
// the vtable writes are blind, so any other build must not be touched.
bool IsSupportedGame()
{
    const auto *image = reinterpret_cast<const uint8_t *>(g_exe);
    uint32_t pe = *reinterpret_cast<const uint32_t *>(image + 0x3C);
    return *reinterpret_cast<const uint32_t *>(image + pe + 8) == 0x6AAA4CB1 &&     // TimeDateStamp
           *reinterpret_cast<const uint32_t *>(image + pe + 24 + 56) == 0x1A33000;  // SizeOfImage
}

// "mov rax, target; jmp rax" within rel32 reach of the exe (as Training_QoL).
void *MakeJumpStub(uintptr_t nearAddress, void *target)
{
    unsigned char *page = nullptr;
    for (uintptr_t address = (nearAddress & ~uintptr_t(0xffff)) - 0x10000; address > nearAddress - 0x40000000; address -= 0x10000) {
        page = static_cast<unsigned char *>(VirtualAlloc(reinterpret_cast<void *>(address), 0x1000,
                                                         MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        if (page) break;
    }
    if (!page) return nullptr;
    page[0] = 0x48; page[1] = 0xB8;
    memcpy(page + 2, &target, 8);
    page[10] = 0xFF; page[11] = 0xE0;
    return page;
}

enum class Redirect { kNotYet, kDone, kFailed };

// Points the e8 call or e9 tail jump at `siteRva` (which must reach `targetRva`)
// at `hook` (one jump stub per hook, shared by its call sites). kNotYet while
// the bytes don't match (Steam DRM still encrypting .text).
Redirect RedirectCall(uintptr_t siteRva, uintptr_t targetRva, void *hook)
{
    unsigned char *site = reinterpret_cast<unsigned char *>(g_exe + siteRva);
    int32_t relative = 0;
    memcpy(&relative, site + 1, 4);
    if ((site[0] != 0xE8 && site[0] != 0xE9) || g_exe + siteRva + 5 + relative != g_exe + targetRva) return Redirect::kNotYet;
    static std::map<void *, void *> stubs; // hook -> stub; only the Start thread gets here
    void *&stub = stubs[hook];
    if (!stub) stub = MakeJumpStub(g_exe, hook);
    if (!stub) return Redirect::kFailed;
    int32_t newRelative = static_cast<int32_t>(reinterpret_cast<intptr_t>(stub) - reinterpret_cast<intptr_t>(site + 5));
    DWORD oldProtect = 0;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return Redirect::kFailed;
    memcpy(site + 1, &newRelative, 4);
    VirtualProtect(site, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return Redirect::kDone;
}

// Raises EditChara's user-data slot count to kSlotCountWanted unless another
// mod already raised it further (compare-exchange: never lowers it).
Redirect RaiseSlotCount()
{
    auto *code = reinterpret_cast<unsigned char *>(g_exe + kSlotCountCodeRva);
    if (memcmp(code, kSlotCountCode, sizeof(kSlotCountCode)) != 0) return Redirect::kNotYet;
    auto *count = reinterpret_cast<volatile char *>(g_exe + kSlotCountByteRva);
    if (static_cast<uint8_t>(*count) < kSlotCountVanilla || static_cast<uint8_t>(*count) > 0x7f) return Redirect::kFailed;
    DWORD oldProtect = 0;
    if (!VirtualProtect(const_cast<char *>(count), 1, PAGE_EXECUTE_READWRITE, &oldProtect)) return Redirect::kFailed;
    char seen = *count;
    while (static_cast<uint8_t>(seen) < kSlotCountWanted) {
        char before = _InterlockedCompareExchange8(count, static_cast<char>(kSlotCountWanted), seen);
        if (before == seen) break;
        seen = before;
    }
    VirtualProtect(const_cast<char *>(count), 1, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), const_cast<char *>(count), 1);
    return Redirect::kDone;
}

// The list ctor patch (see kListCtorCheckRva); bytes Battle Styles already
// patched count as done.
Redirect PatchListCtor()
{
    auto *code = reinterpret_cast<unsigned char *>(g_exe + kListCtorCheckRva);
    if (memcmp(code, kListCtorCheckCode, 13) != 0) return Redirect::kNotYet; // not decrypted yet
    unsigned char *skip = reinterpret_cast<unsigned char *>(g_exe + kListCtorSkipRva);
    if (memcmp(skip, kNop2, sizeof(kNop2)) == 0) return Redirect::kDone;
    if (memcmp(code, kListCtorCheckCode, sizeof(kListCtorCheckCode)) != 0) return Redirect::kNotYet;
    DWORD oldProtect = 0;
    if (!VirtualProtect(skip, sizeof(kNop2), PAGE_EXECUTE_READWRITE, &oldProtect)) return Redirect::kFailed;
    memcpy(skip, kNop2, sizeof(kNop2));
    VirtualProtect(skip, sizeof(kNop2), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), skip, sizeof(kNop2));
    return Redirect::kDone;
}

// The skill EAN path 0x260ba0 asks 0x2900c0 "is this a created character" before trying
// <name>_<code>.ean, so CaCs never get a file of their own. Answering no here (this one
// call) lets a ported blast have one per race; a code without a file goes on to the same
// _F, _MAM or plain lookup as before. Only skills with the 0x800 flag get to this call.
Redirect PatchCharaEanLookup()
{
    static const unsigned char kAnswerNo[5] = {0x31, 0xC0, 0x90, 0x90, 0x90}; // xor eax, eax
    auto *site = reinterpret_cast<unsigned char *>(g_exe + kCharaEanCreatedCheckRva);
    if (memcmp(site, kAnswerNo, sizeof(kAnswerNo)) == 0) return Redirect::kDone;
    int32_t relative = 0;
    memcpy(&relative, site + 1, 4);
    if (site[0] != 0xE8 || g_exe + kCharaEanCreatedCheckRva + 5 + relative != g_exe + kIsCreatedCharaRva) return Redirect::kNotYet;
    DWORD oldProtect = 0;
    if (!VirtualProtect(site, sizeof(kAnswerNo), PAGE_EXECUTE_READWRITE, &oldProtect)) return Redirect::kFailed;
    memcpy(site, kAnswerNo, sizeof(kAnswerNo));
    VirtualProtect(site, sizeof(kAnswerNo), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(kAnswerNo));
    return Redirect::kDone;
}

// The battle loader registers a character's effects here. Every character also gets those
// of every blast owner the port names: the BAC record now names the owner for all users
// (a roster user of 21110 too), which blast a player uses isn't known to every machine at
// this point, and an owner is one small EEPK. Same call as the game's, which already
// repeats for two players of one character.
uintptr_t __fastcall RegisterEffectsHook(void *manager, int type, int chara, int zero, int one, int one2)
{
    using RegisterEffectsFn = uintptr_t(__fastcall *)(void *, int, int, int, int, int);
    const auto original = reinterpret_cast<RegisterEffectsFn>(g_exe + kRegisterEffectsRva);
    const uintptr_t result = original(manager, type, chara, zero, one, one2);
    if (type == kEepkCharacter && chara >= 0 && g_listState.load() == 1) {
        static std::atomic<int> logged{0};
        for (int owner : g_effectOwners) {
            if (owner == chara) continue;
            original(manager, type, owner, zero, one, one2);
            if (logged.fetch_add(1) < kEffectLogLines) Log("effects of chara %d registered for chara %d", owner, chara);
        }
    }
    return result;
}

// Chains a vtable slot (.rdata: no DRM wait). Another mod may have hooked it
// already; we call whatever was there.
bool ChainUserCallback(uintptr_t vtableRva, UserCallbackFn hook, UserCallbackFn &previousOut)
{
    auto **slot = reinterpret_cast<void **>(g_exe + vtableRva) + kUserCallbackSlot;
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtect)) return false;
    void *previous;
    do {
        previous = *slot;
        previousOut = reinterpret_cast<UserCallbackFn>(previous);
    } while (InterlockedCompareExchangePointer(slot, reinterpret_cast<void *>(hook), previous) != previous);
    VirtualProtect(slot, sizeof(*slot), oldProtect, &oldProtect);
    return true;
}

DWORD WINAPI Start(LPVOID)
{
    ReadConfig();
    Log("---- Ki Blast Override v" MOD_VERSION " starting");
    g_exe = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!IsSupportedGame()) {
        Log("unsupported game build (this build targets 1.27) - nothing hooked");
        return 0;
    }
    // Before anything else: the game reads the CUS once, early in its boot.
    char stageReason[128] = {};
    if (!Files().AddStage || !Files().AddStage("KiBlastOverride", kCusPath, 0, &PrepareCusBlasts, nullptr, stageReason, sizeof(stageReason)))
        Log("custom_skill.cus: NOT patched - most roster blasts won't work: %s", stageReason);
    Log("Presets panel callback: %s", ChainUserCallback(kEditMysetVtableRva, &MysetCallbackHook, g_mysetCallback)
                                          ? "hooked" : "NOT hooked - presets don't copy the pick");
    bool menuEdited = RegisterAvatarEdit();
    RegisterRowEdits();
    Log("character select callback: %s", ChainUserCallback(kSelectCharaVtableRva, &SelectCharaCallbackHook, g_selectCharaCallback)
                                             ? "hooked" : "NOT hooked");
    bool rowHooked = ChainUserCallback(kEditCharaVtableRva, &EditCharaCallbackHook, g_editCharaCallback);
    Log("Customize row: AVATAR edit %s, callback %s", menuEdited ? "registered" : "NOT registered",
        rowHooked ? "hooked" : "NOT hooked - the row opens no list");

    struct Site { uintptr_t call, target; void *hook; Redirect state; };
    std::vector<Site> sites;
    for (uintptr_t call : kSkillPushCallRvas)
        sites.push_back({call, kSkillPushRva, reinterpret_cast<void *>(&SkillPushHook), Redirect::kNotYet});
    sites.push_back({kListPollCallRva, kListPollRva, reinterpret_cast<void *>(&ListPollHook), Redirect::kNotYet});
    sites.push_back({kFillSkillPopupJumpRva, kFillSkillPopupRva, reinterpret_cast<void *>(&FillSkillPopupHook), Redirect::kNotYet});
    for (uintptr_t call : kRegisterEffectsCallRvas)
        sites.push_back({call, kRegisterEffectsRva, reinterpret_cast<void *>(&RegisterEffectsHook), Redirect::kNotYet});
    Redirect slotCount = Redirect::kNotYet, listCtor = Redirect::kNotYet, charaEan = Redirect::kNotYet;
    bool blastSites = false;
    // Steam DRM decrypts .text after start: retry for up to 2 minutes.
    for (int attempt = 0; attempt < 1200; attempt++, Sleep(100)) {
        if (slotCount == Redirect::kNotYet) slotCount = RaiseSlotCount();
        if (listCtor == Redirect::kNotYet) listCtor = PatchListCtor();
        if (charaEan == Redirect::kNotYet) charaEan = PatchCharaEanLookup();
        if (!blastSites) blastSites = BlastSites_Install();
        bool pending = slotCount == Redirect::kNotYet || listCtor == Redirect::kNotYet || charaEan == Redirect::kNotYet || !blastSites;
        for (Site &site : sites) {
            if (site.state == Redirect::kNotYet) site.state = RedirectCall(site.call, site.target, site.hook);
            pending |= site.state == Redirect::kNotYet;
        }
        if (!pending) break;
    }
    Log("user-data slots: %s", slotCount == Redirect::kDone ? "raised" : "NOT raised - the row stays empty");
    Log("list object in the Customize menu: %s", listCtor == Redirect::kDone ? "patched" : "NOT patched - the row opens no list");
    Log("blast choice sites: %s", blastSites ? "hooked" : "NOT hooked - the pick has no effect");
    Log("per-race skill EANs: %s", charaEan == Redirect::kDone ? "patched" : "NOT patched - carried animations don't play");
    for (const Site &site : sites)
        Log("call at 0x%llx: %s", static_cast<unsigned long long>(site.call),
            site.state == Redirect::kDone ? "hooked" : site.state == Redirect::kFailed ? "hook failed" : "never matched 1.27");
    EnsureBlastList(); // build it now rather than in the middle of a menu
    return 0;
}

} // namespace

// Exports for blast_sites.cpp (outside the anonymous namespace).

void KiBlast_Log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    VLog(format, args);
    va_end(args);
}

// Save slot (0..7) of the CaC a character object is, -1 if it is none of ours:
// the loadout builder 0x653830 is given the live save's CaC itself
// (manager + 0x120d0 + slot * 0xe880; Multi Accessory's BuildLoadoutHook).
int KiBlast_SlotOfCharacter(const char *character)
{
    char *manager = SaveManager();
    if (!manager || !character) return -1;
    const char *firstCac = manager + kSaveSlots;
    if (character < firstCac) return -1;
    size_t offset = static_cast<size_t>(character - firstCac);
    if (offset % kSaveSlotSize != 0 || offset / kSaveSlotSize >= static_cast<size_t>(kSlotCount)) return -1;
    return static_cast<int>(offset / kSaveSlotSize);
}

// The pick for a save slot's preset: a blast ID1, kPickRaceDefault, or
// kPickDefault (nothing equipped, or a saved blast no longer in the list).
int KiBlast_Equipped(int slot, int preset)
{
    if (slot < 0 || slot >= kSlotCount || preset < 0 || preset >= kPresetCount) return kPickDefault;
    int id1;
    {
        std::lock_guard<std::mutex> lock(g_savedLock);
        id1 = SavedPick(slot, preset);
    }
    if (id1 < 0) return id1;
    if (IndexOfBlast(id1) < 0) {
        static std::set<int> reported;
        static std::mutex reportLock;
        std::lock_guard<std::mutex> lock(reportLock);
        if (reported.insert(id1).second) Log("saved blast %d is not in the list (or the list is not ready) - Default", id1);
        return kPickDefault;
    }
    return id1;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        wchar_t path[MAX_PATH];
        DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        g_modDir.assign(path, length);
        g_modDir.resize(g_modDir.find_last_of(L"\\/") + 1);
        // Off the loader lock: the hook retries must not block DllMain.
        HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
//  The in-game sync.
//
//  The save slot screen (AVATAR_SLOT.iggy) is served with a rebuilt script
//  (as3\AvatarSlot.as -> avatarslot_abc.h via ClaudeTools\iggy\menu_abc.py) that
//  adds a "SaveSync" column with an ON / OFF toggle per slot, empty slots
//  included. It talks to the menu's AS callback, hooked here:
//    9910 (int, unused)  the toggles, please (sent when the screen opens)
//    9911 (int slot)     flip that slot's toggle
//    9912 (string)       a line for savesync.log (the column's layout)
//    9913 (int slot)     that slot was picked to load (no answer)
//  and reads the answer from user data ReceiveType_ResultDeleteSlot (2):
//  0x10000 | toggle mask (bit n = slot n). The tag tells it apart from the
//  game's own delete result (0 or 1).
//
//  The toggles are kept per Steam account in savesync.sav. The toggled slots
//  that hold a character are merged (merge.cpp, the same rules as
//  offline\savesync.py) only when one of them is about to be played:
//    * when a toggled slot holding a character is picked to load, before
//      the game loads it (the other slots get its progress at the same
//      time - it was saved into the image);
//    * when a toggled slot picked while empty gets its character: once, in
//      the first game save after creation, so it starts with the others'
//      progress.
//  Nothing runs on opening the screen, on toggling or on other saves.
//  A merge works on the save's decrypted image the way a save does: the
//  game's state is written into the image (the serializer every save uses),
//  the image is merged, the merged slots are read back into the game's state
//  (the readers the save loader uses), and the result is checked by
//  writing the state out again - only an exact match is saved to disk.
//
//  Research_Notes\save-flags-and-unlocks.md "SaveSync in game" has the
//  findings (addresses, layouts, the save thread's flags).
// ---------------------------------------------------------------------------
#include "savesync.h"
#include "avatarslot_abc.h"
#include "iggy_abc.h"
#include "json.h"
#include "merge.h"
#include "mod.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

template <typename T> static T& Field(void* object, size_t offset) { return *(T*)((uint8_t*)object + offset); }

// ------------------------------------------------------------ game state ---
// Save data singleton [0x1554448] (1.26.01). It holds the save's decrypted
// image at +0x68 (file offset F = +0x68 + F; matched byte for byte against
// the decrypted DBXV2.sav, ClaudeTools\save\steamid_probe.py). The image starts
// "#SAV" and holds the owning Steam id at +8, XORed with "thebecon"
// (Xv2CoreLib SAV_File.cs).
static const uintptr_t kSaveData = 0x1554448;
static const size_t kSaveImage = 0x68;
static const size_t kSaveImageSteamId = 8;
static const uint64_t kSteamIdXor = 0x7468656265636F6E;

// The serializer every save runs before encrypting (0x85afb0, called from
// CAccount's save-now 0x2d9200 with these four parts of the save data:
// the image and three more blocks at +0x7b3f8, +0xb0880, +0xdf260).
static const uintptr_t kSerializeSave = 0x85afb0;
static const size_t kSerializePart2 = 0x7b3f8, kSerializePart3 = 0xb0880, kSerializePart4 = 0xdf260;
typedef void (__fastcall* SerializeSaveFn)(void* image, void* part2, void* part3, void* part4);

// The save account object [0x156f118] (CAccount). +0x6c: a save is being
// written by its thread (the image must not change); +0x94: a second save
// is queued (its copy at save data +0x12a260). Vtable 0xf9a6f8 slot 13
// (+0x68) = save now (0x2d9200; does nothing while save data +0x254458 is
// 0, i.e. before a save is loaded). Nothing calls 0x2d9200 directly, so
// hooking the slot sees every save the game makes; the serializer's only
// other caller (0x2d7f90) is the account's own load/create step.
static const uintptr_t kAccount = 0x156f118;
static const size_t kAccountSaving = 0x6c, kAccountSaveQueued = 0x94;
static const uintptr_t kAccountVtable = 0xf9a6f8;
static const int kAccountSaveNowSlot = 13;
static const uintptr_t kAccountSaveNow = 0x2d9200;
typedef void (__fastcall* SaveNowFn)(void* account);
static SaveNowFn g_origSaveNow = nullptr;

// The CaC manager [0x15545b0]: the playing slot (0x81acc0 get, 0x81af30
// set) and the per-slot parts the save loader 0x85bb70 reads each slot
// into (it sets the slot, then calls these getters with -1 = "the set slot").
static const uintptr_t kManager = 0x15545b0;
static const uintptr_t kGetSlot = 0x81acc0, kSetSlot = 0x81af30;
static const uintptr_t kFlagsPart = 0x81ad00, kQuestsPart = 0x81ac70, kMentorsPart = 0x81ac30;
typedef int   (__fastcall* GetSlotFn)(void* manager);
typedef void  (__fastcall* SetSlotFn)(void* manager, int slot);
typedef void* (__fastcall* PartOfSlotFn)(void* manager, int slot);
typedef void* (__fastcall* PartFn)(void* manager);

// The save loader's per-slot readers (0x85bb70, its loop at 0x85bcc5), with
// their arguments: save data + slot x stride + offset. The first stride is
// the main CaC block (0xc6c8 = 50888, Xv2CoreLib CAC_SIZE), the second the
// DLC block (0x6330 = 25392, CAC_DLC_SIZE); the other two are blocks after
// the image.
//   flags  (SYSFLAG at file 95280 = +0x17498, DLC flags, ...)
//   quests (all quest arrays; the DLC ones at file 524984 = +0x80320; also
//          Tokipedia, inside the DLC quest block - the self-check shows if not)
//   mentors (file 125956 = +0x1ec6c)
static const uintptr_t kReadFlags = 0x841470, kReadQuests = 0x83bbf0, kReadMentors = 0x82fe50;
typedef void (__fastcall* ReadPartFn)(void* part, void* a, void* b, void* c);
typedef void (__fastcall* ReadOnePartFn)(void* part, void* a);
static const size_t kMainStride = 0xc6c8, kDlcStride = 0x6330, kFlagsStride3 = 0x4c84, kQuestsStride3 = 0x1830;
static const size_t kFlagsArg1 = 0x17498, kFlagsArg2 = 0x7ef00, kFlagsArg3 = 0xb8e40;
static const size_t kQuestsArg1 = 0x17e98, kQuestsArg2 = 0x80320, kQuestsArg3 = 0xe8640;
static const size_t kMentorsArg1 = 0x1ec6c;
// The set slot's PlayData (0x81acd0: manager + 0x12660 + slot x 0xe880; the
// loader reads file 126340 into it with 0x83fbf0, a 1:1 copy). Its wish
// counts sit at +0x68 (0x83f1f0), "more characters" = index 7 (0x84ad70).
static const uintptr_t kPlayDataPart = 0x81acd0;
static const size_t kPlayDataWishCounts = 0x68, kWishCharacters = 7;
static const size_t kFileWishCharacters = 126276 + 168 + kWishCharacters;  // same byte in the image
static const int kCurrentSlot = -1;

template <typename Fn> static Fn GameFunction(uintptr_t rva) { return (Fn)(g_base + rva); }
static uint8_t* SaveData() { return *(uint8_t**)(g_base + kSaveData); }

// The save's Steam account (64-bit id as text), or empty while no save is
// loaded. Read from the save rather than Steam: saves are per account.
static std::string SteamAccount()
{
    const uint8_t* saveData = SaveData();
    if (!saveData) return {};
    const uint8_t* image = saveData + kSaveImage;
    if (memcmp(image, "#SAV", 4) != 0) return {};
    uint64_t id = *(const uint64_t*)(image + kSaveImageSteamId) ^ kSteamIdXor;
    return id ? std::to_string(id) : std::string();
}

// ---------------------------------------------------------------- toggles ---
// savesync.sav (JSON text; .sav so a tool that shares the mod's folder
// treats it as save data): Steam account -> toggled slots. Game thread only.
//   {"76561198000000000": [2, 7]}
static const wchar_t kTogglesFile[] = L"savesync.sav";
static std::map<std::string, std::set<int>> g_toggles;
static bool g_togglesLoaded = false;

static std::wstring TogglesPath() { return std::wstring(g_dir) + L"\\" + kTogglesFile; }

static void LoadToggles()
{
    g_togglesLoaded = true;
    std::string text, error;
    JVal root;
    if (!ReadWhole(TogglesPath().c_str(), text)) return;  // none yet
    if (!JsonParse(text, root, error) || root.type != JVal::Obj) { Log("%ls unreadable (%s) - ignored", kTogglesFile, error.c_str()); return; }
    for (const auto& [account, slots] : root.o) {
        if (slots.type != JVal::Arr || account.find_first_of("\"\\") != std::string::npos) continue;
        for (const JVal& slot : slots.a)
            if (slot.type == JVal::Num && slot.n >= 0 && slot.n < kSlotCount) g_toggles[account].insert((int)slot.n);
    }
}

// Written to a temporary file first so a crash never leaves half a file.
static void SaveToggles()
{
    std::string text = "{";
    const char* accountComma = "";
    for (const auto& [account, slots] : g_toggles) {
        if (slots.empty()) continue;
        text += std::string(accountComma) + "\n    \"" + account + "\": [";
        accountComma = ",";
        const char* slotComma = "";
        for (int slot : slots) {
            text += slotComma + std::to_string(slot);
            slotComma = ", ";
        }
        text += "]";
    }
    text += "\n}\n";
    std::wstring path = TogglesPath(), temporary = path + L".tmp";
    HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { Log("%ls: cannot write", kTogglesFile); return; }
    DWORD written = 0;
    bool ok = WriteFile(handle, text.data(), (DWORD)text.size(), &written, nullptr) && written == text.size();
    CloseHandle(handle);
    if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) Log("%ls: cannot write", kTogglesFile);
}

// The loaded save's toggled slots; null while no save is loaded.
static std::set<int>* AccountToggles()
{
    std::string account = SteamAccount();
    if (account.empty()) return nullptr;
    if (!g_togglesLoaded) LoadToggles();
    return &g_toggles[account];
}

static int ToggleMask()
{
    int mask = 0;
    if (const std::set<int>* toggles = AccountToggles())
        for (int slot : *toggles) mask |= 1 << slot;
    return mask;
}

// ------------------------------------------------------------- the merge ---
enum class SyncResult { Synced, AlreadyInSync, NothingToSync, SlotEmpty, Busy, Failed };

static void SerializeState(uint8_t* saveData)
{
    GameFunction<SerializeSaveFn>(kSerializeSave)(saveData + kSaveImage, saveData + kSerializePart2,
                                                  saveData + kSerializePart3, saveData + kSerializePart4);
}

// Reads one slot's flags, quests, mentors and wish characters back from the image into the
// game's state, as the save loader does (the slot must be the set one).
static void ReadSlotBack(void* manager, uint8_t* saveData, int slot)
{
    uint8_t* main = saveData + slot * kMainStride;
    uint8_t* dlc = saveData + slot * kDlcStride;
    GameFunction<ReadPartFn>(kReadFlags)(GameFunction<PartOfSlotFn>(kFlagsPart)(manager, kCurrentSlot), main + kFlagsArg1,
                                         dlc + kFlagsArg2, saveData + slot * kFlagsStride3 + kFlagsArg3);
    GameFunction<ReadPartFn>(kReadQuests)(GameFunction<PartOfSlotFn>(kQuestsPart)(manager, kCurrentSlot), main + kQuestsArg1,
                                          dlc + kQuestsArg2, saveData + slot * kQuestsStride3 + kQuestsArg3);
    GameFunction<ReadOnePartFn>(kReadMentors)(GameFunction<PartFn>(kMentorsPart)(manager), main + kMentorsArg1);
    // only the merged byte, not the whole PlayData (level, play time... stay as the game holds them)
    uint8_t* playData = (uint8_t*)GameFunction<PartFn>(kPlayDataPart)(manager);
    playData[kPlayDataWishCounts + kWishCharacters] = saveData[kSaveImage + slot * kMainStride + kFileWishCharacters];
}

// How a merge was asked for: from the slot menu (log everything, save the
// result now) or from inside a game save (quiet when nothing changes; the
// save that is running writes the result).
struct SyncOptions {
    const char* reason;    // for the log
    bool saveWhenDone;     // run save-now afterwards
    bool quietWhenInSync;  // no "already in sync" line
    int pickedSlot;        // the slot about to be played: SlotEmpty when it holds no character
};

// Whether the merge changed anything in a slot.
static bool Gained(const MergeReport& gained)
{
    return gained.flags || gained.questRecords || gained.tokipedia || gained.mentors || gained.wishCharacters;
}

// Merges the toggled slots holding a character. Game thread only (the game
// changes its state there).
static SyncResult SyncToggledSlots(const SyncOptions& options)
{
    uint8_t* saveData = SaveData();
    void* account = *(void**)(g_base + kAccount);
    void* manager = *(void**)(g_base + kManager);
    if (!saveData || !account || !manager) return SyncResult::NothingToSync;
    int mask = ToggleMask();
    if (!mask) return SyncResult::NothingToSync;
    if (Field<int>(account, kAccountSaving) || Field<int>(account, kAccountSaveQueued)) return SyncResult::Busy;

    // the image as it would be saved now
    SerializeState(saveData);
    uint8_t* image = saveData + kSaveImage;
    if (memcmp(image, "#SAV", 4) != 0) { Log("%s: no save image - nothing synced", options.reason); return SyncResult::Failed; }
    std::vector<int> slots;
    std::string names;
    for (int slot = 0; slot < kSlotCount; ++slot) {
        if (!(mask & (1 << slot))) continue;
        std::string name = CharacterName(image, slot);
        if (name.empty()) continue;  // an empty toggled slot joins once it has a character
        slots.push_back(slot);
        names += (names.empty() ? "" : ", ") + std::to_string(slot) + " " + name;
    }
    if (std::find(slots.begin(), slots.end(), options.pickedSlot) == slots.end()) return SyncResult::SlotEmpty;
    if (slots.size() < 2) return SyncResult::NothingToSync;

    std::vector<uint8_t> merged(image, image + kMergedImageSize);
    std::map<int, MergeReport> report;
    std::string error;
    if (!MergeSlots(merged, slots, report, error)) {
        Log("%s: slots %s NOT synced - %s", options.reason, names.c_str(), error.c_str());
        return SyncResult::Failed;
    }
    if (memcmp(merged.data(), image, kMergedImageSize) == 0) {
        if (!options.quietWhenInSync) Log("%s: slots %s already in sync", options.reason, names.c_str());
        return SyncResult::AlreadyInSync;
    }
    memcpy(image, merged.data(), kMergedImageSize);

    // into the game's state, only the slots that gained something (the
    // playing slot is re-read only when it changed), then back to the
    // playing slot
    int playing = GameFunction<GetSlotFn>(kGetSlot)(manager);
    for (int slot : slots) {
        if (!Gained(report[slot])) continue;
        GameFunction<SetSlotFn>(kSetSlot)(manager, slot);
        ReadSlotBack(manager, saveData, slot);
    }
    GameFunction<SetSlotFn>(kSetSlot)(manager, playing);

    // self-check: the state written out again must be the merged image
    SerializeState(saveData);
    size_t differences = 0;
    std::string firstDifferences;
    for (size_t offset = 0; offset < kMergedImageSize; ++offset) {
        if (image[offset] == merged[offset]) continue;
        if (++differences <= 8) firstDifferences += " " + std::to_string(offset);
    }

    Log("%s: synced slots %s (playing slot %d)", options.reason, names.c_str(), playing);
    for (const auto& [slot, gained] : report)
        Log("  slot %d: +%d flags, %d quest records, +%d Tokipedia bits, %d mentors, +%d wish characters", slot,
            gained.flags, gained.questRecords, gained.tokipedia, gained.mentors, gained.wishCharacters);
    if (differences) {
        Log("  self-check: %zu bytes differ from the merge (file offsets%s) - not saved by SaveSync; the game's "
            "next save writes what it holds", differences, firstDifferences.c_str());
        return SyncResult::Failed;
    }
    if (options.saveWhenDone) {
        g_origSaveNow(account);
        Log("  self-check passed, saved");
    } else {
        Log("  self-check passed");
    }
    return SyncResult::Synced;
}

// ------------------------------------------------------------- game saves ---
// A toggled slot picked while empty is synced once, in the first save after
// its character exists: the game saves a new character as soon as creation
// ends (2026-10-11: the remade CaC was in DBXV2.sav by the prologue;
// synced there, it had everything after the intro). Other saves pass
// straight through.
static int g_newCharacterSlot = -1;  // the empty toggled slot picked last, or -1
static bool g_inSaveHook = false;

static void __fastcall HkSaveNow(void* account)
{
    void* manager = *(void**)(g_base + kManager);
    int playing = manager ? GameFunction<GetSlotFn>(kGetSlot)(manager) : -1;
    if (!g_inSaveHook && g_newCharacterSlot >= 0 && playing == g_newCharacterSlot) {
        g_inSaveHook = true;
        SyncResult result = SyncToggledSlots({"new character", false, true, playing});
        if (result == SyncResult::Busy) Log("new character: a save is still being written - synced at the next one");
        else if (result != SyncResult::SlotEmpty) g_newCharacterSlot = -1;  // done (or failed, logged) - once only
        g_inSaveHook = false;
    }
    g_origSaveNow(account);
}

static bool HookSaveNow(bool& vanilla)
{
    uintptr_t* slot = (uintptr_t*)(g_base + kAccountVtable) + kAccountSaveNowSlot;
    vanilla = *slot == g_base + kAccountSaveNow;
    DWORD old;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) return false;
    g_origSaveNow = (SaveNowFn)*slot;
    *slot = (uintptr_t)&HkSaveNow;
    VirtualProtect(slot, sizeof(*slot), old, &old);
    return true;
}

// -------------------------------------------------------- slot menu (AS) ---
// The save slot menu (EditSlot, vtable 0x10a1a88). Slot 17 (+0x88, vanilla
// 0x509b60) is the AS callback CallbackUserData("user", sendType, value):
// argc at +0x10, arg 0 as a double at +0x28, arg 1 at +0x48 (a double, or a
// UTF-8 char* for a string). The game's own handler ignores sendTypes it
// does not know.
static const uintptr_t kEditSlotVtable = 0x10a1a88;
static const int kUserCallbackSlot = 17;
static const uintptr_t kEditSlotCallback = 0x509b60;
static const size_t kCallArgCount = 0x10, kCallArg0 = 0x28, kCallArg1 = 0x48;
typedef void (__fastcall* UserCallbackFn)(void* menu, const uint8_t* call);
static UserCallbackFn g_origEditSlotCallback = nullptr;

static const int kSendGetToggles = 9910, kSendToggle = 9911, kSendLog = 9912, kSendLoad = 9913;
static const int kAnswerTag = 0x10000;

// The OggyBase user-data vector (+0x160 begin, +0x168 end): 16-byte entries
// {int set, int type, value}; ints are type 4 with the value at +8. Then
// menu +0x54 |= 1 (user data changed). Written directly: xv2patcher hooks
// the setter (0x2c81a0) and maps slots by caller (chara-select-search.md).
static const size_t kUserDataBegin = 0x160, kUserDataEnd = 0x168, kUserDataEntrySize = 16;
static const size_t kUserDataType = 4, kUserDataValue = 8;
static const int kUserDataTypeInt = 4;
static const size_t kUserDataChangedFlags = 0x54;
static const int kAnswerUserData = 2;  // AvatarSlot.ReceiveType_ResultDeleteSlot

static void WriteUserDataInt(void* menu, int index, int value)
{
    uint8_t* begin = Field<uint8_t*>(menu, kUserDataBegin);
    uint8_t* end = Field<uint8_t*>(menu, kUserDataEnd);
    if (!begin || index < 0 || index >= (end - begin) / (ptrdiff_t)kUserDataEntrySize) {
        Log("slot menu: no user data entry %d - no answer", index);
        return;
    }
    uint8_t* entry = begin + index * kUserDataEntrySize;
    *(int*)(entry + kUserDataValue) = value;
    *(int*)(entry + kUserDataType) = kUserDataTypeInt;
    *(int*)entry = 1;
    *(uint32_t*)((uint8_t*)menu + kUserDataChangedFlags) |= 1;
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

// From the slot menu. A save the game is still writing (it saves after
// each toggle) is waited for, up to kMenuWaitMs: the menu is idle meanwhile.
static const int kMenuWaitMs = 1500, kMenuWaitStepMs = 20;

static void SyncPickedSlot(int slot)
{
    g_newCharacterSlot = -1;
    if (slot < 0 || slot >= kSlotCount || !(ToggleMask() & (1 << slot))) return;
    SyncOptions options{"slot picked", true, false, slot};
    SyncResult result = SyncToggledSlots(options);
    for (int waited = 0; result == SyncResult::Busy && waited < kMenuWaitMs; waited += kMenuWaitStepMs) {
        Sleep(kMenuWaitStepMs);
        result = SyncToggledSlots(options);
    }
    if (result == SyncResult::Busy) Log("slot picked: a save is being written - slot %d not synced", slot);
    if (result == SyncResult::SlotEmpty) {
        g_newCharacterSlot = slot;
        Log("slot %d picked empty - synced once its character is saved", slot);
    }
}

// True when `call` was one of SaveSync's messages (never passed on).
static bool HandleSaveSync(void* menu, const uint8_t* call)
{
    if (*(const int*)(call + kCallArgCount) < 2) return false;
    int sendType = (int)*(const double*)(call + kCallArg0);
    if (sendType == kSendGetToggles) {
        WriteUserDataInt(menu, kAnswerUserData, kAnswerTag | ToggleMask());
        return true;
    }
    if (sendType == kSendToggle) {
        int slot = (int)*(const double*)(call + kCallArg1);
        std::set<int>* toggles = AccountToggles();
        if (!toggles) Log("toggle: no save loaded - ignored");
        else if (slot < 0 || slot >= kSlotCount) Log("toggle: slot %d ignored", slot);
        else {
            bool on = toggles->insert(slot).second;
            if (!on) toggles->erase(slot);
            SaveToggles();
            Log("slot %d toggled %s", slot, on ? "ON" : "OFF");
        }
        WriteUserDataInt(menu, kAnswerUserData, kAnswerTag | ToggleMask());
        return true;
    }
    if (sendType == kSendLoad) {
        SyncPickedSlot((int)*(const double*)(call + kCallArg1));
        return true;
    }
    if (sendType == kSendLog) {
        char text[2048];
        const char* passed = *(const char* const*)(call + kCallArg1);
        if (passed && CopyAsString(passed, text, sizeof(text))) Log("slot screen: %s", text);
        return true;
    }
    return false;
}

static void __fastcall HkEditSlotCallback(void* menu, const uint8_t* call)
{
    if (HandleSaveSync(menu, call)) return;
    g_origEditSlotCallback(menu, call);
}

static bool HookEditSlotCallback(bool& vanilla)
{
    uintptr_t* slot = (uintptr_t*)(g_base + kEditSlotVtable) + kUserCallbackSlot;
    vanilla = *slot == g_base + kEditSlotCallback;
    DWORD old;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) return false;
    g_origEditSlotCallback = (UserCallbackFn)*slot;
    *slot = (uintptr_t)&HkEditSlotCallback;
    VirtualProtect(slot, sizeof(*slot), old, &old);
    return true;
}

// --------------------------------------------------------------- the iggy ---
// File request class (General_QoL / Chara Search use the same
// hook, chained under one mutex): vtable slots 3/4 return the file's data,
// 6/7 its size; the path is stored inline at +8 (up to 0x118 chars).
static const uintptr_t kRequestVtable = 0xf9b270;
static const size_t kRequestPath = 8, kRequestPathMax = 0x118;
static const char kAvatarSlotPath[] = "ui/iggy/avatar_slot.iggy";  // lower case, '/'

typedef void*    (__fastcall* GetDataFn)(void* request);
typedef uint64_t (__fastcall* GetSizeFn)(void* request);
static GetDataFn g_origData18 = nullptr, g_origData20 = nullptr;
static GetSizeFn g_origSize30 = nullptr, g_origSize38 = nullptr;

// Patched copies, per source content. Kept for the whole session: the game
// may hold on to a buffer it was given.
static SRWLOCK g_servedLock = SRWLOCK_INIT;
static std::map<std::pair<uint64_t, uint64_t>, std::vector<uint8_t>*> g_served;  // (hash, size) -> copy or null

// Whether a request is for AVATAR_SLOT.iggy ('/' and '\' equal, any case).
static bool IsAvatarSlotRequest(void* request)
{
    const char* path = (const char*)request + kRequestPath;
    size_t length = strnlen(path, kRequestPathMax);
    size_t suffix = sizeof(kAvatarSlotPath) - 1;
    if (length < suffix + 1) return false;
    const char* tail = path + length - suffix;
    if (tail[-1] != '/' && tail[-1] != '\\') return false;
    for (size_t i = 0; i < suffix; ++i) {
        char have = tail[i] == '\\' ? '/' : (char)tolower((unsigned char)tail[i]);
        if (have != kAvatarSlotPath[i]) return false;
    }
    return true;
}

static std::vector<uint8_t>* Lookup(void* request, void* data, uint64_t size)
{
    if (!data || !IsAvatarSlotRequest(request)) return nullptr;
    uint64_t hash = Fnv64((const uint8_t*)data, (size_t)size);
    AcquireSRWLockExclusive(&g_servedLock);
    auto found = g_served.find({hash, size});
    std::vector<uint8_t>* served;
    if (found != g_served.end()) {
        served = found->second;
    } else {
        served = new std::vector<uint8_t>;
        std::string note;
        if (ReplaceIggyAbc((const uint8_t*)data, (size_t)size, kVanillaAvatarSlotAbcSize, kVanillaAvatarSlotAbcHash,
                           kAvatarSlotAbc, sizeof(kAvatarSlotAbc), *served, note)) {
            Log("AVATAR_SLOT.iggy (%llu bytes): %s", (unsigned long long)size, note.c_str());
        } else {
            Log("AVATAR_SLOT.iggy (%llu bytes): NOT patched, no toggles - %s", (unsigned long long)size, note.c_str());
            delete served;
            served = nullptr;
        }
        g_served[{hash, size}] = served;
    }
    ReleaseSRWLockExclusive(&g_servedLock);
    return served;
}

// Every mod in the file-request chain calls the next one's data and size;
// fetching them for files this mod never edits multiplied the calls ~600x
// down a six-mod chain (host hitches online), so the path is checked first.
static void* Serve(void* request, void* data)
{
    if (!data || !IsAvatarSlotRequest(request)) return data;
    std::vector<uint8_t>* served = Lookup(request, data, g_origSize38(request));
    return served ? served->data() : data;
}
static uint64_t ServeSize(void* request, uint64_t size)
{
    if (!IsAvatarSlotRequest(request)) return size;
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

// ---------------------------------------------------------------- install ---
void SaveSync_InstallHooks()
{
    // the callback first: a served script whose messages reach nobody would
    // show no toggles
    bool vanilla = false;
    bool callback = HookEditSlotCallback(vanilla);
    bool fileHook = callback && InstallFileHook();
    Log("save slot menu callback %s%s, file hook %s", callback ? "hooked" : "NOT hooked",
        vanilla ? "" : " (slot was already changed by another mod)", fileHook ? "installed" : "NOT installed");
    bool saveVanilla = false;
    bool saveHook = HookSaveNow(saveVanilla);
    Log("game save hook %s%s", saveHook ? "installed" : "NOT installed",
        saveVanilla ? "" : " (slot was already changed by another mod)");
}

// Ki blast choice: every call to the game's blast pick 0x618d30(talisman,
// race) that builds the player's own loadout is pointed at a small stub
// that passes the caller's context registers on to a C++ function here.
// The battle setup's copy, which sets the soul's blast again, gets one too.
// The function itself is never hooked, so other mods hooking the loadout
// builder 0x653830 (Battle Styles: entry hook; Multi Accessory: its call
// sites) are not touched. Call sites and registers: Research_Notes\
// ki-blast-type.md "Callers of 0x618d30" (game 1.27, found by disassembly).
#include "blast_sites.h"

#include <windows.h>
#include <cstring>

namespace {

constexpr uintptr_t kPickBlastRva = 0x618d30;  // int(int talisman, int race) -> blast skill ID1
constexpr intptr_t kPresetRecordSize = 0x68;   // preset records at character + 0xf4
constexpr intptr_t kPresetRecordsOffset = 0xf4;
constexpr uintptr_t kLoadoutBlastOffset = 0xa2; // word: loadout skill index 7 = the blast
constexpr int kRaceDefaultTalisman = -1;        // the souls-off path passes -1: race default
constexpr uintptr_t kSkillBaseRva = 0x2c5b70;   // int(skill type): ID1 base, 4 = blasts = 20000
constexpr uint32_t kSoulBlastOffset = 0x437;    // soul's blast = base + 0x437 + IDB Model
constexpr uintptr_t kReceiveRecordRva = 0x66ca00; // CSS record receiver (state, record)
constexpr size_t kRecordKind = 9;               // record byte: 1 = save character, 3 = NPC
constexpr unsigned char kRecordKindSaveCharacter = 1;
constexpr size_t kRecordBlast = 0xa2;           // record +0x2c loadout, its word +0x76
constexpr uintptr_t kSkillsetLookupRva = 0x2c5870; // CUS skillset(cus, chara, costume, preset)
constexpr size_t kSkillsetSize = 32;            // Xv2CoreLib CUS: 9 words from +8, blast is index 7
constexpr size_t kSkillsetBlast = 0x16;
constexpr uint16_t kSkillsetKeep = 0xfffe;      // 0xe8139: below the type's base = slot left alone
// The fighter's entry (0xe7ab5 stride 0x1f0, the same entries 0x92d00 walks).
constexpr size_t kEntryChara = 0;               // 0xe80b8
constexpr size_t kEntryBlast = 0x42;            // 0xe810c: skills at +0x34, blast is index 7
constexpr size_t kEntryTalisman = 0xb4;         // 0x92d4e, int
constexpr int kFirstCreatedChara = 100, kLastCreatedChara = 107; // HUM..MAF, 0x618d7c's race = id - 100

using PickBlastFn = int(__fastcall *)(int talisman, int race);
using SkillBaseFn = uint32_t(__fastcall *)(int skillType);
using ReceiveRecordFn = uintptr_t(__fastcall *)(void *state, const char *record);
using SkillsetLookupFn = const char *(__fastcall *)(void *cus, int chara, int costume, int preset);

int VanillaPick(int talisman, int race)
{
    return reinterpret_cast<PickBlastFn>(g_exe + kPickBlastRva)(talisman, race);
}

// --- remembered picks ------------------------------------------------------
// The loadout builder's pick is remembered by output pointer (later copies
// of that loadout re-pick from the talisman and would bring the soul back)
// and the CSS packer's by character (its souls-off branch re-picks later).
// A small ring: only the last few loadouts matter, never grows.
struct Remembered { const void *key; int blast; };
constexpr int kRememberedCount = 16;
Remembered g_byLoadout[kRememberedCount]{};
Remembered g_byCharacter[kRememberedCount]{};
int g_nextLoadout = 0, g_nextCharacter = 0;
SRWLOCK g_lock = SRWLOCK_INIT;

void Remember(Remembered *table, int &next, const void *key, int blast)
{
    AcquireSRWLockExclusive(&g_lock);
    int index = -1;
    for (int i = 0; i < kRememberedCount; i++)
        if (table[i].key == key) { index = i; break; }
    if (index < 0) { index = next; next = (next + 1) % kRememberedCount; }
    table[index] = {key, blast};
    ReleaseSRWLockExclusive(&g_lock);
}

bool Recall(const Remembered *table, const void *key, int &blast)
{
    AcquireSRWLockShared(&g_lock);
    bool found = false;
    for (int i = 0; i < kRememberedCount; i++)
        if (table[i].key == key) { blast = table[i].blast; found = true; break; }
    ReleaseSRWLockShared(&g_lock);
    return found;
}

void Forget(Remembered *table, const void *key)
{
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < kRememberedCount; i++)
        if (table[i].key == key) table[i] = {};
    ReleaseSRWLockExclusive(&g_lock);
}

// A few log lines per site per session, never per frame.
constexpr int kLogLinesPerSite = 8;
int g_logged[8]{};

bool g_keepThroughTransformations = false;

// --- words the battle setup must keep -----------------------------------------
// The setup copy 0x92d00 sets every soul wearer's blast again from the soul.
// A word in this list came from an override (ours from the sites below, other
// players' from their CSS records) and stays. A ring, like the tables above.
constexpr int kOverrideWordCount = 16;
uint16_t g_overrideWords[kOverrideWordCount]{};
int g_nextOverrideWord = 0;

void NoteOverrideWord(uint16_t word)
{
    AcquireSRWLockExclusive(&g_lock);
    bool known = false;
    for (uint16_t existing : g_overrideWords) known |= existing == word;
    if (!known) {
        g_overrideWords[g_nextOverrideWord] = word;
        g_nextOverrideWord = (g_nextOverrideWord + 1) % kOverrideWordCount;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

bool IsOverrideWord(uint16_t word)
{
    AcquireSRWLockShared(&g_lock);
    bool known = false;
    for (uint16_t existing : g_overrideWords) known |= word != 0 && existing == word;
    ReleaseSRWLockShared(&g_lock);
    return known;
}

// The blast the player chose for this CaC; false = "Default", the game decides.
bool OurBlast(int slot, int preset, int race, int &blast)
{
    int equipped = KiBlast_Equipped(slot, preset);
    if (equipped == kPickDefault) return false;
    blast = equipped == kPickRaceDefault ? VanillaPick(kRaceDefaultTalisman, race) : equipped;
    NoteOverrideWord(static_cast<uint16_t>(blast));
    return true;
}

// --- site functions: (talisman ecx, race edx, context r8, r9, [rsp+0x28]) ----

// 0x653b18, loadout builder 0x653830: r8 = character (rdi), r9 = preset *
// 0x68 (r12, already clamped to >= 0), 5th = loadout out (r15, word +0xa2).
uint32_t __fastcall SiteLoadoutBuilder(int talisman, int race, char *character, intptr_t presetOffset, char *out)
{
    int slot = KiBlast_SlotOfCharacter(character);
    if (slot < 0) return VanillaPick(talisman, race);
    int preset = static_cast<int>(presetOffset / kPresetRecordSize);
    int blast = 0;
    if (!OurBlast(slot, preset, race, blast)) {
        Forget(g_byLoadout, out);
        return VanillaPick(talisman, race);
    }
    Remember(g_byLoadout, g_nextLoadout, out, blast);
    if (g_logged[0] < kLogLinesPerSite && ++g_logged[0])
        KiBlast_Log("loadout: slot %d preset %d -> blast %d (soul %d would give %d)", slot + 1, preset, blast,
                    talisman, VanillaPick(talisman, race));
    return static_cast<uint32_t>(blast);
}

// 0x653d55, CSS record fill 0x653b90 (called by packer 0x66addd): r8 =
// character (rbp), r9 = preset record (r15), 5th = destination (rsi; word
// +0x76 = record +0xa2).
uint32_t __fastcall SiteCssRecord(int talisman, int race, char *character, intptr_t presetRecord, char *destination)
{
    (void)destination;
    int slot = KiBlast_SlotOfCharacter(character);
    if (slot < 0) return VanillaPick(talisman, race);
    intptr_t offset = presetRecord - reinterpret_cast<intptr_t>(character) - kPresetRecordsOffset;
    int preset = offset >= 0 && offset % kPresetRecordSize == 0 ? static_cast<int>(offset / kPresetRecordSize) : 0;
    int blast = 0;
    if (!OurBlast(slot, preset, race, blast)) {
        Forget(g_byCharacter, character);
        return VanillaPick(talisman, race);
    }
    Remember(g_byCharacter, g_nextCharacter, character, blast);
    if (g_logged[1] < kLogLinesPerSite && ++g_logged[1])
        KiBlast_Log("online record: slot %d preset %d -> blast %d", slot + 1, preset, blast);
    return static_cast<uint32_t>(blast);
}

// 0x6542c9 (0x653eb0) and 0x38ba8c (0x38b910): r8 = a 0x653830-format
// loadout. Ours only if the builder above filled it and its blast word is
// still what we gave it (NPC loadouts and reused stack buffers stay vanilla).
uint32_t LoadoutCopy(int site, int talisman, int race, const char *loadout)
{
    int blast = 0;
    if (Recall(g_byLoadout, loadout, blast) &&
        *reinterpret_cast<const uint16_t *>(loadout + kLoadoutBlastOffset) == static_cast<uint16_t>(blast)) {
        if (g_logged[site] < kLogLinesPerSite && ++g_logged[site])
            KiBlast_Log("loadout copy (site %d): keeps blast %d", site, blast);
        return static_cast<uint32_t>(blast);
    }
    return VanillaPick(talisman, race);
}

uint32_t __fastcall SiteLoadoutCopyA(int talisman, int race, char *loadout, intptr_t, char *)
{
    return LoadoutCopy(2, talisman, race, loadout);
}

uint32_t __fastcall SiteLoadoutCopyB(int talisman, int race, char *loadout, intptr_t, char *)
{
    return LoadoutCopy(3, talisman, race, loadout);
}

// 0x66aed7, packer 0x66abb0 save-CaC path, runs only when super souls are
// off ([r14+0x5c] != 0, talisman -1): r8 = character (rsi).
uint32_t __fastcall SiteSoulsOff(int talisman, int race, char *character, intptr_t, char *)
{
    int blast = 0;
    if (Recall(g_byCharacter, character, blast)) {
        if (g_logged[4] < kLogLinesPerSite && ++g_logged[4])
            KiBlast_Log("souls-off record: keeps blast %d", blast);
        return static_cast<uint32_t>(blast);
    }
    return VanillaPick(talisman, race);
}

// 0x66b25f, packer 0x66abb0, the other souls-off branch (0x66b205, talisman
// -1): r8 = the object from 0x27a330 (rdi; dword +0 = CMS id). Not known yet
// whether it is ever a save CaC, so the log says which case fired.
uint32_t __fastcall SiteSoulsOffOther(int talisman, int race, char *object, intptr_t, char *)
{
    int blast = 0;
    const char *source = "vanilla (not a save CaC)";
    bool ours = Recall(g_byCharacter, object, blast);
    if (ours) {
        source = "remembered";
    } else {
        int slot = KiBlast_SlotOfCharacter(object);
        if (slot >= 0) {
            ours = OurBlast(slot, 0, race, blast);
            source = ours ? "save CaC, preset 0" : "vanilla (Default)";
        }
    }
    if (!ours) blast = VanillaPick(talisman, race);
    if (g_logged[5] < kLogLinesPerSite && ++g_logged[5])
        KiBlast_Log("souls-off other: race %d -> blast %d, %s", race, blast, source);
    return static_cast<uint32_t>(blast);
}

// 0x92d87, setup copy 0x92d00 (not its tournament twin 0x92dc0, whose entries
// have no pick yet): call 0x2c5b70(4) = 20000, then word = that + Model +
// 0x437. r8 = the entry's blast word (rbx), r9 = Model (rdi). Returning a
// base that adds up to the word already there keeps it.
uint32_t __fastcall SiteSetupSoul(int skillType, int, const uint16_t *word, intptr_t model, char *)
{
    uint32_t base = reinterpret_cast<SkillBaseFn>(g_exe + kSkillBaseRva)(skillType);
    uint16_t soulBlast = static_cast<uint16_t>(base + model + kSoulBlastOffset);
    uint16_t current = *word;
    if (current == soulBlast) return base;
    bool keep = IsOverrideWord(current);
    if (g_logged[6] < kLogLinesPerSite && ++g_logged[6])
        KiBlast_Log("battle setup: blast %d %s (soul gives %d)", current, keep ? "kept" : "replaced", soulBlast);
    return keep ? current - static_cast<uint32_t>(model) - kSoulBlastOffset : base;
}

// 0x66c83a, 0x66c9e8: calls of the CSS record receiver 0x66ca00(state, record),
// local players' records included. A vanilla record's word is the soul's, so
// noting it changes nothing; Battle Styles hooks the entry, which this calls.
uintptr_t __fastcall SiteReceiveRecord(void *state, const char *record, intptr_t, intptr_t, char *)
{
    uint16_t word = *reinterpret_cast<const uint16_t *>(record + kRecordBlast);
    // Blast IDs start at 20000; 0xffff = no blast, which must stay replaceable.
    if (static_cast<unsigned char>(record[kRecordKind]) == kRecordKindSaveCharacter && word >= 20000 && word != 0xffff)
        NoteOverrideWord(word);
    return reinterpret_cast<ReceiveRecordFn>(g_exe + kReceiveRecordRva)(state, record);
}

// 0xe80c2, the battle's skillset change 0xe7a00 (transform and revert), only
// with "override_transformation_blasts": r9 = the awoken skill's preset, 5th =
// the fighter's entry (rbx). The loop after it reads the returned words right
// away (0xe8100), and "keep" there leaves the slot as it is, so a copy of the
// skillset with blast = keep holds the chosen one. "Chosen" comes only from the
// entry, which every player's game holds alike: a blast that neither the soul
// nor the race gives. So a CaC on Default still changes.
const char *__fastcall SiteTransformSkillset(void *cus, int chara, int costume, int preset, const char *entry)
{
    const char *skillset = reinterpret_cast<SkillsetLookupFn>(g_exe + kSkillsetLookupRva)(cus, chara, costume, preset);
    if (!skillset) return skillset;
    int entryChara = *reinterpret_cast<const int *>(entry + kEntryChara);
    if (entryChara < kFirstCreatedChara || entryChara > kLastCreatedChara) return skillset;
    int race = entryChara - kFirstCreatedChara;
    int talisman = *reinterpret_cast<const int *>(entry + kEntryTalisman);
    uint16_t current = *reinterpret_cast<const uint16_t *>(entry + kEntryBlast);
    bool chosen = current != 0xffff && current != static_cast<uint16_t>(VanillaPick(talisman, race)) &&
                  current != static_cast<uint16_t>(VanillaPick(kRaceDefaultTalisman, race));
    uint16_t presetBlast = *reinterpret_cast<const uint16_t *>(skillset + kSkillsetBlast);
    if (g_logged[7] < kLogLinesPerSite && ++g_logged[7])
        KiBlast_Log("transform: chara %d soul %d blast %d %s (preset %d has %d)", entryChara, talisman, current,
                    chosen ? "kept" : "follows the preset", preset, presetBlast);
    if (!chosen) return skillset;
    thread_local char copy[kSkillsetSize];
    memcpy(copy, skillset, sizeof(copy));
    memcpy(copy + kSkillsetBlast, &kSkillsetKeep, sizeof(kSkillsetKeep));
    return copy;
}

// --- stubs -------------------------------------------------------------------
// Per site: sub rsp,0x38; [mov [rsp+0x20], <5th>]; [mov r8, <a>]; [mov r9, <b>];
// mov rax, fn; call rax; add rsp,0x38; ret. Entered with rsp = 8 mod 16 (the
// e8 pushed the return), so after sub 0x38 it is 16-aligned for the call.
// ecx/edx (the original's arguments) pass through untouched; r8/r9/rax are
// volatile across the original call anyway.
struct Site {
    uintptr_t rva;
    uintptr_t target;                           // what the e8 there must call
    unsigned char fifth[5]; size_t fifthSize;   // mov [rsp+0x20], reg
    unsigned char r8[3];                        // mov r8, reg
    unsigned char r9[3]; size_t r9Size;         // mov r9, reg
    void *function;
    bool done;
};

Site g_sites[] = {
    // 0x653b18: [rsp+0x20] = r15, r8 = rdi, r9 = r12
    {0x653b18, kPickBlastRva, {0x4C, 0x89, 0x7C, 0x24, 0x20}, 5, {0x49, 0x89, 0xF8}, {0x4D, 0x89, 0xE1}, 3,
     reinterpret_cast<void *>(&SiteLoadoutBuilder), false},
    // 0x653d55: [rsp+0x20] = rsi, r8 = rbp, r9 = r15
    {0x653d55, kPickBlastRva, {0x48, 0x89, 0x74, 0x24, 0x20}, 5, {0x49, 0x89, 0xE8}, {0x4D, 0x89, 0xF9}, 3,
     reinterpret_cast<void *>(&SiteCssRecord), false},
    // 0x6542c9: r8 = rsi
    {0x6542c9, kPickBlastRva, {}, 0, {0x49, 0x89, 0xF0}, {}, 0, reinterpret_cast<void *>(&SiteLoadoutCopyA), false},
    // 0x38ba8c: r8 = rbx
    {0x38ba8c, kPickBlastRva, {}, 0, {0x49, 0x89, 0xD8}, {}, 0, reinterpret_cast<void *>(&SiteLoadoutCopyB), false},
    // 0x66aed7: r8 = rsi
    {0x66aed7, kPickBlastRva, {}, 0, {0x49, 0x89, 0xF0}, {}, 0, reinterpret_cast<void *>(&SiteSoulsOff), false},
    // 0x66b25f: r8 = rdi
    {0x66b25f, kPickBlastRva, {}, 0, {0x49, 0x89, 0xF8}, {}, 0, reinterpret_cast<void *>(&SiteSoulsOffOther), false},
    // 0x92d87: r8 = rbx, r9 = rdi
    {0x92d87, kSkillBaseRva, {}, 0, {0x49, 0x89, 0xD8}, {0x49, 0x89, 0xF9}, 3, reinterpret_cast<void *>(&SiteSetupSoul), false},
    // 0x66c83a, 0x66c9e8: r8 unused
    {0x66c83a, kReceiveRecordRva, {}, 0, {0x49, 0x89, 0xD0}, {}, 0, reinterpret_cast<void *>(&SiteReceiveRecord), false},
    {0x66c9e8, kReceiveRecordRva, {}, 0, {0x49, 0x89, 0xD0}, {}, 0, reinterpret_cast<void *>(&SiteReceiveRecord), false},
    // 0xe80c2: [rsp+0x20] = rbx; r8 and r9 are the lookup's own arguments. Keep last: optional.
    {0xe80c2, kSkillsetLookupRva, {0x48, 0x89, 0x5C, 0x24, 0x20}, 5, {}, {}, 0, reinterpret_cast<void *>(&SiteTransformSkillset), false},
};
constexpr size_t kStubSize = 0x30; // longest stub is 37 bytes

unsigned char *g_stubPage = nullptr;

// An executable page within rel32 reach below the exe (as MakeJumpStub).
unsigned char *NearPage()
{
    for (uintptr_t address = (g_exe & ~uintptr_t(0xffff)) - 0x10000; address > g_exe - 0x40000000; address -= 0x10000) {
        void *page = VirtualAlloc(reinterpret_cast<void *>(address), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (page) return static_cast<unsigned char *>(page);
    }
    return nullptr;
}

void WriteStub(unsigned char *at, const Site &site)
{
    size_t n = 0;
    auto put = [&](const unsigned char *bytes, size_t size) { memcpy(at + n, bytes, size); n += size; };
    const unsigned char subRsp[] = {0x48, 0x83, 0xEC, 0x38};
    const unsigned char movRax[] = {0x48, 0xB8};
    const unsigned char tail[] = {0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x38, 0xC3}; // call rax; add rsp,0x38; ret
    put(subRsp, sizeof(subRsp));
    put(site.fifth, site.fifthSize);
    if (site.r8[0]) put(site.r8, sizeof(site.r8));
    put(site.r9, site.r9Size);
    put(movRax, sizeof(movRax));
    put(reinterpret_cast<const unsigned char *>(&site.function), 8);
    put(tail, sizeof(tail));
}

} // namespace

void BlastSites_KeepThroughTransformations(bool keep)
{
    g_keepThroughTransformations = keep;
}

bool BlastSites_Install()
{
    static bool failed = false;
    if (failed) return false;
    // Off: the skillset change isn't touched at all.
    if (!g_keepThroughTransformations) g_sites[_countof(g_sites) - 1].done = true;
    if (!g_stubPage) {
        g_stubPage = NearPage();
        if (!g_stubPage) {
            KiBlast_Log("ki blast sites: no executable page near the game - blast override off");
            failed = true;
            return false;
        }
        for (size_t i = 0; i < _countof(g_sites); i++) WriteStub(g_stubPage + i * kStubSize, g_sites[i]);
        FlushInstructionCache(GetCurrentProcess(), g_stubPage, 0x1000);
    }

    bool all = true;
    for (size_t i = 0; i < _countof(g_sites); i++) {
        Site &site = g_sites[i];
        if (site.done) continue;
        unsigned char *code = reinterpret_cast<unsigned char *>(g_exe + site.rva);
        int32_t relative = 0;
        memcpy(&relative, code + 1, 4);
        if (code[0] != 0xE8 || g_exe + site.rva + 5 + relative != g_exe + site.target) {
            all = false; // .text still encrypted, or another mod owns the call: retry
            continue;
        }
        unsigned char *stub = g_stubPage + i * kStubSize;
        int32_t newRelative = static_cast<int32_t>(reinterpret_cast<intptr_t>(stub) - reinterpret_cast<intptr_t>(code + 5));
        DWORD oldProtect = 0;
        if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) { all = false; continue; }
        memcpy(code + 1, &newRelative, 4);
        VirtualProtect(code, 5, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), code, 5);
        site.done = true;
        KiBlast_Log("ki blast site 0x%llx redirected", static_cast<unsigned long long>(site.rva));
    }
    return all;
}

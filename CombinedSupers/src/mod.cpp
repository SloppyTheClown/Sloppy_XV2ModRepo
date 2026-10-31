// ---------------------------------------------------------------------------
//  Combined Super Stats - mod core
//
//  Makes strike supers and ki blast supers both scale off the COMBINED
//  Strike Supers + Ki Blast Supers investment, instead of each using only its
//  own stat.
//
//  How it works, in short:
//    * Two instructions in the super-damage path multiply by the character's
//      strike / ki-blast stat. We redirect each to a small code cave that
//      multiplies the value by a per-character factor first. Nothing else in
//      the game sees a modified stat - the character sheet, basic attacks and
//      defence are untouched.
//    * The game recomputes a character's stats (spawn, buffs, transformations)
//      and pushes them into the live battle struct through one function
//      (0x6bd70). Both of its call sites are redirected through us: after the
//      push we read the character's unbuffed stats from the game's own base
//      block, compute the factor and publish it to the table the caves read.
//      No memory scanning, no polling.
//    * Every input comes from data all players share (race, body type, level,
//      the stats the game computed), so two people running this compute
//      identical numbers.
// ---------------------------------------------------------------------------
#include "mod.h"
#include "tables.h"
#include "level_table.h"
#include "default_config.h"
#include "psc.h"
#include "menu_scripts.h"
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>

// ---------------------------------------------------------------- config ---
// Defaults match the shipped combined_supers.json, so a missing file behaves
// the same as an untouched one.
struct Config {
    bool  enabled   = true;
    char  stats[16] = "additive";   // additive | highest | vanilla
    char  weight[16]= "highest";    // highest  | vanilla
    char  height[16]= "highest";    // highest  | vanilla   (health only)
    float share     = 1.0f;
    int   capPoints = 200;
    float falloff   = 0.35f;
    bool  logging   = false;        // combined_supers.log + status file
};
static Config  g_cfg;

// How close a candidate must sit to the half-point lattice, in half points.
// Not configurable: loosening it lets wrong solutions through, tightening it
// rejects valid ones.
static const float kTol = 0.02f;

// Config, log and status: next to the DLL in its mod folder for the loader
// build; in <game>\XV2CombinedSupers for the standalone winmm.dll, which sits
// in bin. The game folder is found from the running exe either way.
static wchar_t g_root[MAX_PATH];    // game folder (parent of bin)
static wchar_t g_dir[MAX_PATH];     // where config, log and status live

static void TrimTo(char* dst, size_t n, const char* s, size_t len)
{
    if (len >= n) len = n - 1;
    memcpy(dst, s, len); dst[len] = 0;
}

static bool FindStr(const std::string& j, const char* key, char* out, size_t n)
{
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k); if (p == std::string::npos) return false;
    p = j.find(':', p);   if (p == std::string::npos) return false;
    size_t a = j.find('"', p); if (a == std::string::npos) return false;
    size_t b = j.find('"', a + 1); if (b == std::string::npos) return false;
    TrimTo(out, n, j.c_str() + a + 1, b - a - 1);
    return true;
}
static bool FindNum(const std::string& j, const char* key, double* out)
{
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k); if (p == std::string::npos) return false;
    p = j.find(':', p);   if (p == std::string::npos) return false;
    return sscanf_s(j.c_str() + p + 1, " %lf", out) == 1;
}
static bool FindBool(const std::string& j, const char* key, bool* out)
{
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k); if (p == std::string::npos) return false;
    p = j.find(':', p);   if (p == std::string::npos) return false;
    while (p < j.size() && (j[p] == ':' || j[p] == ' ')) p++;
    if (!j.compare(p, 4, "true"))  { *out = true;  return true; }
    if (!j.compare(p, 5, "false")) { *out = false; return true; }
    return false;
}

// First run: write the shipped default config so players have a file to edit.
// CREATE_NEW - an existing config, even a half-written one, is never touched.
static void EnsureConfig()
{
    CreateDirectoryW(g_dir, nullptr);              // standalone: XV2CombinedSupers may not exist yet
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\combined_supers.json", g_dir);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;          // already there (or unwritable)
    DWORD wrote = 0;
    WriteFile(h, kDefaultConfig, (DWORD)(sizeof(kDefaultConfig) - 1), &wrote, nullptr);
    CloseHandle(h);
}

static void LoadConfig()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\combined_supers.json", g_dir);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    char buf[8192]; DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
    CloseHandle(h);
    buf[got] = 0;
    std::string j(buf);

    Config c;
    FindBool(j, "enabled", &c.enabled);
    FindStr (j, "stats",  c.stats,  sizeof(c.stats));
    FindStr (j, "weight", c.weight, sizeof(c.weight));
    FindStr (j, "height", c.height, sizeof(c.height));
    double d;
    if (FindNum(j, "share",             &d)) c.share     = (float)d;
    if (FindNum(j, "soft_cap_points",   &d)) c.capPoints = (int)d;
    if (FindNum(j, "falloff",           &d)) c.falloff   = (float)d;
    FindBool(j, "logging", &c.logging);
    g_cfg = c;
}

// ------------------------------------------------------------------ log ---
// Silent unless "logging": true.
static void Log(const char* fmt, ...)
{
    if (!g_cfg.logging) return;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\combined_supers.log", g_dir);
    // One session per log: the first write of a run moves the last run's log to <name>.prev.log.
    static volatile LONG rotated = 0;
    if (!InterlockedExchange(&rotated, 1)) {
        wchar_t previous[MAX_PATH];
        swprintf_s(previous, L"%s\\combined_supers.prev.log", g_dir);
        MoveFileExW(path, previous, MOVEFILE_REPLACE_EXISTING);
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") || !f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "%02d:%02d:%02d  ", st.wHour, st.wMinute, st.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

// --------------------------------------------------- factor table + cave ---
// Read by the damage caves on the game's threads, written by OnStatsPushed.
#pragma pack(push, 1)
struct Slot { uint64_t ptr; float fs; float fk; };   // 16 bytes
#pragma pack(pop)
static const int  kSlots = 64;
static Slot*      g_tab  = nullptr;

static uint8_t*   g_cave = nullptr;                  // damage caves + push trampoline
static uintptr_t  g_siteA = 0, g_siteB = 0;          // the two damage multiplies
static uint8_t    g_origA[8], g_origB[8];
static uintptr_t  g_pushSites[2] = {};               // the two `call StatPush` sites
static uint8_t    g_pushOrig[2][5];
static bool       g_hooked = false;

// The game's stat push, 0x6bd70 in 1.27: (statObject, finalBlock). It
// copies the final stat block into statObject+0xb0, the live battle struct.
typedef uint64_t (*StatPushFn)(uint8_t* statObject, uint8_t* finalBlock);
static StatPushFn g_statPush = nullptr;

// Reserve executable memory within +/-2GB of `target`, so a 5-byte relative
// jmp can reach it. (Not called `near`: windows.h defines that as nothing.)
static uint8_t* AllocNear(uintptr_t target, size_t size)
{
    SYSTEM_INFO si; GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity;
    for (uintptr_t delta = gran; delta < 0x40000000; delta += gran) {
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t addr = dir ? target + delta : target - delta;
            addr -= addr % gran;
            void* p = VirtualAlloc((void*)addr, size, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
            if (p) return (uint8_t*)p;
        }
    }
    return nullptr;
}

// Damage cave. Looks rbx (the attacker's live struct) up in the factor table
// and multiplies the stat by its factor before the game's own multiply.
//
//   pushfq / push rax / push rcx      flags and the two scratch registers
//   mov  rcx, <table>
//   mov  eax, 64
// L:cmp  [rcx], rbx
//   je   F
//   add  rcx, 16
//   sub  eax, 1
//   jnz  L
//   movss xmm1,[rbx+off]              not found -> factor 1.0
//   jmp  D
// F:movss xmm1,[rbx+off]
//   mulss xmm1,[rcx+fo]
// D:mulss xmm6, xmm1
//   pop rcx / pop rax / popfq
//   jmp  back
static size_t BuildCave(uint8_t* out, uintptr_t tab, uint8_t statOff,
                        uint8_t facOff, uintptr_t caveAddr, uintptr_t retAddr)
{
    size_t i = 0;
    out[i++] = 0x9C;                                   // pushfq
    out[i++] = 0x50;                                   // push rax
    out[i++] = 0x51;                                   // push rcx
    out[i++] = 0x48; out[i++] = 0xB9;                  // mov rcx, imm64
    memcpy(out + i, &tab, 8); i += 8;
    out[i++] = 0xB8; out[i++] = (uint8_t)kSlots; out[i++] = 0; out[i++] = 0; out[i++] = 0;
    size_t L = i;
    out[i++] = 0x48; out[i++] = 0x39; out[i++] = 0x19; // cmp [rcx], rbx
    out[i++] = 0x74; size_t jeAt = i++;                // je F
    out[i++] = 0x48; out[i++] = 0x83; out[i++] = 0xC1; out[i++] = 0x10; // add rcx,16
    out[i++] = 0x83; out[i++] = 0xE8; out[i++] = 0x01; // sub eax,1
    out[i++] = 0x75; out[i++] = (uint8_t)(int8_t)((int)L - (int)(i + 1)); // jnz L
    // not found
    out[i++] = 0xF3; out[i++] = 0x0F; out[i++] = 0x10; out[i++] = 0x8B;
    out[i++] = statOff; out[i++] = 0; out[i++] = 0; out[i++] = 0;
    out[i++] = 0xEB; size_t jmpAt = i++;               // jmp D
    size_t F = i;
    out[jeAt] = (uint8_t)(int8_t)((int)F - (int)(jeAt + 1));
    out[i++] = 0xF3; out[i++] = 0x0F; out[i++] = 0x10; out[i++] = 0x8B;
    out[i++] = statOff; out[i++] = 0; out[i++] = 0; out[i++] = 0;
    out[i++] = 0xF3; out[i++] = 0x0F; out[i++] = 0x59; out[i++] = 0x49;
    out[i++] = facOff;                                 // mulss xmm1,[rcx+fo]
    size_t D = i;
    out[jmpAt] = (uint8_t)(int8_t)((int)D - (int)(jmpAt + 1));
    out[i++] = 0xF3; out[i++] = 0x0F; out[i++] = 0x59; out[i++] = 0xF1; // mulss xmm6,xmm1
    out[i++] = 0x59; out[i++] = 0x58; out[i++] = 0x9D; // pop rcx / pop rax / popfq
    out[i++] = 0xE9;                                   // jmp back
    int32_t rel = (int32_t)(retAddr - (caveAddr + i + 4));
    memcpy(out + i, &rel, 4); i += 4;
    return i;
}

// Counts matches of `pat` in [lo, hi). Guarded: returns -1 if the range faults.
static int CountIn(uintptr_t lo, uintptr_t hi, const uint8_t* pat, size_t n, uintptr_t* hit)
{
    int count = 0;
    __try {
        for (uintptr_t q = lo; q + n <= hi; ++q)
            if (memcmp((void*)q, pat, n) == 0) { *hit = q; if (++count > 1) break; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    return count;
}

// Unique match of `pat` within the image, reading only readable pages.
static uintptr_t FindUnique(uintptr_t base, size_t len, const uint8_t* pat, size_t n)
{
    uintptr_t hit = 0; int count = 0;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t addr = base; addr < base + len; ) {
        if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi))) break;
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next > base + len) next = base + len;
        DWORD rd = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                   PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & rd) && !(mbi.Protect & PAGE_GUARD)) {
            uintptr_t h = 0;
            int c = CountIn(addr, next, pat, n, &h);
            if (c < 0) return 0;
            if (c) { hit = h; count += c; if (count > 1) return 0; }
        }
        if (next <= addr) break;
        addr = next;
    }
    return count == 1 ? hit : 0;
}

// Where the rel32 call at `site` (an E8) goes.
static uintptr_t CallTarget(uintptr_t site)
{
    return site + 5 + *(int32_t*)(site + 1);
}

// Counts `prefix` + `call target` in [lo, hi). Guarded like CountIn.
static int CountCallsIn(uintptr_t lo, uintptr_t hi, uintptr_t target,
                        const uint8_t* prefix, size_t prefixLen, uintptr_t* hit)
{
    int count = 0;
    __try {
        for (uintptr_t q = lo; q + prefixLen + 5 <= hi; ++q) {
            if (memcmp((void*)q, prefix, prefixLen) != 0 || *(uint8_t*)(q + prefixLen) != 0xE8) continue;
            if (CallTarget(q + prefixLen) == target) { *hit = q + prefixLen; ++count; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    return count;
}

// The single call to `target` in the image preceded by `prefix`, or 0. Finds
// the 0x58230 call site again after a game patch: its surroundings alone
// (mov rdx,rsi / mov rcx,rax) are far too common to be unique.
static uintptr_t FindCallTo(uintptr_t base, size_t len, uintptr_t target,
                            const uint8_t* prefix, size_t prefixLen)
{
    uintptr_t hit = 0; int count = 0;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t addr = base; addr < base + len; ) {
        if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi))) break;
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next > base + len) next = base + len;
        DWORD code = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & code) && !(mbi.Protect & PAGE_GUARD)) {
            uintptr_t h = 0;
            int c = CountCallsIn(addr, next, target, prefix, prefixLen, &h);
            if (c < 0) return 0;
            if (c) { hit = h; count += c; }
        }
        if (next <= addr) break;
        addr = next;
    }
    return count == 1 ? hit : 0;
}

static void AfterStatsPushed(uint8_t* statObject, uint8_t* finalBlock,
                             float hpCurBefore, float hpMaxBefore);

// What both redirected call sites now call: run the game's push first, then
// recompute this character's factor from what it just pushed.
static uint64_t OnStatsPushed(uint8_t* statObject, uint8_t* finalBlock)
{
    float hpCurBefore = 0, hpMaxBefore = 0;
    __try {
        hpCurBefore = *(float*)(statObject + 0xb0 + OFF_HP_CUR);
        hpMaxBefore = *(float*)(statObject + 0xb0 + OFF_HP_MAX);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    uint64_t result = g_statPush(statObject, finalBlock);
    AfterStatsPushed(statObject, finalBlock, hpCurBefore, hpMaxBefore);
    return result;
}

// Patches a 5-byte rel32 call to go to `to` instead.
static bool RetargetCall(uintptr_t site, uintptr_t to)
{
    DWORD old;
    if (!VirtualProtect((void*)site, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    int32_t rel = (int32_t)(to - (site + 5));
    memcpy((void*)(site + 1), &rel, 4);
    VirtualProtect((void*)site, 5, old, &old);
    return true;
}

// Returns false without logging while the sites are not there yet - Steam's
// DRM decrypts .text some time after launch - unless this is the last try.
static bool InstallHooks(bool lastTry)
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!exe) return false;
    auto* dos = (IMAGE_DOS_HEADER*)exe;
    auto* nt  = (IMAGE_NT_HEADERS64*)((uint8_t*)exe + dos->e_lfanew);
    uintptr_t base = (uintptr_t)exe;
    size_t    size = nt->OptionalHeader.SizeOfImage;

    // Damage multiplies: mulss xmm6,[rbx+0x90] / [rbx+0x94]
    const uint8_t patA[8] = { 0xF3,0x0F,0x59,0xB3,0x90,0x00,0x00,0x00 };
    const uint8_t patB[8] = { 0xF3,0x0F,0x59,0xB3,0x94,0x00,0x00,0x00 };
    // Stat push call sites (1.27):
    //   0x4d71e  lea rdx,[rbx+0x340] / mov rcx,rax / call StatPush   (in 0x4d700)
    //   0x5822a  mov rdx,rsi / mov rcx,rax / call StatPush           (in 0x57b20,
    //            the stat recompute; rsi = its param + 0x340)
    const uint8_t patPushA[10] = { 0x48,0x8D,0x93,0x40,0x03,0x00,0x00, 0x48,0x8B,0xC8 };
    const uint8_t patPushB[6]  = { 0x48,0x8B,0xD6, 0x48,0x8B,0xC8 };

    // Known offsets for build 1.27; fall back to a signature search so a
    // game patch that moves code does not break the mod.
    // The first check stops the fixed offsets being read past the end of an
    // image that is not the game's (it happens if the DLL is loaded elsewhere).
    if (size < 0x11839D + 8) {
        if (lastTry) Log("host image too small - not DBXV2.exe?");
        return false;
    }
    uintptr_t a = base + 0x1181FD, b = base + 0x11839D;
    if (memcmp((void*)a, patA, 8) != 0) a = FindUnique(base, size, patA, 8);
    if (memcmp((void*)b, patB, 8) != 0) b = FindUnique(base, size, patB, 8);

    uintptr_t pushA = base + 0x4d728, pushB = base + 0x58230;
    if (memcmp((void*)(pushA - 10), patPushA, 10) != 0 || *(uint8_t*)pushA != 0xE8) {
        uint8_t full[11]; memcpy(full, patPushA, 10); full[10] = 0xE8;
        uintptr_t p = FindUnique(base, size, full, 11);
        pushA = p ? p + 10 : 0;
    }
    uintptr_t statPush = pushA ? CallTarget(pushA) : 0;
    if (statPush && (memcmp((void*)(pushB - 6), patPushB, 6) != 0 || *(uint8_t*)pushB != 0xE8 ||
                     CallTarget(pushB) != statPush))
        pushB = FindCallTo(base, size, statPush, patPushB, 6);
    if (!a || !b || !pushA || !pushB) {
        if (lastTry) Log("hook sites not found - game version not supported "
                         "(strike %p, ki %p, push %p/%p)", (void*)a, (void*)b,
                         (void*)pushA, (void*)pushB);
        return false;
    }
    g_siteA = a; g_siteB = b;
    memcpy(g_origA, (void*)a, 8);
    memcpy(g_origB, (void*)b, 8);
    g_pushSites[0] = pushA; g_pushSites[1] = pushB;
    memcpy(g_pushOrig[0], (void*)pushA, 5);
    memcpy(g_pushOrig[1], (void*)pushB, 5);
    g_statPush = (StatPushFn)statPush;

    g_tab = (Slot*)VirtualAlloc(nullptr, sizeof(Slot) * kSlots,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_tab) { Log("factor table alloc failed"); return false; }
    memset(g_tab, 0, sizeof(Slot) * kSlots);

    g_cave = AllocNear(base, 0x200);
    if (!g_cave) { Log("code cave alloc failed"); return false; }

    size_t lenA = BuildCave(g_cave, (uintptr_t)g_tab, OFF_STRIKE, 0x08, (uintptr_t)g_cave, a + 8);
    uint8_t* caveB = g_cave + ((lenA + 15) & ~15u);
    size_t lenB = BuildCave(caveB, (uintptr_t)g_tab, OFF_KIBLAST, 0x0C, (uintptr_t)caveB, b + 8);
    // Push trampoline: mov rax, OnStatsPushed / jmp rax. rax is free at a
    // call (volatile, and StatPush takes its arguments in rcx/rdx).
    uint8_t* tramp = caveB + ((lenB + 15) & ~15u);
    uintptr_t handler = (uintptr_t)&OnStatsPushed;
    tramp[0] = 0x48; tramp[1] = 0xB8; memcpy(tramp + 2, &handler, 8);
    tramp[10] = 0xFF; tramp[11] = 0xE0;

    struct { uintptr_t site; uint8_t* cave; } jobs[2] = { { a, g_cave }, { b, caveB } };
    for (auto& j : jobs) {
        DWORD old;
        if (!VirtualProtect((void*)j.site, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
        uint8_t p[8] = { 0xE9,0,0,0,0, 0x90,0x90,0x90 };
        int32_t rel = (int32_t)((uintptr_t)j.cave - (j.site + 5));
        memcpy(p + 1, &rel, 4);
        memcpy((void*)j.site, p, 8);
        VirtualProtect((void*)j.site, 8, old, &old);
    }
    RetargetCall(pushA, (uintptr_t)tramp);
    RetargetCall(pushB, (uintptr_t)tramp);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    g_hooked = true;
    Log("hooks installed  strike=%p kiblast=%p  push=%p/%p -> %p  cave=%p table=%p",
        (void*)a, (void*)b, (void*)pushA, (void*)pushB, (void*)statPush, g_cave, g_tab);
    return true;
}

static void RestoreBytes(uintptr_t site, const uint8_t* bytes, size_t n)
{
    DWORD old;
    if (!VirtualProtect((void*)site, n, PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy((void*)site, bytes, n);
    VirtualProtect((void*)site, n, old, &old);
}

static void RemoveHooks()
{
    if (!g_hooked) return;
    RestoreBytes(g_siteA, g_origA, 8);
    RestoreBytes(g_siteB, g_origB, 8);
    RestoreBytes(g_pushSites[0], g_pushOrig[0], 5);
    RestoreBytes(g_pushSites[1], g_pushOrig[1], 5);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    g_hooked = false;
}

// ----------------------------------------------------------- characters ---
// One CaC whose stats the game has pushed. Index i matches g_tab[i].
// "Unbuffed" = the stat before buffs, transformations and raid scaling (U in
// the maths of Solve below).
struct Fighter {
    uintptr_t live      = 0;       // the live battle struct (= damage code's rbx)
    int       race      = 0;
    int       spec      = 0;       // body type: height tier * 3 + weight tier
    int       level     = 0;
    bool      solved    = false;
    bool      fromLive  = false;   // base block did not fit; solved from the live stats
    bool      approx    = false;   // no exact fit: snapped to the nearest half point
    float     baseBlockStrike = 0, baseBlockKi = 0;    // what the game's base block held
    float     unbuffedStrike = 0, unbuffedKi = 0;      // solved stats at M = 1
    int       strikeHalfPts = 0, kiHalfPts = 0;        // investment above base, in HALF points
    float     liveStrike = 0, liveKi = 0;              // after the push
    float     hpCur = 0, hpMax = 0;
    float     strikeFactor = 1, kiFactor = 1;
    uint32_t  pushes    = 0;
};
static Fighter g_fighters[kSlots];
static int     g_nextVictim = 0;          // round-robin replacement when full
static SRWLOCK g_lock = SRWLOCK_INIT;     // g_fighters + g_cfg between the game and our thread
static volatile LONG g_statusDirty = 0;
static volatile LONG g_pushCount = 0;

static bool Readable(uintptr_t p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD ok = PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
    if (!(mbi.Protect & ok) || (mbi.Protect & PAGE_GUARD)) return false;
    return (uintptr_t)mbi.BaseAddress + mbi.RegionSize >= p + n;
}

// A CaC's live battle struct (NPCs have other race ids and are left alone).
static bool LooksLikeCaC(uintptr_t p)
{
    if (!Readable(p, 0x200)) return false;
    if (*(uint32_t*)(p + OFF_MARK) != 0x3D) return false;
    int race = *(int*)(p + OFF_RACE);
    int spec = *(int*)(p + OFF_SPEC);
    if (race < 100 || race > 107) return false;
    if (spec < 0 || spec > 11) return false;
    float s = *(float*)(p + OFF_STRIKE), k = *(float*)(p + OFF_KIBLAST);
    // A Lv1 Namekian has strike/ki 0.2992, so the floor stays low.
    return s > 0.05f && s < 30.0f && k > 0.05f && k < 30.0f;
}

// ---------------------------------------------------------------- solve ---
// live = M * (base + step*k), k an integer number of half points, M a multiple
// of 0.025. base = G(level)*(1+psc), step = G(level)/1000. Two stats share one
// M, which pins it down.
static bool SolveAt(float m, float ls, float lk, float Bs, float Bk, float step, float tol,
                    float* Us, float* Uk, int* pks, int* pkk)
{
    float fs = (ls / m - Bs) / step;
    float fk = (lk / m - Bk) / step;
    if (fs < 2 * EXLO || fs > 2 * EXHI || fk < 2 * EXLO || fk > 2 * EXHI) return false;
    int rs = (int)floorf(fs + 0.5f), rk = (int)floorf(fk + 0.5f);
    if (fabsf(fs - rs) >= tol || fabsf(fk - rk) >= tol) return false;
    *pks = rs; *pkk = rk;
    *Us = Bs + step * rs; *Uk = Bk + step * rk;
    return true;
}

// Buffs first (M = 1.0 .. 3.0, smallest wins). Only if nothing fits,
// multipliers BELOW 1 (0.975 .. 0.25): raids halve the stats of low-level
// characters (M = 0.5 measured). F does not depend on M, so these characters
// get their normal factor.
//
// Below 1 the FIRST fit is not safe: every PSC value is a multiple of 0.025,
// so multipliers in ratios like 4:5 map one lattice point exactly onto another
// (M 0.5 with +30/+80 pts also fits as M 0.625 with -118.5/-78.5). The true
// one is the smallest investment - a low-level character cannot be 118 points
// below base - so all fits are compared and that one wins.
static bool Solve(float ls, float lk, float Bs, float Bk, float step, float tol,
                  float* Us, float* Uk, int* pks, int* pkk)
{
    for (int i = 0; i <= 80; ++i)                     // 1.000 .. 3.000
        if (SolveAt(1.0f + i * MGRID, ls, lk, Bs, Bk, step, tol, Us, Uk, pks, pkk)) return true;

    bool any = false; int bestCost = 1 << 30;
    for (int i = 1; i <= 30; ++i) {                   // 0.975 .. 0.250
        float us, uk; int ks, kk;
        if (!SolveAt(1.0f - i * MGRID, ls, lk, Bs, Bk, step, tol, &us, &uk, &ks, &kk)) continue;
        int cost = abs(ks) + abs(kk);
        if (cost < bestCost) {
            bestCost = cost; any = true;
            *Us = us; *Uk = uk; *pks = ks; *pkk = kk;
        }
    }
    return any;
}

static float Blend(const char* mode, float own, float other, float share)
{
    if (!strcmp(mode, "vanilla")) return own;
    if (!strcmp(mode, "highest")) return own + share * (other > own ? other - own : 0.0f);
    return own + share * other;                       // additive
}

static float SoftCap(float x, float cap, float fall)
{
    return x <= cap ? x : cap + (x - cap) * fall;
}

// Factors for a solved fighter under the current config.
static void ComputeFactors(Fighter& f)
{
    f.strikeFactor = f.kiFactor = 1.0f;
    if (!f.solved || !g_cfg.enabled) return;
    const RaceRow& raceRow = kRacesLive[f.race - 100];
    int weightTier = f.spec % 3;
    const float G = kLevelG[f.level - 1];
    float baseStrike = G * (1.0f + raceRow.strike[weightTier]), baseKi = G * (1.0f + raceRow.ki[weightTier]);

    float ownStrike = f.unbuffedStrike - baseStrike, ownKi = f.unbuffedKi - baseKi;     // points + gear, stat units
    float floorStrike = baseStrike, floorKi = baseKi, weightSpanStrike = 0, weightSpanKi = 0;
    if (strcmp(g_cfg.weight, "vanilla") != 0) {
        float bestStrikeTier = raceRow.strike[0], bestKiTier = raceRow.ki[0];
        float lightStrikeTier = raceRow.strike[0], lightKiTier = raceRow.ki[0];
        for (int i = 1; i < 3; ++i) {
            if (raceRow.strike[i] > bestStrikeTier) bestStrikeTier = raceRow.strike[i];
            if (raceRow.ki[i]     > bestKiTier) bestKiTier = raceRow.ki[i];
            if (raceRow.strike[i] < lightStrikeTier) lightStrikeTier = raceRow.strike[i];
            if (raceRow.ki[i]     < lightKiTier) lightKiTier = raceRow.ki[i];
        }
        // build the target from the lightest tier's floor, plus the full
        // span to the best tier
        floorStrike = G * (1.0f + lightStrikeTier); floorKi = G * (1.0f + lightKiTier);
        weightSpanStrike = G * (bestStrikeTier - lightStrikeTier); weightSpanKi = G * (bestKiTier - lightKiTier);
    }

    float sharedStrike = Blend(g_cfg.stats, ownStrike, ownKi, g_cfg.share);
    float sharedKi = Blend(g_cfg.stats, ownKi, ownStrike, g_cfg.share);
    float cap = g_cfg.capPoints * (G / 500.0f);
    float targetStrike = floorStrike + weightSpanStrike + SoftCap(sharedStrike, cap, g_cfg.falloff);
    float targetKi = floorKi + weightSpanKi + SoftCap(sharedKi, cap, g_cfg.falloff);
    f.strikeFactor = targetStrike / f.unbuffedStrike;
    f.kiFactor = targetKi / f.unbuffedKi;
}

// Recovers the fighter's unbuffed stats. The game's base block (the push's
// finalBlock - 0x340, multiplied by the buff block to make finalBlock in
// 0x57b20) should hold them directly, so M = 1 there; the live stats are the
// fallback, solved with M free as before.
static void SolveFighter(Fighter& f)
{
    const RaceRow& raceRow = kRacesLive[f.race - 100];
    int weightTier = f.spec % 3;
    const float G    = kLevelG[f.level - 1];
    const float step = G / 1000.0f;
    float baseStrike = G * (1.0f + raceRow.strike[weightTier]), baseKi = G * (1.0f + raceRow.ki[weightTier]);

    f.solved = f.approx = f.fromLive = false;
    float us, uk; int ks, kk;
    if (SolveAt(1.0f, f.baseBlockStrike, f.baseBlockKi, baseStrike, baseKi, step, kTol, &us, &uk, &ks, &kk)) {
        f.solved = true;
    } else if (Solve(f.liveStrike, f.liveKi, baseStrike, baseKi, step, kTol, &us, &uk, &ks, &kk)) {
        f.solved = true; f.fromLive = true;
    } else {
        // Nothing on the lattice. Assume the base block is unbuffed and snap
        // to the nearest half point - every client rounds the same way.
        ks = (int)floorf((f.baseBlockStrike - baseStrike) / step + 0.5f);
        kk = (int)floorf((f.baseBlockKi - baseKi) / step + 0.5f);
        if (ks >= 2 * EXLO && ks <= 2 * EXHI && kk >= 2 * EXLO && kk <= 2 * EXHI) {
            us = baseStrike + step * ks; uk = baseKi + step * kk;
            f.solved = true; f.approx = true;
        }
    }
    if (f.solved) {
        f.unbuffedStrike = us; f.unbuffedKi = uk;
        f.strikeHalfPts = ks; f.kiHalfPts = kk;
    }
}

// Publishes fighter i's factors. Factors before the pointer, so the cave
// never pairs a new pointer with a previous occupant's factors.
static void Publish(int i)
{
    if (!g_tab) return;
    if ((uint64_t)g_fighters[i].live != g_tab[i].ptr) g_tab[i].ptr = 0;
    g_tab[i].fs  = g_fighters[i].strikeFactor;
    g_tab[i].fk  = g_fighters[i].kiFactor;
    g_tab[i].ptr = (uint64_t)g_fighters[i].live;
}

// ---------------------------------------------------------------- height ---
// Height: health only. Max HP = 1000*G*(1 + psc + points/500), so the gap to
// the best Height tier is exactly 1000*G*(best - own) at any level, HEA or
// gear. Each push resets max HP to the game's value, so the bonus is simply
// added after every push - never twice. Current HP only rises with it if it
// was full before the push (or the game just filled it), so this never heals
// anyone mid-fight.
static void ApplyHeight(const Fighter& f, uintptr_t live, float hpCurBefore, float hpMaxBefore)
{
    if (!strcmp(g_cfg.height, "vanilla")) return;
    const RaceRow& raceRow = kRacesLive[f.race - 100];
    float best = raceRow.health[0];
    for (int i = 1; i < 4; ++i) if (raceRow.health[i] > best) best = raceRow.health[i];
    float bonus = 1000.0f * kLevelG[f.level - 1] * (best - raceRow.health[f.spec / 3]);
    float* hpMax = (float*)(live + OFF_HP_MAX);
    float* hpCur = (float*)(live + OFF_HP_CUR);
    float gameMaxHp = *hpMax;
    if (bonus <= 0.5f || gameMaxHp <= 1.0f || gameMaxHp >= 1.0e6f) return;
    bool wasFull = hpMaxBefore > 1.0f && hpCurBefore >= hpMaxBefore - 0.5f;
    bool gameFilled = *hpCur >= gameMaxHp - 0.5f;
    *hpMax = gameMaxHp + bonus;
    if (wasFull || gameFilled) *hpCur = gameMaxHp + bonus;
}

// Updates the fighter behind `live` after a push; g_lock held. Bounded: a
// 64-entry lookup and the solver's fixed 111-step loop. Returns true for a
// fighter seen for the first time (logged by the caller, outside the lock).
static bool UpdateFighter(uintptr_t live, uint8_t* baseBlock, float hpCurBefore, float hpMaxBefore)
{
    int index = -1;
    for (int i = 0; i < kSlots; ++i) if (g_fighters[i].live == live) { index = i; break; }

    if (!LooksLikeCaC(live)) {
        // Not a CaC (any more): make sure no stale factor sticks to it.
        if (index >= 0) { g_fighters[index] = Fighter(); if (g_tab) g_tab[index].ptr = 0; }
        return false;
    }
    if (index < 0) {
        for (int i = 0; i < kSlots; ++i) if (!g_fighters[i].live) { index = i; break; }
        if (index < 0) { index = g_nextVictim; g_nextVictim = (g_nextVictim + 1) % kSlots; }
        g_fighters[index] = Fighter();
        if (g_tab) g_tab[index].ptr = 0;
    }
    Fighter& f = g_fighters[index];
    bool isNew = !f.live;
    int level = *(int*)(live + OFF_LEVEL);
    f.live  = live;
    f.race  = *(int*)(live + OFF_RACE);
    f.spec  = *(int*)(live + OFF_SPEC);
    f.level = level;
    f.liveStrike = *(float*)(live + OFF_STRIKE);
    f.liveKi     = *(float*)(live + OFF_KIBLAST);
    bool baseReadable = Readable((uintptr_t)baseBlock, 0x100);
    f.baseBlockStrike = baseReadable ? *(float*)(baseBlock + OFF_STRIKE) : 0;
    f.baseBlockKi     = baseReadable ? *(float*)(baseBlock + OFF_KIBLAST) : 0;
    f.pushes++;

    if (level >= 1 && level <= kMaxLevel) {
        SolveFighter(f);
        ComputeFactors(f);
        ApplyHeight(f, live, hpCurBefore, hpMaxBefore);
    } else {
        f.solved = false;                             // outside the level table: vanilla
        f.strikeFactor = f.kiFactor = 1.0f;
    }
    f.hpCur = *(float*)(live + OFF_HP_CUR);
    f.hpMax = *(float*)(live + OFF_HP_MAX);
    Publish(index);
    return isNew;
}

// Runs on the game's thread right after every stat push. A fault in here
// must never take the game down: the character just stays vanilla.
static void AfterStatsPushed(uint8_t* statObject, uint8_t* finalBlock,
                             float hpCurBefore, float hpMaxBefore)
{
    InterlockedIncrement(&g_pushCount);
    uintptr_t live = (uintptr_t)statObject + 0xb0;      // 0x6bd70 copies into obj+0xb0
    uint8_t*  baseBlock = finalBlock - 0x340;           // 0x57b20: final = mul(+0x340) * base(+0)
    bool isNew = false;
    AcquireSRWLockExclusive(&g_lock);
    __try { isNew = UpdateFighter(live, baseBlock, hpCurBefore, hpMaxBefore); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    ReleaseSRWLockExclusive(&g_lock);
    g_statusDirty = 1;
    if (isNew) Log("new fighter %llX (details in the status file)", (unsigned long long)live);
}

// --------------------------------------------------------------- status ---
// Rewritten only when a push changed something, at most once a second.
static void WriteStatus()
{
    if (!g_cfg.logging) return;
    static Fighter copy[kSlots];
    Config cfg;
    AcquireSRWLockShared(&g_lock);
    memcpy(copy, g_fighters, sizeof(copy));
    cfg = g_cfg;
    ReleaseSRWLockShared(&g_lock);

    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\combined_supers_status.txt", g_dir);
    FILE* out = nullptr;
    if (_wfopen_s(&out, path, L"w") || !out) return;
    fprintf(out, "dll  %s  stats=%s  weight=%s  height=%s  share=%.2f  cap=%dpts  falloff=%.2f\n",
            cfg.enabled ? "ENABLED" : "disabled", cfg.stats, cfg.weight,
            cfg.height, cfg.share, cfg.capPoints, cfg.falloff);
    fprintf(out, "stat pushes seen: %ld\n", g_pushCount);
    fprintf(out, "characters (src: base = game's unbuffed block, live = solved from live stats, ~ = snapped):\n");
    int shown = 0;
    for (auto& f : copy) {
        if (!f.live) continue;
        ++shown;
        const RaceRow& raceRow = kRacesLive[f.race - 100];
        if (!f.solved) {
            fprintf(out, "  %llX %-9s h%d w%d  Lv%d  hp=%.0f/%.0f  base=%.4f/%.4f  live=%.4f/%.4f  UNSOLVED  pushes=%u\n",
                    (unsigned long long)f.live, raceRow.name, f.spec / 3, f.spec % 3, f.level,
                    f.hpCur, f.hpMax, f.baseBlockStrike, f.baseBlockKi, f.liveStrike, f.liveKi, f.pushes);
            continue;
        }
        fprintf(out, "  %llX %-9s h%d w%d  Lv%d  hp=%.0f/%.0f  tot=%+.1f/%+.1f pts  U=%.4f/%.4f (%s)"
                     "  live=%.4f/%.4f  M=%.3f  F=%.4f/%.4f  pushes=%u\n",
                (unsigned long long)f.live, raceRow.name, f.spec / 3, f.spec % 3, f.level,
                f.hpCur, f.hpMax, f.strikeHalfPts / 2.0, f.kiHalfPts / 2.0, f.unbuffedStrike, f.unbuffedKi,
                f.approx ? "~" : f.fromLive ? "live" : "base", f.liveStrike, f.liveKi,
                f.liveStrike / f.unbuffedStrike, f.strikeFactor, f.kiFactor, f.pushes);
    }
    if (!shown) fprintf(out, "  (none yet - the game pushes stats when a fight loads)\n");
    fclose(out);
}

// --------------------------------------------------------------- thread ---
// Only waits for the DRM, then idles: a once-a-second check of the config's
// timestamp and the status file. All the real work happens in AfterStatsPushed.
static HANDLE   g_thread = nullptr;
static volatile bool g_run = false;

static FILETIME ConfigTime()
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\combined_supers.json", g_dir);
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    GetFileAttributesExW(path, GetFileExInfoStandard, &data);
    return data.ftLastWriteTime;
}

// Loads the config and re-applies it to every known fighter.
static void ReloadConfig()
{
    AcquireSRWLockExclusive(&g_lock);
    LoadConfig();
    for (int i = 0; i < kSlots; ++i) {
        if (!g_fighters[i].live) continue;
        ComputeFactors(g_fighters[i]);
        Publish(i);
    }
    ReleaseSRWLockExclusive(&g_lock);
    g_statusDirty = 1;
}

// The Attributes screen's Supers box and the radar chart's link
// (menu_scripts.cpp), on while the supers are combined. The standalone
// winmm.dll has no loader to serve them.
static void ApplyMenuScripts()
{
#if !COMBINED_STANDALONE
    char mode = '0';
    if (g_cfg.enabled && strcmp(g_cfg.stats, "vanilla") != 0) mode = strcmp(g_cfg.stats, "highest") == 0 ? 'h' : 'a';
    std::string line = MenuScripts_Apply(mode);
    if (!line.empty()) Log("%s", line.c_str());
#endif
}

static DWORD WINAPI ThreadMain(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    EnsureConfig();
    LoadConfig();
    Log("--- Combined Super Stats v" MOD_VERSION " loaded ---");
    ApplyMenuScripts();  // before the game first opens the menus

    // Prefer the game's own stat tables over the ones baked into this mod, so
    // a game update - or a mod that edits the PSC - is followed automatically.
    {
        char why[128] = "";
        bool ok = Psc_Load(g_root, why, sizeof(why));
        Log("stat tables: %s (%s)", ok ? "game PSC" : "built-in fallback", why);
    }
    // Steam's DRM decrypts the game code some time after launch; keep trying
    // for a minute before concluding this game version is unsupported. The
    // hooks are in before the title screen, long before the first fight.
    bool hooked = false;
    for (int t = 0; t < 120 && g_run && !hooked; ++t) {
        hooked = InstallHooks(t == 119);
        if (!hooked) Sleep(500);
    }
    if (!hooked) return 0;

    FILETIME lastConfig = ConfigTime();
    while (g_run) {
        Sleep(1000);
        FILETIME now = ConfigTime();
        if (CompareFileTime(&now, &lastConfig) != 0) {
            lastConfig = now;
            ReloadConfig();
            Log("config reloaded");
            ApplyMenuScripts();
        }
        if (InterlockedExchange(&g_statusDirty, 0)) WriteStatus();
    }
    return 0;
}

void Mod_Start(HMODULE self)
{
    GetModuleFileNameW(nullptr, g_root, MAX_PATH); // <game>\bin\DBXV2.exe
    for (int i = 0; i < 2; ++i)
        if (wchar_t* slash = wcsrchr(g_root, L'\\')) *slash = 0;
#if COMBINED_STANDALONE
    (void)self;
    swprintf_s(g_dir, L"%s\\XV2CombinedSupers", g_root);
#else
    GetModuleFileNameW(self, g_dir, MAX_PATH);     // ...\mods\CombinedSupers\CombinedSupers.dll
    if (wchar_t* slash = wcsrchr(g_dir, L'\\')) *slash = 0;
#endif
    g_run = true;
    g_thread = CreateThread(nullptr, 0, ThreadMain, nullptr, 0, nullptr);
}

void Mod_Stop()
{
    g_run = false;
    RemoveHooks();
}

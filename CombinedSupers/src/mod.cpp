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
//    * A background thread finds every CaC in the scene, works out what their
//      stats would be with no buffs, computes the factor, and publishes it to
//      a table the caves read.
//    * Every input comes from data all players share (race, body type, the
//      live stats), so two people running this compute identical numbers.
// ---------------------------------------------------------------------------
#include "mod.h"
#include "tables.h"
#include "level_table.h"
#include "default_config.h"
#include "psc.h"
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <psapi.h>
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
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") || !f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "%02d:%02d:%02d  ", st.wHour, st.wMinute, st.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

// --------------------------------------------------- factor table + cave ---
#pragma pack(push, 1)
struct Slot { uint64_t ptr; float fs; float fk; };   // 16 bytes
#pragma pack(pop)
static const int  kSlots = 64;
static Slot*      g_tab  = nullptr;

// Ring of the rbx values the caves actually see, as in combined.lua. The
// memory scan finds copies of a character that the damage code never uses;
// the ring is what identifies the real attacker struct.
//   +0x000  32 entries x 16 bytes (pointer, padding)
//   +0x200  write index       +0x204  strike hits     +0x208  ki blast hits
static const int  kRing  = 32;
static uint8_t*   g_ring = nullptr;
static const uint32_t RING_IDX = 0x200, RING_HITS_A = 0x204, RING_HITS_B = 0x208;
static uint8_t*   g_cave = nullptr;
static uintptr_t  g_siteA = 0, g_siteB = 0;
static uint8_t    g_origA[8], g_origB[8];
static bool       g_hooked = false;

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

// Cave body. Records rbx in the ring, looks it up in the factor table and
// multiplies the stat by its factor before the game's own multiply.
//
//   pushfq / push rax / push rcx      flags and the two scratch registers
//   mov  rcx, <ring>
//   mov  eax, [rcx+0x200]             ring write index
//   and  eax, 31
//   shl  eax, 4
//   mov  [rcx+rax], rbx
//   inc  dword [rcx+0x200]
//   inc  dword [rcx+hitsOff]
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
                        uint8_t facOff, uintptr_t caveAddr, uintptr_t retAddr,
                        uintptr_t ring, uint32_t hitsOff)
{
    size_t i = 0;
    auto d32 = [&](uint32_t v) { memcpy(out + i, &v, 4); i += 4; };
    out[i++] = 0x9C;                                   // pushfq
    out[i++] = 0x50;                                   // push rax
    out[i++] = 0x51;                                   // push rcx
    out[i++] = 0x48; out[i++] = 0xB9;                  // mov rcx, imm64
    memcpy(out + i, &ring, 8); i += 8;
    out[i++] = 0x8B; out[i++] = 0x81; d32(RING_IDX);   // mov eax,[rcx+idx]
    out[i++] = 0x83; out[i++] = 0xE0; out[i++] = kRing - 1; // and eax,31
    out[i++] = 0xC1; out[i++] = 0xE0; out[i++] = 0x04; // shl eax,4
    out[i++] = 0x48; out[i++] = 0x89; out[i++] = 0x1C; out[i++] = 0x01; // mov [rcx+rax],rbx
    out[i++] = 0xFF; out[i++] = 0x81; d32(RING_IDX);   // inc dword [rcx+idx]
    out[i++] = 0xFF; out[i++] = 0x81; d32(hitsOff);    // inc dword [rcx+hits]
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

    const uint8_t patA[8] = { 0xF3,0x0F,0x59,0xB3,0x90,0x00,0x00,0x00 };
    const uint8_t patB[8] = { 0xF3,0x0F,0x59,0xB3,0x94,0x00,0x00,0x00 };

    // Known offsets for build 1.26.01; fall back to a signature search so a
    // game patch that moves code does not break the mod. Both signatures are
    // unique across the whole image.
    // The first check stops the fixed offsets being read past the end of an
    // image that is not the game's (it happens if the DLL is loaded elsewhere).
    if (size < 0x1182DD + 8) {
        if (lastTry) Log("host image too small - not DBXV2.exe?");
        return false;
    }
    uintptr_t a = base + 0x11813D, b = base + 0x1182DD;
    if (memcmp((void*)a, patA, 8) != 0) a = FindUnique(base, size, patA, 8);
    if (memcmp((void*)b, patB, 8) != 0) b = FindUnique(base, size, patB, 8);
    if (!a || !b) {
        if (lastTry) Log("hook sites not found - game version not supported");
        return false;
    }
    g_siteA = a; g_siteB = b;
    memcpy(g_origA, (void*)a, 8);
    memcpy(g_origB, (void*)b, 8);

    g_tab = (Slot*)VirtualAlloc(nullptr, sizeof(Slot) * kSlots,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_tab) { Log("factor table alloc failed"); return false; }
    memset(g_tab, 0, sizeof(Slot) * kSlots);
    g_ring = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_ring) { Log("ring alloc failed"); return false; }   // zero-filled by VirtualAlloc

    g_cave = AllocNear(base, 0x200);
    if (!g_cave) { Log("code cave alloc failed"); return false; }

    size_t lenA = BuildCave(g_cave, (uintptr_t)g_tab, OFF_STRIKE, 0x08,
                            (uintptr_t)g_cave, a + 8, (uintptr_t)g_ring, RING_HITS_A);
    uint8_t* caveB = g_cave + ((lenA + 15) & ~15u);
    BuildCave(caveB, (uintptr_t)g_tab, OFF_KIBLAST, 0x0C, (uintptr_t)caveB, b + 8,
              (uintptr_t)g_ring, RING_HITS_B);

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
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    g_hooked = true;
    Log("hooks installed  strike=%p kiblast=%p  cave=%p table=%p",
        (void*)a, (void*)b, g_cave, g_tab);
    return true;
}

static void RemoveHooks()
{
    if (!g_hooked) return;
    DWORD old;
    if (VirtualProtect((void*)g_siteA, 8, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy((void*)g_siteA, g_origA, 8);
        VirtualProtect((void*)g_siteA, 8, old, &old);
    }
    if (VirtualProtect((void*)g_siteB, 8, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy((void*)g_siteB, g_origB, 8);
        VirtualProtect((void*)g_siteB, 8, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    g_hooked = false;
}

// ----------------------------------------------------------- characters ---
// One character struct being tracked. "Unbuffed" = the stat with the battle
// multiplier M taken out (U in the maths of Solve below).
struct Char {
    uintptr_t addr      = 0;       // the struct in game memory
    int       race      = 0;
    int       spec      = 0;       // body type: height tier * 3 + weight tier
    int       level     = 0;       // min tracking restarts if this changes
    float     minStrike = 0, minKi = 0;            // smallest stats seen = unbuffed
    int       samples   = 0;
    bool      solved    = false;
    float     unbuffedStrike = 0, unbuffedKi = 0;  // solved stats at M = 1
    int       strikeHalfPts = 0, kiHalfPts = 0;    // investment above base, in HALF points
    bool      approx    = false;   // no exact fit: snapped to the nearest half point
    bool      hit       = false;   // seen in the ring = the struct the damage code uses
};
static std::vector<Char> g_chars;

static bool Readable(uintptr_t p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD ok = PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
    if (!(mbi.Protect & ok) || (mbi.Protect & PAGE_GUARD)) return false;
    return (uintptr_t)mbi.BaseAddress + mbi.RegionSize >= p + n;
}

// The 36-byte fingerprint combined.lua scans for, starting at +0x04:
//   ?? 00 00 00 | 00 00 00 00 | ?? 00 00 00 | ?? x4 | ?? x8 | 05 00 00 00 | 05 00 00 00 | 3D 00 00 00
// Everything this DLL writes (HP for Height) goes through this check, so it
// must be at least as strict as the Lua's.
static bool Fingerprint(uintptr_t p)
{
    return *(uint32_t*)(p + OFF_MARK) == 0x3D &&
           *(uint32_t*)(p + 0x20) == 5 && *(uint32_t*)(p + 0x1C) == 5 &&
           *(uint32_t*)(p + 0x08) == 0 &&
           *(uint32_t*)(p + 0x04) < 0x100 && *(uint32_t*)(p + 0x0C) < 0x100;
}

// combined.lua's per-tick check. Enough for pointers taken from the ring,
// which the game itself has already shown to be the attacker.
static bool LooksLikeCharLoose(uintptr_t p)
{
    if (!Readable(p, 0x200)) return false;
    if (*(uint32_t*)(p + OFF_MARK) != 0x3D) return false;
    int race = *(int*)(p + OFF_RACE);
    int spec = *(int*)(p + OFF_SPEC);
    if (race < 100 || race > 107) return false;
    if (spec < 0 || spec > 11) return false;
    float s = *(float*)(p + OFF_STRIKE), k = *(float*)(p + OFF_KIBLAST);
    // Lower bound was 0.5 (from combined.lua), which rejects low-level
    // characters: a Lv1 Namekian has strike/ki 0.2992.
    return s > 0.05f && s < 30.0f && k > 0.05f && k < 30.0f;
}

// For pointers found by scanning: the full fingerprint as well.
static bool LooksLikeChar(uintptr_t p)
{
    return LooksLikeCharLoose(p) && Fingerprint(p);
}

static bool Valid(const struct Char& c)
{
    return c.hit ? LooksLikeCharLoose(c.addr) : LooksLikeChar(c.addr);
}

// Adopt every pointer the caves have seen. Guarded like the scan.
static int ReadRing(uintptr_t* out)
{
    int n = 0;
    __try {
        for (int i = 0; i < kRing; ++i) {
            uintptr_t p = *(uintptr_t*)(g_ring + i * 16);
            if (p) out[n++] = p;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return n;
}

static void AddChar(uintptr_t q, bool hit);

static void AdoptRing()
{
    if (!g_ring) return;
    uintptr_t ptrs[kRing];
    int n = ReadRing(ptrs);
    for (int i = 0; i < n; ++i) {
        uintptr_t q = ptrs[i];
        Char* known = nullptr;
        for (auto& c : g_chars) if (c.addr == q) { known = &c; break; }
        if (known) { known->hit = true; continue; }
        if (!LooksLikeCharLoose(q)) continue;
        if (g_chars.size() >= (size_t)kSlots) {
            // Full - raids spawn pools of identical copies. A struct the damage
            // code actually used outranks any scan-only copy: evict one.
            size_t victim = g_chars.size();
            for (size_t j = 0; j < g_chars.size(); ++j)
                if (!g_chars[j].hit) { victim = j; break; }
            if (victim == g_chars.size()) continue;       // all real attackers
            g_chars.erase(g_chars.begin() + victim);
        }
        AddChar(q, true);
    }
}

// Collects fingerprint hits for structs starting in [lo, startsEnd) that fit
// whole below readEnd (the end of the memory region). Guarded: the game can
// free a region while we walk it, and that must cost one region, not the
// whole scan.
static int ScanRegion(uintptr_t lo, uintptr_t startsEnd, uintptr_t readEnd,
                      uintptr_t* out, int max)
{
    int n = 0;
    __try {
        for (uintptr_t q = lo; q < startsEnd && q + 0x200 <= readEnd && n < max; q += 4) {
            if (*(uint32_t*)(q + OFF_MARK) != 0x3D) continue;
            if (Fingerprint(q)) out[n++] = q;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return n;
}

// Graphics-driver memory (write-combined or uncached) never holds a character,
// and reading it bypasses the CPU cache: an online session had 101 MB of it
// in the full scan's path, which took 2.6 s (ClaudeTools\regionmap.py, 2026-10-11).
static const DWORD kUncachedMemory = PAGE_WRITECOMBINE | PAGE_NOCACHE;

// Full-scan pacing: a 1 ms pause after every 32 MB read, so a ~3.7 GB pass is
// spread out instead of one burst competing with the game for memory bandwidth.
static const size_t kScanBurstBytes = (size_t)32 << 20;
static size_t g_scanSincePause = 0;

// Full scan of one region: only the pages already in the game's working set.
// Reading a paged-out page would fault it back in from disk, and a live
// character struct is always resident. Pages are checked 4 MB at a time.
static int ScanResident(uintptr_t lo, uintptr_t hi, uintptr_t* out, int max)
{
    const size_t kPage = 0x1000;
    const size_t kPagesPerQuery = 1024;
    static PSAPI_WORKING_SET_EX_INFORMATION pages[kPagesPerQuery];   // scan thread only
    int n = 0;
    for (uintptr_t chunk = lo; chunk < hi && n < max; ) {
        size_t count = (std::min)((size_t)(hi - chunk) / kPage, kPagesPerQuery);
        for (size_t i = 0; i < count; ++i)
            pages[i].VirtualAddress = (void*)(chunk + i * kPage);
        if (!QueryWorkingSetEx(GetCurrentProcess(), pages, (DWORD)(count * sizeof(pages[0])))) {
            n += ScanRegion(chunk, chunk + count * kPage, hi, out + n, max - n);  // unknown: scan all
        } else {
            for (size_t i = 0; i < count; ) {              // each run of resident pages
                if (!pages[i].VirtualAttributes.Valid) { ++i; continue; }
                size_t runEnd = i;
                while (runEnd < count && pages[runEnd].VirtualAttributes.Valid) ++runEnd;
                n += ScanRegion(chunk + i * kPage, chunk + runEnd * kPage, hi, out + n, max - n);
                i = runEnd;
            }
        }
        chunk += count * kPage;
        g_scanSincePause += count * kPage;
        if (g_scanSincePause >= kScanBurstBytes) { Sleep(1); g_scanSincePause = 0; }
    }
    return n;
}

// ---- arenas: the allocations character structs have been found in.
// A full scan of the process takes ~2s (2.6 GB), so it can only run every
// ~40s. But a fight's structs - the live one and its copies - come out of one
// ~16 MB allocation, so once a character has been seen, that allocation alone
// is rescanned every second and a new fight's struct is found during the
// intro, before its first super.
struct Arena { uintptr_t base = 0; };
static const int kArenas = 8;
static Arena g_arenas[kArenas];
static int   g_arenaNext = 0;

static void NoteArena(uintptr_t p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)p, &mbi, sizeof(mbi)) || !mbi.AllocationBase) return;
    uintptr_t base = (uintptr_t)mbi.AllocationBase;
    for (auto& a : g_arenas) if (a.base == base) return;
    g_arenas[g_arenaNext].base = base;
    g_arenaNext = (g_arenaNext + 1) % kArenas;
}

static void AddChar(uintptr_t q, bool hit)
{
    Char c; c.addr = q; c.hit = hit;
    c.race = *(int*)(q + OFF_RACE);
    c.spec = *(int*)(q + OFF_SPEC);
    g_chars.push_back(c);
    NoteArena(q);
}

// Walk committed private memory in [lo, hi) - or, with `alloc` set, only the
// regions belonging to that one allocation - looking for the fingerprint.
// Runs on a below-normal background thread, but still competes with the game
// for memory bandwidth - hence the uncached-memory skip and ScanResident.
static int ScanRange(uintptr_t lo, uintptr_t hi, uintptr_t alloc)
{
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr = lo;
    int found = 0;
    uintptr_t hits[kSlots];
    while (addr < hi && VirtualQuery((void*)addr, &mbi, sizeof(mbi)) &&
           g_chars.size() < (size_t)kSlots) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (alloc && (uintptr_t)mbi.AllocationBase != alloc) break;   // left the arena
        bool usable = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
                      (mbi.Protect & PAGE_READWRITE) && !(mbi.Protect & PAGE_GUARD) &&
                      !(mbi.Protect & kUncachedMemory) &&
                      mbi.RegionSize >= 0x1000 && mbi.RegionSize <= 0x10000000;
        if (usable) {
            // a known arena is small and hot: read it straight through
            uintptr_t base = (uintptr_t)mbi.BaseAddress;
            int n = alloc ? ScanRegion(base, next, next, hits, kSlots)
                          : ScanResident(base, next, hits, kSlots);
            for (int i = 0; i < n && g_chars.size() < (size_t)kSlots; ++i) {
                uintptr_t q = hits[i];
                if (!LooksLikeChar(q)) continue;
                bool known = false;
                for (auto& c : g_chars) if (c.addr == q) { known = true; break; }
                if (known) continue;
                AddChar(q, false);
                ++found;
            }
        }
        if (next <= addr) break;
        addr = next;
    }
    return found;
}

static double g_scanMs = 0;          // duration of the last full scan
static int    g_scanEveryTicks = 0;  // current full-scan interval
static double g_arenaMs = 0;         // duration of the last arena rescan

static void ScanForCharacters() { ScanRange(0x10000, (uintptr_t)1 << 47, 0); }

static void ScanArenas()
{
    for (auto& a : g_arenas) {
        if (!a.base) continue;
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)a.base, &mbi, sizeof(mbi)) ||
            mbi.State == MEM_FREE || (uintptr_t)mbi.AllocationBase != a.base) {
            a.base = 0;                                   // released - forget it
            continue;
        }
        ScanRange(a.base, (uintptr_t)1 << 47, a.base);
    }
}

// ---------------------------------------------------------------- solve ---
// live = M * (base + step*k), k an integer number of half points, M a multiple
// of 0.025. base = G(level)*(1+psc), step = G(level)/1000. Two stats share one
// M, which pins it down. Fed the minimum seen, where M is almost certainly 1,
// so the smallest valid M is the right one.
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

// Buffs first (M = 1.0 .. 3.0, smallest wins), exactly as before. Only if
// nothing fits, multipliers BELOW 1 (0.975 .. 0.25): raids halve the stats of
// low-level characters (M = 0.5 measured). F does not depend on M, so these
// characters get their normal factor.
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

// ---------------------------------------------------------------- height ---
// Remembers the max HP we last wrote per struct, so a struct is bumped once
// per value the GAME gives it. Kept apart from g_chars on purpose: a character
// that is dropped and re-adopted must not be bumped a second time.
struct HpMark { uintptr_t p; float written; };
static HpMark g_hpMarks[128];
static int    g_hpNext = 0;

static HpMark* HpFind(uintptr_t p)
{
    for (auto& m : g_hpMarks) if (m.p == p) return &m;
    return nullptr;
}

static void HpRemember(uintptr_t p, float written)
{
    HpMark* m = HpFind(p);
    if (!m) { m = &g_hpMarks[g_hpNext]; g_hpNext = (g_hpNext + 1) % 128; }
    m->p = p; m->written = written;
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

// --------------------------------------------------------------- status ---
// Mirrors the Cheat Engine prototype's combined.txt so the two can be compared
// line for line. Rewritten in place a few times a second.
static char  g_lines[kSlots][256];
static int   g_lineCount = 0;

static void WriteStatus(int /*slot*/)
{
    if (!g_cfg.logging) return;
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\combined_supers_status.txt", g_dir);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"w") || !f) return;
    fprintf(f, "dll  %s  stats=%s  weight=%s  height=%s  share=%.2f  cap=%dpts  falloff=%.2f\n",
            g_cfg.enabled ? "ENABLED" : "disabled", g_cfg.stats, g_cfg.weight,
            g_cfg.height, g_cfg.share, g_cfg.capPoints, g_cfg.falloff);
    if (g_ring)
        fprintf(f, "cave hits  strike=%u  kiblast=%u\n",
                *(volatile uint32_t*)(g_ring + RING_HITS_A),
                *(volatile uint32_t*)(g_ring + RING_HITS_B));
    int arenas = 0;
    for (auto& a : g_arenas) if (a.base) ++arenas;
    fprintf(f, "scan  full %.1f ms every %.2f s   arenas %d, %.1f ms every 1 s\n",
            g_scanMs, g_scanEveryTicks * 0.25, arenas, g_arenaMs);
    fprintf(f, "characters (* = has dealt damage, . = found by scan):\n");
    if (!g_lineCount) fprintf(f, "  (none found yet)\n");
    for (int i = 0; i < g_lineCount; ++i) fprintf(f, "%s\n", g_lines[i]);
    fclose(f);
}

// ----------------------------------------------------------------- tick ---
static void Tick()
{
    LoadConfig();

    AdoptRing();

    // drop dead entries
    for (size_t i = 0; i < g_chars.size(); ) {
        Char& c = g_chars[i];
        if (!Valid(c) ||
            *(int*)(c.addr + OFF_RACE) != c.race ||
            *(int*)(c.addr + OFF_SPEC) != c.spec) {
            g_chars.erase(g_chars.begin() + i);
        } else ++i;
    }

    int slot = 0;
    g_lineCount = 0;
    for (auto& c : g_chars) {
        const RaceRow& raceRow = kRacesLive[c.race - 100];
        int heightTier = c.spec / 3, weightTier = c.spec % 3;
        int   level = *(int*)(c.addr + OFF_LEVEL);
        float* hpMax = (float*)(c.addr + OFF_HP_MAX);
        float* hpCur = (float*)(c.addr + OFF_HP_CUR);

        if (level < 1 || level > kMaxLevel) {
            if (g_lineCount < kSlots)
                sprintf_s(g_lines[g_lineCount++], sizeof(g_lines[0]),
                    "  %llX %c %-9s h%d w%d  Lv%d  OUTSIDE THE LEVEL TABLE - left vanilla",
                    (unsigned long long)c.addr, c.hit ? '*' : '.', raceRow.name, heightTier, weightTier, level);
            continue;
        }

        // Every CaC stat = G(level) * (1 + psc + points/500). Solving in steps
        // of G/1000 (half points) makes the whole thing level-independent, and
        // since F is a ratio G cancels out of it again.
        const float G    = kLevelG[level - 1];
        const float step = G / 1000.0f;
        float baseStrike = G * (1.0f + raceRow.strike[weightTier]), baseKi = G * (1.0f + raceRow.ki[weightTier]);

        if (c.level != level) { c.level = level; c.samples = 0; c.solved = false; }

        float liveStrike = *(float*)(c.addr + OFF_STRIKE);
        float liveKi = *(float*)(c.addr + OFF_KIBLAST);
        if (c.samples == 0 || liveStrike < c.minStrike) c.minStrike = liveStrike;
        if (c.samples == 0 || liveKi < c.minKi) c.minKi = liveKi;
        c.samples++;

        // Re-solved every tick from the running minimum, like combined.lua: a
        // character first seen while buffed gets corrected once the buff drops.
        {
            float unbuffedStrike, unbuffedKi; int strikeHalfPts, kiHalfPts;
            if (Solve(c.minStrike, c.minKi, baseStrike, baseKi, step, kTol, &unbuffedStrike, &unbuffedKi, &strikeHalfPts, &kiHalfPts)) {
                c.unbuffedStrike = unbuffedStrike; c.unbuffedKi = unbuffedKi; c.strikeHalfPts = strikeHalfPts; c.kiHalfPts = kiHalfPts;
                c.solved = true; c.approx = false;
            } else if (c.samples > 8) {
                // Nothing on the lattice at any M. Rather than skip the
                // character, assume unbuffed and snap to the nearest half point
                // - every client rounds the same way, so this stays consistent.
                strikeHalfPts = (int)floorf((c.minStrike - baseStrike) / step + 0.5f);
                kiHalfPts = (int)floorf((c.minKi - baseKi) / step + 0.5f);
                if (strikeHalfPts >= 2 * EXLO && strikeHalfPts <= 2 * EXHI && kiHalfPts >= 2 * EXLO && kiHalfPts <= 2 * EXHI) {
                    c.strikeHalfPts = strikeHalfPts; c.kiHalfPts = kiHalfPts;
                    c.unbuffedStrike = baseStrike + step * strikeHalfPts; c.unbuffedKi = baseKi + step * kiHalfPts;
                    c.solved = true; c.approx = true;
                }
            }
        }

        // ---- Height: health only. Speed is deliberately untouched; the
        // fields that mirror the Height speed value are derived values the
        // game never reads back, so writing them does nothing.
        //
        // Max HP = 1000*G*(1 + psc + points/500), so the gap to the best Height
        // tier is exactly 1000*G*(best - own) at any level, HEA or gear - it is
        // ADDED to whatever the game gave this struct. Only on structs the
        // damage code has used (*): those hold live HP, the scanned copies do
        // not. A struct is bumped once per value the game gives it.
        if (strcmp(g_cfg.height, "vanilla") != 0 && c.hit) {
            float best = raceRow.health[0];
            for (int i = 1; i < 4; ++i) if (raceRow.health[i] > best) best = raceRow.health[i];
            float bonus = 1000.0f * G * (best - raceRow.health[heightTier]);
            float gameMaxHp = *hpMax;
            if (bonus > 0.5f && gameMaxHp > 1.0f && gameMaxHp < 1.0e6f) {
                HpMark* mark = HpFind(c.addr);
                if (!mark || fabsf(gameMaxHp - mark->written) > 0.5f) {
                    // Current HP only rises if it was full at that moment, so
                    // this can never heal anyone mid-fight.
                    bool full = *hpCur >= gameMaxHp - 0.5f;
                    *hpMax = gameMaxHp + bonus;
                    if (full) *hpCur = gameMaxHp + bonus;
                    HpRemember(c.addr, gameMaxHp + bonus);
                }
            }
        }

        float hpNow = *hpCur, hpMaxNow = *hpMax;

        if (!c.solved) {
            // As combined.lua does: show who is being left vanilla, and why.
            if (g_lineCount < kSlots)
                sprintf_s(g_lines[g_lineCount++], sizeof(g_lines[0]),
                    "  %llX %c %-9s h%d w%d  Lv%d G=%.4f  hp=%.0f/%.0f  live=%.4f/%.4f  base=%.4f/%.4f"
                    "  off=%.2f/%.2f pts  basic=%.4f/%.4f  UNSOLVED (n=%d)",
                    (unsigned long long)c.addr, c.hit ? '*' : '.', raceRow.name, heightTier, weightTier,
                    level, G, hpNow, hpMaxNow, c.minStrike, c.minKi, baseStrike, baseKi,
                    (c.minStrike - baseStrike) / (2 * step), (c.minKi - baseKi) / (2 * step),
                    *(float*)(c.addr + OFF_BASIC), *(float*)(c.addr + OFF_BASIC_KI), c.samples);
            continue;
        }
        if (!g_tab || slot >= kSlots) continue;

        float ownStrike = c.unbuffedStrike - baseStrike, ownKi = c.unbuffedKi - baseKi;     // points + gear, stat units
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
            weightSpanStrike = G * (bestStrikeTier - lightStrikeTier);       weightSpanKi = G * (bestKiTier - lightKiTier);
        }

        float sharedStrike = Blend(g_cfg.stats, ownStrike, ownKi, g_cfg.share);
        float sharedKi = Blend(g_cfg.stats, ownKi, ownStrike, g_cfg.share);
        float cap = g_cfg.capPoints * (G / 500.0f);
        float targetStrike = floorStrike + weightSpanStrike + SoftCap(sharedStrike, cap, g_cfg.falloff);
        float targetKi = floorKi + weightSpanKi + SoftCap(sharedKi, cap, g_cfg.falloff);

        float strikeFactor = g_cfg.enabled ? targetStrike / c.unbuffedStrike : 1.0f;
        float kiFactor = g_cfg.enabled ? targetKi / c.unbuffedKi : 1.0f;

        float multiplier = liveStrike / c.unbuffedStrike;
        float gridError  = fabsf(multiplier / MGRID - floorf(multiplier / MGRID + 0.5f)) * MGRID;
        if (g_lineCount < kSlots)
            sprintf_s(g_lines[g_lineCount++], sizeof(g_lines[0]),
                "  %llX %c %-9s h%d w%d  Lv%d G=%.4f  hp=%.0f/%.0f  tot=%+.1f/%+.1f pts"
                "  U=%.4f/%.4f  M=%.3f%s  ->%.4f/%.4f  F=%.4f/%.4f",
                (unsigned long long)c.addr, c.hit ? '*' : '.', raceRow.name,
                heightTier, weightTier, level, G, hpNow, hpMaxNow, c.strikeHalfPts / 2.0, c.kiHalfPts / 2.0, c.unbuffedStrike, c.unbuffedKi,
                multiplier, c.approx ? "~" : (gridError < 0.004f ? "" : "?"), targetStrike, targetKi, strikeFactor, kiFactor);
        // factors before the pointer, so the cave never pairs a new pointer
        // with a previous occupant's factors
        g_tab[slot].fs  = strikeFactor;
        g_tab[slot].fk  = kiFactor;
        g_tab[slot].ptr = (uint64_t)c.addr;
        slot++;
    }
    for (int i = slot; i < kSlots; ++i) g_tab[i].ptr = 0;
    WriteStatus(slot);
}

// --------------------------------------------------------------- thread ---
static HANDLE   g_thread = nullptr;
static volatile bool g_run = false;

static DWORD WINAPI ThreadMain(LPVOID)
{
    // Scanning and the 250 ms tick must never take CPU time from the game's
    // own threads (FPS drops while hosting online, 2026-10-11).
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    EnsureConfig();
    Sleep(5000);                                   // let the game finish booting
    LoadConfig();
    Log("--- Combined Super Stats v" MOD_VERSION " loaded ---");

    // Prefer the game's own stat tables over the ones baked into this mod, so
    // a game update - or a mod that edits the PSC - is followed automatically.
    {
        char why[128] = "";
        bool ok = Psc_Load(g_root, why, sizeof(why));
        Log("stat tables: %s (%s)", ok ? "game PSC" : "built-in fallback", why);
    }
    // Steam's DRM decrypts the game code some time after launch; keep trying
    // for a minute before concluding this game version is unsupported.
    bool hooked = false;
    for (int t = 0; t < 60 && g_run && !hooked; ++t) {
        hooked = InstallHooks(t == 59);
        if (!hooked) Sleep(1000);
    }
    if (!hooked) return 0;

    // Rescan often enough that the struct a fight creates is found during the
    // intro, before its first super - otherwise that super's opening hits land
    // un-boosted until the ring adopts the attacker. Every 2s at most, but the
    // scan may use no more than 5% of this thread: the interval stretches to
    // 20x the last scan's duration. (Ticks are 250ms.)
    LARGE_INTEGER qpf; QueryPerformanceFrequency(&qpf);
    int sinceScan = 1 << 30;
    while (g_run) {
        __try {
            int wait = (int)ceil(20.0 * g_scanMs / 250.0);
            if (wait < 8)   wait = 8;
            if (wait > 480) wait = 480;
            g_scanEveryTicks = wait;
            if (sinceScan >= wait) {
                LARGE_INTEGER t0, t1;
                QueryPerformanceCounter(&t0);
                ScanForCharacters();
                QueryPerformanceCounter(&t1);
                g_scanMs = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / qpf.QuadPart;
                sinceScan = 0;
            } else if (sinceScan % 4 == 0) {              // once a second
                LARGE_INTEGER t0, t1;
                QueryPerformanceCounter(&t0);
                ScanArenas();
                QueryPerformanceCounter(&t1);
                g_arenaMs = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / qpf.QuadPart;
            }
            sinceScan++;
            Tick();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // a character can be freed mid-read; drop everything and re-find
            g_chars.clear();
        }
        Sleep(250);
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

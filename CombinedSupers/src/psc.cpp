// ---------------------------------------------------------------------------
//  Reads the character stat tables out of the game's own data file rather than
//  trusting values baked into this mod, so it follows game updates and any
//  mod that edits the PSC.
//
//  parameter_spec_char.psc layout (format documented by eterniti's
//  eternity_common and LazyBone152's XV2-Tools):
//
//    0x00  "#PSC", endian check, header size (20)
//    0x08  num_entries
//    0x10  num_configs
//    0x14  num_configs blocks of num_entries x {char_id, num_specs, unused}
//          then every spec entry packed together, config-major.
//
//  A spec entry is 200 bytes (196 before game 1.20). CaC races are char_id
//  100..107 with 12 specs each: height*3 + weight.
//    +0x18 health   +0x58 strike_attack   +0x5C ki_blast_super
// ---------------------------------------------------------------------------
#include "psc.h"
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

RaceRow kRacesLive[8];
static bool g_init = false;

static void EnsureDefaults()
{
    if (g_init) return;
    memcpy(kRacesLive, kRaces, sizeof(kRacesLive));
    g_init = true;
}

static bool ReadWhole(const wchar_t* path, std::vector<uint8_t>& out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64 << 20)) {
        CloseHandle(h); return false;
    }
    out.resize((size_t)sz.QuadPart);
    DWORD got = 0;
    BOOL ok = ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr);
    CloseHandle(h);
    return ok && got == out.size();
}

template <typename T> static T Rd(const std::vector<uint8_t>& d, size_t off)
{
    T v{}; memcpy(&v, d.data() + off, sizeof(T)); return v;
}

bool Psc_Load(const wchar_t* gameRoot, char* why, size_t whyLen)
{
    EnsureDefaults();
    auto fail = [&](const char* msg) { if (why) strncpy_s(why, whyLen, msg, _TRUNCATE); return false; };

    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\data\\system\\parameter_spec_char.psc", gameRoot);

    std::vector<uint8_t> d;
    if (!ReadWhole(path, d)) return fail("no loose data/system/parameter_spec_char.psc");
    if (d.size() < 20 || memcmp(d.data(), "#PSC", 4) != 0) return fail("not a PSC file");

    uint32_t nEntries = Rd<uint32_t>(d, 0x08);
    uint32_t nConfigs = Rd<uint32_t>(d, 0x10);
    if (!nEntries || nEntries > 4096 || !nConfigs || nConfigs > 16)
        return fail("implausible PSC header");

    size_t idxBytes = (size_t)12 * nEntries * nConfigs;
    size_t specBase = 20 + idxBytes;
    if (specBase >= d.size()) return fail("PSC truncated");

    // Entry size is not stored; derive it from the total spec count.
    uint64_t totalSpecs = 0;
    for (uint32_t c = 0; c < nConfigs; ++c)
        for (uint32_t i = 0; i < nEntries; ++i)
            totalSpecs += Rd<uint32_t>(d, 20 + (size_t)12 * nEntries * c + (size_t)12 * i + 4);
    if (!totalSpecs) return fail("PSC has no spec entries");
    size_t entSize = (d.size() - specBase) / (size_t)totalSpecs;
    if (entSize != 200 && entSize != 196) return fail("unexpected PSC entry size");
    const size_t offHealth = (entSize == 200) ? 0x18 : 0x14;
    const size_t offStrike = (entSize == 200) ? 0x58 : 0x54;
    const size_t offKi     = (entSize == 200) ? 0x5C : 0x58;

    RaceRow rows[8];
    memcpy(rows, kRaces, sizeof(rows));
    bool got[8] = {};

    size_t pos = specBase;
    for (uint32_t c = 0; c < nConfigs; ++c) {
        for (uint32_t i = 0; i < nEntries; ++i) {
            size_t rec = 20 + (size_t)12 * nEntries * c + (size_t)12 * i;
            uint32_t id = Rd<uint32_t>(d, rec);
            uint32_t ns = Rd<uint32_t>(d, rec + 4);
            if (c == 0 && id >= 100 && id <= 107 && ns == 12 &&
                pos + entSize * 12 <= d.size()) {
                RaceRow& r = rows[id - 100];
                for (int s = 0; s < 12; ++s) {
                    size_t p = pos + entSize * s;
                    float st = Rd<float>(d, p + offStrike);
                    float ki = Rd<float>(d, p + offKi);
                    float hp = Rd<float>(d, p + offHealth);
                    if (s < 3) {                       // weight tier, height 0
                        r.strike[s] = st;              // raw psc; stat = G*(1+psc+pts/500)
                        r.ki[s]     = ki;
                    }
                    if (s % 3 == 0) r.health[s / 3] = hp;
                }
                got[id - 100] = true;
            }
            pos += (size_t)entSize * ns;
            if (pos > d.size()) return fail("PSC index overruns the file");
        }
    }
    for (int i = 0; i < 8; ++i) if (!got[i]) return fail("a CaC race is missing from the PSC");

    memcpy(kRacesLive, rows, sizeof(kRacesLive));
    if (why) strncpy_s(why, whyLen, "loaded from the game's PSC", _TRUNCATE);
    return true;
}

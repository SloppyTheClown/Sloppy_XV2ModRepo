// Lobby script (.tsr) layout, from XV2-Tools (Xv2CoreLib/TSR):
//   u16 function count, i32 size per function, then the functions.
//   Function: i32, name, u16, u16 tag count, tags (u16 index + name),
//   u16 sentence count, sentences (command, u16, u16 argument count,
//   arguments: u16 type; 1 int, 2 float, 3-6 string).
//   Strings are i32 length + bytes XORed with kTsrKey; the key position starts
//   at 4 for each function and runs on across all its strings.
#include "tsr.h"
#include "tsr_key.h"
#include <cstring>

namespace {

struct Reader {
    const uint8_t* d;
    size_t n, p;
    unsigned enc = 4;
    bool bad = false;

    uint16_t u16()
    {
        if (p + 2 > n) { bad = true; return 0; }
        uint16_t v; memcpy(&v, d + p, 2); p += 2; return v;
    }
    int32_t i32()
    {
        if (p + 4 > n) { bad = true; return 0; }
        int32_t v; memcpy(&v, d + p, 4); p += 4; return v;
    }
    // Decodes the next string; `at` / `key` say where its bytes and key start.
    std::string str(size_t* at = nullptr, unsigned* key = nullptr)
    {
        int32_t len = i32();
        if (bad || len < 0 || (size_t)len > n - p) { bad = true; return {}; }
        if (enc >= 1024) enc = 0;
        if (at) *at = p;
        if (key) *key = enc;
        std::string s(len, '\0');
        for (int32_t i = 0; i < len; ++i) {
            s[i] = (char)(d[p + i] ^ kTsrKey[enc]);
            if (++enc == 1024) enc = 0;
        }
        p += len;
        size_t z = s.find('\0');
        if (z != std::string::npos) s.resize(z);
        return s;
    }
};

}  // namespace

int TsrReplaceInIf(const uint8_t* src, size_t n, const char* label, const char* from, const char* to,
                   std::vector<uint8_t>& out)
{
    size_t fromLen = strlen(from);
    if (fromLen != strlen(to) || n < 2) return -1;
    uint16_t count;
    memcpy(&count, src, 2);
    size_t off = 2 + (size_t)count * 4;
    if (off > n) return -1;
    out.assign(src, src + n);
    int changed = 0;
    for (uint16_t f = 0; f < count; ++f) {
        int32_t size;
        memcpy(&size, src + 2 + f * 4, 4);
        if (size < 0 || off + (size_t)size > n) return -1;
        Reader r{src, off + (size_t)size, off};
        r.i32();
        r.str();
        r.u16();
        for (uint16_t t = r.u16(); t && !r.bad; --t) { r.u16(); r.str(); }
        for (uint16_t s = r.u16(); s && !r.bad; --s) {
            std::string cmd = r.str();
            r.u16();
            uint16_t argc = r.u16();
            std::string cond, elseLabel;
            size_t condAt = 0;
            unsigned condKey = 0;
            for (uint16_t a = 0; a < argc && !r.bad; ++a) {
                uint16_t type = r.u16();
                if (type == 1 || type == 2) { r.i32(); continue; }
                if (type < 3 || type > 6) return -1;
                size_t at; unsigned key;
                std::string v = r.str(&at, &key);
                if (a == 0) { cond = v; condAt = at; condKey = key; }
                if (a == 2) elseLabel = v;
            }
            if (r.bad) return -1;
            size_t hit = cond.find(from);
            if (cmd != "IF" || elseLabel != label || hit == std::string::npos) continue;
            for (size_t i = 0; i < fromLen; ++i)  // re-encode the replaced bytes in place
                out[condAt + hit + i] = (uint8_t)(to[i] ^ kTsrKey[(condKey + hit + i) % 1024]);
            ++changed;
        }
        if (r.bad) return -1;
        off += (size_t)size;
    }
    return changed;
}

int ReplaceInUnique(const uint8_t* src, size_t n, const char* needle, const char* from, const char* to,
                    std::vector<uint8_t>& out)
{
    size_t needleLen = strlen(needle), fromLen = strlen(from);
    const char* inNeedle = strstr(needle, from);
    if (fromLen != strlen(to) || !inNeedle || needleLen > n) return -1;
    size_t at = SIZE_MAX;
    int found = 0;
    for (size_t i = 0; i + needleLen <= n; ++i) {
        if (src[i] == (uint8_t)needle[0] && memcmp(src + i, needle, needleLen) == 0) {
            at = i;
            ++found;
        }
    }
    if (found != 1) return found;
    out.assign(src, src + n);
    memcpy(out.data() + at + (inNeedle - needle), to, fromLen);
    return 1;
}

// TSD section 1 (triggers, from XV2-Tools Xv2CoreLib/TSD): 8 ints (type at
// +12, NPC ID at +16), then the condition as i32 length + text. Ends with an
// 8-byte marker. `edit` changes a condition in place and returns whether it
// did. Only type 1 triggers (the player talks to the NPC) are edited: type 2
// ones start by themselves (chase gimmicks: the NPC runs to the player, whose
// controls stay locked until it arrives - forever if the player is flying).
static const int32_t kTalkTrigger = 1;
template <class Edit>
static int TsdEditNpcs(const uint8_t* src, size_t n, const NpcRanges& npcs, std::vector<uint8_t>& out, Edit edit)
{
    static const uint8_t kEnd[8] = {0x10, 0x32, 0x54, 0x76, 0xEF, 0xCD, 0xAB, 0x89};
    if (n < 10 || src[0] != 1) return -1;
    out.assign(src, src + n);
    int changed = 0;
    size_t o = 1;
    while (o < n && !(o + 8 <= n && !memcmp(src + o, kEnd, 8))) {
        if (o + 36 > n) return -1;
        int32_t type, npc, len;
        memcpy(&type, src + o + 12, 4);
        memcpy(&npc, src + o + 16, 4);
        memcpy(&len, src + o + 32, 4);
        if (len < 0 || (size_t)len > n - o - 36) return -1;
        for (const NpcRange& r : npcs) {
            if (type == kTalkTrigger && npc >= r.first && npc <= r.last) {
                changed += edit((char*)out.data() + o + 36, (size_t)len);
                break;
            }
        }
        o += 36 + (size_t)len;
    }
    return changed;
}

int TsdReplaceForNpcs(const uint8_t* src, size_t n, const NpcRanges& npcs, const char* from, const char* to,
                      std::vector<uint8_t>& out)
{
    size_t fromLen = strlen(from);
    if (fromLen != strlen(to)) return -1;
    return TsdEditNpcs(src, n, npcs, out, [&](char* c, size_t len) {
        bool hit = false;
        for (size_t at = 0; at + fromLen <= len; ++at) {
            if (memcmp(c + at, from, fromLen)) continue;
            memcpy(c + at, to, fromLen);
            hit = true;
        }
        return hit;
    });
}

int TsdStoryAlways(const uint8_t* src, size_t n, const NpcRanges& npcs, std::vector<uint8_t>& out)
{
    static const char pre[] = "QUEST(TPQ,STATE,CTP_";  // then "NN_NN)==3"
    const size_t preLen = sizeof(pre) - 1, fullLen = preLen + 9;
    static_assert(sizeof(kAlwaysEvent) - 1 == sizeof(pre) - 1 + 9, "kAlwaysEvent length");
    return TsdEditNpcs(src, n, npcs, out, [&](char* c, size_t len) {
        bool hit = false;
        for (size_t at = 0; at + fullLen <= len; ++at) {
            const char* q = c + at + preLen;
            if (memcmp(c + at, pre, preLen) || q[2] != '_' || memcmp(q + 5, ")==3", 4)) continue;
            memcpy(c + at, kAlwaysEvent, fullLen);
            hit = true;
        }
        return hit;
    });
}

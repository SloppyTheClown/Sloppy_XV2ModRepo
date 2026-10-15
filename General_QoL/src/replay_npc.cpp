// ---------------------------------------------------------------------------
//  The replay NPC's file edits, applied to a copy as the game loads them.
//
//  Layouts follow LazyBone's XV2-Tools (Xv2CoreLib TSD, TNL, MSG) for game
//  1.26.01. Each editor checks the layout it expects and gives up (the game
//  then gets the file as it was) if anything does not fit.
// ---------------------------------------------------------------------------
#include "replay_npc.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <set>

namespace {

uint32_t rd32(const uint8_t* d, size_t o) { uint32_t v; memcpy(&v, d + o, 4); return v; }
uint16_t rd16(const uint8_t* d, size_t o) { uint16_t v; memcpy(&v, d + o, 2); return v; }
void wr32(uint8_t* d, size_t o, uint32_t v) { memcpy(d + o, &v, 4); }
void wr16(uint8_t* d, size_t o, uint16_t v) { memcpy(d + o, &v, 2); }
void put32(std::vector<uint8_t>& o, uint32_t v) { o.insert(o.end(), (uint8_t*)&v, (uint8_t*)&v + 4); }
void putBytes(std::vector<uint8_t>& o, const uint8_t* p, size_t n) { o.insert(o.end(), p, p + n); }

// Sections of TSD and TNL files end with this 8-byte marker.
const uint8_t kEnd[8] = {0x10, 0x32, 0x54, 0x76, 0xEF, 0xCD, 0xAB, 0x89};
bool AtEnd(const uint8_t* d, size_t n, size_t o) { return o >= n || (o + 8 <= n && !memcmp(d + o, kEnd, 8)); }

// A length-prefixed string from a TSD/TNL entry, kept byte-exact so an
// untouched entry is written back identically. `nul` = stored with a 0 byte.
struct LStr {
    std::string s;
    bool nul = false;
};
bool ReadLStr(const uint8_t* d, size_t n, size_t& p, LStr& out)
{
    if (p + 4 > n) return false;
    uint32_t len = rd32(d, p);
    if (len > n - p - 4) return false;
    const char* c = (const char*)d + p + 4;
    out.nul = len && c[len - 1] == 0;
    out.s.assign(c, out.nul ? len - 1 : len);
    p += 4 + len;
    return true;
}
void WriteLStr(std::vector<uint8_t>& o, const LStr& s)
{
    put32(o, (uint32_t)(s.s.size() + (s.nul ? 1 : 0)));
    putBytes(o, (const uint8_t*)s.s.data(), s.s.size());
    if (s.nul) o.push_back(0);
}

// ------------------------------------------------------------ lobby events
// TSD: section 1 = triggers (8 ints + condition), section 2 = events (2 ints,
// 5 strings: name, path, script, function, args; then a list of NPC IDs).
// Both are rebuilt; everything after them is copied unchanged.
// True for every character (level >= 1) - the game's own trigger 425 uses it.
const char* kAlways = "( AVATAR(LEVEL)>=1)";
const char* kNever = "(QUEST(TPQ,STATE,CTP_00_01)==99)";

bool EditLobbyEvents(const ReplayNpc& e, const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (!e.enabled) { note = "replay NPC off"; return false; }
    if (n < 10 || d[0] != 1) { note = "unexpected TSD layout (no trigger section)"; return false; }
    std::set<int> hide(e.hideNpcs.begin(), e.hideNpcs.end());
    std::set<uint32_t> npcEvents;
    out.clear();
    out.reserve(n + 4096);
    out.push_back(1);
    size_t o = 1;
    int shown = 0, hidden = 0;
    while (!AtEnd(d, n, o)) {
        if (o + 36 > n) { note = "truncated trigger"; return false; }
        int tnl = (int)rd32(d, o + 16);
        uint8_t head[32];
        memcpy(head, d + o, 32);
        size_t p = o + 32;
        LStr cond;
        if (!ReadLStr(d, n, p, cond)) { note = "bad trigger condition"; return false; }
        if (tnl == e.npcId) {
            cond.s = kAlways;
            wr32(head, 4, 0xFFFFFFFF);  // DLC_Flag: Dastar's need a DLC story done
            npcEvents.insert(rd32(d, o + 24));
            ++shown;
        } else if (hide.count(tnl)) {
            cond.s = kNever;
            ++hidden;
        }
        putBytes(out, head, 32);
        WriteLStr(out, cond);
        o = p;
    }
    if (o + 9 > n || d[o + 8] != 2) { note = "unexpected TSD layout (no event section)"; return false; }
    putBytes(out, d + o, 9);  // end marker + section id 2
    o += 9;
    int rewired = 0;
    while (!AtEnd(d, n, o)) {
        if (o + 12 > n) { note = "truncated event"; return false; }
        uint32_t id = rd32(d, o);
        size_t p = o + 8;
        LStr s[5];
        for (auto& x : s)
            if (!ReadLStr(d, n, p, x)) { note = "bad event string"; return false; }
        if (p + 4 > n) { note = "truncated event"; return false; }
        uint32_t cnt = rd32(d, p);
        if (cnt > (n - p - 4) / 4) { note = "bad event NPC list"; return false; }
        size_t end = p + 4 + (size_t)cnt * 4;
        if (npcEvents.count(id)) {
            // Keep the NPC's own talk function (Dastar: SELECT_EXTPQ_FUTURE_SAGA,
            // whose quest menu this mod builds); only swap the robot's talk
            // animation (argument 1) for one the new model has.
            size_t comma = s[4].s.find(',');
            if (comma != std::string::npos) {
                s[4].s = "CONST(" + e.talkMotion + ")" + s[4].s.substr(comma);
                ++rewired;
            }
        }
        putBytes(out, d + o, 8);
        for (auto& x : s) WriteLStr(out, x);
        putBytes(out, d + p, end - p);
        o = end;
    }
    putBytes(out, d + o, n - o);  // remaining sections, byte for byte
    note = "NPC " + std::to_string(e.npcId) + ": " + std::to_string(shown) + " triggers shown, " +
           std::to_string(rewired) + " talk animations set; " + std::to_string(hidden) + " other triggers hidden";
    return shown > 0 || hidden > 0;
}

// -------------------------------------------------------------- lobby NPCs
// TNL: section 1 = characters: id, 3 bytes, CMS, costume, i16, lobby name,
// i32, position, zone, action. Rebuilt; the rest copied unchanged.
bool EditLobbyNpcs(const ReplayNpc& e, const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (!e.enabled) { note = "replay NPC off"; return false; }
    if (n < 10 || d[0] != 1) { note = "unexpected TNL layout"; return false; }
    out.clear();
    out.reserve(n + 64);
    out.push_back(1);
    size_t o = 1;
    int changed = 0;
    while (!AtEnd(d, n, o)) {
        if (o + 11 > n) { note = "truncated character"; return false; }
        int id = (int)rd32(d, o);
        size_t p = o + 7;
        LStr cms, lobbyName, position, zone;
        if (!ReadLStr(d, n, p, cms) || p + 4 > n) { note = "bad character"; return false; }
        uint16_t costume = rd16(d, p), variation = rd16(d, p + 2);
        p += 4;
        if (!ReadLStr(d, n, p, lobbyName) || p + 4 > n) { note = "bad character"; return false; }
        uint32_t i32 = rd32(d, p);
        p += 4;
        if (!ReadLStr(d, n, p, position) || !ReadLStr(d, n, p, zone) || p + 4 > n) { note = "bad character"; return false; }
        uint32_t action = rd32(d, p);
        p += 4;
        if (id == e.npcId) {
            cms.s = e.model;
            costume = (uint16_t)e.costume;
            variation = 0;
            action = (uint32_t)e.idleAction;
            ++changed;
        }
        putBytes(out, d + o, 7);
        WriteLStr(out, cms);
        out.insert(out.end(), (uint8_t*)&costume, (uint8_t*)&costume + 2);
        out.insert(out.end(), (uint8_t*)&variation, (uint8_t*)&variation + 2);
        WriteLStr(out, lobbyName);
        put32(out, i32);
        WriteLStr(out, position);
        WriteLStr(out, zone);
        put32(out, action);
        o = p;
    }
    putBytes(out, d + o, n - o);
    note = changed ? "NPC " + std::to_string(e.npcId) + " is now " + e.model + " costume " + std::to_string(e.costume)
                   : "NPC " + std::to_string(e.npcId) + " not found";
    return changed > 0;
}

// -------------------------------------------------------------- lobby text
// MSG: entry names (16-byte records) and, per entry, lines (16-byte records:
// string offset, length in characters, size in bytes, unknown). A replaced
// line's text is appended at the end and its record pointed at it.
std::wstring ToGameText(const std::string& utf8)
{
    std::string esc;
    for (char c : utf8) {
        switch (c) {
        case '&': esc += "&amp;"; break;
        case '<': esc += "&lt;"; break;
        case '>': esc += "&gt;"; break;
        case '\'': esc += "&apos;"; break;
        case '"': esc += "&quot;"; break;
        default: esc += c;
        }
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, esc.data(), (int)esc.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, esc.data(), (int)esc.size(), w.data(), len);
    return w;
}

struct Want { const char* key; const std::string* text; };

// Replaces the first line of each wanted entry that has non-empty text.
template <size_t N>
bool ReplaceMsgLines(const Want (&want)[N], const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (n < 32 || memcmp(d, "#MSG", 4)) { note = "not a MSG file"; return false; }
    bool uniNames = rd16(d, 4) == 256, uniText = rd16(d, 6) == 1;
    uint32_t count = rd32(d, 8), names = rd32(d, 12), lines = rd32(d, 20);
    if (!uniText) { note = "text is not UTF-16"; return false; }
    if ((uint64_t)names + (uint64_t)count * 16 > n || (uint64_t)lines + (uint64_t)count * 8 > n) {
        note = "unexpected MSG layout";
        return false;
    }
    out.assign(d, d + n);
    int replaced = 0;
    for (uint32_t i = 0; i < count; ++i) {
        size_t rec = names + (size_t)i * 16;
        uint32_t so = rd32(d, rec), size = rd32(d, rec + (uniNames ? 8 : 4));
        if ((uint64_t)so + size > n) continue;
        std::string key;
        if (uniNames) {
            for (uint32_t k = 0; k + 1 < size; k += 2) {
                uint16_t ch = rd16(d, so + k);
                if (!ch) break;
                key += (char)ch;
            }
        } else {
            key.assign((const char*)d + so, strnlen((const char*)d + so, size));
        }
        for (const Want& w : want) {
            if (w.text->empty() || key != w.key) continue;
            uint32_t lineCount = rd32(d, lines + (size_t)i * 8), lrec = rd32(d, lines + (size_t)i * 8 + 4);
            if (!lineCount || (uint64_t)lrec + 16 > n) continue;
            std::wstring t = ToGameText(*w.text);
            if (out.size() & 1) out.push_back(0);
            uint32_t at = (uint32_t)out.size();
            putBytes(out, (const uint8_t*)t.data(), t.size() * 2);
            out.push_back(0); out.push_back(0);
            wr32(out.data(), lrec, at);
            wr32(out.data(), lrec + 4, (uint32_t)t.size());
            wr32(out.data(), lrec + 8, (uint32_t)(t.size() * 2 + 2));
            ++replaced;
        }
    }
    note = std::to_string(replaced) + " lines replaced";
    return replaced > 0;
}

bool EditLobbyText(const ReplayNpc& e, const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (!e.enabled) { note = "replay NPC off"; return false; }
    // Dastar's lines, in the order his talk function uses them.
    const Want want[] = {
        {e.npcNameKey.c_str(), &e.name},
        {"MSG_FUTURE_EXTPQ_CHOICE", &e.greeting},
        {"MSG_FUTURE_EXTPQ_CHOICE_01", &e.option},
        {"MSG_FUTURE_EXTPQ_SELECT", &e.menuLine},
        {"MSG_FUTURE_EXTPQ_CHOICE_YES", &e.farewellGo},
        {"MSG_FUTURE_EXTPQ_CHOICE_NO", &e.farewellStay},
    };
    return ReplaceMsgLines(want, d, n, out, note);
}

// The quest menu's header (title and subtitle): Dastar's "Future Saga".
bool EditMenuTitles(const ReplayNpc& e, const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (!e.enabled) { note = "replay NPC off"; return false; }
    const Want want[] = {{"HED_FTS_00_000", &e.menuTitle}};
    return ReplaceMsgLines(want, d, n, out, note);
}

// ------------------------------------------------------- menu title art
// CMNTITLE_<lang>.iggytex: "oggytex", texture count at 8, then per texture
// {index, size | 0x80000000, offset | 0x80000000, 0} from 16; each one a DDS.
// Texture 14 (768x1536 DXT5) holds the newer big menu titles, one per band;
// "Future Saga" (with its glow) is block rows 254..272 (y 1016..1091), block
// columns 0..96, in every language. Those blocks become transparent black. The rows just
// above and below are the gaps between titles (alpha <= 3 from neighbouring
// glow) and the rest of the band must be empty, or the file is left alone.

// Highest alpha of any pixel in a DXT5 block.
unsigned BlockMaxAlpha(const uint8_t* b)
{
    unsigned a0 = b[0], a1 = b[1], level[8] = {a0, a1};
    if (a0 > a1)
        for (unsigned k = 2; k < 8; ++k) level[k] = ((8 - k) * a0 + (k - 1) * a1) / 7;
    else {
        for (unsigned k = 2; k < 6; ++k) level[k] = ((6 - k) * a0 + (k - 1) * a1) / 5;
        level[6] = 0;
        level[7] = 255;
    }
    uint64_t bits = 0;
    memcpy(&bits, b + 2, 6);
    unsigned m = 0;
    for (int i = 0; i < 16; ++i) m = (std::max)(m, level[(bits >> (3 * i)) & 7]);
    return m;
}

bool EditMenuTitleArt(const ReplayNpc& e, const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (!e.enabled || e.menuTitle.empty()) { note = "keeping the game's title"; return false; }
    if (n < 16 || memcmp(d, "oggytex", 8)) { note = "not an iggytex file"; return false; }
    uint32_t count = rd32(d, 8);
    if (count != 14 || 16 + (size_t)count * 16 > n) { note = "unexpected texture count"; return false; }
    size_t t = 16 + 13 * 16;
    uint32_t size = rd32(d, t + 4) & 0x7FFFFFFF, off = rd32(d, t + 8) & 0x7FFFFFFF;
    if (rd32(d, t) != 14 || (uint64_t)off + size > n || size < 128) { note = "unexpected texture table"; return false; }
    const uint8_t* dds = d + off;
    const size_t w = 768, h = 1536;
    if (memcmp(dds, "DDS ", 4) || rd32(dds, 12) != h || rd32(dds, 16) != w || memcmp(dds + 84, "DXT5", 4) ||
        128 + (w / 4) * (h / 4) * 16 > size) {
        note = "texture 14 is not the expected 768x1536 DXT5";
        return false;
    }
    const size_t cols = 97, rowFirst = 254, rowEnd = 273;
    auto block = [&](size_t row, size_t col) { return off + 128 + (row * (w / 4) + col) * 16; };
    for (size_t row : {rowFirst - 1, rowEnd})
        for (size_t col = 0; col < w / 4; ++col)
            if (BlockMaxAlpha(d + block(row, col)) > 3) { note = "title band not where expected"; return false; }
    for (size_t row = rowFirst; row < rowEnd; ++row)
        for (size_t col = cols; col < w / 4; ++col)
            if (BlockMaxAlpha(d + block(row, col))) { note = "title band not where expected"; return false; }
    out.assign(d, d + n);
    for (size_t row = rowFirst; row < rowEnd; ++row)
        for (size_t col = 0; col < cols; ++col)
            memset(out.data() + block(row, col), 0, 16);  // alpha 0 and colour black: Iggy blends
                                                          // premultiplied, so colour alone still shows
    note = "\"Future Saga\" title art hidden";
    return true;
}

// -------------------------------------------------------------- talk voice
// CAQR_ALL_VOX.acb (CRI ACB): @UTF tables, big-endian. The game plays a lobby
// NPC's greeting by cue name - Dastar's CAQR_DST_000 for this NPC,
// BDQR_MST_000 for the lobby Hercule (both heard in game, 2026-09-29).
// CueNameTable (CueName, CueIndex) gives each name its CueTable row; that
// row's ReferenceType / ReferenceIndex / Length say what it plays. Dastar's
// row gets the configured cue's three values, in place. Names and indices
// stay one to one: pointing a second name at a cue crashed the game (CRI
// maps cue index -> name).
uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

struct UtfCol {
    std::string name;
    uint8_t storage = 0, type = 0;
    size_t rowOff = 0;  // within a row (per-row columns)
};
struct Utf {
    size_t at = 0;                   // table start (after "@UTF" + size) in the file
    size_t size = 0, rows = 0, strings = 0, data = 0, rowLen = 0, count = 0;
    std::vector<UtfCol> cols;
};

size_t UtfTypeSize(uint8_t type)
{
    static const uint8_t sizes[12] = {1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 4, 8};
    return type < 12 ? sizes[type] : 0;
}

// Parses the table header and columns of the @UTF at file offset `o`.
bool ParseUtf(const uint8_t* d, size_t n, size_t o, Utf& u)
{
    if (o + 32 > n || memcmp(d + o, "@UTF", 4)) return false;
    u.at = o + 8;
    u.size = be32(d + o + 4);
    if (u.size < 24 || u.size > n - u.at) return false;
    const uint8_t* t = d + u.at;
    u.rows = be16(t + 2);
    u.strings = be32(t + 4);
    u.data = be32(t + 8);
    size_t ncols = be16(t + 16);
    u.rowLen = be16(t + 18);
    u.count = be32(t + 20);
    if (u.strings > u.size || u.data > u.size || u.rows + u.rowLen * u.count > u.size) return false;
    auto str = [&](uint32_t off, std::string& s) {
        size_t a = u.strings + (size_t)off;
        if (a >= u.size) return false;
        const char* c = (const char*)t + a;
        s.assign(c, strnlen(c, u.size - a));
        return true;
    };
    size_t p = 24, rowOff = 0;
    for (size_t i = 0; i < ncols; ++i) {
        if (p + 5 > u.size) return false;
        UtfCol c;
        c.storage = t[p] & 0xF0;
        c.type = t[p] & 0x0F;
        if (!str(be32(t + p + 1), c.name)) return false;
        p += 5;
        size_t sz = UtfTypeSize(c.type);
        if (!sz) return false;
        if (c.storage == 0x30) p += sz;  // constant: value in the column list
        else if (c.storage == 0x50) { c.rowOff = rowOff; rowOff += sz; }
        u.cols.push_back(c);
    }
    return rowOff <= u.rowLen;
}

const UtfCol* FindCol(const Utf& u, const char* name, int type = -1)
{
    for (const UtfCol& c : u.cols)
        if (c.name == name && c.storage == 0x50 && (type < 0 || c.type == type)) return &c;
    return nullptr;
}

// The table in a top-table data column (row 0), parsed into `sub`.
bool SubTable(const uint8_t* d, size_t n, const Utf& top, const char* column, Utf& sub)
{
    const UtfCol* c = FindCol(top, column, 0x0B);
    if (!c || top.count < 1) return false;
    size_t at = top.at + top.data + be32(d + top.at + top.rows + c->rowOff);
    return ParseUtf(d, n, at, sub);
}

bool EditVoiceBank(const ReplayNpc& e, const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& note)
{
    if (!e.enabled || e.talkVoice.empty()) { note = "keeping Dastar's voice"; return false; }
    Utf top, names, cues;
    const UtfCol *cueName, *cueIndex;
    if (!ParseUtf(d, n, 0, top) || !SubTable(d, n, top, "CueNameTable", names) ||
        !SubTable(d, n, top, "CueTable", cues) || !(cueName = FindCol(names, "CueName", 0x0A)) ||
        !(cueIndex = FindCol(names, "CueIndex", 0x02))) {
        note = "unexpected ACB layout";
        return false;
    }
    size_t from = SIZE_MAX, to = SIZE_MAX;  // CueTable rows of the two cues
    for (size_t r = 0; r < names.count; ++r) {
        size_t row = names.at + names.rows + r * names.rowLen;
        size_t s = names.strings + be32(d + row + cueName->rowOff);
        if (s >= names.size) continue;
        const char* c = (const char*)d + names.at + s;
        std::string name(c, strnlen(c, names.size - s));
        if (name == e.npcVoiceCue) from = be16(d + row + cueIndex->rowOff);
        if (name == e.talkVoice) to = be16(d + row + cueIndex->rowOff);
    }
    if (from == SIZE_MAX) { note = e.npcVoiceCue + " not in the bank"; return false; }
    if (to == SIZE_MAX) { note = "talk_voice " + e.talkVoice + " not in the bank"; return false; }
    if (from >= cues.count || to >= cues.count) { note = "cue index out of range"; return false; }
    const char* fields[] = {"ReferenceType", "ReferenceIndex", "Length"};
    out.assign(d, d + n);
    for (const char* f : fields) {
        const UtfCol* c = FindCol(cues, f);
        if (!c) {  // a constant column (the same for every cue) needs no copy
            bool constant = false;
            for (const UtfCol& k : cues.cols) constant |= k.name == f && k.storage == 0x30;
            if (constant) continue;
            note = std::string("CueTable has no ") + f;
            return false;
        }
        size_t a = cues.at + cues.rows + from * cues.rowLen + c->rowOff;
        size_t b = cues.at + cues.rows + to * cues.rowLen + c->rowOff;
        memcpy(out.data() + a, d + b, UtfTypeSize(c->type));
    }
    note = e.npcVoiceCue + " (cue " + std::to_string(from) + ") now plays what " + e.talkVoice + " (cue " +
           std::to_string(to) + ") plays";
    return true;
}

}  // namespace

NpcFile ClassifyNpcFile(const std::string& p)
{
    // The path may carry a prefix, so only its tail is compared, from a '/'.
    auto ends = [&](const char* s) {
        size_t l = strlen(s);
        return p.size() >= l && !p.compare(p.size() - l, l, s) && (p.size() == l || p[p.size() - l - 1] == '/');
    };
    // `stem` + a two-letter language + `ext`, e.g. data/msg/lobby_text_en.msg.
    auto perLanguage = [&](const char* stem, const char* ext) {
        size_t sl = strlen(stem), el = strlen(ext), l = sl + 2 + el;
        if (p.size() < l || (p.size() > l && p[p.size() - l - 1] != '/')) return false;
        return !p.compare(p.size() - l, sl, stem) && !p.compare(p.size() - el, el, ext);
    };
    if (ends("data/event/lobby/lobby_event_manage.tsd")) return NpcFile::LobbyEvents;
    if (ends("data/event/lobby/lobby_npc_list.tnl")) return NpcFile::LobbyNpcs;
    if (perLanguage("data/msg/lobby_text_", ".msg")) return NpcFile::LobbyText;
    if (perLanguage("data/msg/menu_title_text_", ".msg")) return NpcFile::MenuTitles;
    if (perLanguage("data/ui/iggy/cmntitle_", ".iggytex")) return NpcFile::MenuTitleArt;
    if (ends("data/sound/vox/quest/tmq/caqr_all_vox.acb") || ends("data/sound/vox/quest/tmq/en/caqr_all_vox.acb"))
        return NpcFile::VoiceBank;
    return NpcFile::None;
}

bool EditNpcFile(NpcFile kind, const ReplayNpc& npc, const uint8_t* src, size_t n, std::vector<uint8_t>& out,
                 std::string& note)
{
    switch (kind) {
    case NpcFile::LobbyEvents: return EditLobbyEvents(npc, src, n, out, note);
    case NpcFile::LobbyNpcs: return EditLobbyNpcs(npc, src, n, out, note);
    case NpcFile::LobbyText: return EditLobbyText(npc, src, n, out, note);
    case NpcFile::MenuTitles: return EditMenuTitles(npc, src, n, out, note);
    case NpcFile::MenuTitleArt: return EditMenuTitleArt(npc, src, n, out, note);
    case NpcFile::VoiceBank: return EditVoiceBank(npc, src, n, out, note);
    default: return false;
    }
}

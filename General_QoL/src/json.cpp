#include "json.h"
#include <cstdlib>

const JVal* JVal::get(const char* key) const
{
    if (type != Obj) return nullptr;
    auto it = o.find(key);
    return it == o.end() ? nullptr : &it->second;
}
bool JVal::boolOr(const char* key, bool def) const
{
    const JVal* v = get(key);
    return v && v->type == Bool ? v->b : def;
}
double JVal::numOr(const char* key, double def) const
{
    const JVal* v = get(key);
    return v && v->type == Num ? v->n : def;
}
std::string JVal::strOr(const char* key, const char* def) const
{
    const JVal* v = get(key);
    return v && v->type == Str ? v->s : std::string(def);
}

namespace {
struct P {
    const std::string& t;
    size_t i = 0;
    std::string err;

    int line() const
    {
        int l = 1;
        for (size_t k = 0; k < i && k < t.size(); ++k) l += t[k] == '\n';
        return l;
    }
    bool fail(const char* what)
    {
        if (err.empty()) err = std::string(what) + " at line " + std::to_string(line());
        return false;
    }
    void ws()
    {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n')) ++i;
    }
    static void utf8(std::string& s, unsigned cp)
    {
        if (cp < 0x80) s += (char)cp;
        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F));
        } else {
            s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F));
            s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F));
        }
    }
    bool hex4(unsigned& v)
    {
        if (i + 4 > t.size()) return false;
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = t[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }
    bool str(std::string& s)
    {
        ++i;  // opening quote
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (c != '\\') { s += c; continue; }
            if (i >= t.size()) break;
            char e = t[i++];
            switch (e) {
            case '"': s += '"'; break;
            case '\\': s += '\\'; break;
            case '/': s += '/'; break;
            case 'b': s += '\b'; break;
            case 'f': s += '\f'; break;
            case 'n': s += '\n'; break;
            case 'r': s += '\r'; break;
            case 't': s += '\t'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return fail("bad \\u escape");
                if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                    i += 2;
                    unsigned lo;
                    if (!hex4(lo)) return fail("bad \\u escape");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                utf8(s, cp);
                break;
            }
            default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool val(JVal& v)
    {
        ws();
        if (i >= t.size()) return fail("unexpected end");
        char c = t[i];
        if (c == '{') {
            v.type = JVal::Obj;
            ++i; ws();
            if (i < t.size() && t[i] == '}') { ++i; return true; }
            for (;;) {
                ws();
                if (i >= t.size() || t[i] != '"') return fail("expected a \"key\"");
                std::string k;
                if (!str(k)) return false;
                ws();
                if (i >= t.size() || t[i] != ':') return fail("expected ':'");
                ++i;
                if (!val(v.o[k])) return false;
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == '}') { ++i; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = JVal::Arr;
            ++i; ws();
            if (i < t.size() && t[i] == ']') { ++i; return true; }
            for (;;) {
                v.a.emplace_back();
                if (!val(v.a.back())) return false;
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = JVal::Str; return str(v.s); }
        if (!t.compare(i, 4, "true"))  { v.type = JVal::Bool; v.b = true;  i += 4; return true; }
        if (!t.compare(i, 5, "false")) { v.type = JVal::Bool; v.b = false; i += 5; return true; }
        if (!t.compare(i, 4, "null"))  { v.type = JVal::Null; i += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            const char* start = t.c_str() + i;
            char* end = nullptr;
            v.type = JVal::Num;
            v.n = strtod(start, &end);
            if (end == start) return fail("bad number");
            i += end - start;
            return true;
        }
        return fail("unexpected character");
    }
};
}  // namespace

bool JsonParse(const std::string& text, JVal& out, std::string& err)
{
    P p{text};
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        p.i = 3;  // UTF-8 BOM (Notepad)
    out = JVal();
    bool ok = p.val(out);
    if (ok) {
        p.ws();
        if (p.i != text.size()) ok = p.fail("extra text after the config");
    }
    err = p.err;
    return ok;
}

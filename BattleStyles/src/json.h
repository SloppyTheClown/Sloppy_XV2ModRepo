// Minimal JSON reader for the mod's config: objects, arrays, strings (UTF-8,
// with the usual escapes), numbers, true/false/null.
#pragma once
#include <map>
#include <string>
#include <vector>

struct JVal {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JVal> a;
    std::map<std::string, JVal> o;

    const JVal* get(const char* key) const;          // null if missing or not an object
    bool        boolOr(const char* key, bool def) const;
    double      numOr(const char* key, double def) const;
    std::string strOr(const char* key, const char* def) const;
};

// Returns false and sets err (with a line number) on malformed input.
bool JsonParse(const std::string& text, JVal& out, std::string& err);

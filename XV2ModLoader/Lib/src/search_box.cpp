// ---------------------------------------------------------------------------
//  Search boxes for mods: a text field the loader draws over the game.
//
//  A mod asks for a box by id every frame its list is open (Lua search.Show,
//  or XV2Search_Show from a DLL mod - sdk\xv2_search.h) and reads what was
//  typed. The box shows while it is asked for and hides by itself shortly
//  after the last request, so a mod never has to say "hide". The loader
//  draws it in the overlay (overlay.cpp), which also keeps the typed keys
//  from the game, and focuses the newest box on Ctrl+F.
//
//  Boxes are kept here, not in a script's Lua state: DLL mods call from
//  their own threads, so every access goes through g_boxesLock. Only
//  DrawSearchBoxes (render thread) touches ImGui.
//
//  Matching ignores case for A-Z only (names are compared as UTF-8 bytes),
//  as Lua's string.lower did in Item Search before the box moved here.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
#include "lua.h"
#include "lauxlib.h"
#include "host.h"
#include "overlay.h"
#include "search_box.h"

constexpr size_t kMaxTextBytes = 128;             // what a box holds (UTF-8)
constexpr ULONGLONG kShownForMs = 100;            // a box stays up this long after the last Show
constexpr int kFocusKey = 'F';                    // with Ctrl: type in the newest box
static const char kTypingLine[] = "Enter: back to the list";

struct SearchBox {
    std::string hint;
    std::string note;                             // greyed-out line under the field
    float x = 0.0f, y = 0.0f, width = 0.3f;       // fractions of the game's picture
    std::string text;                             // as typed
    std::string query;                            // text in lower case
    unsigned changes = 0;                         // +1 for every change of query
    unsigned resets = 0;                          // +1 for every Clear/SetText (an edit drawn meanwhile is dropped)
    ULONGLONG shownAt = 0;                        // GetTickCount64 of the last Show
    unsigned shownInDraw = 0;                     // g_drawCount at the last Show
    ULONGLONG appearedAt = 0;                     // when it last came up (Ctrl+F picks the newest)
    bool typing = false;
    bool focusRequested = false;
    bool resetRequested = false;                  // the field must drop ImGui's own copy of the text
};

static SRWLOCK g_boxesLock = SRWLOCK_INIT;
static std::map<std::string, SearchBox> g_boxes;
static volatile unsigned g_drawCount = 0;         // DrawSearchBoxes calls so far

// `text` with A-Z made lower case (other bytes, UTF-8 included, untouched).
static std::string LowerAscii(std::string text)
{
    for (char& c : text)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return text;
}

// UTF-16 -> UTF-8 ("" for an empty or invalid string).
static std::string Utf8(const wchar_t* text)
{
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string out(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
    return out;
}

// Shown by the caller in this or the previous drawn frame (render-thread
// callers), or within kShownForMs (callers on other threads).
static bool IsShown(const SearchBox& box, ULONGLONG now)
{
    if (box.shownAt == 0) return false;
    return box.shownInDraw + 1 >= g_drawCount || now - box.shownAt <= kShownForMs;
}

// Sets the box's text (cut to kMaxTextBytes) and its lower-case query;
// counts a change when the query differs. Call with g_boxesLock held.
static void SetBoxText(SearchBox& box, const std::string& text)
{
    box.text = text.substr(0, kMaxTextBytes);
    const std::string query = LowerAscii(box.text);
    if (query != box.query) {
        box.query = query;
        ++box.changes;
    }
}

// True when `name` contains `query` (already lower case), ignoring case
// for A-Z, or the query is empty.
static bool Matches(const std::string& query, const std::string& name)
{
    return query.empty() || LowerAscii(name).find(query) != std::string::npos;
}

// ---------------------------------------------------------------------------
//  What both Lua and the exports call. The getters don't create a box.
// ---------------------------------------------------------------------------

// Marks the box `id` (created if new) as shown now and stores how to draw
// it; returns its change count. note: null keeps the box's note (the
// exports set it with XV2Search_SetNote).
static unsigned ShowBox(const std::string& id, const char* hint, float x, float y, float width, const char* note)
{
    const ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&g_boxesLock);
    SearchBox& box = g_boxes[id];
    if (!IsShown(box, now)) box.appearedAt = now;
    // x, y: where the box first comes up in a session (the player can drag it)
    box.x = x, box.y = y, box.width = width;
    box.hint = hint ? hint : "";
    if (note) box.note = note;
    box.shownAt = now;
    box.shownInDraw = g_drawCount;
    const unsigned changes = box.changes;
    ReleaseSRWLockExclusive(&g_boxesLock);
    return changes;
}

// Runs `change` on the box (created if new) under the lock.
template <typename Change>
static void ChangeBox(const std::string& id, Change change)
{
    AcquireSRWLockExclusive(&g_boxesLock);
    change(g_boxes[id]);
    ReleaseSRWLockExclusive(&g_boxesLock);
}

// Runs `read` on the box, or returns `missing` for an unknown id.
template <typename Result, typename Read>
static Result ReadBox(const std::string& id, Result missing, Read read)
{
    AcquireSRWLockShared(&g_boxesLock);
    const auto found = g_boxes.find(id);
    const Result result = found == g_boxes.end() ? missing : read(found->second);
    ReleaseSRWLockShared(&g_boxesLock);
    return result;
}

// Clear / SetText: replaces the text and makes the field drop the copy
// ImGui edits while it is active. Call with g_boxesLock held.
static void ResetBoxText(SearchBox& box, const std::string& text)
{
    SetBoxText(box, text);
    ++box.resets;
    box.resetRequested = true;
}

// ---------------------------------------------------------------------------
//  Drawing (render thread, inside the overlay's ImGui frame)
// ---------------------------------------------------------------------------

// True while any box is shown (see search_box.h).
bool SearchBoxesVisible()
{
    const ULONGLONG now = GetTickCount64();
    AcquireSRWLockShared(&g_boxesLock);
    bool visible = false;
    for (const auto& entry : g_boxes) visible = visible || IsShown(entry.second, now);
    ReleaseSRWLockShared(&g_boxesLock);
    return visible;
}

// What one frame draws of a box: copied out, so DLL threads aren't held up
// while ImGui runs.
struct BoxToDraw {
    std::string id;
    SearchBox box;
};

// Draws every shown box as its own small ImGui window, Ctrl+F focusing the
// newest, and stores what was typed (see search_box.h).
void DrawSearchBoxes()
{
    static bool focusKeyWasDown = false;
    const bool focusKeyDown = OverlayGameHasFocus() && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                              (GetAsyncKeyState(kFocusKey) & 0x8000);
    const bool focusPressed = focusKeyDown && !focusKeyWasDown;
    focusKeyWasDown = focusKeyDown;

    const ULONGLONG now = GetTickCount64();
    std::vector<BoxToDraw> boxes;
    AcquireSRWLockExclusive(&g_boxesLock);
    SearchBox* newest = nullptr;
    for (auto& entry : g_boxes) {
        SearchBox& box = entry.second;
        if (!IsShown(box, now)) {
            box.typing = false;
            continue;
        }
        if (!newest || box.appearedAt >= newest->appearedAt) newest = &box;
    }
    if (focusPressed && newest) newest->focusRequested = true;
    for (auto& entry : g_boxes) {
        SearchBox& box = entry.second;
        if (!IsShown(box, now)) continue;
        boxes.push_back({entry.first, box});
        box.focusRequested = box.resetRequested = false;
    }
    ++g_drawCount;
    ReleaseSRWLockExclusive(&g_boxesLock);

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav;
    for (auto& [id, box] : boxes) {
        ImGui::SetNextWindowPos(ImVec2(display.x * box.x, display.y * box.y), ImGuiCond_FirstUseEver);
        const std::string window = "##xv2search_" + id;
        bool typing = false, edited = false;
        char buffer[kMaxTextBytes + 1] = {};
        if (ImGui::Begin(window.c_str(), nullptr, flags)) {
            // Clear/SetText while the field is active: ImGui edits its own
            // copy then, so it has to let go of it to show the new text
            const ImGuiID fieldId = ImGui::GetID("##query");
            if (box.resetRequested && ImGui::GetActiveID() == fieldId) ImGui::ClearActiveID();
            ImGui::SetNextItemWidth(display.x * box.width);
            if (box.focusRequested) ImGui::SetKeyboardFocusHere();
            box.text.copy(buffer, kMaxTextBytes);
            edited = ImGui::InputTextWithHint("##query", box.hint.c_str(), buffer, sizeof(buffer));
            typing = ImGui::IsItemActive();
            if (typing) ImGui::TextDisabled("%s", kTypingLine);
            if (!box.note.empty()) ImGui::TextDisabled("%s", box.note.c_str());
        }
        ImGui::End();

        AcquireSRWLockExclusive(&g_boxesLock);
        const auto found = g_boxes.find(id);
        if (found != g_boxes.end()) {
            SearchBox& live = found->second;
            live.typing = typing;
            if (edited && live.resets == box.resets) SetBoxText(live, buffer);
        }
        ReleaseSRWLockExclusive(&g_boxesLock);
    }
}

// ---------------------------------------------------------------------------
//  For DLL mods (sdk\xv2_search.h documents each one). A null id is ignored:
//  getters return their "unknown box" value.
// ---------------------------------------------------------------------------

// Shows the box this frame; returns its change count.
extern "C" __declspec(dllexport) unsigned XV2Search_Show(const char* id, const char* hint, float x, float y,
                                                         float width)
{
    if (!id) return 0;
    if (g_host) g_host->EnsureFrameHook();   // a mod without Lua scripts still gets its box drawn
    return ShowBox(id, hint, x, y, width, nullptr);
}

// Sets (or, with null / "", removes) the greyed-out line under the box.
extern "C" __declspec(dllexport) void XV2Search_SetNote(const char* id, const char* note)
{
    if (id) ChangeBox(id, [&](SearchBox& box) { box.note = note ? note : ""; });
}

// Copies the lower-case query into out (cut, 0-terminated); returns its full length.
extern "C" __declspec(dllexport) int XV2Search_Query(const char* id, char* out, int outBytes)
{
    const std::string query = id ? ReadBox(id, std::string(), [](const SearchBox& box) { return box.query; }) : "";
    if (out && outBytes > 0) {
        const size_t copied = query.copy(out, static_cast<size_t>(outBytes) - 1);
        out[copied] = '\0';
    }
    return static_cast<int>(query.size());
}

// 1 when the UTF-8 name contains the query (or the box is empty / unknown).
extern "C" __declspec(dllexport) int XV2Search_Matches(const char* id, const char* name)
{
    if (!id) return 1;
    const std::string query = ReadBox(id, std::string(), [](const SearchBox& box) { return box.query; });
    return Matches(query, name ? std::string(name) : std::string()) ? 1 : 0;
}

// 1 when the UTF-16 name contains the query (or the box is empty / unknown).
extern "C" __declspec(dllexport) int XV2Search_MatchesW(const char* id, const wchar_t* name)
{
    if (!id) return 1;
    const std::string query = ReadBox(id, std::string(), [](const SearchBox& box) { return box.query; });
    return Matches(query, name ? Utf8(name) : std::string()) ? 1 : 0;
}

// 1 while the player is typing in the box.
extern "C" __declspec(dllexport) int XV2Search_Typing(const char* id)
{
    return id && ReadBox(id, false, [](const SearchBox& box) { return box.typing; }) ? 1 : 0;
}

// Puts the keyboard in the box the next time it is drawn.
extern "C" __declspec(dllexport) void XV2Search_Focus(const char* id)
{
    if (id) ChangeBox(id, [](SearchBox& box) { box.focusRequested = true; });
}

// Empties the box.
extern "C" __declspec(dllexport) void XV2Search_Clear(const char* id)
{
    if (id) ChangeBox(id, [](SearchBox& box) { ResetBoxText(box, ""); });
}

// Replaces the box's text (UTF-8; null = "").
extern "C" __declspec(dllexport) void XV2Search_SetText(const char* id, const char* text)
{
    if (id) ChangeBox(id, [&](SearchBox& box) { ResetBoxText(box, text ? text : ""); });
}

// The box's change count (+1 per query change); 0 for an unknown box.
extern "C" __declspec(dllexport) unsigned XV2Search_Changes(const char* id)
{
    return id ? ReadBox(id, 0u, [](const SearchBox& box) { return box.changes; }) : 0;
}

// ---------------------------------------------------------------------------
//  Lua: the "search" table - the same boxes as the exports (an id typed in
//  Lua and in a DLL is one box). sdk\modloader_lib.lua documents each one:
//
//      search.Show(id, {hint = "", x = 0, y = 0, width = 0.3, note = nil}) -> changes
//      search.SetNote(id, note)     search.Query(id) -> text
//      search.Matches(id, name) -> bool          search.Typing(id) -> bool
//      search.Focus(id)    search.Clear(id)    search.SetText(id, text)
//      search.Changes(id) -> count
// ---------------------------------------------------------------------------

// The number field `field` of the options table at `table`; `fallback` when
// it is absent. Raises a Lua error when it is not a number.
static float OptionNumber(lua_State* L, int table, const char* field, float fallback)
{
    lua_getfield(L, table, field);
    const float value = lua_isnil(L, -1) ? fallback : static_cast<float>(luaL_checknumber(L, -1));
    lua_pop(L, 1);
    return value;
}

// The string field `field` of the options table at `table`; "" when absent.
static std::string OptionString(lua_State* L, int table, const char* field)
{
    lua_getfield(L, table, field);
    const std::string value = lua_isnil(L, -1) ? std::string() : luaL_checkstring(L, -1);
    lua_pop(L, 1);
    return value;
}

// search.Show(id, options = nil) -> changes
static int SearchShow(lua_State* L)
{
    const std::string id = luaL_checkstring(L, 1);
    std::string hint, note;
    bool hasNote = false;                         // no `note`: keep the box's note (another mod may own it)
    float x = 0.0f, y = 0.0f, width = 0.3f;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        hint = OptionString(L, 2, "hint");
        lua_getfield(L, 2, "note");
        hasNote = !lua_isnil(L, -1);
        lua_pop(L, 1);
        note = OptionString(L, 2, "note");
        x = OptionNumber(L, 2, "x", x);
        y = OptionNumber(L, 2, "y", y);
        width = OptionNumber(L, 2, "width", width);
    }
    lua_pushinteger(L, ShowBox(id, hint.c_str(), x, y, width, hasNote ? note.c_str() : nullptr));
    return 1;
}

// search.SetNote(id, note)   (nil or "" removes the line)
static int SearchSetNote(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    XV2Search_SetNote(id, luaL_optstring(L, 2, ""));
    return 0;
}

// search.Query(id) -> text   (lower case; "" for an empty or unknown box)
static int SearchQuery(lua_State* L)
{
    const std::string query = ReadBox(luaL_checkstring(L, 1), std::string(), [](const SearchBox& box) { return box.query; });
    lua_pushlstring(L, query.data(), query.size());
    return 1;
}

// search.Matches(id, name) -> bool
static int SearchMatches(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    lua_pushboolean(L, XV2Search_Matches(id, luaL_checkstring(L, 2)));
    return 1;
}

// search.Typing(id) -> bool
static int SearchTyping(lua_State* L)
{
    lua_pushboolean(L, XV2Search_Typing(luaL_checkstring(L, 1)));
    return 1;
}

// search.Focus(id)
static int SearchFocus(lua_State* L)
{
    XV2Search_Focus(luaL_checkstring(L, 1));
    return 0;
}

// search.Clear(id)
static int SearchClear(lua_State* L)
{
    XV2Search_Clear(luaL_checkstring(L, 1));
    return 0;
}

// search.SetText(id, text)
static int SearchSetText(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    XV2Search_SetText(id, luaL_checkstring(L, 2));
    return 0;
}

// search.Changes(id) -> count
static int SearchChanges(lua_State* L)
{
    lua_pushinteger(L, XV2Search_Changes(luaL_checkstring(L, 1)));
    return 1;
}

void RegisterLuaSearch(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {"Show", SearchShow},       {"SetNote", SearchSetNote}, {"Query", SearchQuery},
        {"Matches", SearchMatches}, {"Typing", SearchTyping},   {"Focus", SearchFocus},
        {"Clear", SearchClear},     {"SetText", SearchSetText}, {"Changes", SearchChanges},
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);
    lua_setglobal(L, "search");
}

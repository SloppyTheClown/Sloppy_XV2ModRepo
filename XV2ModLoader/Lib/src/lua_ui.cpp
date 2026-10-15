// ---------------------------------------------------------------------------
//  The "ui" table: Dear ImGui widgets for the in-game menu (overlay.cpp).
//
//  A script that defines _OnMenu() gets a collapsible section in the menu
//  (~); _OnMenu runs every frame while the menu is open and draws with these
//  (sdk\modloader_lib.lua documents each one for script authors):
//
//    Text
//      ui.Text(text)    ui.TextDisabled(text)    ui.TextWrapped(text)
//      ui.TextColored(text, r, g, b, a = 1)       (colour parts 0..1)
//      ui.TextOutlined(text)                      (white text, black outline)
//    Layout
//      ui.Separator()   ui.Spacing()   ui.SameLine(offsetX = 0, spacing = -1)
//      ui.Indent(width = 0)   ui.Unindent(width = 0)      (0 = the style's indent)
//      ui.PushID(id)    ui.PopID()     (id: string or integer; tells widgets
//                                       with equal labels apart, e.g. in loops)
//    Widgets ("changed" = the player changed the value this frame)
//      ui.Button(label, width = 0, height = 0) -> clicked     (0 = fit the label)
//      ui.Checkbox(label, value) -> value, changed
//      ui.SliderFloat(label, value, min, max, format = "%.2f") -> value, changed
//      ui.SliderInt(label, value, min, max, format = "%d") -> value, changed
//      ui.InputFloat(label, value, step = 0, format = "%.3f") -> value, changed
//      ui.InputInt(label, value, step = 1) -> value, changed
//      ui.InputText(label, text, maxBytes = 256, hint = nil) -> text, changed
//          (Enter or a click elsewhere ends typing; Esc undoes the edit)
//      ui.BeginCombo(label, preview, maxRows = 8) -> open, ui.EndCombo() (only when open)
//          (a dropdown: draw ui.Selectable rows between the two)
//      ui.Selectable(label, selected = false) -> clicked
//      ui.SetItemDefaultFocus()   (scroll an open dropdown to the last widget)
//      ui.CollapsingHeader(label, defaultOpen = false) -> open
//      ui.TreeNode(label) -> open, ui.TreePop() (only when open)
//    About the last widget
//      ui.IsItemActive() -> bool    ui.IsItemHovered() -> bool
//      ui.SetTooltip(text)          (a tooltip at the cursor this frame;
//                                    usually: if ui.IsItemHovered() then ... end)
//    Windows
//      ui.Begin(title, flags = 0) -> visible, ui.End()   (a window of its own;
//          always call End, also when visible is false; flags: sum of
//          ui.WindowFlags.NoTitleBar, .AlwaysAutoResize, ...)
//      ui.SetNextWindowPos(x, y, firstUseOnly = false, pivotX = 0, pivotY = 0)
//          (pixels of the game's picture; pivot (1, 0) puts the window's top
//          right corner at x, y)
//      ui.SetNextWindowSize(width, height, firstUseOnly = false)   (pixels)
//      ui.SetNextItemWidth(width)   ui.SetKeyboardFocusHere()   (the next widget)
//      ui.GetDisplaySize() -> width, height       (the game's picture, pixels)
//
//  Callable any time:
//      ui.IsMenuOpen() -> bool
//      ui.ShowOverlay()     draw the overlay this frame even with the menu
//                           closed: every script's _OnOverlay() then runs and
//                           can draw windows of its own over the game (call
//                           it from _OnFrame each frame the windows should
//                           show). While the cursor is over such a window the
//                           game gets no clicks, and while a text field is
//                           active it gets no key presses.
//
//  Every other function raises a Lua error outside _OnMenu / _OnOverlay.
//  Labels follow ImGui's rules: two widgets with the same label in one
//  section need "##suffix" ("Speed##fly") or a ui.PushID around them; text
//  after "##" is not shown. Anything a script leaves open (Begin without
//  End, PushID without PopID, an error halfway) is closed for it.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cfloat>
#include <iterator>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "lua.h"
#include "lauxlib.h"
#include "lua_ui.h"
#include "overlay.h"

// Every drawing function starts with this: ImGui only accepts widgets
// between NewFrame and Render, which is while _OnMenu / _OnOverlay run.
static void RequireMenuFrame(lua_State* L, const char* function)
{
    if (!OverlayDrawing()) luaL_error(L, "ui.%s can only be used inside _OnMenu or _OnOverlay", function);
}

// Argument `index` as a float (raises a Lua error when it is not a number).
static float CheckFloat(lua_State* L, int index)
{
    return static_cast<float>(luaL_checknumber(L, index));
}

// Argument `index` as a float, `fallback` when absent or nil.
static float OptFloat(lua_State* L, int index, float fallback)
{
    return static_cast<float>(luaL_optnumber(L, index, fallback));
}

// Argument `index` as an int, `fallback` when absent or nil.
static int OptInt(lua_State* L, int index, int fallback)
{
    return static_cast<int>(luaL_optinteger(L, index, fallback));
}

// Value widgets push their new value, then call this to push whether the
// player changed it. Returns 2: the value and the flag.
static int PushChanged(lua_State* L, bool changed)
{
    lua_pushboolean(L, changed);
    return 2;
}

// ---------------------------------------------------------------------------
//  Text. Every text function takes any value (tostring is applied: numbers,
//  booleans and nil print as Lua prints them).
// ---------------------------------------------------------------------------

// ui.Text(text)
static int UiText(lua_State* L)
{
    RequireMenuFrame(L, "Text");
    size_t length = 0;
    const char* text = luaL_tolstring(L, 1, &length);
    ImGui::TextUnformatted(text, text + length);
    return 0;
}

// ui.TextDisabled(text)   (greyed out)
static int UiTextDisabled(lua_State* L)
{
    RequireMenuFrame(L, "TextDisabled");
    size_t length = 0;
    const char* text = luaL_tolstring(L, 1, &length);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextUnformatted(text, text + length);
    ImGui::PopStyleColor();
    return 0;
}

// ui.TextWrapped(text)   (breaks lines at the window's right edge)
static int UiTextWrapped(lua_State* L)
{
    RequireMenuFrame(L, "TextWrapped");
    size_t length = 0;
    const char* text = luaL_tolstring(L, 1, &length);
    ImGui::PushTextWrapPos(0.0f);                 // 0 = wrap at the window's edge
    ImGui::TextUnformatted(text, text + length);
    ImGui::PopTextWrapPos();
    return 0;
}

// ui.TextColored(text, r, g, b, a = 1)   (colour parts 0..1)
static int UiTextColored(lua_State* L)
{
    RequireMenuFrame(L, "TextColored");
    const ImVec4 color(CheckFloat(L, 2), CheckFloat(L, 3), CheckFloat(L, 4), OptFloat(L, 5, 1.0f));
    size_t length = 0;
    const char* text = luaL_tolstring(L, 1, &length);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text, text + length);
    ImGui::PopStyleColor();
    return 0;
}

// ui.TextOutlined(text)
// Text with a 1-pixel black outline (the text drawn black at the 8
// neighbouring offsets, then white on top), readable over any game picture.
static int UiTextOutlined(lua_State* L)
{
    RequireMenuFrame(L, "TextOutlined");
    size_t length = 0;
    const char* text = luaL_tolstring(L, 1, &length);
    const ImVec2 size = ImGui::CalcTextSize(text, text + length);
    // one pixel of room on every side for the outline
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 position(origin.x + 1, origin.y + 1);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 outlineColor = IM_COL32(0, 0, 0, 255);
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            if (offsetX == 0 && offsetY == 0) continue;
            drawList->AddText(ImVec2(position.x + offsetX, position.y + offsetY), outlineColor, text, text + length);
        }
    }
    drawList->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), text, text + length);
    ImGui::Dummy(ImVec2(size.x + 2, size.y + 2));
    return 0;
}

// ---------------------------------------------------------------------------
//  Layout
// ---------------------------------------------------------------------------

// ui.Separator()   (a horizontal line)
static int UiSeparator(lua_State* L)
{
    RequireMenuFrame(L, "Separator");
    ImGui::Separator();
    return 0;
}

// ui.Spacing()   (a little vertical space)
static int UiSpacing(lua_State* L)
{
    RequireMenuFrame(L, "Spacing");
    ImGui::Spacing();
    return 0;
}

// ui.SameLine(offsetX = 0, spacing = -1)
// The next widget goes right of the last one instead of below it.
//   offsetX  > 0: pixels from the window's left edge to place it at
//   spacing  pixels between the two; -1 = the style's spacing
static int UiSameLine(lua_State* L)
{
    RequireMenuFrame(L, "SameLine");
    ImGui::SameLine(OptFloat(L, 1, 0.0f), OptFloat(L, 2, -1.0f));
    return 0;
}

// ui.Indent(width = 0)   (moves the following widgets right; 0 = the style's indent)
static int UiIndent(lua_State* L)
{
    RequireMenuFrame(L, "Indent");
    ImGui::Indent(OptFloat(L, 1, 0.0f));
    return 0;
}

// ui.Unindent(width = 0)   (undoes an Indent of the same width)
static int UiUnindent(lua_State* L)
{
    RequireMenuFrame(L, "Unindent");
    ImGui::Unindent(OptFloat(L, 1, 0.0f));
    return 0;
}

// ui.PushID(id)   (id: string or integer)
// Widgets until the matching PopID get `id` mixed into theirs, so equal
// labels in a loop ("Give" once per item) stay apart.
static int UiPushID(lua_State* L)
{
    RequireMenuFrame(L, "PushID");
    if (lua_isinteger(L, 1)) {
        ImGui::PushID(static_cast<int>(lua_tointeger(L, 1)));
    } else {
        size_t length = 0;
        const char* id = luaL_checklstring(L, 1, &length);
        ImGui::PushID(id, id + length);
    }
    return 0;
}

// ui.PopID()
static int UiPopID(lua_State* L)
{
    RequireMenuFrame(L, "PopID");
    ImGui::PopID();
    return 0;
}

// ---------------------------------------------------------------------------
//  Widgets. A value widget takes the current value and returns the new one
//  plus whether the player changed it this frame:
//      speed, changed = ui.SliderFloat("Speed", speed, 0, 10)
// ---------------------------------------------------------------------------

// ui.Button(label, width = 0, height = 0) -> clicked   (0 = fit the label)
static int UiButton(lua_State* L)
{
    RequireMenuFrame(L, "Button");
    const char* label = luaL_checkstring(L, 1);
    lua_pushboolean(L, ImGui::Button(label, ImVec2(OptFloat(L, 2, 0.0f), OptFloat(L, 3, 0.0f))));
    return 1;
}

// ui.Checkbox(label, value) -> value, changed   (value: any; truthy = ticked)
static int UiCheckbox(lua_State* L)
{
    RequireMenuFrame(L, "Checkbox");
    bool value = lua_toboolean(L, 2);
    const bool changed = ImGui::Checkbox(luaL_checkstring(L, 1), &value);
    lua_pushboolean(L, value);
    return PushChanged(L, changed);
}

// ui.SliderFloat(label, value, min, max, format = "%.2f") -> value, changed
//   format  printf format for the number shown on the slider
static int UiSliderFloat(lua_State* L)
{
    RequireMenuFrame(L, "SliderFloat");
    float value = CheckFloat(L, 2);
    const bool changed =
        ImGui::SliderFloat(luaL_checkstring(L, 1), &value, CheckFloat(L, 3), CheckFloat(L, 4), luaL_optstring(L, 5, "%.2f"));
    lua_pushnumber(L, value);
    return PushChanged(L, changed);
}

// ui.SliderInt(label, value, min, max, format = "%d") -> value, changed
static int UiSliderInt(lua_State* L)
{
    RequireMenuFrame(L, "SliderInt");
    int value = static_cast<int>(luaL_checkinteger(L, 2));
    const bool changed = ImGui::SliderInt(luaL_checkstring(L, 1), &value, static_cast<int>(luaL_checkinteger(L, 3)),
                                          static_cast<int>(luaL_checkinteger(L, 4)), luaL_optstring(L, 5, "%d"));
    lua_pushinteger(L, value);
    return PushChanged(L, changed);
}

// ui.InputFloat(label, value, step = 0, format = "%.3f") -> value, changed
//   step  > 0 adds -/+ buttons that change the value by step
static int UiInputFloat(lua_State* L)
{
    RequireMenuFrame(L, "InputFloat");
    float value = CheckFloat(L, 2);
    const bool changed =
        ImGui::InputFloat(luaL_checkstring(L, 1), &value, OptFloat(L, 3, 0.0f), 0.0f, luaL_optstring(L, 4, "%.3f"));
    lua_pushnumber(L, value);
    return PushChanged(L, changed);
}

// ui.InputInt(label, value, step = 1) -> value, changed
//   step  the -/+ buttons' change; 0 = no buttons
static int UiInputInt(lua_State* L)
{
    RequireMenuFrame(L, "InputInt");
    int value = static_cast<int>(luaL_checkinteger(L, 2));
    const int step = OptInt(L, 3, 1);
    const bool changed = ImGui::InputInt(luaL_checkstring(L, 1), &value, step, step * 10);
    lua_pushinteger(L, value);
    return PushChanged(L, changed);
}

// ui.InputText(label, text, maxBytes = 256, hint = nil) -> text, changed
// A UTF-8 text field. The text lives in Lua; ImGui edits a copy each frame
// (it keeps its own copy while the field is active).
//   maxBytes  longest text accepted, 1..65536 bytes
//   hint      greyed-out text shown while the field is empty
static int UiInputText(lua_State* L)
{
    RequireMenuFrame(L, "InputText");
    const char* label = luaL_checkstring(L, 1);
    size_t length = 0;
    const char* text = luaL_optlstring(L, 2, "", &length);
    const lua_Integer maxBytes = luaL_optinteger(L, 3, 256);
    luaL_argcheck(L, maxBytes > 0 && maxBytes <= 0x10000, 3, "maxBytes must be 1..65536");
    const char* hint = luaL_optstring(L, 4, nullptr);
    std::string buffer(text, std::min(length, static_cast<size_t>(maxBytes)));
    buffer.resize(static_cast<size_t>(maxBytes) + 1, '\0');
    const bool changed = hint ? ImGui::InputTextWithHint(label, hint, buffer.data(), buffer.size())
                              : ImGui::InputText(label, buffer.data(), buffer.size());
    lua_pushstring(L, buffer.c_str());
    return PushChanged(L, changed);
}

// ui.BeginCombo(label, preview = "", maxRows = 8) -> open
// A dropdown showing `preview` while closed. When open, draw its rows
// (ui.Selectable) and then call ui.EndCombo() - only when open.
//   maxRows  rows its list shows before it scrolls (1..100)
static int UiBeginCombo(lua_State* L)
{
    RequireMenuFrame(L, "BeginCombo");
    const char* label = luaL_checkstring(L, 1);
    const char* preview = luaL_optstring(L, 2, "");
    const lua_Integer maxRows = luaL_optinteger(L, 3, 8);
    luaL_argcheck(L, maxRows > 0 && maxRows <= 100, 3, "maxRows must be 1..100");
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
    const ImGuiStyle& style = ImGui::GetStyle();
    // same sum ImGui uses for its HeightRegular etc.: rows + the popup's padding
    const float height = rowHeight * static_cast<float>(maxRows) - style.ItemSpacing.y + style.WindowPadding.y * 2;
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, height));
    lua_pushboolean(L, ImGui::BeginCombo(label, preview));
    return 1;
}

// ui.EndCombo()   (after an open BeginCombo)
static int UiEndCombo(lua_State* L)
{
    RequireMenuFrame(L, "EndCombo");
    ImGui::EndCombo();
    return 0;
}

// ui.Selectable(label, selected = false) -> clicked
// A clickable row (a dropdown entry, or a list row anywhere); `selected`
// draws it highlighted. Inside a dropdown a click also closes it.
static int UiSelectable(lua_State* L)
{
    RequireMenuFrame(L, "Selectable");
    lua_pushboolean(L, ImGui::Selectable(luaL_checkstring(L, 1), lua_toboolean(L, 2)));
    return 1;
}

// ui.SetItemDefaultFocus()   (an open dropdown scrolls to the last widget)
static int UiSetItemDefaultFocus(lua_State* L)
{
    RequireMenuFrame(L, "SetItemDefaultFocus");
    ImGui::SetItemDefaultFocus();
    return 0;
}

// ui.CollapsingHeader(label, defaultOpen = false) -> open
// A header bar the player opens and closes; draw its contents while open
// (no closing call needed).
static int UiCollapsingHeader(lua_State* L)
{
    RequireMenuFrame(L, "CollapsingHeader");
    const ImGuiTreeNodeFlags flags = lua_toboolean(L, 2) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    lua_pushboolean(L, ImGui::CollapsingHeader(luaL_checkstring(L, 1), flags));
    return 1;
}

// ui.TreeNode(label) -> open   (indented contents; call ui.TreePop() only when open)
static int UiTreeNode(lua_State* L)
{
    RequireMenuFrame(L, "TreeNode");
    lua_pushboolean(L, ImGui::TreeNode(luaL_checkstring(L, 1)));
    return 1;
}

// ui.TreePop()
static int UiTreePop(lua_State* L)
{
    RequireMenuFrame(L, "TreePop");
    ImGui::TreePop();
    return 0;
}

// ---------------------------------------------------------------------------
//  About the last widget drawn
// ---------------------------------------------------------------------------

// ui.IsItemActive() -> bool   (held down / being typed in)
static int UiIsItemActive(lua_State* L)
{
    RequireMenuFrame(L, "IsItemActive");
    lua_pushboolean(L, ImGui::IsItemActive());
    return 1;
}

// ui.IsItemHovered() -> bool   (the mouse is over it)
static int UiIsItemHovered(lua_State* L)
{
    RequireMenuFrame(L, "IsItemHovered");
    lua_pushboolean(L, ImGui::IsItemHovered());
    return 1;
}

// ui.SetTooltip(text)   (a tooltip next to the mouse, this frame only)
static int UiSetTooltip(lua_State* L)
{
    RequireMenuFrame(L, "SetTooltip");
    const char* text = luaL_tolstring(L, 1, nullptr);
    ImGui::SetTooltip("%s", text);
    return 0;
}

// ---------------------------------------------------------------------------
//  Windows
// ---------------------------------------------------------------------------

// ui.Begin(title, flags = 0) -> visible
// Starts a window of its own (the title is also its id: the same title
// continues the same window). Always pair with ui.End(), also when visible
// is false (collapsed or off screen: skip drawing the contents then).
static int UiBegin(lua_State* L)
{
    RequireMenuFrame(L, "Begin");
    const auto flags = static_cast<ImGuiWindowFlags>(luaL_optinteger(L, 2, 0));
    lua_pushboolean(L, ImGui::Begin(luaL_checkstring(L, 1), nullptr, flags));
    return 1;
}

// ui.End()
static int UiEnd(lua_State* L)
{
    RequireMenuFrame(L, "End");
    ImGui::End();
    return 0;
}

// ui.SetNextWindowPos(x, y, firstUseOnly = false, pivotX = 0, pivotY = 0)
// Places the next ui.Begin window at x, y (pixels of the game's picture).
//   firstUseOnly  true: only the first time (the player can move it after)
//   pivot         which point of the window lands on (x, y), as fractions of
//                 its size: (0, 0) top left, (1, 0) top right, (0.5, 0.5) centre
static int UiSetNextWindowPos(lua_State* L)
{
    RequireMenuFrame(L, "SetNextWindowPos");
    const ImVec2 pivot(OptFloat(L, 4, 0.0f), OptFloat(L, 5, 0.0f));
    ImGui::SetNextWindowPos(ImVec2(CheckFloat(L, 1), CheckFloat(L, 2)),
                            lua_toboolean(L, 3) ? ImGuiCond_FirstUseEver : ImGuiCond_Always, pivot);
    return 0;
}

// ui.SetNextWindowSize(width, height, firstUseOnly = false)   (pixels; 0 = fit the contents)
static int UiSetNextWindowSize(lua_State* L)
{
    RequireMenuFrame(L, "SetNextWindowSize");
    ImGui::SetNextWindowSize(ImVec2(CheckFloat(L, 1), CheckFloat(L, 2)),
                             lua_toboolean(L, 3) ? ImGuiCond_FirstUseEver : ImGuiCond_Always);
    return 0;
}

// ui.SetNextItemWidth(width)   (pixels for the next widget's field; negative = up to that far from the right edge)
static int UiSetNextItemWidth(lua_State* L)
{
    RequireMenuFrame(L, "SetNextItemWidth");
    ImGui::SetNextItemWidth(CheckFloat(L, 1));
    return 0;
}

// ui.SetKeyboardFocusHere()   (puts the keyboard in the next widget, e.g. a text field)
static int UiSetKeyboardFocusHere(lua_State* L)
{
    RequireMenuFrame(L, "SetKeyboardFocusHere");
    ImGui::SetKeyboardFocusHere();
    return 0;
}

// ui.GetDisplaySize() -> width, height   (the game's picture, pixels)
static int UiGetDisplaySize(lua_State* L)
{
    RequireMenuFrame(L, "GetDisplaySize");
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    lua_pushnumber(L, size.x);
    lua_pushnumber(L, size.y);
    return 2;
}

// ---------------------------------------------------------------------------
//  Callable any time (e.g. from _OnFrame)
// ---------------------------------------------------------------------------

// ui.IsMenuOpen() -> bool
static int UiIsMenuOpen(lua_State* L)
{
    lua_pushboolean(L, OverlayMenuOpen());
    return 1;
}

// ui.ShowOverlay()   (this frame: run every script's _OnOverlay, menu open or not)
static int UiShowOverlay(lua_State*)
{
    OverlayRequest();
    return 0;
}

// Puts the "ui" table (with ui.WindowFlags) into a script's state.
void RegisterLuaUi(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {"Text", UiText},
        {"TextDisabled", UiTextDisabled},
        {"TextWrapped", UiTextWrapped},
        {"TextColored", UiTextColored},
        {"TextOutlined", UiTextOutlined},
        {"Separator", UiSeparator},
        {"Spacing", UiSpacing},
        {"SameLine", UiSameLine},
        {"Indent", UiIndent},
        {"Unindent", UiUnindent},
        {"PushID", UiPushID},
        {"PopID", UiPopID},
        {"Button", UiButton},
        {"Checkbox", UiCheckbox},
        {"SliderFloat", UiSliderFloat},
        {"SliderInt", UiSliderInt},
        {"InputFloat", UiInputFloat},
        {"InputInt", UiInputInt},
        {"InputText", UiInputText},
        {"BeginCombo", UiBeginCombo},
        {"EndCombo", UiEndCombo},
        {"Selectable", UiSelectable},
        {"SetItemDefaultFocus", UiSetItemDefaultFocus},
        {"CollapsingHeader", UiCollapsingHeader},
        {"TreeNode", UiTreeNode},
        {"TreePop", UiTreePop},
        {"IsItemActive", UiIsItemActive},
        {"IsItemHovered", UiIsItemHovered},
        {"SetTooltip", UiSetTooltip},
        {"Begin", UiBegin},
        {"End", UiEnd},
        {"SetNextWindowPos", UiSetNextWindowPos},
        {"SetNextWindowSize", UiSetNextWindowSize},
        {"SetNextItemWidth", UiSetNextItemWidth},
        {"SetKeyboardFocusHere", UiSetKeyboardFocusHere},
        {"GetDisplaySize", UiGetDisplaySize},
        {"IsMenuOpen", UiIsMenuOpen},
        {"ShowOverlay", UiShowOverlay},
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);

    // ui.WindowFlags.X: ImGui's window flags for ui.Begin (add them up)
    static const struct { const char* name; ImGuiWindowFlags value; } windowFlags[] = {
        {"NoTitleBar", ImGuiWindowFlags_NoTitleBar},
        {"NoResize", ImGuiWindowFlags_NoResize},
        {"NoMove", ImGuiWindowFlags_NoMove},
        {"NoScrollbar", ImGuiWindowFlags_NoScrollbar},
        {"NoCollapse", ImGuiWindowFlags_NoCollapse},
        {"AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize},
        {"NoBackground", ImGuiWindowFlags_NoBackground},
        {"NoSavedSettings", ImGuiWindowFlags_NoSavedSettings},
        {"NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing},
        {"NoNav", ImGuiWindowFlags_NoNav},
        {"NoDecoration", ImGuiWindowFlags_NoDecoration},
    };
    lua_createtable(L, 0, static_cast<int>(std::size(windowFlags)));
    for (const auto& flag : windowFlags) {
        lua_pushinteger(L, flag.value);
        lua_setfield(L, -2, flag.name);
    }
    lua_setfield(L, -2, "WindowFlags");
    lua_setglobal(L, "ui");
}

// The script's section of the menu: a header with its name, then _OnMenu.
// ImGui's error recovery closes whatever the script left open (a Begin
// without End, an error halfway through a TreeNode) so one script can't
// break the menu for the others. False when _OnMenu raised an error.
bool DrawScriptMenu(lua_State* L, const std::string& title, bool (*callOnMenu)(lua_State*))
{
    if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return true;
    ImGui::PushID(title.c_str());
    ImGuiErrorRecoveryState state;
    ImGui::ErrorRecoveryStoreState(&state);
    const bool ok = callOnMenu(L);
    ImGui::ErrorRecoveryTryToRecoverState(&state);
    ImGui::PopID();
    return ok;
}

// Same recovery for _OnOverlay; no header, the script draws its own windows.
bool DrawScriptOverlay(lua_State* L, const std::string& title, bool (*callOnOverlay)(lua_State*))
{
    ImGui::PushID(title.c_str());
    ImGuiErrorRecoveryState state;
    ImGui::ErrorRecoveryStoreState(&state);
    const bool ok = callOnOverlay(L);
    ImGui::ErrorRecoveryTryToRecoverState(&state);
    ImGui::PopID();
    return ok;
}

// A greyed-out line in the menu (e.g. "No Lua script has a menu yet").
void DrawMenuNote(const char* text)
{
    ImGui::TextDisabled("%s", text);
}

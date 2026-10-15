// The "ui" table (lua_ui.cpp) and the scripts' sections of the in-game menu.
#pragma once
#include <string>

struct lua_State;

void RegisterLuaUi(lua_State* state);

// One script's menu section: a header titled `title`, then callOnMenu(state)
// runs its _OnMenu. False when that failed.
bool DrawScriptMenu(lua_State* state, const std::string& title, bool (*callOnMenu)(lua_State*));
// The script's _OnOverlay (callOnOverlay), with whatever it leaves open
// closed again. False when it failed.
bool DrawScriptOverlay(lua_State* state, const std::string& title, bool (*callOnOverlay)(lua_State*));
void DrawMenuNote(const char* text);     // greyed-out line in the menu

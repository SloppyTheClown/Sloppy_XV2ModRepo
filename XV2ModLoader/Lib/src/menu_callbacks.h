// A menu's ActionScript -> mod messages for Lua scripts (the "menus" table) -
// see menu_callbacks.cpp.
#pragma once

struct lua_State;

// Puts the "menus" table into a script's state (a host Lua registrar).
void RegisterLuaMenus(lua_State* state);

// Detaches every script's callbacks (F1 reload: the scripts hook again);
// the vtable slots stay chained and pass every call on.
void ClearScriptMenuCallbacks();

// Search boxes this library draws for mods (search_box.cpp): Lua's "search"
// table and the XV2Search_* exports (sdk\xv2_search.h).
#pragma once

struct lua_State;

// True while some box is being asked for: the overlay is drawn for it even
// when no script called ui.ShowOverlay().
bool SearchBoxesVisible();

// Draws the visible boxes; overlay.cpp calls it inside each ImGui frame, on
// the render thread.
void DrawSearchBoxes();

// Puts the "search" table into a script's state.
void RegisterLuaSearch(lua_State* state);

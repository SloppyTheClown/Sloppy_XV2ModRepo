// The in-game menu: a Dear ImGui window drawn over the game's frame from the
// loader's Present hook (lib_main.cpp OnFrame). ~ (the key left of 1)
// opens/closes it; every Lua script that defines _OnMenu() gets a section in it.
#pragma once

struct IDXGISwapChain;

// Called from the Present hook before each frame is shown, on the render
// thread, after the scripts' _OnFrame. drawSections fills the menu window
// (one call per frame while the menu is open); drawOverlays draws the
// scripts' own windows (every frame the menu is open or the overlay was
// requested). Does nothing - and creates nothing - until ~ is first pressed
// or a script requests the overlay.
void OverlayFrame(IDXGISwapChain* swapChain, void (*drawSections)(), void (*drawOverlays)());

// Draw the overlay this frame even with the menu closed (ui.ShowOverlay).
void OverlayRequest();

// True while the menu is open (for ui.IsMenuOpen and the input hotkeys).
bool OverlayMenuOpen();

// True between ImGui's NewFrame and Render: the only time the ui.* Lua
// functions may draw.
bool OverlayDrawing();

// True while the game's window is in front (hotkeys only count then).
bool OverlayGameHasFocus();

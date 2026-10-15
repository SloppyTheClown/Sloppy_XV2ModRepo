---@meta
-- XV2 Mod Loader library - every Lua function XV2ModLoader\Lib\XV2ModLoaderLib.dll
-- gives scripts: the "ui", "search", "files" and "menus" tables and the _OnMenu /
-- _OnOverlay callbacks.
--
-- This file is documentation only (never run it). Editors with the Lua
-- language server (e.g. VS Code's "Lua" extension by sumneko) show these
-- descriptions and check calls: add this sdk folder to "Lua.workspace.library".
--
-- The loader's own functions (_OnInit, _OnFrame, the LuaBackend and Cheat
-- Engine memory functions) are described in the README.

-- ===========================================================================
-- Callbacks a script may define
-- ===========================================================================

--- Draws the script's section of the in-game menu (~). Defining it gives the
--- script a collapsible section titled with its name; it runs every frame
--- while the menu is open, on the render thread. Draw with the ui.* functions.
--- An error unloads the script (the console shows it).
function _OnMenu() end

--- Draws the script's own windows over the game. Runs every frame the menu
--- is open, and every frame any script called ui.ShowOverlay() (call that
--- from _OnFrame). Open windows with ui.Begin / ui.End.
function _OnOverlay() end

-- ===========================================================================
-- ui - Dear ImGui widgets
-- ===========================================================================
-- Every ui function except ui.IsMenuOpen and ui.ShowOverlay raises an error
-- outside _OnMenu / _OnOverlay. Two widgets with the same label in one
-- section need "##suffix" ("Speed##fly", the text after ## isn't shown) or a
-- ui.PushID around them. Whatever a script leaves open (Begin without End,
-- PushID without PopID, an error halfway) is closed for it.
-- Text arguments take any value; tostring is applied.

---@class ui
ui = {}

--- Window flags for ui.Begin; add them up (ui.WindowFlags.NoTitleBar + ui.WindowFlags.NoResize).
---@class ui.WindowFlags
---@field NoTitleBar integer
---@field NoResize integer
---@field NoMove integer
---@field NoScrollbar integer
---@field NoCollapse integer
---@field AlwaysAutoResize integer  fit the window to its contents every frame
---@field NoBackground integer
---@field NoSavedSettings integer   don't remember position/size in menu_layout.ini
---@field NoFocusOnAppearing integer
---@field NoNav integer             no keyboard navigation
---@field NoDecoration integer      NoTitleBar + NoResize + NoScrollbar + NoCollapse
ui.WindowFlags = {}

-- Text ----------------------------------------------------------------------

--- A line of text.
---@param text any
function ui.Text(text) end

--- A greyed-out line of text.
---@param text any
function ui.TextDisabled(text) end

--- Text that breaks into lines at the window's right edge.
---@param text any
function ui.TextWrapped(text) end

--- Text in a colour.
---@param text any
---@param r number red 0..1
---@param g number green 0..1
---@param b number blue 0..1
---@param a? number opacity 0..1 (default 1)
function ui.TextColored(text, r, g, b, a) end

--- White text with a 1-pixel black outline, readable over any game picture
--- (e.g. in a ui.WindowFlags.NoBackground window).
---@param text any
function ui.TextOutlined(text) end

-- Layout --------------------------------------------------------------------

--- A horizontal line.
function ui.Separator() end

--- A little vertical space.
function ui.Spacing() end

--- Puts the next widget right of the last one instead of below it.
---@param offsetX? number > 0: pixels from the window's left edge to put it at (default 0 = right after)
---@param spacing? number pixels between the two (default -1 = the style's spacing)
function ui.SameLine(offsetX, spacing) end

--- Moves the following widgets right until ui.Unindent.
---@param width? number pixels (default 0 = the style's indent)
function ui.Indent(width) end

--- Undoes a ui.Indent of the same width.
---@param width? number pixels (default 0 = the style's indent)
function ui.Unindent(width) end

--- Mixes `id` into the ids of the widgets until ui.PopID, so equal labels
--- stay apart (e.g. a "Give" button per row in a loop).
---@param id string|integer
function ui.PushID(id) end

--- Ends the last ui.PushID.
function ui.PopID() end

-- Widgets -------------------------------------------------------------------
-- Value widgets take the current value and return the new one plus whether
-- the player changed it this frame:
--     speed, changed = ui.SliderFloat("Speed", speed, 0, 10)

--- A button.
---@param label string
---@param width? number pixels (default 0 = fit the label)
---@param height? number pixels (default 0 = fit the label)
---@return boolean clicked true in the frame it was clicked
function ui.Button(label, width, height) end

--- A checkbox.
---@param label string
---@param value any truthy = ticked
---@return boolean value
---@return boolean changed
function ui.Checkbox(label, value) end

--- A slider for a decimal number.
---@param label string
---@param value number
---@param min number
---@param max number
---@param format? string printf format of the number shown (default "%.2f")
---@return number value
---@return boolean changed
function ui.SliderFloat(label, value, min, max, format) end

--- A slider for a whole number.
---@param label string
---@param value integer
---@param min integer
---@param max integer
---@param format? string printf format of the number shown (default "%d")
---@return integer value
---@return boolean changed
function ui.SliderInt(label, value, min, max, format) end

--- A field for a decimal number.
---@param label string
---@param value number
---@param step? number > 0 adds -/+ buttons changing it by step (default 0 = none)
---@param format? string printf format of the number shown (default "%.3f")
---@return number value
---@return boolean changed
function ui.InputFloat(label, value, step, format) end

--- A field for a whole number, with -/+ buttons.
---@param label string
---@param value integer
---@param step? integer the buttons' change (default 1; 0 = no buttons)
---@return integer value
---@return boolean changed
function ui.InputInt(label, value, step) end

--- A UTF-8 text field. Enter or a click elsewhere ends typing; Esc undoes
--- the edit. While the player types, the game gets no key presses.
---@param label string
---@param text? string (default "")
---@param maxBytes? integer longest text accepted, 1..65536 (default 256)
---@param hint? string greyed-out text shown while the field is empty
---@return string text
---@return boolean changed
function ui.InputText(label, text, maxBytes, hint) end

--- Starts a dropdown. When it returns true, draw its rows (ui.Selectable)
--- and then call ui.EndCombo() - only when it returned true.
---@param label string
---@param preview? string text shown while closed (default "")
---@param maxRows? integer rows shown before the list scrolls, 1..100 (default 8)
---@return boolean open
function ui.BeginCombo(label, preview, maxRows) end

--- Ends a dropdown whose ui.BeginCombo returned true.
function ui.EndCombo() end

--- A clickable row - a dropdown entry or a list row anywhere. Inside a
--- dropdown a click also closes it.
---@param label string
---@param selected? boolean draw it highlighted (default false)
---@return boolean clicked
function ui.Selectable(label, selected) end

--- An open dropdown scrolls to the last widget (call after the selected row).
function ui.SetItemDefaultFocus() end

--- A header bar the player opens and closes; draw its contents while it
--- returns true (no closing call).
---@param label string
---@param defaultOpen? boolean open the first time it shows (default false)
---@return boolean open
function ui.CollapsingHeader(label, defaultOpen) end

--- A tree node with indented contents; call ui.TreePop() only when it returned true.
---@param label string
---@return boolean open
function ui.TreeNode(label) end

--- Ends an open ui.TreeNode.
function ui.TreePop() end

-- About the last widget -----------------------------------------------------

--- Whether the last widget is in use (held down or being typed in).
---@return boolean
function ui.IsItemActive() end

--- Whether the mouse is over the last widget.
---@return boolean
function ui.IsItemHovered() end

--- Shows a tooltip next to the mouse this frame. Usually:
---     if ui.IsItemHovered() then ui.SetTooltip("What this does") end
---@param text any
function ui.SetTooltip(text) end

-- Windows -------------------------------------------------------------------

--- Starts a window of its own (the title is also its id: the same title
--- continues the same window). Always pair it with ui.End(), also when it
--- returns false (collapsed or off screen - skip its contents then).
--- Positions and sizes the player gives windows are remembered.
---@param title string
---@param flags? integer sum of ui.WindowFlags (default 0)
---@return boolean visible
function ui.Begin(title, flags) end

--- Ends the window started by ui.Begin.
function ui.End() end

--- Places the next ui.Begin window.
---@param x number pixels of the game's picture (see ui.GetDisplaySize)
---@param y number
---@param firstUseOnly? boolean only the first time, so the player can move it (default false)
---@param pivotX? number which point of the window lands on x: 0 left (default), 0.5 centre, 1 right
---@param pivotY? number which point lands on y: 0 top (default), 0.5 centre, 1 bottom
function ui.SetNextWindowPos(x, y, firstUseOnly, pivotX, pivotY) end

--- Sizes the next ui.Begin window.
---@param width number pixels (0 = fit the contents)
---@param height number pixels (0 = fit the contents)
---@param firstUseOnly? boolean only the first time, so the player can resize it (default false)
function ui.SetNextWindowSize(width, height, firstUseOnly) end

--- Width of the next widget's field.
---@param width number pixels; negative = up to that far from the window's right edge
function ui.SetNextItemWidth(width) end

--- Puts the keyboard in the next widget (e.g. a text field).
function ui.SetKeyboardFocusHere() end

--- Size of the game's picture.
---@return number width pixels
---@return number height pixels
function ui.GetDisplaySize() end

-- Callable any time ---------------------------------------------------------

--- Whether the in-game menu (~) is open.
---@return boolean
function ui.IsMenuOpen() end

--- Draws the overlay this frame even with the menu closed: every script's
--- _OnOverlay runs. Call it from _OnFrame each frame your windows should
--- show. While the cursor is over an overlay window the game gets no
--- clicks; while a text field is active it gets no key presses.
function ui.ShowOverlay() end

-- ===========================================================================
-- search - search boxes over the game
-- ===========================================================================
-- A box is named by an id (start it with your mod's name). Scripts and DLL
-- mods (sdk\xv2_search.h) that use the same id share one box. A box shows
-- while search.Show is called (at least every ~0.1 s) and hides by itself;
-- what was typed is kept. The player clicks it or presses Ctrl+F to type;
-- Enter or a click elsewhere ends typing, Esc undoes the edit. It holds up
-- to 128 bytes of UTF-8. Matching ignores case for A-Z only.
-- Getters (Query, Matches, Typing, Changes) never create a box.

---@class search
search = {}

---@class search.ShowOptions
---@field hint? string greyed-out text while the box is empty (default "")
---@field x? number where the box first appears, fraction of the picture's width 0..1 (default 0)
---@field y? number fraction of the picture's height 0..1 (default 0)
---@field width? number width of the text field, fraction of the picture's width (default 0.3)
---@field note? string greyed-out line under the box ("" removes it; absent = keep it)

--- Shows the box this frame; call it every frame (e.g. from _OnFrame) while
--- the box should be up. x and y apply only when the box first comes up in a
--- session (the player can drag it).
---@param id string
---@param options? search.ShowOptions
---@return integer changes the box's change count (see search.Changes)
function search.Show(id, options) end

--- Sets the greyed-out line under the box (e.g. "No item matches").
---@param id string
---@param note? string nil or "" removes it
function search.SetNote(id, note) end

--- The typed text in lower case (A-Z).
---@param id string
---@return string text "" for an empty or unknown box
function search.Query(id) end

--- Whether `name` contains the box's text, ignoring case (A-Z). True for an
--- empty or unknown box.
---@param id string
---@param name string UTF-8
---@return boolean
function search.Matches(id, name) end

--- Whether the player is typing in the box (ignore your own hotkeys then).
---@param id string
---@return boolean
function search.Typing(id) end

--- Puts the keyboard in the box (as Ctrl+F does) the next time it is drawn.
---@param id string
function search.Focus(id) end

--- Empties the box (counts as a change when it held text).
---@param id string
function search.Clear(id) end

--- Replaces the box's text.
---@param id string
---@param text string UTF-8, cut to 128 bytes
function search.SetText(id, text) end

--- A count that goes up by one every time the box's query changes; filter
--- again when it differs from the value you last filtered for.
---@param id string
---@return integer count 0 for an unknown box
function search.Changes(id) end

-- ===========================================================================
-- files - change game files as the game reads them, or read them
-- ===========================================================================
-- path: the game file as the end of its path inside data\ ("ui/iggy/ITEMLIST.iggy";
-- '/' or '\', any case, matched at a folder boundary). One patch per file:
-- when another mod (script or DLL, sdk\xv2_files.h) already patches it, a
-- Replace is refused; the same script calling again replaces its own patch.
-- A patch applies to every later read of the file (menus are read each time
-- they open, most other files once), so register in the script's main body.
-- Only files the game reads through its normal loader; XV2Patcher's own
-- reads (e.g. .x2s) don't pass. DBXV2.exe 1.26.01 only. A script's patches
-- are dropped when the scripts reload (F1) and made again as they run.
-- File arguments are relative to the script's mod folder unless absolute.

---@class files
files = {}

--- Serves the contents of `file` instead of the game file at `path`, whole
--- (read once, now; at most 512 MB).
---@param path string
---@param file string
---@return boolean registered
---@return string? reason why it was refused
function files.ReplaceFile(path, file) end

--- Swaps the ActionScript (AS3 ABC) of the .iggy menu at `path` for the one
--- in `abcFile` - only while the menu's own script is the one you built from
--- (the ClaudeTools\iggy\*_abc.py builders print its size and hash), so a menu
--- another mod changed is left alone (files.Status then says "NOT patched: ...").
--- One script per original: calling again for another original (e.g. the
--- vanilla menu and xv2patcher's) adds it, for the same original replaces
--- it; each original the game loads gets its own.
---@param path string
---@param abcFile string
---@param vanillaAbcSize integer bytes of the ABC it may replace
---@param vanillaAbcHash integer|string its FNV-1a 64 hash (integer, or "0x..." string)
---@return boolean registered
---@return string? reason why it was refused
function files.ReplaceIggyScript(path, abcFile, vanillaAbcSize, vanillaAbcHash) end

--- Removes this script's patch of `path`; later reads get the game's file.
---@param path string
---@return boolean removed false when the script had none
function files.Remove(path) end

--- What happened when the game last read the file: "registered, not loaded
--- by the game yet", "served: ..." or "NOT patched: <why>".
---@param path string
---@return string? status nil when no mod patches the file
function files.Status(path) end

--- Who patches the file: a script's LUA_NAME or a DLL mod's owner name.
---@param path string
---@return string? owner nil when no mod patches the file
function files.Owner(path) end

--- Reads a game file as the game finds it: loose data\ first (so x2m and
--- Revamp files count), then the .cpk archives. Read-only; other mods'
--- patches are not applied. The first call reads the archives' file tables
--- (about a second), later calls only the file. path: inside data\
--- ("system/custom_skill.cus"; a leading "data/" is ignored).
---@param path string
---@return string? contents the file's bytes (use string.unpack), nil when not found
---@return string source "loose" or the .cpk's name; "not found" with nil
function files.ReadGameFile(path) end

--- Old name of files.Status (kept for existing scripts).
---@param path string
---@return string? status
function files.IggyScriptStatus(path) end

-- ===========================================================================
-- menus - a patched menu script talking to the script
-- ===========================================================================
-- A menu's ActionScript reaches the game with
-- m_callback.CallbackUserData("user", sendType, value) (or ...String); the
-- menu class's vtable slot for it switches on sendType. A menu script
-- patched with files.ReplaceIggyScript can send sendTypes of its own:
-- menus.HookCallback takes those (the game never sees them), every other
-- call goes on to the game. The script answers by setting a string in the
-- menu's user data, which the AS reads with
-- m_callback.GetUserDataString(index). Messages are queued; read them each
-- frame. An answer is written at the menu's next taken message, so the AS
-- should keep sending one (e.g. every frame) while it waits. DBXV2.exe
-- 1.26.01 only. A script's hooks detach when the scripts reload (F1) and are
-- made again as they run. Example: CharaSearch\chara_search.lua.

---@class menus
menus = {}

---@class menus.Message
---@field type integer the sendType
---@field value number|string

--- Chains into vtable slot `slot` of the menu class whose vtable is at
--- `vtable` (an address in DBXV2.exe, relative to its base - e.g. 0x10be328
--- = SelectCharacter, slot 17 its user callback) and takes the sendTypes in
--- `messages`: {[sendType] = "number" | "string"} (the value's kind).
--- `mutex`: an optional named mutex other mods hooking the same slot also
--- take, so the slot is chained one at a time. Calling again with the same
--- name replaces the sendTypes; another script's name is refused.
---@param name string
---@param vtable integer
---@param slot integer
---@param messages table<integer, "number"|"string">
---@param mutex? string
---@return boolean hooked
---@return string? reason why not
function menus.HookCallback(name, vtable, slot, messages, mutex) end

--- The messages taken since the last call, oldest first. Numbers of one
--- sendType sent in a row are kept as the latest only; at most 256 wait.
---@param name string
---@return menus.Message[]
function menus.Messages(name) end

--- Seconds since the menu last sent `sendType` (e.g. to tell whether it is
--- still open); nil when it never did since the hook.
---@param name string
---@param sendType integer
---@return number? seconds
function menus.LastSeen(name, sendType) end

--- Sets user data entry `index` (negative = from the end: -1 the last) of the
--- menu to `text`, at its next taken message. A newer text for the same
--- entry replaces one not written yet.
---@param name string
---@param index integer
---@param text string UTF-8
---@return boolean queued false when the script has no such hook
function menus.SetUserDataString(name, index, text) end

--- The hook's state: slot, waiting and dropped messages, the last problem
--- writing an answer.
---@param name string
---@return string? status nil when the script has no such hook
function menus.Status(name) end

--- Stops taking messages (the slot stays chained and passes every call on).
---@param name string
---@return boolean unhooked
function menus.Unhook(name) end

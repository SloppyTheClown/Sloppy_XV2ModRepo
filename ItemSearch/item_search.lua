-- Item Search: a search box over the game's item lists (the equipment
-- screen's torso / Super Soul / ... lists, shops). Type part of a name and
-- the list shows only the items whose name contains it (case-insensitive).
-- Click the box or press Ctrl+F to type; the list follows as you type.
-- Enter (or a click elsewhere) gives the keys back to the list; Esc while
-- typing undoes the edit. The box itself is the loader's (the `search` table: drawing, Ctrl+F, keeping typed
-- keys from the game); this script only filters the list.
--
-- How: every item list is a Menu::ItemList. Its "shown" array (what the
-- player scrolls through) is rebuilt by the game from its "all" array
-- whenever the category or sort changes. The search squeezes the matching
-- entries to the front of the shown array, shortens it, and has the list's
-- script (ITEMLIST.iggy with the mod's ActionScript, swapped in by the loader
-- as the game reads the file) request its rows again; the all
-- array is never touched, and clearing the search lets the game rebuild the
-- shown array. The highlighted item stays highlighted while it matches.
-- Research_Notes\ui-iggy-and-textures.md "Item lists"; ClaudeTools\itemlist_probe.py.
--
-- Favorites: F (keyboard) or clicking the left stick (controller) on an item
-- in a Customize list (equipment, skills) marks it as a favorite, or
-- unmarks it. Favorites lead the list (after "None") whenever it is shown, in the game's order, with a
-- gold star after the name (drawn by the list's script, which is told which
-- rows are favorites); the search still filters them. They are kept per Steam
-- account (read from the loaded save, as Battle Styles does), not per
-- character, in favorites.sav next to this script.

-- Settings
-- The loader's search box this script uses. Battle Styles shows the same id
-- over its style list, so the equipment screen has one box and one query.
local BOX_ID = "equipment_search"
local BOX_HINT = "Search (Ctrl+F)"
local BOX_X, BOX_Y = 0.22, 0.17           -- box position, fractions of the picture (left, top)
local BOX_WIDTH = 0.30                    -- box width, fraction of the picture width
local FAVORITE_KEY = "F"                  -- marks / unmarks the highlighted item (not with Ctrl: Ctrl+F is the box)
local FAVORITES_FILE = "favorites.sav"    -- .sav: save data, like Battle Styles' equipped_styles.sav

-- The config next to this script, item_search.json: written with the defaults when
-- missing, never overwritten; read at every (re)load (F1 in game).
-- "debug_menu": true shows this script's status section in the XV2 Mod
-- Loader menu (~); off by default, the section only shows information.
local CONFIG_FILE = "item_search.json"
local DEFAULT_CONFIG = [[{
  "debug_menu": false,

  "_debug_menu": "true shows Item Search's status (what it hooked, the open list) in the XV2 Mod Loader menu (~). F1 in game re-reads this file."
}
]]

-- Whether the config turns the menu section on (missing file or key: off).
local function MenuSectionWanted()
  local path = SCRIPT_PATH .. "\\" .. CONFIG_FILE
  local file = io.open(path, "rb")
  if not file then
    file = io.open(path, "wb")
    if file then
      file:write(DEFAULT_CONFIG)
      file:close()
    end
    return false
  end
  local text = file:read("a") or ""
  file:close()
  return text:match('"debug_menu"%s*:%s*true') ~= nil
end

-- DBXV2.exe 1.26.01
local ITEM_LIST_VTABLE = 0x10aa508        -- Menu::ItemList (RTTI)
local UPDATE_SLOT = 9                     -- vtable slot 9 = 0x5417e0, the list's per-frame update (rcx = list, xmm1 = time)
local UPDATE_FUNCTION = 0x5417e0
local ROW_NAME = 0x543090                 -- (list, std::wstring* out, shownIndex): the row's name as the list shows it (row pusher 0x545340 uses it)
local REBUILD_SHOWN = 0x542640            -- (list): shown = all filtered by category, sorted
local SET_UI_INT = 0x2c81a0               -- (menu, variable, value): sets one of the menu's Iggy int variables
-- ITEMLIST.iggy's game -> script variables (ReceiveType_* in its script):
-- 5 = ListElemMax (entry count), 2 = RequestResume, which the patched script
-- reads as "rows changed" when it carries REFRESH_TAG (as3\ItemList.as
-- SearchRefreshTag): bits 14-23 = how many favorites there are (they get the
-- star), bit 13 = they follow one leading row ("None"), bits 0-12 = entry to select
local UI_ENTRY_COUNT, UI_REFRESH = 5, 2
local REFRESH_TAG = 0x5E000000
local FAVORITE_SHIFT, FAVORITE_COUNT_MAX = 14, 0x3FF
local FAVORITES_AFTER_FIRST, ROW_MAX = 0x2000, 0x1FFF
-- The menu file and the script the loader swaps into it (files.ReplaceIggyScript)
-- - only over the vanilla script: its ABC size and FNV-1a 64 hash, printed
-- by ClaudeTools\iggy\itemlist_abc.py
local ITEMLIST_IGGY = "ui/iggy/ITEMLIST.iggy"
local ITEMLIST_ABC = "ItemList.abc"       -- in this script's mod folder
local VANILLA_ABC_SIZE, VANILLA_ABC_HASH = 59410, 0x8dd6dfe18ce480c6
local HEAP_FREE = 0xf4ec78                -- import slot of xgcore XG::HeapFree(void*), frees std::wstring buffers (0x54321c)
-- Menu::ItemList fields (live probe with the torso list open; the sequence
-- after the category change in the command handler 0x544220, case 1 at 0x54426f)
local LIST_MODE = 0x1f0                   -- which list (15 = equipment torso)
-- The Customize lists, the only ones with favorites: the list script's
-- ModeType_CustomizeItem/Skill/Costume (as3\ItemList.as). Clothes,
-- accessories and Super Souls are 15 (live probe); the skill lists are
-- another of these (skill search works, favorites did nothing on them).
local FAVORITE_MODES = {[14] = true, [15] = true, [16] = true}
local LIST_STATE = 0x1f4                  -- 2 = list open and idle
local SHOWN_BEGIN, SHOWN_END = 0x210, 0x218
local LIST_CURSOR = 0x234
local ENTRY_SIZE = 0xb0                   -- shown/all entries: +0 type, +4 id (-1 = "None")
local IDLE_STATE = 2
local HIDDEN_STATE = 1                    -- closed: the equipment screen's list object keeps ticking hidden
-- The loaded save's decrypted image, kept by the save/system singleton
-- [0x1554448] at +0x68: "#SAV", then the owning Steam id at +8 XORed with
-- "thebecon" (BattleStyles style_menu.cpp SteamAccount; Xv2CoreLib SAV_File.cs)
local SAVE_SYSTEM, SAVE_IMAGE, SAVE_STEAM_ID = 0x1554448, 0x68, 8
local STEAM_ID_XOR = 0x7468656265636F6E

-- The update stub: vtable slot 9 jumps through it, so it sees every list that
-- ticks. It counts calls and keeps the last 8 lists in a ring:
--   mov eax,[counter]; inc dword [counter]; and eax,7
--   lea r11,[ring]; mov [r11+rax*8],rcx; jmp [original]
-- (rax and r11 are free at a function's entry in the x64 convention; xmm1
-- is left alone for the update's time argument.)
local STUB_COUNTER, STUB_RING = 0x40, 0x48
local STUB_CODE = {
  0x8B, 0x05, 0x3A, 0x00, 0x00, 0x00,     -- mov eax,[rip+0x3a]      -> +0x40
  0xFF, 0x05, 0x34, 0x00, 0x00, 0x00,     -- inc dword [rip+0x34]    -> +0x40
  0x83, 0xE0, 0x07,                       -- and eax,7
  0x4C, 0x8D, 0x1D, 0x32, 0x00, 0x00, 0x00, -- lea r11,[rip+0x32]     -> +0x48
  0x49, 0x89, 0x0C, 0xC3,                 -- mov [r11+rax*8],rcx
  0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,     -- jmp [rip+0]
}
local STUB_TARGET = #STUB_CODE            -- the original update's address follows the jmp

local base = getAddress("DBXV2.exe")
local slotAddress = base + ITEM_LIST_VTABLE + UPDATE_SLOT * 8
local stub = nil
local status = "starting"

-- Sets up the stub, or finds the one an earlier load (F1 reload) left.
local function InstallStub()
  local current = readQword(slotAddress)
  if current == base + UPDATE_FUNCTION then
    local block = allocateMemory(0x100)
    if not block then return "could not allocate the stub" end
    writeBytes(block, STUB_CODE)
    writeQword(block + STUB_TARGET, base + UPDATE_FUNCTION)
    if not writeQword(slotAddress, block) then return "could not patch the vtable" end
    stub = block
    return "ok"
  end
  -- ours from before: same code, same target
  local code = readBytes(current, #STUB_CODE, true)
  if code and readQword(current + STUB_TARGET) == base + UPDATE_FUNCTION then
    for i = 1, #STUB_CODE do
      if code[i] ~= STUB_CODE[i] then return "vtable slot holds something else; off" end
    end
    stub = current
    return "ok"
  end
  return "vtable slot holds something else; off"
end

-- std::wstring (MSVC, 32 bytes): text inline when capacity < 8, else a pointer.
local function ReadWideString(address)
  local length = readQword(address + 0x10)
  local capacity = readQword(address + 0x18)
  local text = capacity >= 8 and readPointer(address) or address
  return readString(text, length, true) or ""
end

-- Frees a wstring's heap buffer the way 0x5431e1 does.
local function FreeWideString(address)
  local capacity = readQword(address + 0x18)
  if capacity < 8 then return end
  local buffer = readPointer(address)
  if capacity * 2 + 2 >= 0x1000 then buffer = readPointer(buffer - 8) end  -- big blocks are aligned; the real start is just before
  CallFunction(readPointer(base + HEAP_FREE), buffer)
end

local scratch = allocateMemory(0x1000)    -- out-strings and the page/cursor ints for the game calls

-- An entry's "type:id": names one item in every list (favorites, names).
local function EntryKey(entry)
  return readInteger(entry, true) .. ":" .. readInteger(entry + 4, true)
end

-- Row names, by "type:id", for the list and mode they came from.
local names = {}

local function RowName(list, shownIndex, entry)
  local key = EntryKey(entry)
  local name = names[key]
  if name then return name end
  CallFunction(base + ROW_NAME, list, scratch, shownIndex)
  name = ReadWideString(scratch):lower()
  FreeWideString(scratch)
  names[key] = name
  return name
end

local function ShownRange(list)
  local first = readPointer(list + SHOWN_BEGIN)
  local count = (readPointer(list + SHOWN_END) - first) // ENTRY_SIZE
  return first, count
end

-- Makes an open list show the changed shown array, keeping the highlight on
-- the entry `keepKey` ("type:id") when it is still listed (else, or when
-- nil, the first favorite or else the first row), with a star on the
-- `favoriteCount` rows from `favoriteFirst` (0 or 1) on. The
-- list's script (ITEMLIST.iggy, action_script.ItemList) only redraws rows
-- through its own page request, so the patched script (as3\ItemList.as
-- checkSearchRefresh) watches Iggy variable 2 for REFRESH_TAG + favorites +
-- entry index: it takes the new count from variable 5, selects the entry and
-- requests the page, and the game answers that as for any page change (rows,
-- cursor, preview). Without the patched script nothing visible happens.
local function Redraw(list, keepKey, favoriteFirst, favoriteCount)
  local first, count = ShownRange(list)
  local row = favoriteCount > 0 and favoriteFirst or 0
  if keepKey then
    for index = 0, count - 1 do
      if EntryKey(first + index * ENTRY_SIZE) == keepKey then row = index break end
    end
  end
  row = math.min(row, ROW_MAX)
  -- the game reads the cursor entry before the script's request arrives, and
  -- the script's next move (up/down) starts from it
  writeInteger(list + LIST_CURSOR, row)
  CallFunction(base + SET_UI_INT, list, UI_ENTRY_COUNT, count)
  CallFunction(base + SET_UI_INT, list, UI_REFRESH,
      REFRESH_TAG | (math.min(favoriteCount, FAVORITE_COUNT_MAX) << FAVORITE_SHIFT)
      | (favoriteFirst > 0 and FAVORITES_AFTER_FIRST or 0) | row)
end

-- "type:id" of the entry under the cursor (nil when there is none).
local function CursorKey(list)
  local first, count = ShownRange(list)
  local cursor = readInteger(list + LIST_CURSOR, true)
  if cursor < 0 or cursor >= count then return nil end
  return EntryKey(first + cursor * ENTRY_SIZE)
end

-- Favorites
local account = nil                       -- the save's Steam id as text; nil until a save is loaded
local favorites = {}                      -- this account's: ["type:id"] = true
local savedFavorites = {}                 -- every account's, as in the file: [account] = {"type:id", ...}
local favoritesVersion = 0                -- goes up with every change; the list is arranged again when it does
local favoriteHeld = false                -- F / left stick held last frame (a press counts once)

local function FavoritesPath() return SCRIPT_PATH .. "\\" .. FAVORITES_FILE end

-- The loaded save's Steam account, or nil while no save is loaded. Read from
-- the save rather than Steam, as Battle Styles does: saves are per account.
local function SteamAccount()
  local saveSystem = readPointer(base + SAVE_SYSTEM)
  if not saveSystem or saveSystem == 0 then return nil end
  local image = saveSystem + SAVE_IMAGE
  if readString(image, 4) ~= "#SAV" then return nil end
  local id = readQword(image + SAVE_STEAM_ID)
  if not id or id == STEAM_ID_XOR then return nil end
  return string.format("%d", id ~ STEAM_ID_XOR)  -- Steam ids are below 2^63: the same text as Battle Styles' std::to_string
end

-- favorites.sav: {"<account>": ["type:id", ...], ...}. A missing file is
-- no favorites; a .tmp left by a save cut short is taken when the file is gone.
local function LoadFavorites()
  savedFavorites = {}
  local file = io.open(FavoritesPath(), "rb") or io.open(FavoritesPath() .. ".tmp", "rb")
  if not file then return end
  local text = file:read("a") or ""
  file:close()
  for owner, body in text:gmatch('"(%d+)"%s*:%s*%[(.-)%]') do
    local keys = {}
    for key in body:gmatch('"(%-?%d+:%-?%d+)"') do keys[#keys + 1] = key end
    savedFavorites[owner] = keys
  end
end

-- Writes every account's favorites, through a temporary file so a crash
-- never leaves half a file (os.rename does not replace a file on Windows).
local function SaveFavorites()
  local keys = {}
  for key in pairs(favorites) do keys[#keys + 1] = key end
  table.sort(keys)
  savedFavorites[account] = keys
  local owners = {}
  for owner, ownerKeys in pairs(savedFavorites) do
    if #ownerKeys > 0 then owners[#owners + 1] = owner end
  end
  table.sort(owners)
  local lines = {}
  for _, owner in ipairs(owners) do
    local quoted = {}
    for index, key in ipairs(savedFavorites[owner]) do quoted[index] = '"' .. key .. '"' end
    lines[#lines + 1] = string.format('    "%s": [%s]', owner, table.concat(quoted, ", "))
  end
  local path = FavoritesPath()
  local file = io.open(path .. ".tmp", "wb")
  if not file then ConsolePrint(FAVORITES_FILE .. ": cannot write", 3) return end
  file:write("{\n" .. table.concat(lines, ",\n") .. "\n}\n")
  file:close()
  os.remove(path)
  local renamed, reason = os.rename(path .. ".tmp", path)
  if not renamed then ConsolePrint(FAVORITES_FILE .. ": cannot write (" .. tostring(reason) .. ")", 3) end
end

-- Learns the account once a save is loaded and reads its favorites.
local function EnsureAccount()
  if account then return end
  account = SteamAccount()
  if not account then return end
  LoadFavorites()
  favorites = {}
  for _, key in ipairs(savedFavorites[account] or {}) do favorites[key] = true end
  favoritesVersion = favoritesVersion + 1
end

-- True on the frame F or the left stick goes down (not Ctrl+F: the box's key).
local function FavoritePressed()
  local held = (isKeyPressed(FAVORITE_KEY) and not isKeyPressed(VK_CONTROL))
      or (isPadButtonPressed ~= nil and isPadButtonPressed(PAD_LEFT_THUMB))
  local pressed = held and not favoriteHeld
  favoriteHeld = held
  return pressed
end

-- Orders the shown array: "None", then the favorites the query matches (in
-- the game's order), then the other matches; entries that don't match are
-- cut off (the caller redraws). The all array is never touched. Returns
-- where the favorites start (the number of "None" rows before them), how
-- many there are, whether the array changed, and whether the query matched
-- nothing (the list is then only ordered, not cut).
local function Arrange(list, query, favoriteSet)
  local first, count = ShownRange(list)
  -- shown indexes of "None", the favorites and the rest, in game order
  local function Collect(filter)
    local none, favorite, rest, matched = {}, {}, {}, false
    for index = 0, count - 1 do
      local entry = first + index * ENTRY_SIZE
      if readInteger(entry + 4, true) == -1 then
        none[#none + 1] = index                   -- "None" stays whatever is typed
      elseif filter == "" or RowName(list, index, entry):find(filter, 1, true) then
        matched = true
        if favoriteSet[EntryKey(entry)] then favorite[#favorite + 1] = index else rest[#rest + 1] = index end
      end
    end
    return none, favorite, rest, matched
  end
  local none, favorite, rest, matched = Collect(query)
  local noMatch = query ~= "" and not matched
  if noMatch then none, favorite, rest = Collect("") end
  local order = {}
  for _, group in ipairs({none, favorite, rest}) do
    for _, index in ipairs(group) do order[#order + 1] = index end
  end
  -- copy from the first entry out of place on (all read before any write)
  local firstMoved = nil
  for position, index in ipairs(order) do
    if index ~= position - 1 then firstMoved = position break end
  end
  if firstMoved then
    local copies = {}
    for position = firstMoved, #order do copies[position] = readBytes(first + order[position] * ENTRY_SIZE, ENTRY_SIZE, true) end
    for position = firstMoved, #order do writeBytes(first + (position - 1) * ENTRY_SIZE, copies[position]) end
  end
  if #order ~= count then writeQword(list + SHOWN_END, first + #order * ENTRY_SIZE) end
  -- the list script stars at most one leading row's offset (FAVORITES_AFTER_FIRST)
  local favoriteFirst = math.min(#none, 1)
  return favoriteFirst, #favorite, firstMoved ~= nil or #order ~= count, noMatch
end

-- The first `length` entries' keys, joined: tells a game rebuild (sort,
-- category change) apart from our own order when begin and count stay.
local HEAD_CHECK = 16                     -- entries compared each frame, at most
local function HeadKeys(list, length)
  local first = readPointer(list + SHOWN_BEGIN)
  local keys = {}
  for index = 0, length - 1 do keys[#keys + 1] = EntryKey(first + index * ENTRY_SIZE) end
  return table.concat(keys, ",")
end

-- List state
local arranged = nil                      -- how the shown array was last left: list, begin, count, head, query, version
local modified = false                    -- the shown array is ours, not the game's (rebuild it before arranging again)
local noMatch = false
local activeList = nil
local lastCounter = nil
local quietFrames = 0

local activeMode = nil
local shownNote = nil                     -- the note this script last set (nil: none yet)
local hiddenSince = false                 -- the list was hidden (closed) since it was last arranged

local function Reset()
  search.Clear(BOX_ID)
  arranged, modified, noMatch, hiddenSince = nil, false, false, false
  names = {}
  shownNote = nil
end

-- Brings the list in line with the query and the favorites. Only while the
-- list is idle, so a confirm box or a closing list is never disturbed.
local function Sync(list)
  local state = readInteger(list + LIST_STATE, true)
  if state == HIDDEN_STATE then hiddenSince = true end
  if state ~= IDLE_STATE then return false end
  local wanted = search.Query(BOX_ID)    -- already lower case (ASCII)
  local first, count = ShownRange(list)
  local rebuilt = arranged == nil or arranged.list ~= list or arranged.begin ~= first or arranged.count ~= count
      or HeadKeys(list, arranged.headLength) ~= arranged.head
  -- a reopened list's script starts without stars and with the game's cursor:
  -- it is told again even when the game kept the arranged array
  local reopened = hiddenSince
  hiddenSince = false
  if not rebuilt and not reopened and arranged.query == wanted and arranged.version == favoritesVersion then return true end
  local opened = arranged == nil or reopened
  local keepKey = CursorKey(list)
  local cursorAtTop = readInteger(list + LIST_CURSOR, true) == 0
  -- the game rebuilt the array: it is the game's again. Otherwise (query or
  -- favorites changed) start again from the game's own rebuild, keeping its
  -- category and sort.
  if not rebuilt and modified then CallFunction(base + REBUILD_SHOWN, list) end
  local favoriteSet = FAVORITE_MODES[activeMode] and favorites or {}
  local favoriteFirst, favoriteCount, changed
  favoriteFirst, favoriteCount, changed, noMatch = Arrange(list, wanted, favoriteSet)
  modified = changed
  -- favorites are what an opened list shows first (also after the game put
  -- the cursor back on top); else the highlight stays on its item
  if favoriteCount > 0 and (opened or (rebuilt and cursorAtTop)) then keepKey = nil end
  -- a game rebuild (or a reopening) the arrangement left alone is already on
  -- screen; stars are always sent again (the script forgets them on opening)
  if changed or favoriteCount > 0 or not (rebuilt or reopened) then Redraw(list, keepKey, favoriteFirst, favoriteCount) end
  local newFirst, newCount = ShownRange(list)
  local headLength = math.min(newCount, favoriteFirst + favoriteCount + 1, HEAD_CHECK)
  arranged = {list = list, begin = newFirst, count = newCount, headLength = headLength,
              head = HeadKeys(list, headLength), query = wanted, version = favoritesVersion}
  return true
end

-- F / left stick on the highlighted item: marks or unmarks it (Customize
-- lists only, never "None", not while typing in the box).
local function CheckFavoriteToggle(list)
  if not FavoritePressed() then return end
  if not account or not FAVORITE_MODES[activeMode] or search.Typing(BOX_ID) then return end
  if readInteger(list + LIST_STATE, true) ~= IDLE_STATE then return end
  local key = CursorKey(list)
  if not key or key:match(":%-1$") then return end
  favorites[key] = not favorites[key] or nil
  favoritesVersion = favoritesVersion + 1
  SaveFavorites()
end

-- The list that ticked since the last frame (an idle one if several did).
local function TickedList()
  local counter = readInteger(stub + STUB_COUNTER)
  if counter == lastCounter then return nil end
  local calls = counter - (lastCounter or counter - 1)
  lastCounter = counter
  local found, idle = nil, nil
  for back = 1, math.min(calls, 8) do
    local list = readPointer(stub + STUB_RING + ((counter - back) % 8) * 8)
    if list and readQword(list) == base + ITEM_LIST_VTABLE then
      found = found or list
      if not idle and readInteger(list + LIST_STATE, true) == IDLE_STATE then idle = list end
    end
  end
  return idle or found
end

function _OnInit()
  -- no _OnMenu, no section in the loader's menu
  if not MenuSectionWanted() then _OnMenu = nil end
  if not search then
    status = "XV2 Mod Loader is too old (no search box); update it"
    ConsolePrint(status, 2)
    return
  end
  status = InstallStub()
  -- without the list script the filter still works, but the rows only
  -- change when the list is opened again
  if not files then ConsolePrint("XV2 Mod Loader is too old to swap the list script; update it", 2) return end
  local replaced, reason = files.ReplaceIggyScript(ITEMLIST_IGGY, ITEMLIST_ABC, VANILLA_ABC_SIZE, VANILLA_ABC_HASH)
  if not replaced then ConsolePrint("list script not replaced (" .. reason .. "): rows redraw only when a list is reopened", 2) end
end

function _OnFrame()
  if not stub then return end
  local list = TickedList()
  if list then
    quietFrames = 0
    local mode = readInteger(list + LIST_MODE, true)
    if list ~= activeList or mode ~= activeMode then
      -- another list (or the same object showing another list): fresh search
      activeList, activeMode = list, mode
      Reset()
    end
  elseif activeList then
    -- a list that hasn't ticked for a few frames is closed
    quietFrames = quietFrames + 1
    if quietFrames > 3 then
      activeList, activeMode = nil, nil
      Reset()
    end
  end
  if not activeList then
    FavoritePressed()                     -- keeps the held state current
    return
  end
  EnsureAccount()
  CheckFavoriteToggle(activeList)
  -- every change is shown while typing; ending the typing (Enter) changes
  -- nothing, so the list is never refreshed while the game handles a key
  local idle = Sync(activeList)
  -- The note is set only when it changes: while the item list is hidden
  -- (another list in front, e.g. Battle Styles' style list on the same box)
  -- it is "" once and then left to that list's mod.
  local note = idle and noMatch and "No item matches" or ""
  if note == shownNote then note = nil else shownNote = note end
  -- shown while called each frame; hides itself once the list closes
  search.Show(BOX_ID, {hint = BOX_HINT, x = BOX_X, y = BOX_Y, width = BOX_WIDTH, note = note})
end

function _OnMenu()
  ui.Text("Item search: " .. status)
  ui.Text("List script: " .. (files and files.IggyScriptStatus(ITEMLIST_IGGY) or "not replaced"))
  if activeList then
    local _, count = ShownRange(activeList)
    ui.Text(string.format("List %X  mode %d  state %d  shown %d  cursor %d", activeList,
        readInteger(activeList + LIST_MODE, true), readInteger(activeList + LIST_STATE, true), count,
        readInteger(activeList + LIST_CURSOR, true)))
    ui.Text(string.format("Typed \"%s\", list filtered by \"%s\"%s", search.Query(BOX_ID),
        arranged and arranged.query or "", search.Typing(BOX_ID) and " (typing)" or ""))
  else
    ui.TextDisabled("No item list open")
  end
  local favoriteCount = 0
  for _ in pairs(favorites) do favoriteCount = favoriteCount + 1 end
  ui.Text(account and string.format("Favorites: %d (Steam account %s)", favoriteCount, account)
      or "Favorites: no save loaded yet")
  if not isPadButtonPressed then ui.TextDisabled("Left stick needs a newer XV2 Mod Loader (F works)") end
end

-- Chara Search: a search box and favorites on the character select.
-- Type part of a name or a character code (Ctrl+F or a click on the box) and
-- the grid shows only the characters that match; F (keyboard, not with
-- Ctrl) or a click of the left stick marks the character under the cursor
-- as a favorite, or unmarks it. The grid shows the loaded CaC first, then the
-- favorites (with a gold star), then the rest. Favorites are kept per Steam
-- account in favorite_characters.sav next to this script.
--
-- How: the loader swaps CHARASELE.iggy's ActionScript for the mod's
-- (iggy\CharaSele.abc, or CharaSele_XV2Patcher.abc over xv2ins 4.7's menu;
-- as3\CharaSele.as + as3\chara_search.as, built by ClaudeTools\iggy\menu_abc.py).
-- That script shows the grid as a view of the game's list and talks to this
-- one through the menu's AS callback (the loader's `menus` table):
--   9902 (string) once per screen: "token\n" + per list entry
--        "key\tcode,code" (key: the favorite's name, empty for padding)
--   9904 (number) every frame: the list entry under the cursor (-1: none)
-- and reads the answer from the menu's last user-data entry, one the game
-- never sets: "token\t<shown entries, or * for all>\t<favorite entries>".
-- The order is local only: the script sends the game its own list numbers,
-- so nothing changes online. Research_Notes\chara-select-search.md.
--
-- Battle Styles rebuilds the same menu script for its skill popup row. Only
-- one script can replace the game's, so while this script patches CHARASELE
-- (files.Owner says "CharaSearch") Battle Styles serves one with both, over
-- the game's script or over this one's, whichever runs first.

LUA_NAME = "CharaSearch"                  -- owner name of the patches and the hook; Battle Styles checks for it

-- Settings
local BOX_ID = "character_search"
local BOX_HINT = "Search (Ctrl+F)"
local BOX_X, BOX_Y = 0.35, 0.03           -- top middle, over the screen title's row (fractions of the picture)
local BOX_WIDTH = 0.30
local NO_MATCH_NOTE = "No character matches"
local FAVORITE_KEY = "F"                  -- not with Ctrl: Ctrl+F is the box
local FAVORITES_FILE = "favorite_characters.sav"  -- .sav: save data, like Item Search's favorites.sav
local NAMES_MSG = "msg/proper_noun_character_name_en.msg"  -- entry "chara_<CODE>_000" = the character's name

-- The config next to this script, chara_search.json: written with the defaults when
-- missing, never overwritten; read at every (re)load (F1 in game).
-- "debug_menu": true shows this script's status section in the XV2 Mod
-- Loader menu (~); off by default, the section only shows information.
local CONFIG_FILE = "chara_search.json"
local DEFAULT_CONFIG = [[{
  "debug_menu": false,

  "_debug_menu": "true shows Chara Search's status (what it hooked, the open list) in the XV2 Mod Loader menu (~). F1 in game re-reads this file."
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
-- SelectCharacter (CHARASELE.iggy): vtable slot 17 = 0x5e1630, its AS
-- callback CallbackUserData("user", sendType, value).
local MENU_NAME = "chara_select"
local SELECT_CHARA_VTABLE, USER_CALLBACK_SLOT = 0x10be328, 17
local SEND_LIST, SEND_CURSOR = 9902, 9904
-- Battle Styles chains the same slot under this mutex
local CALLBACK_MUTEX = "XV2Mods_SelectCharaCallbackHook"
-- The answer's entry: the last one (ReceiveType_CustomList_PartnerJudge_End,
-- 20562 vanilla, 40758 with xv2patcher). Never through the game's
-- SetUserDataStr: xv2patcher hooks it and stops the game for this entry.
local ANSWER_ENTRY = -1
-- The grid sends its cursor every frame while open (and while it waits for
-- the answer, up to 30 frames); older than this, it is closed.
local OPEN_SECONDS = 0.1
-- The menu file and the scripts swapped into it - only over the game's own
-- scripts: their ABC sizes and FNV-1a 64 hashes (menu_abc.py prints them)
local CHARASELE_IGGY = "ui/iggy/CHARASELE.iggy"
local SCRIPTS = {
  {abc = "CharaSele.abc", vanillaSize = 0xb52d, vanillaHash = 0x18865d7ce449251c},             -- the cpk's menu
  {abc = "CharaSele_XV2Patcher.abc", vanillaSize = 0xba46, vanillaHash = 0xaa2bcacb32cbd1fb},  -- xv2ins 4.7's loose menu
}
-- The loaded save's decrypted image, kept by the save/system singleton
-- [0x1554448] at +0x68: "#SAV", then the owning Steam id at +8 XORed with
-- "thebecon" (as Item Search and Battle Styles read it; Xv2CoreLib SAV_File.cs)
local SAVE_SYSTEM, SAVE_IMAGE, SAVE_STEAM_ID = 0x1554448, 0x68, 8
local STEAM_ID_XOR = 0x7468656265636F6E

local base = getAddress("DBXV2.exe")
local status = "starting"

-- Character names ------------------------------------------------------------
-- msg file layout (Xv2CoreLib MSG): +4 flags (names UTF-16 when the low half
-- is 256, text when the high half is 1), +8 entry count, +0xc names (16-byte
-- string records), +0x14 lines (8 bytes: count, offset of the first line's
-- string record). String record: +0 offset, +4 size (8-bit), +8 size (UTF-16).
local names = nil                         -- msg entry name -> text; nil until read
local namesNote = "not read yet"

local function I32(data, offset) return (string.unpack("<i4", data, offset + 1)) end

-- UTF-16LE bytes -> UTF-8 (unpaired surrogates become U+FFFD).
local function Utf16ToUtf8(data, offset, size)
  local out, at, stop = {}, offset, offset + size - 1
  while at < stop do
    local unit = string.unpack("<I2", data, at + 1)
    at = at + 2
    if unit >= 0xd800 and unit <= 0xdbff and at < stop then
      local low = string.unpack("<I2", data, at + 1)
      if low >= 0xdc00 and low <= 0xdfff then
        unit = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00)
        at = at + 2
      else
        unit = 0xfffd
      end
    elseif unit >= 0xd800 and unit <= 0xdfff then
      unit = 0xfffd
    end
    if unit == 0 then break end
    out[#out + 1] = utf8.char(unit)
  end
  return table.concat(out)
end

local function MsgString(data, record, wide)
  local offset, size = I32(data, record), I32(data, record + (wide and 8 or 4))
  if size <= 0 or offset < 0 or offset + size > #data then return "" end
  if wide then return Utf16ToUtf8(data, offset, size) end
  return (data:sub(offset + 1, offset + size):gsub("%z.*", ""))
end

-- The game writes text with HTML entities (&apos; for ', ...).
local ENTITIES = {apos = "'", quot = '"', amp = "&", lt = "<", gt = ">"}
local function CleanText(text)
  text = text:gsub("&(%w+);", ENTITIES)
  return (text:gsub("%c", " "):gsub("^%s+", ""):gsub("%s+$", ""))
end

-- Reads the names once; without them the search matches codes only.
local function ReadNames()
  names = {}
  local data, source = files.ReadGameFile(NAMES_MSG)
  if not data or data:sub(1, 4) ~= "#MSG" or #data < 0x18 then
    namesNote = "not found - the search matches codes only"
    ConsolePrint("character names " .. namesNote, 2)
    return
  end
  local wideNames, wideText = I32(data, 4) & 0xffff == 256, (I32(data, 4) >> 16) & 0xffff == 1
  local count, nameTable, lineTable = I32(data, 8), I32(data, 12), I32(data, 20)
  local read = 0
  for index = 0, count - 1 do
    if nameTable + index * 16 + 16 > #data or lineTable + index * 8 + 8 > #data then break end
    local lineCount, firstLine = I32(data, lineTable + index * 8), I32(data, lineTable + index * 8 + 4)
    if lineCount > 0 and firstLine >= 0 and firstLine + 16 <= #data then
      names[MsgString(data, nameTable + index * 16, wideNames)] = CleanText(MsgString(data, firstLine, wideText))
      read = read + 1
    end
  end
  namesNote = string.format("%d (from %s)", read, source)
end

-- Favorites ------------------------------------------------------------------
-- favorite_characters.sav: {"<account>": ["GOK_0", "HUM@2", ...], ...} - the
-- grid's keys (as3\chara_search.as), the same file CharaSearch.dll wrote.
local account = nil                       -- the save's Steam id as text; nil until a save is loaded
local favorites = {}                      -- this account's: [key] = true
local savedFavorites = {}                 -- every account's, as in the file: [account] = {key, ...}
local favoriteHeld = false                -- F / left stick held last frame (a press counts once)

local function FavoritesPath() return SCRIPT_PATH .. "\\" .. FAVORITES_FILE end

-- Only plain keys are kept (codes, "HUM@2"): they are written unescaped.
local function PlainKey(key) return key ~= "" and not key:find('["\\\r\n]') end

-- The favorites of older versions lived in Battle Styles' folder (the search
-- was part of it), next to this mod's: taken over once, when there is no file here.
local function TakeOverOldFavorites()
  local here = io.open(FavoritesPath(), "rb")
  if here then here:close() return end
  local old = io.open(SCRIPT_PATH .. "\\..\\BattleStyles\\" .. FAVORITES_FILE, "rb")
  if not old then return end
  local text = old:read("a")
  old:close()
  local file = io.open(FavoritesPath(), "wb")
  if not file then return end
  file:write(text)
  file:close()
  ConsolePrint("favorites copied from Battle Styles' folder", 1)
end

-- A missing file is no favorites; a .tmp left by a save cut short is taken
-- when the file is gone.
local function LoadFavorites()
  savedFavorites = {}
  local file = io.open(FavoritesPath(), "rb") or io.open(FavoritesPath() .. ".tmp", "rb")
  if not file then return end
  local text = file:read("a") or ""
  file:close()
  for owner, body in text:gmatch('"(%d+)"%s*:%s*%[(.-)%]') do
    local keys = {}
    for key in body:gmatch('"([^"\\\r\n]+)"') do keys[#keys + 1] = key end
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

-- The loaded save's Steam account, or nil while no save is loaded. Read from
-- the save rather than Steam: saves are per account.
local function SteamAccount()
  local saveSystem = readPointer(base + SAVE_SYSTEM)
  if not saveSystem or saveSystem == 0 then return nil end
  local image = saveSystem + SAVE_IMAGE
  if readString(image, 4) ~= "#SAV" then return nil end
  local id = readQword(image + SAVE_STEAM_ID)
  if not id or id == STEAM_ID_XOR then return nil end
  return string.format("%d", id ~ STEAM_ID_XOR)  -- Steam ids are below 2^63: the same text as std::to_string
end

-- Learns the account once a save is loaded and reads its favorites.
local function EnsureAccount()
  if account then return true end
  account = SteamAccount()
  if not account then return false end
  LoadFavorites()
  favorites = {}
  for _, key in ipairs(savedFavorites[account] or {}) do favorites[key] = true end
  return true
end

-- True on the frame F or the left stick goes down (not Ctrl+F: the box's key).
local function FavoritePressed()
  local held = (isKeyPressed(FAVORITE_KEY) and not isKeyPressed(VK_CONTROL))
      or (isPadButtonPressed ~= nil and isPadButtonPressed(PAD_LEFT_THUMB))
  local pressed = held and not favoriteHeld
  favoriteHeld = held
  return pressed
end

-- The grid -------------------------------------------------------------------
local token = nil                         -- the grid's, sent with its list and echoed in every answer
local entries = {}                        -- per list entry (0-based index + 1): {key, words}
local cursorEntry = -1
local stale = false                       -- new list or favorite toggled: answer again
local answeredChanges = nil               -- the box's change count the last answer was made for
local noMatches = false

-- Parses the 9902 list ("token\nkey\tcodes\n...").
local function ReadList(text)
  if not names then ReadNames() end
  entries = {}
  local first = true
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do
    if first then
      token, first = line, false
    else
      local key, codes = line:match("^([^\t]*)\t?(.*)$")
      local words = {}
      for code in codes:gmatch("[^,]+") do
        words[#words + 1] = code
        local name = names["chara_" .. code .. "_000"]
        if name and name ~= "" then words[#words + 1] = name end
        if code == "HUM" then words[#words + 1] = "CaC" end  -- the custom characters (AvatarCode)
      end
      entries[#entries + 1] = {key = key, words = words}
    end
  end
  -- an empty last line is the text's own ending, not an entry
  if #entries > 0 and entries[#entries].key == "" and #entries[#entries].words == 0 and text:sub(-1) == "\n" then
    entries[#entries] = nil
  end
  cursorEntry, stale = -1, true
end

-- The answer for the current list, box and favorites.
local function Answer()
  local searching = search.Query(BOX_ID) ~= ""
  local shown, favoriteEntries = {}, {}
  local characters, matches = 0, 0
  for index, entry in ipairs(entries) do
    if entry.key ~= "" then
      characters = characters + 1
      local match = not searching
      for _, word in ipairs(entry.words) do
        if match then break end
        match = search.Matches(BOX_ID, word)
      end
      if match then
        matches = matches + 1
        shown[#shown + 1] = index - 1
      end
      if favorites[entry.key] then favoriteEntries[#favoriteEntries + 1] = index - 1 end
    end
  end
  noMatches = matches == 0 and characters > 0
  -- nothing matching shows everything, with the note
  local filter = (matches == characters or noMatches) and "*" or table.concat(shown, ",")
  return token .. "\t" .. filter .. "\t" .. table.concat(favoriteEntries, ",")
end

-- F / left stick: toggles the favorite of the entry under the cursor.
local function ToggleFavorite()
  local entry = entries[cursorEntry + 1]
  if not entry or not PlainKey(entry.key) then return false end
  if not EnsureAccount() then ConsolePrint("favorite: no save loaded - ignored", 2) return false end
  favorites[entry.key] = not favorites[entry.key] or nil
  SaveFavorites()
  return true
end

function _OnInit()
  -- no _OnMenu, no section in the loader's menu
  if not MenuSectionWanted() then _OnMenu = nil end
  if not menus or not files or not search or not files.Owner then
    status = "XV2 Mod Loader is too old (no menus table); update it"
    ConsolePrint(status, 2)
    return
  end
  -- the callback first: a served script whose messages reach nobody would
  -- hold the grid for its wait each time
  local hooked, reason = menus.HookCallback(MENU_NAME, SELECT_CHARA_VTABLE, USER_CALLBACK_SLOT,
      {[SEND_LIST] = "string", [SEND_CURSOR] = "number"}, CALLBACK_MUTEX)
  if not hooked then
    status = "character select not hooked (" .. tostring(reason) .. "); off"
    ConsolePrint(status, 3)
    return
  end
  local refused = {}
  for _, script in ipairs(SCRIPTS) do
    local replaced, why = files.ReplaceIggyScript(CHARASELE_IGGY, script.abc, script.vanillaSize, script.vanillaHash)
    if not replaced then refused[#refused + 1] = script.abc .. ": " .. tostring(why) end
  end
  if #refused == #SCRIPTS then
    menus.Unhook(MENU_NAME)
    status = "menu script not registered (" .. table.concat(refused, "; ") .. "); off"
    ConsolePrint(status, 3)
    return
  end
  for _, note in ipairs(refused) do ConsolePrint("menu script not registered: " .. note, 2) end
  TakeOverOldFavorites()
  status = "ok"
end

function _OnFrame()
  if status ~= "ok" then return end
  for _, message in ipairs(menus.Messages(MENU_NAME)) do
    if message.type == SEND_LIST then ReadList(message.value)
    elseif message.type == SEND_CURSOR then cursorEntry = math.floor(message.value) end
  end
  local seen = menus.LastSeen(MENU_NAME, SEND_CURSOR)
  if not token or not seen or seen > OPEN_SECONDS then
    FavoritePressed()                     -- keeps the held state current
    return
  end
  EnsureAccount()
  -- shown while called each frame; hides itself once the grid closes
  local changes = search.Show(BOX_ID, {hint = BOX_HINT, x = BOX_X, y = BOX_Y, width = BOX_WIDTH,
                                       note = noMatches and NO_MATCH_NOTE or ""})
  -- read every frame so a press held from before (or while typing) never toggles later
  if FavoritePressed() and not search.Typing(BOX_ID) and ToggleFavorite() then stale = true end
  if changes ~= answeredChanges then stale = true end
  if stale then
    stale, answeredChanges = false, changes
    menus.SetUserDataString(MENU_NAME, ANSWER_ENTRY, Answer())
  end
end

function _OnMenu()
  ui.Text("Chara search: " .. status)
  ui.Text("Menu script: " .. (files and files.Status(CHARASELE_IGGY) or "not replaced"))
  if menus then ui.Text("Callback: " .. (menus.Status(MENU_NAME) or "not hooked")) end
  ui.Text("Character names: " .. namesNote)
  local seen = menus and menus.LastSeen(MENU_NAME, SEND_CURSOR)
  if token and seen and seen <= OPEN_SECONDS then
    ui.Text(string.format("Grid open: %d entries, cursor %d, typed \"%s\"", #entries, cursorEntry, search.Query(BOX_ID)))
  else
    ui.TextDisabled("Character select not open")
  end
  local favoriteCount = 0
  for _ in pairs(favorites) do favoriteCount = favoriteCount + 1 end
  ui.Text(account and string.format("Favorites: %d (Steam account %s)", favoriteCount, account)
      or "Favorites: no save loaded yet")
  if not isPadButtonPressed then ui.TextDisabled("Left stick needs a newer XV2 Mod Loader (F works)") end
end

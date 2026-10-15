-- Example Lua script for the XV2 Mod Loader.
--
-- Installed as XV2ModLoader\mods\_LuaExample\ - the "_" keeps it switched
-- off. Rename the folder to LuaExample to run it, and set "lua_console": true
-- in XV2ModLoader\loader.json to see what it prints (it also goes to
-- loader.log when "logging" is true).
--
-- It only reads memory; nothing in the game changes.

local framesSeen = 0

-- Runs once, when the game shows its first frame.
function _OnInit()
    -- LuaBackend-style functions take addresses as offsets from the start of
    -- DBXV2.exe. Offset 0 holds the exe header, which starts with "MZ".
    if ReadShort(0) == 0x5A4D then
        ConsolePrint("found the game's exe header", 1)            -- 1 = SUCCESS
    else
        ConsolePrint("the exe header looks wrong", 3)             -- 3 = ERROR
    end

    -- Cheat Engine-style functions take full addresses, or text such as
    -- "DBXV2.exe+1A2B" (hex), as in a CE table.
    ConsolePrint(string.format("DBXV2.exe starts at %X", getAddress("DBXV2.exe")), 0)

    -- Writing works the same way, for example:
    --   WriteFloat(0x1234560, 2.0)                -- offset into DBXV2.exe
    --   writeFloat("DBXV2.exe+1234560", 2.0)      -- the same, CE style
end

-- Runs every frame. Keep it quick: the game waits for it.
function _OnFrame()
    framesSeen = framesSeen + 1
    if framesSeen % 600 == 0 then                                -- about every 10 s at 60 fps
        ConsolePrint(framesSeen .. " frames so far", 0)          -- 0 = MESSAGE
    end
end

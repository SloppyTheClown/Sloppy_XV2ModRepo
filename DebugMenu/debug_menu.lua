-- Debug Menu: a section in XV2 Mod Loader's in-game menu (~, the key left of 1).
--
-- Noclip (lobby and battle): the player keeps their normal controls (walk,
-- fly) but collides with nothing. The player is a Havok (hknp) body in both
-- modes; noclip puts it on a collision layer no other body uses and switches
-- that layer off in the world's collision filter, then rebuilds the body's
-- collision caches so pairs that already exist (the level) are filtered
-- again. The lobby follow camera's wall check layer is switched off too. Off
-- puts it all back. Noclip stays ticked across mode changes (lobby, loading,
-- battle) and is applied to each new world.
-- Found with ClaudeTools\lobbybodyprobe.py and ClaudeTools\battlebodyprobe.py
-- (Research_Notes\lobby-physics-and-debug-menu.md).

-- Game globals and offsets (DBXV2.exe 1.26.01):
local LOBBY_PLAYER = 0x15712f8           -- CLobbyPlayer*, set after its ctor at 0x2f9715
local LOBBY_CAMERA_MANAGER = 0x15712e8   -- Lobby::CameraManager*, +0x20 = scene camera source
local BATTLE_CAMERA_MANAGER = 0x156b408  -- Battle::CameraManager*, +8 = director, [director] = EmCamera
local CAMERA_POSITION = 0x90             -- XG::Eva::EmCamera +0x90 position, +0xa0 interest
local CAMERA_INTEREST = 0xa0
local PLAYER_PHYSICS = 0x5b0             -- CLobbyPlayer +0x5b0 = PhysChar@Lobby (RTTI)
local PHYSICS_WORLD = 0x8                -- PhysChar +8 = hknpWorld (passed to enableBodyFlags 0xc90ac0)
local PHYSICS_BODY = 0x80                -- PhysChar@Lobby +0x80 = body handle, +0x10 = hknpBodyId (low 24 bits)
-- Battle: the camera director follows the local player's Mob (director +0x90;
-- the three directors at manager +8/+0x18/+0x20 all held it, and every
-- battle camera's +0x3a0 target was that Mob and never the training partner -
-- battlebodyprobe.py in Training). Mob +0x4e0 = PhysVarChar@Battle, laid
-- out like the lobby PhysChar but with the body handle at +0x78.
local DIRECTOR_TARGET_MOB = 0x90
local MOB_VTABLE = 0xf5e020              -- Mob@Battle (RTTI), checked so a stale pointer is never written through
local MOB_PHYSICS = 0x4e0
local BATTLE_PHYSICS_VTABLE = 0xf65588   -- PhysVarChar@Battle (RTTI)
local BATTLE_PHYSICS_BODY = 0x78
local WORLD_BODIES = 0x20                -- hknpWorld +0x20 = body array (0xc90820)
local BODY_SIZE = 0xb0                   -- bytes per hknpBody (imul 0xb0 in 0xc90820)
local BODY_FILTER_INFO = 0x6c            -- hknpBody +0x6c = collisionFilterInfo (setBodyCollisionFilterInfo 0xc90820)
local WORLD_FILTER = 0x468               -- hknpWorld +0x468 = collision filter (live scan)
local FILTER_VTABLE = 0xfbf3f8           -- hknpCustomGroupCollisionFilter<6,5,5,16> vtable (RTTI)
local FILTER_TABLE = 0x28                -- filter +0x28: 64 masks, bit B of mask A = layer A hits layer B (0x13dd50)
local LAYER_COUNT = 64                   -- 6-bit layers (Config<6,5,5,16>)
-- Havok only checks the filter when a body pair is created, so a new filter
-- value needs the body's collision caches rebuilt, or pairs that already exist
-- (the whole lobby level is one of them) keep colliding. These are the fields
-- hknpWorld::rebuildBodyCollisionCaches (0xc90eb0) writes for an active dynamic body:
local BODY_MOTION_ID = 0x40              -- hknpBody +0x40 = motion index
local BODY_FLAGS = 0x44                  -- low bits: 2 = dynamic, 1 | 8 = static / active
local BODY_DIRTY = 0x48                  -- u16, rebuild ORs in 1
local WORLD_PREVIOUS_AABBS = 0x38        -- 16 bytes per body id: quantized AABB the broadphase last saw
local WORLD_MOTIONS = 0x820              -- [+0x820] +8 = 16 bytes per motion; rebuild zeroes byte +0xb unless 0xff
local INVALID_AABB = 0x7fffff7fff7fffff  -- written to +0 (and 0 to +8): "never seen", so the body's pairs are re-added
-- Layer 63: no body uses it, in the lobby (bodies on 0-25) or in Training
-- (0-35), and its row only hits layer 0. filterInfo = 63 also means system
-- group 0, so the pair check always goes through the table.
local NOCLIP_LAYER = 63
-- The lobby follow camera keeps out of walls with a shape cast on layer 22
-- (CameraLocal@Lobby 0x3ba8e0 passes filterInfo 0x16 to the cast wrapper
-- 0x30a4e0, which uses the world filter). No lobby or Training body is on
-- layer 22. The battle camera's wall check is not known.
local CAMERA_LAYER = 22
-- Layers noclip switches off, with their stock rows (lobbybodyprobe.py with
-- noclip off; battlebodyprobe.py shows the same rows in Training). The table
-- is symmetric, so a stock row also gives that layer's bit in every other
-- row - used to undo a patch an F1 reload forgot.
local STOCK_ROWS = {[NOCLIP_LAYER] = 0x1, [CAMERA_LAYER] = 0x3217}

-- Havok freezes a body that leaves the world's broadphase bounds (made static,
-- motion 0: flags 0x41 - battlebodyprobe.py on a player stuck outside a quest
-- map). The body's integration posts a BodyExitedBroadPhase event (type 0xb,
-- the event printer 0xce7070 via the dispatcher 0xcbc9b0) at two copies of
-- the same code; the freeze is done by whoever handles the event. A Lua script
-- can't turn a static body back into a dynamic one (that needs a new motion),
-- so noclip stops the event for bodies on the noclip layer instead. Each
-- 56-byte event block gets `cmp byte [rbx+0x6c], 63; je <past the event>`
-- (rbx = the body, +0x6c its filterInfo) in front; the room comes from
-- dropping the block's last constant reload into xmm6/xmm7 after the event
-- call - those registers are callee-saved on x64 Windows and the path that
-- skips the event joins without the reload, so it changed nothing. No branch
-- lands inside either block (scanned 0x3000 bytes around each). The world's
-- bounds and every other body are untouched. Applied the first time noclip
-- is ticked and left in (it only matters to layer 63, which nothing else uses).
local FREEZE_EVENT_SITES = {
    {offset = 0xd1a55f,
     stock = {0x8b,0x43,0x70, 0x48,0x8d,0x54,0x24,0x6c, 0x48,0x8b,0x8f,0xb0,0x03,0x00,0x00,
              0x66,0x44,0x89,0x74,0x24,0x6c, 0xc7,0x44,0x24,0x6e,0x00,0x04,0x0b,0x00,
              0x89,0x44,0x24,0x74, 0x48,0x8b,0x01, 0xff,0x50,0x18,
              0x0f,0x28,0x35,0x43,0x9c,0x47,0x00, 0x44,0x0f,0x28,0x55,0xb0, 0x44,0x0f,0x28,0x4d,0xa0},
     noclip = {0x80,0x7b,0x6c,0x3f, 0x74,0x32,
               0x8b,0x43,0x70, 0x48,0x8d,0x54,0x24,0x6c, 0x48,0x8b,0x8f,0xb0,0x03,0x00,0x00,
               0x66,0x44,0x89,0x74,0x24,0x6c, 0xc7,0x44,0x24,0x6e,0x00,0x04,0x0b,0x00,
               0x89,0x44,0x24,0x74, 0x48,0x8b,0x01, 0xff,0x50,0x18,
               0x44,0x0f,0x28,0x55,0xb0, 0x44,0x0f,0x28,0x4d,0xa0, 0x90}},
    {offset = 0xdad0de,
     stock = {0x8b,0x43,0x70, 0x48,0x8d,0x54,0x24,0x70, 0x49,0x8b,0x8c,0x24,0xb0,0x03,0x00,0x00,
              0x66,0x89,0x7c,0x24,0x70, 0xc7,0x44,0x24,0x72,0x00,0x04,0x0b,0x00,
              0x89,0x44,0x24,0x78, 0x48,0x8b,0x01, 0xff,0x50,0x18,
              0x0f,0x28,0x3d,0xc4,0x70,0x3e,0x00, 0x44,0x0f,0x28,0x55,0xc0, 0x44,0x0f,0x28,0x4d,0xb0},
     noclip = {0x80,0x7b,0x6c,0x3f, 0x74,0x32,
               0x8b,0x43,0x70, 0x48,0x8d,0x54,0x24,0x70, 0x49,0x8b,0x8c,0x24,0xb0,0x03,0x00,0x00,
               0x66,0x89,0x7c,0x24,0x70, 0xc7,0x44,0x24,0x72,0x00,0x04,0x0b,0x00,
               0x89,0x44,0x24,0x78, 0x48,0x8b,0x01, 0xff,0x50,0x18,
               0x44,0x0f,0x28,0x55,0xc0, 0x44,0x0f,0x28,0x4d,0xb0, 0x90}},
}
local freezePatch                        -- nil = not tried yet, "on", or why it wasn't applied

local noclip = false
local patched                            -- what noclip changed, to put back: {filter, rows, body, world, bodies, bodyId, filterInfo}

local function BytesEqual(offset, expected)
    local actual = ReadArray(offset, #expected)
    for i = 1, #expected do
        if actual[i] ~= expected[i] then return false end
    end
    return true
end

-- Puts the layer-63 check into both event blocks, once. Each block is only
-- run for a body leaving the bounds, so writing it while the player is inside
-- them can't catch a thread half way through. Leaves the code alone if it is
-- neither the stock nor the patched bytes (another game version or mod).
local function PatchFreezeEvent()
    if freezePatch then return end
    for _, site in ipairs(FREEZE_EVENT_SITES) do
        if not BytesEqual(site.offset, site.stock) and not BytesEqual(site.offset, site.noclip) then
            freezePatch = string.format("unexpected code at %x", site.offset)
            return
        end
    end
    for _, site in ipairs(FREEZE_EVENT_SITES) do
        if not BytesEqual(site.offset, site.noclip) then WriteExec(site.offset, site.noclip) end
    end
    freezePatch = "on"
end

-- A pointer stored in the exe at `offset`, or nil when it is null.
-- (The loader's reads return 0 for an unreadable address.)
local function GlobalPointer(offset)
    local value = ReadLong(offset)
    if value == nil or value == 0 then return nil end
    return value
end

local function ReadVector(address)
    return ReadFloat(address, true), ReadFloat(address + 4, true), ReadFloat(address + 8, true)
end

local function FormatVector(x, y, z)
    return string.format("%9.2f %9.2f %9.2f", x, y, z)
end

-- The EmCamera the scene is drawn with: the lobby's when in the lobby, else
-- the battle director's. Returns the camera and where it came from.
local function CurrentCamera()
    local lobbyManager = GlobalPointer(LOBBY_CAMERA_MANAGER)
    if lobbyManager then
        local camera = ReadLong(lobbyManager + 0x20, true)
        if camera ~= 0 then return camera, "lobby" end
    end
    local battleManager = GlobalPointer(BATTLE_CAMERA_MANAGER)
    if battleManager then
        local director = ReadLong(battleManager + 8, true)
        if director ~= 0 then
            local camera = ReadLong(director, true)
            if camera ~= 0 then return camera, "battle" end
        end
    end
    return nil, "none"
end

-- The player's physics object (PhysChar@Lobby / PhysVarChar@Battle), where
-- its body handle is, and the mode name; nil when there is no player.
local function PlayerPhysicsObject()
    local player = GlobalPointer(LOBBY_PLAYER)
    if player then
        local physics = ReadLong(player + PLAYER_PHYSICS, true)
        if physics ~= 0 then return physics, PHYSICS_BODY, "lobby" end
    end
    local battleManager = GlobalPointer(BATTLE_CAMERA_MANAGER)
    if not battleManager then return nil end
    local director = ReadLong(battleManager + 8, true)
    if director == 0 then return nil end
    local mob = ReadLong(director + DIRECTOR_TARGET_MOB, true)
    if mob == 0 or ReadLong(mob, true) ~= BASE_ADDR + MOB_VTABLE then return nil end
    local physics = ReadLong(mob + MOB_PHYSICS, true)
    if physics == 0 or ReadLong(physics, true) ~= BASE_ADDR + BATTLE_PHYSICS_VTABLE then return nil end
    return physics, BATTLE_PHYSICS_BODY, "battle"
end

-- The player's Havok body, the world's collision filter, the world, the body
-- id and the mode, or nil when there is no player (loading, menus) or the
-- filter isn't the expected class.
local function PlayerPhysics()
    local physics, bodyOffset, mode = PlayerPhysicsObject()
    if not physics then return nil end
    local world = ReadLong(physics + PHYSICS_WORLD, true)
    local handle = ReadLong(physics + bodyOffset, true)
    if world == 0 or handle == 0 then return nil end
    local bodyId = ReadInt(handle + 0x10, true) & 0xffffff
    local bodies = ReadLong(world + WORLD_BODIES, true)
    local filter = ReadLong(world + WORLD_FILTER, true)
    if bodies == 0 or filter == 0 or ReadLong(filter, true) ~= BASE_ADDR + FILTER_VTABLE then
        return nil
    end
    return bodies + bodyId * BODY_SIZE, filter, world, bodyId, mode
end

-- Gives the body a new collisionFilterInfo and rebuilds its collision caches
-- the way 0xc90eb0 does. Only for an active dynamic body (the player while
-- the game runs); returns false otherwise, so the caller tries again later.
local function SetFilterInfo(world, body, bodyId, filterInfo)
    local flags = ReadInt(body + BODY_FLAGS, true)
    if flags & 2 == 0 or flags & 9 == 0 then return false end
    WriteInt(body + BODY_FILTER_INFO, filterInfo, true)
    local motions = ReadLong(ReadLong(world + WORLD_MOTIONS, true) + 8, true)
    local motion = motions + ReadInt(body + BODY_MOTION_ID, true) * 16
    if ReadByte(motion + 0xb, true) ~= 0xff then WriteByte(motion + 0xb, 0, true) end
    local previousAabb = ReadLong(world + WORLD_PREVIOUS_AABBS, true) + bodyId * 16
    WriteLong(previousAabb, INVALID_AABB, true)
    WriteLong(previousAabb + 8, 0, true)
    WriteShort(body + BODY_DIRTY, ReadShort(body + BODY_DIRTY, true) | 1, true)
    return true
end

-- Turning collision back on needs the rebuild repeated: the game applies
-- physics changes at a safe point before its step (XG::HavokWorld command
-- processor 0x303b90, run from JobCallbackPreUpdatePhys), while this script
-- writes from the render thread. One rebuild left the player without
-- collision (seen in game, twice); 10 rebuilds 100 ms apart from
-- ClaudeTools\lobbycacherebuild.py brought it back (seen in game). Here: one every
-- REBUILD_INTERVAL frames, REBUILD_COUNT times (about 1 s at 60 fps).
-- Only an awake body can be rebuilt: standing still (e.g. stuck in a wall)
-- puts it to sleep (flags 0x10042, probe), and the game's rebuild wakes it
-- first with 0xc754d0, which a script can't call. So the rebuilds wait until
-- the player moves, which wakes the body.
local REBUILD_COUNT = 10
local REBUILD_INTERVAL = 6
local restoring                          -- {body, world, bodyId, filterInfo, rebuilds, wait} while rebuilding after off

-- Switches the STOCK_ROWS layers off in the filter (their rows and their bit
-- in every row), keeping the old values.
local function PatchFilter(filter)
    local rows = {}
    local clearBits = 0
    for layer in pairs(STOCK_ROWS) do clearBits = clearBits | (1 << layer) end
    for layer = 0, LAYER_COUNT - 1 do
        local address = filter + FILTER_TABLE + layer * 8
        rows[layer] = ReadLong(address, true)
        local mask = STOCK_ROWS[layer] and 0 or (rows[layer] & ~clearBits)
        WriteLong(address, mask, true)
    end
    return rows
end

local function RestoreFilter(filter, rows)
    for layer = 0, LAYER_COUNT - 1 do
        WriteLong(filter + FILTER_TABLE + layer * 8, rows[layer], true)
    end
end

-- True when `world` is the world noclip patched: same address and same body
-- array, so a new world allocated where the old one was isn't mistaken for it.
local function IsPatchedWorld(world)
    return patched and world == patched.world and ReadLong(world + WORLD_BODIES, true) == patched.bodies
end

-- Puts back what noclip changed, as far as it still exists: the filter if
-- the current world still uses it, the body if it is still in the current
-- world. The body is the one patched, not whoever the camera follows now
-- (a battle camera could switch target).
local function StopNoclip()
    if not patched then return end
    local _, filter, world = PlayerPhysics()
    if filter == patched.filter then
        RestoreFilter(patched.filter, patched.rows)
    end
    if IsPatchedWorld(world) then
        WriteInt(patched.body + BODY_FILTER_INFO, patched.filterInfo, true)
        restoring = {body = patched.body, world = world, bodyId = patched.bodyId,
                     filterInfo = patched.filterInfo, rebuilds = 0, wait = 0}
    end
    patched = nil
end

-- One step of the repeated rebuild after noclip is turned off; a sleeping
-- body is retried every frame until it wakes. Stops early if the world (and
-- with it the body) is gone or the game changed the filter info.
local function KeepRestoring()
    local _, _, world = PlayerPhysics()
    local body = restoring.body
    if world ~= restoring.world or ReadInt(body + BODY_FILTER_INFO, true) ~= restoring.filterInfo then
        restoring = nil
        return
    end
    if restoring.wait > 0 then
        restoring.wait = restoring.wait - 1
    elseif SetFilterInfo(world, body, restoring.bodyId, restoring.filterInfo) then
        restoring.rebuilds = restoring.rebuilds + 1
        restoring.wait = REBUILD_INTERVAL - 1
        if restoring.rebuilds >= REBUILD_COUNT then restoring = nil end
    end
end

-- Keeps the player on the noclip layer: checked every frame in case the game
-- resets the body's filter info, and redone for each new world (lobby ->
-- battle and back). With no player (loading, menus) it waits.
local function KeepNoclip()
    PatchFreezeEvent()
    local body, filter, world, bodyId = PlayerPhysics()
    if not body then return end
    if IsPatchedWorld(world) then
        body, bodyId = patched.body, patched.bodyId   -- stay on the patched body for this world
    else
        StopNoclip()
        restoring = nil
        local filterInfo = ReadInt(body + BODY_FILTER_INFO, true)
        local rows = PatchFilter(filter)
        if rows[NOCLIP_LAYER] == 0 then
            -- Left on by a script reload (F1) or a world we never saw end:
            -- put back the stock values (probes: player filterInfo 0 in the
            -- lobby and in battle, STOCK_ROWS).
            if filterInfo == NOCLIP_LAYER then filterInfo = 0 end
            for layer, stockRow in pairs(STOCK_ROWS) do
                rows[layer] = stockRow
                for other = 0, LAYER_COUNT - 1 do
                    if other ~= layer and not STOCK_ROWS[other] then
                        rows[other] = rows[other] | (((stockRow >> other) & 1) << layer)
                    end
                end
            end
        end
        patched = {filter = filter, rows = rows, body = body, world = world,
                   bodies = ReadLong(world + WORLD_BODIES, true), bodyId = bodyId, filterInfo = filterInfo}
    end
    if ReadInt(body + BODY_FILTER_INFO, true) ~= NOCLIP_LAYER then
        SetFilterInfo(world, body, bodyId, NOCLIP_LAYER)
    end
end

-- Item getter: pick an item or skill from a list (search + dropdown) and get
-- +1 / +10 / as many as fit, or unlock / lock a skill. The list is read from
-- the game's own data files the first time the getter is drawn (loader
-- files.ReadGameFile: loose data\ first, then the .cpk archives, so x2m and
-- Revamp items are in it). Inventory and skills are save-wide; the game
-- writes them with its next save.
-- Found by reading the save serializer (0x85afb0) and the exe's own
-- give/unlock calls (Research_Notes\save-flags-and-unlocks.md "Inventory and
-- skills in memory").
local SAVE_MANAGER = 0x15545b0           -- save manager*, holds the inventory and skill lists
local MANAGER_INVENTORY = 0x20d8         -- manager +0x20d8 = inventory (0x81ac10), written to the save at +0x1410
local MANAGER_SKILLS = 0x4098            -- manager +0x4098 = skill lists (0x81acb0), written to the save at +0xb430
local LIST_SIZE = 0x10                   -- one std::list per inventory type / skill category: +0 head node, +8 count
local NODE_ITEM_ID = 0x14                -- inventory list node: +0 next, +0x10 type, +0x14 id, +0x18 count
local NODE_ITEM_COUNT = 0x18
local INVENTORY_CAPACITY = 0x400         -- add-item primitive 0x82ce30 refuses more nodes (0x100 for QQ bangs)
local GIVE_ITEM = 0x612ea0               -- (type, id, count, silent) -> 1/0; checks the item exists in the IDB, then 0x846d70
local FIND_SKILL = 0x83e770              -- (skill lists, category, ID2) -> skill record or 0
local SET_SKILL_UNLOCKED = 0x83eac0      -- (record, value): record +0 = value + the game's change event (callers pass 1)
-- DLC gate, read from the game's loaded IDB data (so flags cleared in the
-- loaded data count). IDB record (0x310 bytes): +0 id, +0xc type, +0xe DLC_FLAG.
local FIND_ITEM_RECORD = 0x616d60        -- (inventory type, id) -> loaded IDB record or 0
local IDB_MANAGER = 0x15545a8            -- loaded IDB files; 0x85c8f0(manager, index) -> raw file data
local IDB_FILE_DATA = 0x85c8f0
local SKILL_IDB_INDEX = 9                -- skill_item.idb (0x10c85a0 maps give types 10-15 to it)
local IDB_COUNT, IDB_FIRST = 0x8, 0xc    -- file header: record count, offset of the first record
local IDB_RECORD_SIZE = 0x310            -- 0x40 item + 3 effects of 0xf0 (0x851020's stride)
local IDB_RECORD_TYPE, IDB_RECORD_DLC = 0xc, 0xe
local IDB_MAX_RECORDS = 0x2000           -- bound for the skill walk (vanilla has 843)
local NO_PACK = 0x44                     -- 0x612f30: DLC_FLAG -1 / 0xff -> 0x44 = none
local ALWAYS_OWNED_PACK = 0x43           -- 0x2b9c30 returns 1 for packs >= 0x43
-- (pack, 1) -> 1 if the player has that DLC content pack, else 0; the check the
-- game runs on an IDB entry's DLC_FLAG (e.g. 0x6639b0).
local DLC_PACK_OWNED = 0x2b9c30
local MAX_SHOWN = 300                    -- rows the dropdown lists; type more to narrow it down
local COUNT_INTERVAL = 0.5               -- seconds between re-reading the selected item's count

-- The item list's groups. Items: (label, IDB file under system\item\, inventory
-- type = the save's EquipmentType, Xv2CoreLib SAV_File.cs, the first argument
-- of 0x612ea0; names from msg\<msg>_en.msg by entry id = the IDB record's
-- Name_MsgID). Max per item 99, important items 1 (add primitive 0x82ce30).
local ITEM_GROUPS = {
    {"Tops", "costume_top", 0, "proper_noun_costume_name"},
    {"Bottoms", "costume_bottom", 1, "proper_noun_costume_name"},
    {"Gloves", "costume_gloves", 2, "proper_noun_costume_name"},
    {"Shoes", "costume_shoes", 3, "proper_noun_costume_name"},
    {"Accessories", "accessory", 4, "proper_noun_accessory_name"},
    {"Super Souls", "talisman", 5, "proper_noun_talisman_name"},
    {"Materials", "material", 6, "proper_noun_material_name"},
    {"Important items", "extra", 7, "proper_noun_extra_name"},
    {"Capsules", "battle", 8, "proper_noun_battle_name"},
}
local IMPORTANT_ITEMS_TYPE, IMPORTANT_ITEMS_MAX, ITEM_MAX = 7, 1, 99
-- Skills: (label, CUS category index = the skill list index in the save
-- manager's lists (mgr+0x4098, CUS order: super, ultimate, evasive, unknown,
-- blast, awoken); names from the msg file by entry name = prefix + ID2, 4 digits).
local SKILL_GROUPS = {
    {"Super skills", 0, "proper_noun_skill_spa_name", "spe_skill_"},
    {"Ultimate skills", 1, "proper_noun_skill_ult_name", "ult_"},
    {"Evasive skills", 2, "proper_noun_skill_esc_name", "avoid_skill_"},
    {"Awoken skills (transformations)", 5, "proper_noun_skill_met_name", "met_skill_"},
}
local MSG_LANGUAGE = "en"
-- File layouts (as XV2-Tools Xv2CoreLib reads them; game 1.25+):
-- IDB: as loaded (IDB_COUNT, IDB_FIRST, IDB_RECORD_SIZE above); record +0 id,
-- +4 Name_MsgID (u16).
-- CUS: +0x10.. skill counts and +0x28.. their offsets per category (6 each),
-- 92-byte skills: +0 short name (4 chars), +0xa ID2 (u16).
-- msg: +4 i16 256 = UTF-16 names, +6 i16 1 = UTF-16 text; +8 entry count,
-- +0xc names (16 bytes each: +0 offset, +4/+8 size for 8-bit/UTF-16), +0x10
-- ids (i32 each), +0x14 lines (8 bytes each: count, offset of the first
-- line's string record, laid out like a name's).
local CUS_SKILL_SIZE = 92
local CUS_COUNTS, CUS_OFFSETS = 0x10, 0x28

local itemGroups                         -- the list read from the game files, or nil
local itemListError                      -- why it could not be read
local itemListState = "unread"           -- "unread" -> "reading" (one frame, so the message shows) -> "read"

local function U16(data, offset) return (string.unpack("<I2", data, offset + 1)) end
local function I32(data, offset) return (string.unpack("<i4", data, offset + 1)) end

-- UTF-16LE bytes -> UTF-8 (unpaired surrogates become U+FFFD).
local function Utf16ToUtf8(data, offset, size)
    local out, at, stop = {}, offset, offset + size - 1
    while at < stop do
        local unit = U16(data, at)
        at = at + 2
        if unit >= 0xd800 and unit <= 0xdbff and at < stop then
            local low = U16(data, at)
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

-- The game writes text with HTML entities (&apos; for ', ...).
local ENTITIES = {apos = "'", quot = '"', amp = "&", lt = "<", gt = ">"}
local function CleanText(text)
    text = text:gsub("&(#?)(%w+);", function(numeric, name)
        if numeric == "#" then
            local code = tonumber(name) or (name:sub(1, 1) == "x" and tonumber(name:sub(2), 16))
            return code and utf8.char(code) or nil
        end
        return ENTITIES[name]
    end)
    return (text:gsub("[%c]", " "):gsub("^%s+", ""):gsub("%s+$", ""))
end

-- One string record of a msg file (+0 offset, size at +4 or +8 for UTF-16).
local function MsgString(data, record, wide)
    local offset, size = I32(data, record), I32(data, record + (wide and 8 or 4))
    if size <= 0 then return "" end
    if wide then return Utf16ToUtf8(data, offset, size) end
    return (data:sub(offset + 1, offset + size):gsub("%z.*", ""))
end

-- msg\<base>_en.msg -> {[entry id] = text, [entry name] = text} (first line
-- of each entry); names only when byName. Empty when the file is missing.
local msgCache = {}
local function MsgEntries(base, byName)
    local key = base .. (byName and "#names" or "")
    if msgCache[key] then return msgCache[key] end
    local entries = {}
    msgCache[key] = entries
    local data = files.ReadGameFile("msg/" .. base .. "_" .. MSG_LANGUAGE .. ".msg")
    if not data then return entries end
    local wideNames, wideText = I32(data, 4) & 0xffff == 256, (I32(data, 4) >> 16) & 0xffff == 1
    local count, names, ids, lines = I32(data, 8), I32(data, 12), I32(data, 16), I32(data, 20)
    if count < 0 or count * 16 > #data then return entries end  -- not a msg file
    for index = 0, count - 1 do
        local lineCount, firstLine = I32(data, lines + index * 8), I32(data, lines + index * 8 + 4)
        local text = lineCount > 0 and CleanText(MsgString(data, firstLine, wideText)) or ""
        if byName then
            entries[MsgString(data, names + index * 16, wideNames)] = text
        else
            entries[I32(data, ids + index * 4)] = text
        end
    end
    return entries
end

-- {{id, name}} of one IDB file, sorted by id (first record of an id wins);
-- nameless items show their id.
local function ReadItemEntries(idbFile, msgBase)
    local data = files.ReadGameFile("system/item/" .. idbFile .. "_item.idb")
    if not data then return {} end
    local count, first = I32(data, IDB_COUNT), I32(data, IDB_FIRST)
    if count < 0 or first + count * IDB_RECORD_SIZE > #data then
        error(idbFile .. "_item.idb: unknown layout (not game 1.25+)")
    end
    local names, seen, entries = MsgEntries(msgBase, false), {}, {}
    for index = 0, count - 1 do
        local record = first + index * IDB_RECORD_SIZE
        local id = U16(data, record)
        if not seen[id] then
            seen[id] = true
            local name = names[U16(data, record + 4)]
            entries[#entries + 1] = {id, (name and name ~= "") and name or ("(no name " .. id .. ")")}
        end
    end
    table.sort(entries, function(a, b) return a[1] < b[1] end)
    return entries
end

-- {[category index] = {{ID2, name}}} from custom_skill.cus, sorted by ID2;
-- nameless skills show their 4-letter short name.
local function ReadSkillEntries()
    local data = files.ReadGameFile("system/custom_skill.cus")
    if not data then error("system/custom_skill.cus not found") end
    local superCount, superOffset, ultimateOffset = I32(data, CUS_COUNTS), I32(data, CUS_OFFSETS), I32(data, CUS_OFFSETS + 4)
    if superCount > 0 and (ultimateOffset - superOffset) // superCount ~= CUS_SKILL_SIZE then
        error("custom_skill.cus: unknown layout (not game 1.25+)")
    end
    local out = {}
    for _, group in ipairs(SKILL_GROUPS) do
        local category, msgBase, prefix = group[2], group[3], group[4]
        local count, offset = I32(data, CUS_COUNTS + category * 4), I32(data, CUS_OFFSETS + category * 4)
        if offset + count * CUS_SKILL_SIZE > #data then error("custom_skill.cus: damaged") end
        local names, seen, entries = MsgEntries(msgBase, true), {}, {}
        for index = 0, count - 1 do
            local skill = offset + index * CUS_SKILL_SIZE
            local id2 = U16(data, skill + 0xa)
            if not seen[id2] then
                seen[id2] = true
                local name = names[string.format("%s%04d", prefix, id2)]
                local shortName = data:sub(skill + 1, skill + 4):gsub("%z.*", "")
                entries[#entries + 1] = {id2, (name and name ~= "") and name or ("(" .. shortName .. ")")}
            end
        end
        table.sort(entries, function(a, b) return a[1] < b[1] end)
        out[category] = entries
    end
    return out
end

-- Builds itemGroups (same shape the old generated file had).
local function ReadItemList()
    local groups = {}
    for _, group in ipairs(ITEM_GROUPS) do
        local label, idbFile, itemType, msgBase = group[1], group[2], group[3], group[4]
        groups[#groups + 1] = {label = label, kind = "item", type = itemType,
                               max = itemType == IMPORTANT_ITEMS_TYPE and IMPORTANT_ITEMS_MAX or ITEM_MAX,
                               entries = ReadItemEntries(idbFile, msgBase)}
    end
    local skills = ReadSkillEntries()
    for _, group in ipairs(SKILL_GROUPS) do
        groups[#groups + 1] = {label = group[1], kind = "skill", category = group[2], max = 1,
                               entries = skills[group[2]] or {}}
    end
    msgCache = {}
    return groups
end

-- Reads the list once, the frame after the getter first showed "Reading ...".
local function LoadItemList()
    if itemListState == "unread" then
        itemListState = "reading"
        return
    end
    itemListState = "read"
    if not (files and files.ReadGameFile) then
        itemListError = "needs a newer XV2 Mod Loader (files.ReadGameFile)"
        return
    end
    local loaded, result = pcall(ReadItemList)
    if loaded then itemGroups = result else itemListError = tostring(result) end
end
local selectedGroup = 0                  -- 0 = every group
local searchText = ""
local matches = {}                       -- {group, id, name, text, label} shown in the dropdown
local matchCount = 0                     -- all matches, also those past MAX_SHOWN
local matchesStale = true
local selected                           -- one entry of matches
local ownedCount, ownedCountTime         -- selected item's count / skill state, re-read every COUNT_INTERVAL
local packMissing                        -- DLC pack the selected entry needs and the player lacks, or nil (re-read with ownedCount)
local pendingAction                      -- {kind = "give" | "unlock", amount / value}, done in _OnFrame
local actionResult = ""

local function SaveManager()
    return GlobalPointer(SAVE_MANAGER)
end

-- Count of an item in the inventory (0 if absent), or nil without a save manager.
local function InventoryCount(itemType, itemId)
    local manager = SaveManager()
    if not manager then return nil end
    local head = ReadLong(manager + MANAGER_INVENTORY + itemType * LIST_SIZE, true)
    if not head or head == 0 then return nil end
    local node = ReadLong(head, true)
    for _ = 1, INVENTORY_CAPACITY do     -- bounded: a list never holds more
        if not node or node == 0 or node == head then break end
        if ReadInt(node + NODE_ITEM_ID, true) == itemId then return ReadInt(node + NODE_ITEM_COUNT, true) end
        node = ReadLong(node, true)
    end
    return 0
end

local function SkillRecord(category, id2)
    local manager = SaveManager()
    if not manager then return nil end
    local record = CallFunction(BASE_ADDR + FIND_SKILL, manager + MANAGER_SKILLS, category, id2)
    if record and record ~= 0 then return record end
    return nil
end

-- Selected item's count, or for a skill its unlocked value (0 = locked); nil = no save loaded.
local function ReadOwned(entry)
    if entry.group.kind == "item" then return InventoryCount(entry.group.type, entry.id) end
    local record = SkillRecord(entry.group.category, entry.id)
    return record and ReadInt(record, true) or nil
end

-- The entry's loaded IDB record, or nil. Items: the game's own lookup. Skills:
-- skill_item.idb holds every category under one id range, so match id and
-- type (= skill list index), which the id-only lookup 0x851020 can't.
local function IdbRecord(entry)
    if entry.group.kind == "item" then
        local record = CallFunction(BASE_ADDR + FIND_ITEM_RECORD, entry.group.type, entry.id)
        return record and record ~= 0 and record or nil
    end
    local manager = GlobalPointer(IDB_MANAGER)
    if not manager then return nil end
    local data = CallFunction(BASE_ADDR + IDB_FILE_DATA, manager, SKILL_IDB_INDEX)
    if not data or data == 0 then return nil end
    local count = math.min(ReadInt(data + IDB_COUNT, true), IDB_MAX_RECORDS)
    local record = data + ReadInt(data + IDB_FIRST, true)
    for _ = 1, count do
        if ReadShort(record, true) == entry.id and ReadShort(record + IDB_RECORD_TYPE, true) == entry.group.category then
            return record
        end
        record = record + IDB_RECORD_SIZE
    end
    return nil
end

-- The DLC pack the entry needs (its loaded IDB DLC_FLAG, mapped like
-- 0x612f30), or nil for none / no record (roster-only skills).
local function DlcPack(entry)
    local record = IdbRecord(entry)
    if not record then return nil end
    local flag = ReadShort(record + IDB_RECORD_DLC, true)
    if flag == 0xffff or flag == 0xff then flag = NO_PACK end
    return flag < ALWAYS_OWNED_PACK and flag or nil
end

-- The pack the entry needs and the player doesn't have, or nil; such
-- entries can't be given.
local function MissingDlcPack(entry)
    local pack = DlcPack(entry)
    if not pack then return nil end
    local owned = CallFunction(BASE_ADDR + DLC_PACK_OWNED, pack, 1)
    if owned and owned & 0xff ~= 0 then return nil end
    return pack
end

-- Rebuilds the dropdown's rows after the group or the search text changed.
local function RefreshMatches()
    matches, matchCount, matchesStale = {}, 0, false
    local needle = searchText:lower()
    for groupIndex, group in ipairs(itemGroups) do
        if selectedGroup == 0 or selectedGroup == groupIndex then
            for _, entry in ipairs(group.entries) do
                local name = entry[2]
                if needle == "" or name:lower():find(needle, 1, true) or tostring(entry[1]) == needle then
                    matchCount = matchCount + 1
                    if matchCount <= MAX_SHOWN then
                        local prefix = selectedGroup == 0 and ("[" .. group.label .. "] ") or ""
                        local text = string.format("%s%s  #%d", prefix, name, entry[1])
                        -- "##n" keeps rows with the same text apart (ImGui ids); not shown
                        matches[#matches + 1] = {group = group, id = entry[1], name = name,
                                                 text = text, label = text .. "##" .. matchCount}
                    end
                end
            end
        end
    end
    ownedCountTime = nil
    -- keep the selection while it still matches, else take the first match
    if selected then
        for _, entry in ipairs(matches) do
            if entry.group == selected.group and entry.id == selected.id then
                selected = entry
                return
            end
        end
    end
    selected = matches[1]
end

-- Runs a button press from _OnFrame, outside ImGui's frame.
local function DoPendingAction()
    local action, entry = pendingAction, selected
    pendingAction = nil
    ownedCountTime = nil
    if not entry then return end
    local group = entry.group
    -- checked again here, not only by hiding the buttons
    if action.kind == "give" or action.value == 1 then
        local missing = MissingDlcPack(entry)
        if missing then
            actionResult = string.format("%s: needs DLC pack %d, not owned", entry.name, missing)
            return
        end
    end
    if action.kind == "give" then
        local owned = InventoryCount(group.type, entry.id)
        if not owned then
            actionResult = "no save loaded"
            return
        end
        -- the game refuses an add that would pass the max, so give only what fits
        local amount = math.min(action.amount, group.max - owned)
        if amount <= 0 then
            actionResult = string.format("%s: already %d (max %d)", entry.name, owned, group.max)
            return
        end
        local result, err = CallFunction(BASE_ADDR + GIVE_ITEM, group.type, entry.id, amount, 0)
        if result and result & 0xff == 1 then
            actionResult = string.format("%s: +%d", entry.name, amount)
        else
            actionResult = string.format("%s: the game refused it%s", entry.name, err and (" (" .. err .. ")") or "")
        end
    else
        local record = SkillRecord(group.category, entry.id)
        if not record then
            actionResult = entry.name .. ": no save loaded, or the skill is not in the save's list"
            return
        end
        local _, err = CallFunction(BASE_ADDR + SET_SKILL_UNLOCKED, record, action.value)
        actionResult = err and (entry.name .. ": " .. err) or
                       string.format("%s: %s", entry.name, action.value == 1 and "unlocked" or "locked")
    end
end

local function DrawItemGetter()
    if itemListState ~= "read" then LoadItemList() end
    if not itemGroups then
        if itemListState ~= "read" then
            ui.Text("Item getter: reading the game's item list...")
        else
            ui.Text("Item getter: could not read the game's item list")
            ui.TextDisabled("  " .. tostring(itemListError))
        end
        return
    end
    if matchesStale then RefreshMatches() end
    ui.SetNextItemWidth(220)
    if ui.BeginCombo("##ItemGroup", selectedGroup == 0 and "All" or itemGroups[selectedGroup].label, 15) then
        if ui.Selectable("All", selectedGroup == 0) then selectedGroup, matchesStale = 0, true end
        for groupIndex, group in ipairs(itemGroups) do
            if ui.Selectable(group.label, selectedGroup == groupIndex) then selectedGroup, matchesStale = groupIndex, true end
        end
        ui.EndCombo()
    end
    ui.SameLine()
    ui.SetNextItemWidth(260)
    local text, changed = ui.InputText("##ItemSearch", searchText, 64, "search name or id")
    if changed then searchText, matchesStale = text, true end

    ui.SetNextItemWidth(490)
    if ui.BeginCombo("##ItemPick", selected and selected.text or "(nothing matches)", 15) then
        for _, entry in ipairs(matches) do
            local isSelected = entry == selected
            if ui.Selectable(entry.label, isSelected) then selected, ownedCountTime = entry, nil end
            if isSelected then ui.SetItemDefaultFocus() end
        end
        if matchCount > MAX_SHOWN then
            ui.TextDisabled(string.format("%d more - type to narrow it down", matchCount - MAX_SHOWN))
        end
        ui.EndCombo()
    end
    if not selected then return end

    local now = os.clock()
    if not ownedCountTime or now - ownedCountTime >= COUNT_INTERVAL then
        ownedCount, ownedCountTime = ReadOwned(selected), now
        packMissing = MissingDlcPack(selected)
    end
    if packMissing then
        -- Lock stays available: taking a skill away needs no DLC
        if selected.group.kind == "skill" and ownedCount and ownedCount ~= 0 then
            if ui.Button("Lock") then pendingAction = {kind = "unlock", value = 0} end
            ui.SameLine()
        end
        ui.Text(string.format("Needs DLC pack %d - not owned", packMissing))
    elseif selected.group.kind == "item" then
        if ui.Button("+1") then pendingAction = {kind = "give", amount = 1} end
        ui.SameLine()
        if ui.Button("+10") then pendingAction = {kind = "give", amount = 10} end
        ui.SameLine()
        if ui.Button("+Max") then pendingAction = {kind = "give", amount = selected.group.max} end
        ui.SameLine()
        ui.Text(ownedCount and string.format("have %d / %d", ownedCount, selected.group.max) or "no save loaded")
    else
        if ui.Button("Unlock") then pendingAction = {kind = "unlock", value = 1} end
        ui.SameLine()
        if ui.Button("Lock") then pendingAction = {kind = "unlock", value = 0} end
        ui.SameLine()
        ui.Text(ownedCount == nil and "no save loaded" or (ownedCount ~= 0 and "unlocked" or "locked"))
    end
    if actionResult ~= "" then ui.TextDisabled(actionResult) end
end

-- FPS counter, shown at the top right while the menu is open. _OnFrame runs
-- once per Present, so frames counted over FPS_INTERVAL seconds of wall time
-- (os.clock is the MSVC clock(): wall time since start, ms steps) give the fps.
local FPS_INTERVAL = 0.5
local FPS_MARGIN = 8                     -- pixels from the picture's top right corner
local fpsFrames = 0
local fpsStart = os.clock()
local fps                                -- nil until the first interval has passed

local function CountFrame()
    fpsFrames = fpsFrames + 1
    local now = os.clock()
    local elapsed = now - fpsStart
    if elapsed >= FPS_INTERVAL then
        fps = fpsFrames / elapsed
        fpsFrames = 0
        fpsStart = now
    end
end

function _OnOverlay()
    if not ui.IsMenuOpen() or not fps then return end
    local width = ui.GetDisplaySize()
    ui.SetNextWindowPos(width - FPS_MARGIN, FPS_MARGIN, false, 1, 0)
    local flags = ui.WindowFlags.NoDecoration + ui.WindowFlags.NoBackground + ui.WindowFlags.AlwaysAutoResize
                  + ui.WindowFlags.NoSavedSettings + ui.WindowFlags.NoFocusOnAppearing + ui.WindowFlags.NoNav
    if ui.Begin("##DebugMenuFps", flags) then
        ui.TextOutlined(string.format("%.0f FPS", fps))
    end
    ui.End()
end

function _OnFrame()
    CountFrame()
    if pendingAction then DoPendingAction() end
    if noclip then
        KeepNoclip()
    elseif patched then
        StopNoclip()
    elseif restoring then
        KeepRestoring()
    end
end

function _OnMenu()
    DrawItemGetter()
    ui.Separator()
    local body, _, _, _, mode = PlayerPhysics()
    noclip = ui.Checkbox("Noclip", noclip)
    if body then
        ui.Text(string.format("  %s, collision layer %d", mode, ReadInt(body + BODY_FILTER_INFO, true) & 0x3f))
        if restoring and restoring.rebuilds == 0 then
            ui.Text("  move to turn collision back on")
        end
    else
        ui.Text("  no player right now")
    end
    if freezePatch and freezePatch ~= "on" then
        ui.Text("  leaving the map will freeze you: " .. freezePatch)
    end
    local camera, source = CurrentCamera()
    if camera then
        ui.Separator()
        ui.Text(string.format("Camera (%s)", source))
        ui.Text("  position " .. FormatVector(ReadVector(camera + CAMERA_POSITION)))
        ui.Text("  interest " .. FormatVector(ReadVector(camera + CAMERA_INTEREST)))
    end
end

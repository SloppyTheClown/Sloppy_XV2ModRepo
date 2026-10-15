# Lobby physics (Havok hknp) and DebugMenu

Covers: the lobby player's Havok body, the collision filter, why noclip needs a
collision-cache rebuild, the debug menu's Lua side. Overlay/loader internals
are in loader-and-steam-deck.md.

## Chain (1.26.01, found with ClaudeTools\noclipprobe.py, ClaudeTools\lobbybodyprobe.py)
* `[exe+0x15712f8]` = CLobbyPlayer; +0x5b0 = PhysChar@Lobby (vtable 0x1094998).
* PhysChar: +8 hknpWorld, +0x10 chara, +0x80 body handle (+0x10 = hknpBodyId,
  low 24 bits), +0x88 character context, +0xa0 position, +0xec in-world flag.
  Step = vtable slot 6 (0x4b3c60): 0x4b4160 (result -> +0xfc), 0x4b4ed0,
  character state machine 0xc97780, then game physics commands 0x308810 /
  0x3086c0 (velocity, cmd 0x21000c) / 0x308500. Commands are allocated by
  0x302e50, header = gameType<<16 | size.
* hknp command printer 0xcb6920 (jump table 0xcb7730 = type -> name, e.g. 0x1b
  setBodyCollisionFilterInfo, 0x1c updateBodyFlags); processor 0xcb6330
  (table 0xcb6830): 0x1b -> 0xc90820, 0x1c -> 0xc90ac0 enable / 0xc90bc0 disable.
* hknpWorld: +0x20 bodies (0xb0 each), +0x38 previous quantized AABB per body
  (16 bytes), +0x468 collision filter (the only filter object in the world),
  +0x470 listener, +0x820 [+8] motions (16 bytes each), +0xa50 lock.
* hknpBody: +0x30 position, +0x40 motion id, +0x44 flags (1 static, 2 dynamic,
  4 keyframed, 8 active), +0x48 u16 flags, +0x6c collisionFilterInfo, +0x78
  (-1 = not in world), +0x90 user data. Live: player body 542, flags 0x1004a,
  filterInfo 0; its keyframed companion on layer 18 sits at the same spot.

## Collision filter
hknpCustomGroupCollisionFilter<6,5,5,16> (vtable 0xfbf3f8, same slots as its
GroupBase). Pair check 0x13dd50 / 0x13de40 / 0x13dec0: same non-zero system
group (bits 16-31) -> subsystem rule (bits 6-10 / 11-15); otherwise
`table[layerB] & (1 << layerA)`, table = filter +0x28, 64 x u64, layer = low 6
bits. Slots 5 and 7 (0x25a40) return true. Live lobby: layer 0 row all ones;
rows 56-63 = 0x1; statics on layers 0/1/2/6/7/10/12/25, dynamic/keyframed on
0/18/25. Layer 63 is unused -> noclip layer.

## Noclip
* Writing filterInfo 63 + clearing layer 63 in the table: NPCs became
  walk-through, walls/floor did not (seen in game). Havok only runs the filter
  when a pair is created; the player's pairs with the level already existed.
* Fix: copy hknpWorld::rebuildBodyCollisionCaches (0xc90eb0, called by
  0xc90820 unless its 4th arg is set) for an active dynamic body: motion entry
  byte +0xb := 0 unless 0xff; world+0x38 entry := 0x7fffff7fff7fffff, 0 (the
  value unused bodies have = "never seen", so the broadphase re-adds the pairs);
  body +0x48 |= 1. Seen in game: walls, floors, ceilings, NPCs all pass.
* body +0x48 is a u16 flag word read by the narrowphase (0xcc95ba:
  `(A|B) & 0xff1`, 0xcc9ec2: `& 4` skips contacts); 0xc90b50 ORs flags into
  it (+ rebuild), the step clears bit 1.
* Turning collision back on: one rebuild from the Lua (render thread) left
  the player without collision, both with and without bit 1 (seen in game;
  filter and filterInfo stock afterwards). Dead-end theory: "bit 1 deletes the
  pairs re-added that step" - the game's own setBodyCollisionFilterInfo sets it
  too. The game queues physics changes and applies them at a safe point
  (XG::HavokWorld command processor 0x303b90, from JobCallbackPreUpdatePhys;
  filter command type 0x16, queued by 0x308600), so a single off-thread write
  is likely lost. What worked: `ClaudeTools\lobbycacherebuild.py full 10` (full
  rebuild incl. bit 1, 10x 100 ms apart) brought collision back (seen in
  game). The script now repeats the full rebuild 10 times, every 6 frames,
  after off - works in game when unticked in open air.
* Unticking while inside a wall still left no collision: the body had gone to
  sleep (flags 0x10042, active bit 8 clear - lobbybodyprobe.py). 0xc90eb0
  wakes a sleeping dynamic body first (`call 0xc754d0`); the script skipped it.
  Now the rebuilds wait until the body is awake (the player moves). Seen in
  game: collision comes back; a player unticked inside a wall stays trapped
  in it (not pushed out) - accepted by the user, ticking noclip again frees them.
* Lobby noclip is done (on, off, camera, F1 reload, off inside a wall).
* The table is symmetric (checked on all 64 x 64 bits of the live lobby
  table), so a stock row also gives that layer's bit in every other row.
* Camera: the lobby follow camera (CameraLocal@Lobby, vtable 0x1074d70) keeps
  out of walls with 0x3ba8e0 -> 0x30a4e0, a wrapper around hknpWorld castShape
  (0xc98880, found via its "TtWorldCastShape" timer string; castRay 0xc987c0,
  getClosestPoints 0xc98a20, queryAabb 0xc981a0). Its 5th arg is the query
  filterInfo, 0x16 = layer 22; the wrapper uses the world filter (+0x468).
  No lobby body is on layer 22 (stock row 0x3217). Noclip switches 22 off too:
  camera passes through walls, and is back to normal after off (seen in game).
* Dead end: flight controls by writing the position / velocity (E is emote,
  slow descent, walk animation in the air, lurch on takeoff) - removed; the
  game's own flight is used.

## Flight speed (General_QoL `lobby_flight_speed`, static analysis)
* PhysChar@Lobby step 0x4b3c60: rbx = PhysChar, [rbx+0x10] = character.
  Character +0x850 = this step's move delta (returned by 0x4879d0). Velocity
  = delta * (1/dt) * xmm10, at 0x4b3e84-0x4b3e91 (stored to [rbp-0x20]).
* xmm10 = 1.0, or 0.025 when |delta|^2 > 1.5 (0x4b3d51 / 0x4b3d5a) - a
  teleport guard, skipped while [character+0x39c] or 0x4888f0(). Every path
  meets at 0x4b3d63 (`48 8b 4b 10 bf 01 00 00 00`; rax free - 0x4b3d76 is
  `mov eax,[rbx+0xf0]`). Probably why Revamp's x1.4 lobby.bac boost stalls:
  faster BAC motion -> longer delta -> guard (not proven).
* Airborne: character +0x3b4 bits 0xc; 0x4868b0 sets bit 4 above 0.5 height
  (0x488650) and clears it at <= 0.21. So jumps/falls count too.
* General_QoL's cave at 0x4b3d63 multiplies xmm10 only when rbx ==
  [[exe+0x15712f8]+0x5b0] (local player) and airborne. Not seen in game
  (Havok may clamp, the animation speed stays the game's).
* Dead end: lobby.bac speeds (Revamp's approach) - the stall above, and plain
  flight has no BAC speed entry. MOVE_SPEED/BOOST_SPEED strings are battle
  buffs (0x7e50d0).
* Battle (found in Training with ClaudeTools\battlebodyprobe.py + ad-hoc scans):
  * Dead end: no live object has the PhysChar@Battle vtable (0xf652d0). The
    battle characters' class is **PhysVarChar@Battle, vtable 0xf65588**, found
    by scanning for pointers to the hknpWorld (world +0x468 = the one
    hknpCustomGroupCollisionFilter object, vtable 0xfbf3f8; refs to the filter
    -> world 0x...20b0; refs to the world -> owners by RTTI).
  * PhysVarChar: +8 hknpWorld, +0x10 Mob, **+0x78 body handle** (+0x10 =
    body id; the lobby has it at +0x80). Mob@Battle (vtable 0xf5e020) +0x4e0
    = its PhysVarChar.
  * Player: `[[[exe+0x156b408]+8]+0x90]` = the Mob the Battle::CameraManager's
    director follows (directors at +8/+0x18/+0x20 all held it). Every battle
    camera (CameraLocal/Default/Motion/Scouter, +0x3a0) targeted that Mob and
    none the training partner, so it is the local player. Other static chains
    found go through Ne* network event objects (e.g. [0x156bc00]+0x120) - not
    used. Unverified in cutscenes / ultimates / online.
  * Bodies: both characters dynamic, flags 0x4a, filterInfo 0 (as the lobby
    player); user data +0x90 points to a small block (shape, XG::HavokWorld)
    referenced only from the body. Layers in Training: statics 0/3/10/25/33/35,
    keyframed 18/25, dynamic 0/29. Rows 56-63 = 0x1, row 22 = 0x3217 - same
    as the lobby, so layer 63 and the camera layer work unchanged.
  * The debug menu now handles both modes (PlayerPhysics: lobby first, then
    the battle chain with Mob/PhysVarChar/filter vtable checks), keeps noclip
    ticked through loading screens and re-applies it per world. It stays on
    the patched body while that world lives (identified by world address +
    body array, so a reused address isn't taken for the old world). Not yet
    seen in game. Unknown: whether battle ground/stage limits are physics
    (noclip helps) or code; the battle camera's wall check layer.
  * Seen in game: noclip works in Training and quests, the player can leave
    the arena. In a quest, a little way past the map the player froze: body
    flags 0x41 (static), motion id 0, +0x48 = 0 (battlebodyprobe.py) - Havok's
    leavingBroadPhaseBehavior "freeze" (the string is in hknpWorldCinfo's
    reflection; enum names are stripped, so the offset wasn't found there).
  * The body integration (two copies: 0xd1a55f, 0xdad0de) tests the body's
    quantized AABB against the limits (0x7ffef07ffe7ffef0 / 0xfffffefffeffffff,
    mask 0x8000008000800000) and if outside posts event type 0xb =
    BodyExitedBroadPhaseEvent (header at stack: u16 0x10, 0x0400, type 0xb;
    +8 body id from body +0x70) through [world +0x3b0] vtable +0x18. The
    printer 0xce7070 is reached from the dispatcher 0xcbc9b0 (type = u16
    header +4). The code after the event is the same either way, so the
    freeze is done by the event's handler (not found; not needed).
  * Fix (debug_menu.lua, in memory only): each 56-byte event block starts
    with `cmp byte [rbx+0x6c], 63; je past` (rbx = body), room made by
    dropping the block's last constant reload into xmm6/xmm7 (callee-saved;
    the skip path joins without it). No branch lands in either block. No
    int3 runs >= 30 bytes exist in .text, and the .text tail after its
    virtual size is not zero, so there was no room for a code cave. Seen in game: the
    player flew far outside a quest map with no freeze or other problem. Dead end / user decision: not changing the world bounds
    or the world's leaving behaviour (affects every body).
  * Do NOT limit it to offline (user's decision): online quests can get
    locked when an enemy is stuck in a wall, and noclip is the way out.
* Probes: ClaudeTools\lobbybodyprobe.py (read-only: body, layer histogram, filter
  table), ClaudeTools\battlebodyprobe.py (read-only: PhysVarChar@Battle objects,
  bodies, layers, filter rows; "refs" lists owners), ClaudeTools\lobbycacherebuild.py (WRITES: repeated cache rebuild),
  ClaudeTools\noclipprobe.py, ClaudeTools\lobbyloop.py.

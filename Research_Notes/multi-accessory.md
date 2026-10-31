# Multi-accessory (phase 0 done in game; phase 1 mod `MultiAccessory\` built, "equipped wins" conflict rule, awaiting in-game test)

Covers: wearing up to 3 accessories at once on a CaC - what the game does
today, the phased plan (rendering, save, equip screen, online), and the
limitations/blockers. "Phase 0 findings" below is static disassembly of
1.26.01 (2026-10-25); the rest is still reasoned from other notes.

## Phase 0 findings (static, 1.26.01)
Model builder, CharaResourceBase paths, physics cap, records: see
`multi-accessory-phase0.md`.

## Phase 1 mod (`MultiAccessory\`, 2026-10-25)
* The probe's three hooks (builder call 0x65d400, resource 0x299c46,
  part replace 0x299b1d / 0x297c3b), config `multi_accessory.json`
  (`all`, `slot_1..slot_8`, max 2 extras).
* Conflict rule (user: both should render when possible, else **equipped
  wins**): an extra whose flags-4 parts replace a part slot already
  replaced by the equipped accessory or an earlier extra is skipped as a
  whole, on both paths. Two hair replacers can never both render (one
  part per slot); attached objects always stack (cap 4 physics objects).
* **Physics-only hats (in game 2026-10-25):** part sets differ per race.
  FRI.bcs (and the loose HUM.bcs) 835 = slot 5 part with model -1, flags
  0 and one flags-4 physics object (part +0x4a count, +0x4c offset; the
  physics entry's flags at +0x18) - the hat is that object, hung on Hair.
  With the FRI helmet 2035 (slot 5, flags 4) equipped, adding 835's physics
  made **head and helmet vanish** (same as the probe's "head missing").
  So the conflict rule also counted slots a part set hangs physics on.
  **Real cause (static, 2026-10-26): hide masks.** Part +0x1c (HideFlags,
  part-slot bits FaceBase=1 .. Hair=0x20; XV2-Tools `BCS_File.cs`) and
  the same field on a physics entry: FRI 835's object hides 0x20 (Hair =
  the helmet), FRI 2035 hides 0x1f. Hiding a part never hides objects hung
  on it (a vanilla hat hides its own Hair and its object still hangs
  there). The halo 831 (slot 5: model -1, flags 0, hide 0, one physics
  object, hide 0) was skipped over wig 1410 by the old rule (retest 16).
  Rule now (`CanShow`): clash when one hides a slot the other replaces, or
  both replace a slot with different parts (then stacked on the resource
  path); a stacked part's own hide mask is not applied (not the slot's
  shown part: wig 612 hides 0x30 yet stacks on 1410). Offline check:
  `ClaudeTools\multiacc_slotuse.py <bcs> --equipped N <ids>`.
  **Replaced again (2026-10-26, retest 18, untested): no rules.** User
  wants extras to never hide/disable anything. Runtime copies (static):
  physics object init 0x29aef0(obj, bcsPhys, ...) copies BCS +0x1c to obj
  +0x108 (0x29b3f7); part init 0x29c070 does the same for parts
  (0x29c763); 0x29e000 just returns +0x108, read by the Chara register
  pass 0x288930 (+ 0x2402d8, 0x3e4030, 0x451b10, unknown). 0x29aef0 is
  called only from 0x29d507 (in 0x29d470, itself only from 0x299c8c /
  0x299cdd in holder build 0x299ac0, right after 0x29a4d0 at 0x299c46) and
  the builder loader (0x65c9af, 0x65cb23). So the mod zeroes obj +0x108 for
  the extras' entries after init, and stacks every slot an extra replaces.
  **Retest 18 (2026-10-27):** multiple hats + FRI helmet/hat show, but
  stacking drew **placeholder replacers**: MAM.bcs 835 = slot 5 model 0
  (files `MAM_000_Face_base`, the bare head + ears) + a hat object hiding
  0x20 - in vanilla the object hides that replacer, so it's never seen;
  stacked, it covered every wig (the config's `all: [835]`). Same shape:
  HUM 918 (slot 5 model -1, object hides 0x20). Rule now: a replacer on a
  slot its own part set hides (objects' masks, or its parts' masks on
  *other* slots - a part never hides its own slot, wig 612 = 0x30) is not
  stacked. Equip screen: an extra's holder won the slot over the equipped
  (apply 0x65e290 walks `vector<holder*>` builder +0x80+0x18b in order,
  later wins - 0x65a730..0x65a77e); the mod rotates the equipped's holder
  to the end after loading the extras.
  **Replaced (2026-10-25, untested):** user wants every extra shown, wigs
  stacking. A clashing extra is now "loose": its physics objects get part
  index -1 (resource path: 0x29d470(holder, phys, -1, ...) as the part
  set's own list; builder: the loader's new entries in the per-buffer int
  vector builder + 0x50 + 0x18*b set to -1). Guess for the vanish: the
  object's part index hides that part slot (hats hide hair). Replacer
  parts become a made-up BCSPhysics: +0/+2 -1, flags 4, name, strings
  [0] = `<name>_<model %03d>_<slot>` (or the part's own file), [4] = bone
  (HUM.bcs physics: [0] model base, [4] bone like b_C_Head / b_C_Chest /
  b_C_Pelvis, [5] SCD/esk). Unknown: whether a skinned wig emd sits right
  as an object.
  **Dead end (retest 6, 2026-10-26):** the made-up object for NMC 612's
  wig (`NMC_612_Hair` on b_C_Head) hung the character's load (avatar gone,
  equip screen blank after). Physics objects ship their own small .esk
  (`hum_980_hair.esk`, `nmc_831.esk`: `EXP_<name>`, b_C_Base,
  x_x_SkinRoot, one bone the emd is skinned to); part models
  (`nmc_612_hair.emd/emb/emm/dyt`) have no .esk and are skinned to body
  bones. Removed: a clashing extra with a replacer part is skipped. Wig
  stacking would need a second part per slot in the resource (RE).
  **Unproven (retest 7):** that build was the lobby avatar's forced
  accessory reload, which on its own hung save select in retest 7 (no
  made-up object involved) - so the made-up wig object may not be what
  hung. Still open: whether a body-skinned wig emd draws right as an
  object (no own .esk). Cheap retry: LooseReplacer back, tried on the
  equip screen only.
* Lobby refresh (static): EditChara exit 0x4f8690 copies the loadout to
  record 0, then (lobby up, [0x158b378] != 0) 0x4f8bc1 calls
  0x4657d0(lobby, record 0, 1) -> 0x465500(.., -1, 1, 3, 2): Monitor lock,
  0x654580(record, packet) builds the appearance message, queued at
  lobby+0x48. EditMyset slot 15 (0x506680) does the same at 0x5067e3.
  **Online lobby only**: 0x4f8b80-0x4f8ba5 skip to 0x4f8122 unless
  [0x1585208] != 0, 0x46ee0([0x1554498]) == 0x14 and [0x158b378] != 0.
  **Offline (static, 2026-10-26):** the lobby's own avatar (ILobbyChara)
  idles in state 0x490670, which calls 0x489120(chara) (call 0x4906b3):
  it compares the record at [chara+0x510] with the copy cached at
  chara+0x530 (record +0x04, 10 words at +0x0c, the accessory +0x20, 0x20
  words at +0x32; mode [0x487660()+4] != 4 always says changed). On a
  change: vt+0x198, 0x416980, drop the +0x728 objects, copy record
  +0..+0x74 into the cache, state 0x4901c0 (new LobbyCharaResourceBase via
  0x3b48e0 -> 0x4140b0 -> 0x4168e0). An extras-only change leaves the
  record identical, so the mod sets the cached accessory to another value
  once (`LobbyCharaChangedHook`; untested). 0x48fca0 (from 0x4881b9 /
  0x48fc0b) is the other caller: reloads without the cache copy.
  **Online receiver (test 7, 1.27):** a received extras-only change lands
  in the copied block but the drawn look is unchanged, so 0x489120 never
  reloads the room avatar (loading screen + battle do show it). Test 8: a
  look whose tagged extras change after it was first seen is marked stale
  and `LobbyCharaChangedHook` flips that avatar's cached accessory once
  (untested).
  Dead end (retests 6 + 7): forcing an accessory
  reload (cached loadout +0x28 scrambled) on a non-equip-screen 0x65cf60
  caller - lobby avatar vanished / save-select previews stopped / save
  load hung.
  Extra-only changes never redrew there (guess: receiver diffs the
  appearance); mod sends a 0xe0-byte record copy with another accessory
  first. Dead ends: 0x3f3bb0 (lobby init), 0x420b10 (lobby script cmd),
  0x49f750 / 0x49a230 (timers / visibility).
* Builder +0x18 is a resource too: 0x65c6c0 reads the BCS from
  `[builder+0x18]+8`, same as CharaResourceBase +0x08.
* Record -> slot: local id + name + record +0x04/+0x08 equal to CaC
  +0x50/+0x4c (0x653830 writes rec+0 = race via 0x43140(CaC) = CaC +8 ->
  0x81b1c0 -> 0x28ff40, rec+4 = 0xb9190(CaC) = CaC +0x50, rec+8 =
  0x92950(CaC) = CaC +0x4c; static). Untested in game.
* A decrypted 1.26.01 image for `ClaudeTools\exe\` is at
  `ClaudeTools\exe\img126.bin`.

## What the game does today (from other notes)
* Two kinds of accessory (costumes-and-models.md):
  * **Hair replacers** (hats, wigs, turbans): the IDB MODEL part set puts
    the headwear in the **Hair** part (flags 0x4), which is why it replaces
    the hair.
  * **Attached objects** (glasses 846, scouters, Clown Nose): Face_base part
    with no model + a flags-0x4 physics object on a bone.
* Accessory loader **0x65c6c0** loads only flags & 4 physics objects, using
  the face's dyt as the external one.
* Save: one accessory id per loadout preset (preset block 8 x 96 bytes,
  every byte used - battle-styles-loading-and-online.md "Saves"); in memory
  8 records of 0x68 at character + 0xf4, record 0 = current. Extra slots
  need a sidecar save, like Battle Styles' equipped_styles.sav.
* Inventory list type 4 = accessories (save-inventory-and-skills.md).
* Equip screen = AVATAR.iggy / EditChara; cloth list [Top, Bottom, Gloves,
  Shoes, Accessory, Material, Talisman]; btn_8 is Battle Styles' slot,
  btn_9 probably free (EditMenuItemNum 9) - battle-styles-ui.md.
* Online battle: each pick is a 0x100-byte setup record; name field
  +0xc0..+0xff zero padded, read with strlen; Battle Styles uses +0xfc..+0xff
  for its style code (battle-styles-loading-and-online.md).
* Accessories carry no stats in XV2 (confirmed by the user; stats come
  from QQ Bangs), so extras are cosmetic and can't desync a battle.

## Plan
0. **Feasibility (go / no-go).** Trace the CaC model builder: accessory id
   -> IDB entry -> BCS part set -> 0x65c6c0. Find where loaded accessory
   objects are stored. Single pointer -> keep our own list of extra objects
   and hook their update / draw / free (+ LOD); a list -> maybe just call the
   loader again. Static first (`ClaudeTools\exe` da.py / xref.py), then a
   live probe that adds a hard-coded second accessory.
1. **Local rendering** from a config list of extra ids. Check: equip screen,
   lobby, character select preview, battle, cutscenes, transformations
   (awoken skills can remove hair accessories). Hook in the shared builder
   (0x65be10 has 11 callers) to cover most screens at once.
2. **Save**: sidecar keyed account (SteamID64 from the save) -> save slot ->
   preset -> [acc2, acc3], same design as equipped_styles.sav. Drop an id
   that isn't in the inventory any more (the game's item-in-use checks
   0x84bbd0 / 0x7444d0 don't know the extra slots, so it can be sold).
   **Built 2026-10-25** (`multi_accessory.sav`, config = fallback per slot
   when a preset has no entry). Which preset is shown: every loadout is
   built by 0x653830(character, out, preset); Battle Styles hooks its entry,
   so all 38 e8 call sites (list in dllmain.cpp) go through one stub that
   records `slot -> preset` (negative = 0). Variable-preset callers: battle
   chara select 0x38bdde/0x38db8e/0x38dc43/0x38dccf/0x38ddc8, its preview
   0x5e66ec/0x5e7294, EditMyset 0x5074a0/0x5075f5; the rest pass -1 (often
   `lea r8d,[reg-1]`). EditMyset slot 17 chained: sendType 2/3 copy the
   entry (or its absence) n->0 / 0->n. Inventory drop not done yet.
3. **Equip screen** (user, 2026-10-25): the Accessory row of the cloth tab
   (mode 3) becomes **one row, three columns**: Accessory | extra 1 |
   extra 2. Battle Styles' row moved to the Skills page (mode 4, btn_9), so
   btn_8 / btn_9 are free on the cloth tab. Design:
   * No new sprites (that would mean editing the iggy display list): AS
     moves btn_8 / btn_9 next to btn_5 and narrows all three (header +
     name text fields + background) while the cloth tab shows; anything
     else restores them. Rows 6-7 (Material, Super Soul) don't move.
   * Cursor: left/right move between the columns (vanilla L/R =
     changeEditMenuItemPrev/Next, a no-op on Part items - verify); up from
     Material lands on the row, down from it on Material. ButtonList is a
     plain up/down list, so the columns need own handling.
   * Shared AVATAR.iggy: Battle Styles serves the whole script today
     (MenuIggy). Move it to UILib `XV2UI_EditScript` (like CHARASELE) and
     have Multi Accessory register its own edit; keep the edits apart -
     own item types / user-data slots after Battle Styles' 96/97 (e.g.
     98..101), new code in own methods, the slot-count byte 0x4f72b7 only
     ever raised (each mod writes max(current, needed)).
   * Picking: reuse the accessory ItemList (+0x6d0, sendType 210 ->
     0x4fc7b0) with the result redirected; hover preview needs its own hook.
   * Needs phase 2 (somewhere to keep the picks) first.
   * **Built 2026-10-25, untested in game.** AS: `MultiAccessory\as3\Avatar.as`
     (`ma*` methods; menu_abc.py `ma_avatar`), merges with Battle Styles'
     edit with 0 conflicts either order (`ClaudeTools\multiacc_ui\merge_check.py`).
     User data 98/99 = extra 1 "icon^name" / count, 100/101 = extra 2; the
     list request 210 sends the column (1, 2; vanilla -1). DLL, all static
     RE: EditChara's cloth lists are indexed by request type - 206 (0x4f98d0:
     index at +0x2bc + 4k, IDB item ids (-1 = none; NOT part sets - 0x61af70(4, id) gives the MODEL part set, found after picks saved item ids) vector<int> at +0x548 + 24k, "icon^name"
     wstring at +0x328 + 0x20k; accessory k = 4), open list type at +0x2f8.
     List callback 0x4fb000(close, ?, value, menu) -> apply 0x4f98d0(menu,
     value, decided) (hover: 0x5b1bc0 preview rebuild; decided: 0x5b2000 +
     0x5b1d20) at e8 0x4fb018, close 0x4f9d90 at e8 0x4fb024. Item push
     0x4fc0d0(menu, type) sends a row (9 e8 sites, list in dllmain.cpp); the
     DLL repoints the index at each extra for one push to get its text, then
     pushes again as it was. Slot 17 of 0x10ae228 chained (CAS, as Battle
     Styles now does too). Slot-count byte raised by CAS to max(current,
     0x64) - Battle Styles now only checks the 8 bytes before it and raises.
     **Retest 2 findings (static):** 0x5b1bc0 / 0x5b2000 are stat-bar paths
     (via 0x5b2700), not the model; 0x4fc5f0 is camera. The model comes from
     request 0x65cf60(builder, loadout, ...): unless busy (+0x108 > 1) it
     compares the 0xb0-byte loadout with its cached copy (+0x244 if +0x240
     != 0, else +0x168) and returns 5 = no rebuild when equal, so a change to
     only an extra never redraws. Callers (e8): 0x4f97be, 0x4fec55,
     0x4fefe1, 0x505cb7, 0x507807, 0x5ec70d, 0x65ac47, 0x65d914; 0x4fec55 is
     in 0x4fdf40 <- 0x4fe570 (EditCharacter event, from 0x398090, likely per
     frame). Fix: hook all 8, flip the cached copy's first dword once after
     an extra changes (untested). Original unknowns: whether 0x5b1bc0 skips a rebuild when the accessory
     id is unchanged (hover preview of an extra), whether timeline
     animations undo the narrowed buttons' scaleX, what a cancel sends
     (assumed: the opening value = the equipped id = "no change").
     Layout findings (retest 3 log): the cloth Accessory row is btn_5 at
     x 32, y 228, scale 1, box 608 x 65; when setupEditMenuItem runs the
     buttons are mid-appear (alpha 0) and on a tab switch can have empty
     bounds (width 0), so the box is measured once and kept. Whether the
     edit_menu timeline also drives x/scale is what the `later:` log tells.
     **Retest 4 (static RE):** 0x65cf60 reloads the accessory (0x65c6c0 via
     0x65d400) only when loadout +0x28 (accessory part set) differs from
     the cached copy's +0x28 (cmp 0x6595c8; busy path: +0x244, i.e.
     +0x26c); body parts reload against the loaded ids at +0x2f4. So the
     retest-2 dword-0 flip did nothing. The equip screen requests every
     frame from 0x4f90b0 (e8 0x4f97be), loadout at rbp-0x40, accessory =
     0x61af70(4, ids[+0x5a8][index +0x2cc]). Apply 0x4f98d0 never requests
     the model (hover: camera 0x4fc5f0, 0x4f9410 + 0x5b1bc0 info; decide:
     0x4f4ca0, 0x5b2000, 0x5b1d20). List open 0x4fc7b0 reads the index at
     +0x2bc (0x4fd2f9) for the check mark. The edit_menu timeline slides
     the rows in after setupEditMenuItem (y 228 read mid-slide, alpha 0),
     so AS drives the moved buttons' y + alpha for 30 frames.
4. **Online battles**: pack two u16 ids at name field +0xf8..+0xfb (before
   Battle Styles' +0xfc) only when the name ends before +0xf8; zeros = none.
   Unmodded players ignore the bytes (strlen). Put the tail layout in the
   loader SDK so mods can't collide.
5. **Online lobby**: Conton City avatars are out of scope (user); the
   room lobby before a quest is the target.
   Started 2026-10-30 (static, img127_clean): 0x465500 builds a big message
   on the stack (ctor 0x71d9a0 at rbp+0xb0, rbp+0x880 frame) and calls
   0x654580(record, msg). 0x654580: 0x654d70(record, msg+0xe0) copies the
   appearance (msg+0xf8 race code, +0xfa/+0xfc = record +4/+8, colour
   words via tables 0x10de38c/0x10de3a4/0x10de3c4), skill words to
   msg+0x13c (table 0x10de430), record +0x74..+0x8e to msg+0x14e..+0x15e,
   then 0x727ff0(msg, name) = **16-byte name + NUL at msg+0x10** (movups
   of 16 bytes, so the tail after the string's NUL is whatever the
   std::string buffer held). 0x465500 later re-copies the name (strncpy
   0x11 at 0x4656df) and may replace it with 0x14c0df0 (0x8347e0 check).
   The name tail can't carry anything: every name copy is strncpy(0x11),
   which zero-pads. **Room lobby (pre-quest room)**: NeRoomEvent
   0x46a690 builds the same profile (ctor 0x71d9a0 at 0x46a745, packer
   0x654580 at 0x46a788) and queues a 0x168-byte item (0x46a270 /
   0x469f90); copies: 0x7212c0 (profile +0..+0xdd), 0x7207a0 (the
   appearance block), 0x721c80 (+0x14e). Receive: Room@Event (vtable
   0xfdad28) job 0x5dad70 loops members calling **0x654830(member,
   record)** at 0x5dafe3 (record from table [0x15681b0] via 0x3f1670) and
   0x5db05b (stack record, ctor 0x2040f0); 0x5d95d0 does the same at
   0x5da805 / 0x5da8e7. 0x654830 = 0x655080(member+0xc8, record) +
   0x655970(member+0x124, record): the member struct holds the block at
   +0xc8 (profile: +0xe0). Wire format not found statically (packets are
   raw buffers, CPacketRoomEvent vtable 0x109abd8).
   **Channel (built 2026-10-30, untested):** appearance block +0x50 (u32)
   and +0x54 (u16) = profile +0x130..+0x135: memset 0 by the ctor, copied
   by 0x7207a0, never written by 0x654d70 nor read by 0x655080 (+0x56 is
   padding, not copied). `ProfilePackHook` (all 11 callers of 0x654580)
   writes magic 0x4d41 | id1 << 16 and id2 (0xffff = none) for our CaCs;
   `MemberUnpackHook` (all 13 callers of 0x654830) stores name + record
   +4/+8 -> ids; `ResourceExtras` then draws them on any resource whose
   record matches (ids 16..0xfff included once something was received).
   First PC<->Deck room test: nothing shown. Sends logged from 0x5b2b79,
   0x46e1b2 and many from 0x46e885 (that function copies the block into
   a 0x720-byte per-player struct at +0x158 via 0x7207a0; CPacketRoomEvent
   slot 1 only checks header bytes of a raw buffer). No unpack logged, but
   untagged unpacks weren't logged then; a diagnostic build logs them.
   **Test 2 (2026-10-30, extras loaded before the room on both):** both
   sides sent their ids, the Deck logged no unpack at all, the PC's caps
   were used up by Conton NPCs (caller 0x3e7a56). Cause: 0x654830 has
   **13** callers - `da.py` cut caller lists at 12 (fixed: it prints all
   with the count). The 13th, 0x856bc3 in 0x856b40(kind, ...) (kind 2/3:
   member from 0x410be0; kind 1: save CaC via 0x654af0 + 0x655970), is
   the one the room receive (0x5db120, 0x5db88a) and battle set-up
   (0x8a01c3, 0x8a03c0) use; now hooked. Other unpackers checked:
   0x654910(profile, record) (1 caller 0x5bbfff, local record 0x1000),
   0x655080 also called from 0x46f8d0 ("Hologram") and 0x3e6bd0.
   **Correction (test 3):** 0x856b40 kind 2/3 takes its member from
   0x410be0 = the preset avatar list object [0x1587d68] (4 vectors of
   0x158-byte members, loaded from lobby_preset_avatar_list.pal by
   0x410d40); 0x3e6bd0 and 0x3e7a56 (Conton NPCs, 0x410b20 by id) use
   the same list. None of them is an online player. The room's records
   come from 0x3f1670 (a 9-entry table, records freed via 0x27a070) in
   0x5dad70 / 0x5d95d0 / 0x3aee10 only. Test 3: no unpack logged at all
   on either machine (sends fine). The profile travels through the agent
   layer (CommandParameter_Profile 0x72b210, NRTCommand_WaitOtherProfile
   0x729bd0, LoginCommand_PlayerData 0x6f8270, all copying the block via
   0x7207a0, which keeps +0x50/+0x54).
   **Test 4 (all 26 0x7207a0 callers logged):** the server keeps the
   bytes. The receiver copies the other player's tagged block at 0x46a4eb,
   0x46d461 and 0x46dbb7 (0x46d461: room member entries of 0x720 bytes,
   profile at +8, rebuilt on the stack and handed to 0x47e170 -> 0x47f060
   with manager [0x1587d70]); a second later the player is drawn as record
   id 2 - no 0x654830 caller involved. Block -> record conversion itself is
   0x655080(block, record) (callers 0x654847, 0x654927 = profile unpack
   0x654910 from 0x5bbfff, 0x46f8d0 = CLobbyNetworkEventListener slot 11
   0x46f7d0 "Hologram", 0x3e6bd0 NPCs); it writes record +0x00..+0x64
   only (+0x04/+0x08 = block +0x1a/+0x1c, +0x20 = accessory via 0x61af70
   lookup, +0x22 = block +0x16, +0x24 = block +0x58, +0x32..+0x64 = words
   from block +0x1e). Mod (test 5): convert each tagged block into a
   scratch record with 0x655080 and key the extras by record
   +0x00..+0x20, independent of which path builds the drawn record.
   **Test 5: works in the room lobby both ways.** A change made inside
   the room doesn't reach the other machine (nor its versus battle, which
   drew the old extras). Sender side of a change: room update 0x46e790
   (pack 0x46e885, copy 0x46e90f) -> 0x480320 -> agent [0x158d110]
   0x733670(agent, block): compares with agent +0x6568 (0x722050: block
   +0..+0x1e, +0x1e..+0x50, +0x50, +0x54, +0x58 - so the tag counts as a
   change), copies, then 0x702980 -> 0x6e62a0 queues an upload command.
   0x7336f0 (callers 0x4e0308, 0x4edf6b) is the same with 0x702ac0. The
   receiver logged no tagged block copy afterwards. Test 6 hooks
   0x655080's player call sites (0x46f8d0, 0x654847, 0x654927) as well.
6. **Other online modes** (PQ, raid, expert missions): check which setup
   path they use (story 0x3a8ac4 doesn't go through the record).

## Limitations / blockers
* The Chara holds at most 4 accessory physics objects in total (phase 0
  findings); extras over that are silently not drawn.
* Two hair replacers can't stack (one Hair part). Likely rule: slot 1 any
  accessory, slots 2-3 attached objects only (or anything not fighting over
  a part); filter the list from the BCS part set.
* Extra slots likely limited to flags-0x4 objects, coloured from the face's
  ramps.
* Receiver must have the accessory (x2m / DLC data): skip ids missing from
  their IDB/BCS, never crash.
* Unmodded players see slot 1 only; replays probably drop the extras.
* Long (multi-byte) names leave no room in the name tail: don't send then.
* First test pair: Clown Nose (attached object) + a vanilla hat (hair
  replacer).
* Home: a new repo mod (e.g. `Sloppy_XV2ModRepo\MultiAccessory\`, name
  not decided).

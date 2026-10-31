// ---------------------------------------------------------------------------
//  MultiAccessory.dll - loaded by XV2 Mod Loader from
//  <game>\XV2ModLoader\mods\MultiAccessory\
//
//  Lets a CaC wear up to two extra accessories on top of the equipped one.
//  The extras are kept per loadout preset, like the equipped accessory, in
//  multi_accessory.sav; multi_accessory.json gives defaults per save slot for
//  presets without saved picks. Online, each player's extras ride along in
//  unused bytes of the avatar profile (ProfilePackHook / MemberUnpackHook),
//  so other players running the mod draw them too. Game 1.27.
//
//  The game draws a character's accessory on two paths, and both keep the
//  loaded objects in lists, so the extras are loaded into the same lists
//  right after the game's own accessory (Research_Notes\multi-accessory.md):
//    * the model builder (equip screen preview): the accessory loader
//      0x65c6c0 is called again per extra into the same buffer;
//    * CharaResourceBase (save select, character select, Conton City,
//      battle, cutscenes): part replacement 0x29a700 and the accessory
//      physics 0x29a4d0 / 0x29d470 are repeated per extra.
//  Every character goes through the second path (NPCs, online players), so
//  a load gets extras only when its record is one of our save slots' CaCs:
//  local record id, same name and same appearance values.
//
//  Extras follow no replacement rules: a "hair replacer" (hat, wig, helmet)
//  swaps a body part when equipped, but as an extra it never swaps or hides
//  anything. On the CharaResourceBase path its replacer parts are drawn on
//  top (part set preloaded into the resource, parts registered in the
//  skeleton at key 10+) and its objects' hide masks are cleared. The equip
//  screen builder has no stacking, so there an extra's wig/helmet still
//  swaps the slot in the preview.
//  Attached objects (glasses, scouters, noses) stack with each
//  other, but the character holds at most 4 of their objects in total; the
//  game drops the rest.
//
//  Presets: the game keeps 8 loadout records per CaC (0 = the one in use,
//  1..7 the saved presets) and builds every shown loadout with 0x653830; its
//  callers are hooked to know which preset each save slot was last built
//  from (battle character select and its preview, the Presets panel pick
//  one). Saving or loading a preset in the Presets panel copies the extras
//  along with the record.
//
//  Equip screen: the cloth tab's Accessory row gets two more columns, one
//  per extra (as3\Avatar.as, handed to the loader's UI library); they open
//  the game's own accessory list, whose pick goes to the column.
//
//  Reads multi_accessory.json and multi_accessory.sav next to the DLL;
//  writes multi_accessory.sav, and MultiAccessory.log only when "logging" is
//  true. No network calls.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <intrin.h>
#include "avatar_abc.h"
#include "default_config.h"
#include "xv2_ui.h"
#include <atomic>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace {

// --- Game addresses (static disassembly of 1.27) ------------------------

// Model builder: the only call to the accessory loader 0x65c6c0(builder,
// partSet), inside set-appearance 0x65cf60.
constexpr uintptr_t kBuilderLoaderCallRva = 0x65d400;
constexpr uintptr_t kBuilderLoaderRva = 0x65c6c0;
constexpr size_t kBuilderResource = 0x18;  // the race's chara resource; its BCS at +0x08, as in CharaResourceBase
constexpr size_t kBuilderBuffer = 0xb0;    // int 0/1; the loader flips it, then fills that buffer
// Per buffer (+0x18 * buffer): physics objects' part indices, int vector
// {first, last, end} (0x65c6c0 pushes at 0x65c9e9: builder + (3b + 10) * 8).
constexpr size_t kBuilderPartIndices = 0x50, kBuilderBufferStride = 0x18;
// Per buffer: part holders (hair replacers, one per loader call that finds a
// flags-4 part), vector<holder *> at +0x80 (multi-accessory-phase0.md)
constexpr size_t kBuilderHolders = 0x80;
constexpr size_t kBuilderLoadState = 0x108; // int; the loader runs only at 0, sets 2 when done
// Apply 0x65e290(builder), called by the builder update 0x65d7b0 at load
// state 2 (0x65d9b4) and 4 (0x65d844): returns 1 at once while builder
// +0x324 is 0, 0 while the ready check 0x65bc90 fails (retried next frame);
// otherwise frees the other buffer's part holders and attaches the current
// buffer's holders' parts slot by slot (Chara vt+0x368 = 0x27eaa0: unregister
// key `slot`, activate the part, register pass 0x288930 for keys 0..9 only).
constexpr uintptr_t kBuilderApplyRva = 0x65e290;
constexpr uintptr_t kBuilderApplyCallRvas[] = {0x65d844, 0x65d9b4};
constexpr uintptr_t kBuilderReadyRva = 0x65bc90;
constexpr size_t kBuilderApplyEnabled = 0x324;
constexpr size_t kBuilderChara = 0xb8;
// Builder teardown 0x65b880(builder) (from the dtor 0x65b120 and the rebuild
// 0x65bb50): deletes the Chara, then the resource, then the holders.
constexpr uintptr_t kBuilderTeardownRva = 0x65b880;
constexpr uintptr_t kBuilderTeardownCallRvas[] = {0x65b137, 0x65bb6f};
constexpr size_t kCharaSkeleton = 0x38; // xgcore Skeleton the attach registers on (0x27ead4, 0x2889f0)

// CharaResourceBase holder build 0x299ac0. Accessory physics: 0x29a4d0(res,
// std::vector<0x28 entries>*, holder, d8, dc) appends the flags-4 physics of
// the record accessory's parts (one call, at 0x299c46; entry +0 = the BCS
// physics entry); right after, still in 0x299ac0, 0x299c8c adds one object
// per entry and the part set's own physics list follows (loop at 0x299cfb,
// one 0x29d470(holder, physData, -1, d8, 0, 0, dc) per entry). 0x29d470 is
// called from nowhere else, so every object it adds on a thread comes after
// that thread's latest 0x299c46.
constexpr uintptr_t kResPhysicsCallRva = 0x299c46;
constexpr uintptr_t kResPartPhysicsRva = 0x29a4d0;
constexpr uintptr_t kAddPhysicsObjectRva = 0x29d470;
constexpr size_t kPhysicsEntrySize = 0x28;
// Physics object init 0x29aef0(obj, BCS physics, r8, r9, 3 stack args)
// copies the entry's hide mask (+0x1c) to obj +0x108 (at 0x29b3f7); the
// Chara register pass 0x288930 reads it (0x29e000) to unregister the part
// slots an object hides (hats hide hair). Called from 0x29d507 (in 0x29d470)
// and the builder loader 0x65c6c0 (0x65c9af, 0x65cb23), all synchronous.
constexpr uintptr_t kPhysicsInitRva = 0x29aef0;
constexpr uintptr_t kPhysicsInitCallRvas[] = {0x29d507, 0x65c9af, 0x65cb23};
constexpr size_t kObjectHideMask = 0x108;
constexpr size_t kResBcs = 0x08;      // resource: the race's BCS file
constexpr size_t kResRecordId = 0xc8; // resource: id of its character record

// Stacking a second hair replacer (Research_Notes\multi-accessory-wigs.md).
// Resource +0x30 = unordered_map<int part set, holder *> (end node at +0x38,
// node +0x18 = holder); find = 0x148e10(&map, &outNode, &key). Resource
// +0xe0 = vector<int> {first, last, end} of part sets 0x299ac0 also loads
// into that map, after the physics call (preload loop at 0x299dd7); its
// push_back grows through 0x4bce0(&vector, where, &id) (0x2985f5). Holders in
// the map are loaded, ready-checked, material-mapped and unloaded with the
// main one (0x297ac0 0x297ea3 / 0x297f1e, Chara vt47 0x282760).
constexpr size_t kResPartSetMap = 0x30, kMapEndNode = 0x38, kNodeHolder = 0x18;
constexpr size_t kResPreloadIds = 0xe0;
constexpr uintptr_t kMapFindRva = 0x148e10, kVectorInsertIntRva = 0x4bce0;
constexpr uintptr_t kHolderPartRva = 0x29d2a0; // (holder, part slot) -> part, null past slot 9
constexpr uintptr_t kPartModelRva = 0x29aee0;  // (part) -> its xgcore Model for the current skeleton
// Skeleton set-up 0x2999b0(res), called by the state machine 0x297ac0 once
// the holders are ready: creates the skeleton at res +0x80 + index * 8
// (index = res +0x118; < 0: res +0x78) and registers the main holder's 10
// parts at keys 0..9 (0x29cfc0 at 0x299a4e). The unload pass frees every
// map holder's models, then the skeleton (~Skeleton leaves models
// registered with flag 0 alone), so models of a map holder registered at
// other keys go the same way as the game's own.
constexpr uintptr_t kResSkeletonRva = 0x2999b0;
constexpr uintptr_t kResSkeletonCallRvas[] = {0x297e91, 0x297f01};
// Resource: part set of its main holder (the one 0x2999b0 registers at keys
// 0..9; the equipped accessory's parts are merged into it, not a holder of their own).
constexpr size_t kResMainPartSet = 0xd4;
constexpr size_t kResSkeletonIndex = 0x118, kResSkeletons = 0x80, kResSkeletonNoIndex = 0x78;
// IAT entry of xgcore Skeleton::RegisterModel(Model *, uint key, int flag):
// refuses a model already registered, replaces the one at `key`.
constexpr uintptr_t kRegisterModelImportRva = 0xf5c3a0;
// IAT entry of xgcore Skeleton::UnregisterModel(uint key) (the attach calls
// it for every slot, registered or not).
constexpr uintptr_t kUnregisterModelImportRva = 0xf5c398;
// Game loops walk models by key, 0 .. GetModelNum() - 1 (shadow 0x33f050,
// clone 0x321ae0): stacked parts go right after the 10 part slots.
constexpr unsigned kFirstStackedKey = 10;
// ModelPartResource vt+0x18 (0x29bd80) = activate: prepares the part's
// DynToon ramp unit from its .dyt.emb (part +0xd0, PrepareToonRenderingUnit)
// and activates its attached objects. The game activates only the active part
// set's parts (0x29d020 from Chara 0x27e7e6); a material whose unit isn't
// prepared skips binding its ramp (0x29dc10) and draws with the last bound one
// (retest 10: stacked wigs took the equipped wig's colours). The part's dtor
// (0x29e420) detaches the unit, so no matching deactivate (vt+0x20) is needed.
constexpr size_t kPartActivateSlot = 0x18;
// Retest 13: at skeleton set-up the equipped wig isn't activated yet (unit
// +0xe0 null); the game activates it later in Chara SetPartSet 0x27e7a0 ->
// 0x297910(res, part set, chara +0x370) -> 0x29d020, once the holders are
// ready. Activating ours that early left the ramp half-built (green skin
// parts, black at angles) or failed (+0xe0 null after a battle), so stacked
// parts are activated right after the game's pass instead.
constexpr uintptr_t kActivatePartSetRva = 0x297910, kActivatePartSetCallRva = 0x27e7e6;
constexpr size_t kPartToonUnit = 0xe0; // set once activate prepared the ramp unit
// Retest 14: stacked wigs' SKIN_ materials stayed green (the .dyt
// placeholder). The CaC colours reach the ramp units only through Chara
// vt[57] 0x27d890(chara, material name part, float4 *colour, int), called per
// BCS PartColor by Chara vt[130-132] (0x2847a0 / 0x284960 / 0x284d40): it
// writes the colour into each matching material's block of the units of the
// 10 parts at chara +0x370, then the 30 physics objects at +0x3c0 (0x28c5b0).
// Our parts aren't in that array, so the hook runs the pass again with only
// them there. Chara +0x30 = its resource (SetPartSet 0x27e7c9).
// .?AVChara@Common@Game@XG@@ (.rdata); slot 57 is its only pointer to 0x27d890.
constexpr uintptr_t kCharaVtableRva = 0xf9bb00;
constexpr size_t kCharaColourSlot = 57;
constexpr size_t kCharaResource = 0x30, kCharaParts = 0x370, kCharaPartCount = 10, kCharaPhysicsCount = 30;
// Retest 15: stacked wigs still black (menus) / white (battle) at some
// angles. Each part's materials call the functors in its draw-callback list
// (MaterialCallbackCaller at part +0x138: vector of {functor*, int owned}
// at +0x140..+0x148, filled by load 0x29e630): [0] DynToon texture (own
// ramp unit), [1] DynToonLightApplying (light source +8, light direction
// +0x10), [2] SceneMaterialParameterApplying (chara sub-buffer index +0xc,
// draw param +8; -1 = skipped). Chara writes [1] every update (0x289850)
// and [2] in vt[133]/[135], all only for the 10 parts at chara +0x370
// (getter 0x29dfb0), so a stacked part kept the load-time light and no
// sub-buffer. Its entries [1]/[2] are pointed at the shown part's functors
// instead (not owned there, so nothing is freed twice; both parts live in
// the same resource map and are unloaded together).
constexpr size_t kPartCallbacksBegin = 0x140, kPartCallbacksEnd = 0x148, kPartCallbackEntrySize = 0x10;
constexpr size_t kLightCallback = 1, kSceneCallback = 2;

// BCS (0x65c6c0 / 0x29a700): part set table count u16 +0x0c, offsets +0x18;
// part set: parts count u16 +0x14, offsets +0x18 (index = part slot),
// physics count u16 +0x16, offset +0x1c, 0x48 bytes each; part: flags +0x18,
// physics count u16 +0x4a, offset +0x4c (from the part), flags +0x18 each
// (0x29a4d0 takes those with flags & 4).
constexpr uint32_t kPartReplacesBase = 4; // part flag: shown instead of the base part (hair replacers)
constexpr size_t kPhysicsSize = 0x48;

// Character records: 0x27a330(*0x1568240, id) -> 0xe0-byte record, filled
// from a CaC by 0x653830: +0x04 = CaC +0x50, +0x08 = CaC +0x4c (appearance
// values), +0x20 accessory part set (i16, < 0 none), +0xb0 the CaC's name as
// an MSVC std::string (buffer or pointer +0xb0, capacity +0xc8).
constexpr uintptr_t kRecordTableRva = 0x1568240;
constexpr uintptr_t kFindRecordRva = 0x27a330;
constexpr size_t kRecordValueA = 0x04, kRecordValueB = 0x08;
constexpr size_t kRecordAccessory = 0x20;
constexpr size_t kRecordName = 0xb0, kRecordNameCapacity = 0xc8;
// Local records are < 16 (0 previews / the player in battle, 1 Conton City,
// 3 training partner) or >= 0x1000 (save and character select). Lobby NPCs
// (128..347) and online players (~410..430) can share a CaC's name.
constexpr int kLocalRecordIdsBelow = 16, kSelectRecordIds = 0x1000;

// Online: a player's avatar goes out as a profile packed by 0x654580(record,
// profile) (room lobby NeRoomEvent 0x46a690, Conton lobby 0x465500, ...) and
// comes back into a record by 0x654830(member, record) (room members
// 0x5dad70 / 0x5d95d0, ...). The appearance block sits at profile +0xe0 and
// member +0xc8; its +0x50 (u32) and +0x54 (u16) are copied along with the
// block (0x7207a0) but neither written by the packer (0x654d70) nor read by
// the unpacker (0x655080), so they carry the extras: u32 = magic | first id
// << 16, u16 = second id (0xffff = none). Research_Notes\multi-accessory.md.
constexpr uintptr_t kProfilePackRva = 0x654580;
constexpr uintptr_t kProfilePackCallRvas[] = {0x4111fd, 0x4655bd, 0x469a0c, 0x46a788, 0x46e1b2, 0x46e885,
                                              0x4e02f4, 0x4edf57, 0x5b2737, 0x5b2b79, 0x74e3ae};
constexpr uintptr_t kMemberUnpackRva = 0x654830;
// The room's 9-member table is 0x3f1670 (0x5dafe3 / 0x5db05b / 0x5da805 /
// 0x5da8e7); 0x3e7a56 and 0x856bc3 unpack NPCs / preset avatars.
constexpr uintptr_t kMemberUnpackCallRvas[] = {0x38bfdb, 0x3aefca, 0x3af050, 0x3af1b3, 0x3e7a56, 0x47721d, 0x5da805,
                                               0x5da8e7, 0x5dafe3, 0x5db05b, 0x5e8646, 0x812813, 0x856bc3};
// 0x7207a0(dst, src) copies an appearance block (leaf, returns dst); its
// callers include the agent layer's profile copies (CommandParameter_Profile
// 0x72b210, NRTCommand_WaitOtherProfile 0x729bd0, LoginCommand_PlayerData
// 0x6f8270). Online test 4: each machine saw the other's tagged block here
// (0x46a4eb, 0x46d461, 0x46dbb7 - room member profiles), so the bytes survive
// the server, but no hooked unpack ever built the other player's record. So
// the receive side lives here (BlockCopyHook). 0x856bc3 above unpacks preset
// lobby avatars (0x410be0 reads lobby_preset_avatar_list.pal), not players.
constexpr uintptr_t kBlockCopyRva = 0x7207a0;
constexpr uintptr_t kBlockCopyCallRvas[] = {0x465147, 0x46573f, 0x4672fa, 0x4680e7, 0x46a4eb, 0x46a8f9, 0x46d461,
                                            0x46dbb7, 0x46e90f, 0x6ee6e7, 0x6f836e, 0x6f8aed, 0x729c7e, 0x72b407,
                                            0x72bca5, 0x72bffa, 0x72c182, 0x72c231, 0x72c2ab, 0x72c37c, 0x72c8b5,
                                            0x72c9fa, 0x72d053, 0x72d8f1, 0x7336b4, 0x733734};
constexpr size_t kProfileAppearance = 0xe0, kMemberAppearance = 0xc8;
constexpr size_t kChannelFirst = 0x50, kChannelSecond = 0x54;
constexpr uint32_t kChannelMagic = 0x4d41; // "MA"
constexpr uint16_t kChannelNone = 0xffff;
// Block -> record converter 0x655080(block, record): every path that turns a
// received block into a record calls it (member unpack 0x654847, profile
// unpack 0x654927, room hologram listener 0x46f8d0, NPCs 0x3e6bd0). It only
// writes record +0x00..+0x64 (race, appearance values, parts, colours; its
// helper 0x61af70 is a table lookup), so running it on a scratch record gives
// the fields the other player's record will hold, whichever path builds it.
// Received extras are kept by record +0x00..+0x20 (+0x20 = the accessory,
// which the mod swaps while loading extras).
// Online test 5: a change made inside the room never reached the other
// machine through a block copy, and battle drew the old extras. Some paths
// convert a block in place without copying it (profile unpack 0x654910 from
// 0x5bbfff, hologram listener), so the converter's player call sites are
// hooked as well (0x3e6bd0 = preset lobby avatars, left alone).
constexpr uintptr_t kBlockToRecordCallRvas[] = {0x46f8d0, 0x654847, 0x654927};
constexpr uintptr_t kBlockToRecordRva = 0x655080;
constexpr size_t kRecordLookSize = 0x20, kScratchRecordSize = 0x100;

// Live save: CaC of slot n = *0x1568310 + 0x120d0 + n * 0xe880, name +0x0c,
// the values copied to record +0x04 / +0x08 at +0x50 / +0x4c; current slot
// = 0x433e0(manager + 8) (all from 0x828b40 and 0x653830).
constexpr uintptr_t kSaveManagerRva = 0x1568310;
constexpr uintptr_t kCurrentSlotRva = 0x433e0;
constexpr size_t kSaveSlots = 0x120d0, kSaveSlotSize = 0xe880;
constexpr size_t kCacName = 0x0c, kCacValueA = 0x50, kCacValueB = 0x4c;
constexpr int kSlotCount = 8;

constexpr size_t kMaxExtras = 2; // the equipped accessory + 2 = 3 at once

// Loadout builder 0x653830(character, out, preset): fills a character's
// battle/preview setup from loadout record `preset` (negative = record 0,
// cmovns at 0x653908; records 0x68 bytes at character + 0xf4). The 38 calls
// below are every one in 1.27; most pass -1, the battle character select
// (0x38bdde, 0x38db8e..0x38ddc8), its preview (0x5e66ec, 0x5e7294) and the
// Presets panel (0x5074a0, 0x5075f5) pass a preset. Call sites, not the
// entry: Battle Styles hooks the entry.
constexpr uintptr_t kBuildLoadoutRva = 0x653830;
constexpr uintptr_t kBuildLoadoutCallRvas[] = {
    0x38bdde, 0x38db8e, 0x38dc43, 0x38dccf, 0x38ddc8, 0x38ff4b, 0x3a5bd0, 0x3a8d1a, 0x3a9971, 0x3a99f0,
    0x3b019d, 0x3f3f96, 0x422504, 0x457136, 0x4dfc89, 0x4e05da, 0x4edf17, 0x4f1176, 0x4f2066, 0x4f8aca,
    0x4f9f25, 0x50676b, 0x5074a0, 0x5075f5, 0x50afa8, 0x50afd3, 0x50c021, 0x55dbb7, 0x581521, 0x5bcf92,
    0x5c4585, 0x5c6a94, 0x5e196a, 0x5e66ec, 0x5e7294, 0x5ec39c, 0x74e38d, 0x9055e3};
constexpr int kPresetCount = 8; // loadout records per CaC: 0 = in use, 1..7 = saved presets
constexpr int kMainPreset = 0;

// The Presets panel, EditMyset (vtable 0x10af228, .rdata): its AS callback
// (slot 17, vanilla 0x506880) gets argc at +0x10, arg 0 (sendType) as a
// double at +0x28, arg 1 (preset) at +0x48. sendType 2 = load preset n
// (record n over record 0), 3 = save preset n (record 0 over record n).
constexpr uintptr_t kEditMysetVtableRva = 0x10af228;
constexpr int kUserCallbackSlot = 17;
constexpr size_t kCallArgCount = 0x10, kCallArg0 = 0x28, kCallArg1 = 0x48;
constexpr int kMysetLoad = 2, kMysetSave = 3;

// The loaded save's decrypted image is kept by the save/system singleton
// [0x15681a8] at +0x68: "#SAV", then the owning Steam id at +8 XORed with
// "thebecon" (Xv2CoreLib SAV_File.cs; same read as Battle Styles).
constexpr uintptr_t kSaveSystemRva = 0x15681a8;
constexpr size_t kSaveImage = 0x68, kSaveImageSteamId = 8;
constexpr uint64_t kSteamIdXor = 0x7468656265636F6E;

// The equip screen, EditChara (vtable 0x10ae228, .rdata; slot 17 = its AS
// callback, same args as EditMyset's; Battle Styles chains it too). Its
// cloth lists are indexed by request type - 206: +0x2bc + 4k = the picked
// index, +0x548 + 24k = vector<int> of the list's IDB item ids (-1 = none),
// +0x328 + 0x20k = wstring "icon^name" of the pick (0x4f98d0, 0x4fc0d0).
// k = 4 is the accessory (request 210, SendTypeRequestListClothAccessory).
// Item id -> part set: 0x61af70(itemType, id) returns the IDB entry's +0x30
// (MODEL); type 4 = accessory, -1 when the id has no entry (0x4f9517 uses it
// for the accessory row).
constexpr uintptr_t kEditCharaVtableRva = 0x10ae228;
constexpr int kListAccessory = 210;
constexpr size_t kMenuListType = 0x2f8;  // int: request type of the open list
constexpr size_t kMenuAccessoryIndex = 0x2bc + 4 * 4;
constexpr size_t kMenuAccessoryIds = 0x548 + 24 * 4;
constexpr size_t kMenuAccessoryText = 0x328 + 0x20 * 4;
constexpr uintptr_t kItemPartSetRva = 0x61af70;
constexpr int kItemTypeAccessory = 4;
// The list's callback 0x4fb000(close, ?, value, menu) calls apply
// 0x4f98d0(menu, value, decided) (hover = 0: preview; decided/cancel = 1),
// then, when closing, close 0x4f9d90(menu).
constexpr uintptr_t kListApplyRva = 0x4f98d0, kListApplyCallRva = 0x4fb018;
constexpr uintptr_t kListCloseRva = 0x4f9d90, kListCloseCallRva = 0x4fb024;
// Item push 0x4fc0d0(menu, requestType): sends a cloth row's pick to the AS
// (user data 23/24 for the accessory). Every call in 1.27.
constexpr uintptr_t kItemPushRva = 0x4fc0d0;
constexpr uintptr_t kItemPushCallRvas[] = {0x4f8e97, 0x4f9da3, 0x4fbcde, 0x4fbe15, 0x4fbef5,
                                           0x4fc782, 0x4fc875, 0x4fe5ed, 0x4fe6d7};
// OggyBase user-data setters (bounds-checked; the string one keeps the
// pointer). Avatar.as: 98 + 2c = column c+1's "icon^name", 99 + 2c = > 0
// when the row is usable (Battle Styles has 96/97).
constexpr uintptr_t kSetUserDataIntRva = 0x2c82a0, kSetUserDataStrRva = 0x2c8340;
constexpr int kUserDataExtraText = 98;
// EditChara's ctor 0x4f7280 passes its user-data slot count as 2 + 0x5e
// (mov edx, 2; lea r8d, [rdx+0x5e] at 0x4f72af). Raised to the most any mod
// needs (Battle Styles 0x60, us 0x64): each mod only ever raises the byte.
constexpr uintptr_t kSlotCountCodeRva = 0x4f72af, kSlotCountByteRva = 0x4f72b7;
constexpr uint8_t kSlotCountCode[] = {0xBA, 0x02, 0x00, 0x00, 0x00, 0x44, 0x8D, 0x42};
constexpr uint8_t kSlotCountVanilla = 0x5e, kSlotCountWanted = 0x64;
constexpr int kNoPreview = INT_MIN;
// Model rebuild request 0x65cf60(builder, loadout, ...), loadout = 0xb0
// bytes, +0x28 = the accessory's part set. Not busy (+0x108 <= 1): the old
// copy at +0x168 is swapped for the new one, the 10 body parts reload where
// they differ from the loaded ids (+0x2f4), and the accessory loader
// 0x65c6c0 runs only when +0x28 differs from the old copy's (0x65d3e5) - so
// an extra-only change never redraws. Busy: compared with the queued copy
// +0x244 (its +0x28 at 0x65d019). Every call site in 1.27 (equip screen,
// presets, chara select, lobby...); the equip screen's is 0x4f97be, made
// every frame from the menu's picks by 0x4f90b0.
constexpr uintptr_t kModelRequestRva = 0x65cf60;
constexpr uintptr_t kModelRequestCallRvas[] = {0x4f97be, 0x4fec55, 0x4fefe1, 0x505cb7,
                                               0x507807, 0x5ec70d, 0x65ac47, 0x65d914};
constexpr uintptr_t kEquipModelRequestReturnRva = 0x4f97be + 5;
constexpr size_t kBuilderBusy = 0x108, kBuilderCachedA = 0x168, kBuilderCachedB = 0x244;
constexpr size_t kLoadoutAccessory = 0x28;
// Lobby appearance send 0x4657d0(lobby, record, 1); called when the equip
// screen (EditChara exit 0x4f8690) and the Presets panel (EditMyset slot 15,
// 0x506680) close. A record is 0xe0 bytes (table 0x1568240 entries).
constexpr uintptr_t kLobbyPushRva = 0x4657d0;
constexpr uintptr_t kLobbyPushCallRvas[] = {0x4f8bc1, 0x5067e3};
constexpr size_t kRecordSize = 0xe0;
// The checks both callers make before that send (0x4f8b80-0x4f8bb0): online
// [0x1585208] != 0, game mode 0x46ee0(*0x15681f8) == 0x14 (a room), lobby
// [0x158b378]; record = 0x27a330(*0x1568240, 0) (the player's CaC).
// The equip screen only reaches it when the bit 0x4 of menu +0x1b4 is set
// (0x4f8a3d, 0x66d580): an extras-only change jumps to 0x4f8bda and sends
// nothing (online test 6), so a pick sends itself (RoomPushAfterPick).
constexpr uintptr_t kOnlineGlobalRva = 0x1585208;
constexpr uintptr_t kModeOwnerGlobalRva = 0x15681f8;
constexpr uintptr_t kGameModeRva = 0x46ee0;
constexpr int kModeRoom = 0x14;
constexpr uintptr_t kRoomLobbyGlobalRva = 0x158b378;
constexpr int kPlayerRecordId = 0;
// The lobby's own avatar (offline too) rebuilds when 0x489120(chara) says
// its record changed: it compares the record at [chara+0x510] with the copy
// cached at chara+0x530 (record +0x04, +0x0c..+0x1f, the accessory +0x20,
// +0x32..+0x71). On a change the idle state 0x490670 drops the model, copies
// the record into the cache and reloads it (state 0x4901c0). Static RE.
constexpr uintptr_t kLobbyCharaChangedRva = 0x489120;
constexpr uintptr_t kLobbyCharaChangedCallRva = 0x4906b3; // in 0x490670
constexpr size_t kLobbyCharaRecord = 0x510, kLobbyCharaCachedRecord = 0x530;

using BuilderLoaderFn = int(__fastcall *)(void *builder, int partSet);
using ResPartPhysicsFn = void(__fastcall *)(void *resource, void *entries, void *holder, int argD8, int argDc);
using AddPhysicsObjectFn = void(__fastcall *)(void *holder, void *physics, int partIndex, int argD8, void *, void *, int argDc);
using PhysicsInitFn = void(__fastcall *)(void *object, void *physics, int r8, int r9, void *, void *, int);
using MapFindFn =void *(__fastcall *)(void *map, char **outNode, const int *key);
using ProfilePackFn = void *(__fastcall *)(char *record, char *profile);
using MemberUnpackFn = void *(__fastcall *)(char *member, char *record);
using BlockCopyFn = char *(__fastcall *)(char *dst, const char *src);
using BlockToRecordFn = void(__fastcall *)(const char *block, char *record);
using VectorInsertIntFn =void(__fastcall *)(void *vector, int *where, const int *value);
using HolderPartFn = char *(__fastcall *)(void *holder, int partSlot);
using PartModelFn = void *(__fastcall *)(char *part);
using ResSkeletonFn = int(__fastcall *)(void *resource);
using RegisterModelFn = int(__fastcall *)(void *skeleton, void *model, unsigned key, int flag);
using UnregisterModelFn = int(__fastcall *)(void *skeleton, unsigned key);
using BuilderFn = int(__fastcall *)(void *builder);
using BuilderTeardownFn = void(__fastcall *)(void *builder);
using FindRecordFn = char *(__fastcall *)(void *table, int id);
using CurrentSlotFn = int(__fastcall *)(void *managerPlus8);
using BuildLoadoutFn = void *(__fastcall *)(void *character, void *out, int preset);
using UserCallbackFn = void(__fastcall *)(void *menu, const uint8_t *call);
using ListApplyFn = void(__fastcall *)(void *menu, int value, int decided);
using ListCloseFn = void(__fastcall *)(void *menu);
using ItemPushFn = void(__fastcall *)(void *menu, int requestType);
using ItemPartSetFn = int(__fastcall *)(int itemType, int itemId);
using ModelRequestFn = int(__fastcall *)(void *builder, void *loadout, intptr_t, intptr_t, intptr_t, intptr_t);
using LobbyPushFn = void(__fastcall *)(void *lobby, char *record, int arg);
using GameModeFn = int(__fastcall *)(void *owner);
using LobbyCharaChangedFn = int(__fastcall *)(void *chara);
using SetUserDataIntFn = void(__fastcall *)(void *menu, int index, int value);
using SetUserDataStrFn = void(__fastcall *)(void *menu, int index, const wchar_t *text);

uintptr_t g_exe = 0;
std::wstring g_modDir;
bool g_logging = false;
volatile LONG g_slotPreset[kSlotCount] = {}; // the preset each save slot was last built from
UserCallbackFn g_mysetCallback = nullptr;    // EditMyset's slot 17 before our hook
UserCallbackFn g_editCharaCallback = nullptr; // EditChara's slot 17 before our hook
// The open accessory list picks for extra column 1 or 2 (0 = the equipped
// accessory, as vanilla); the equipped one's id while it does; the hovered id
// the preview shows in that column. Set on the menu thread, the preview
// reads them on the loader's.
std::atomic<int> g_pickColumn{0}, g_pickMain{-1}, g_previewId{kNoPreview};
std::atomic<int> g_pickMainIndex{-1}; // the equipped one's list index
std::atomic<int> g_pickMainPartSet{-1}; // the equip screen loadout's accessory before the list opened
std::atomic<bool> g_forceRebuild{false}; // the equip screen's next model request reloads the accessory even if unchanged
std::atomic<bool> g_lobbyStale{false}; // preset 0's extras changed since the lobby avatar was last sent
std::atomic<bool> g_lobbyCharaStale{false}; // same, for the lobby's own avatar (LobbyCharaChangedHook)
std::vector<int> g_extras[kSlotCount]; // part set ids per save slot, from the config
std::mutex g_logLock;                  // characters load on several threads

void Log(const char *format, ...)
{
    if (!g_logging) return;
    std::lock_guard<std::mutex> lock(g_logLock);
    FILE *file = nullptr;
    // One session per log: the first write of a run moves the last run's log to <name>.prev.log.
    static volatile LONG rotated = 0;
    std::wstring path = g_modDir + L"MultiAccessory.log";
    if (!InterlockedExchange(&rotated, 1)) MoveFileExW(path.c_str(), (g_modDir + L"MultiAccessory.prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);
    if (_wfopen_s(&file, path.c_str(), L"a") != 0 || !file) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    fprintf(file, "%02d:%02d:%02d ", now.wHour, now.wMinute, now.wSecond);
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

template <typename T> T &Field(void *base, size_t offset)
{
    return *reinterpret_cast<T *>(static_cast<char *>(base) + offset);
}

// --- Config ------------------------------------------------------------------

// Appends the numbers of the JSON array after "key" to ids; no key, no ids.
void ReadIdArray(const char *text, const char *key, std::vector<int> &ids)
{
    const char *found = strstr(text, key);
    const char *open = found ? strchr(found, '[') : nullptr;
    const char *close = open ? strchr(open, ']') : nullptr;
    if (!close) return;
    for (const char *cursor = open + 1; cursor < close;) {
        char *end = nullptr;
        long id = strtol(cursor, &end, 10);
        if (end == cursor) { cursor++; continue; } // a comma or a space
        if (id >= 0 && id < 0x8000) ids.push_back(static_cast<int>(id));
        cursor = end;
    }
}

// Reads multi_accessory.json; a missing config is written from the default.
void ReadConfig()
{
    std::wstring path = g_modDir + L"multi_accessory.json";
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) {
        if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file) {
            fputs(kDefaultConfig, file);
            fclose(file);
        }
        return;
    }
    std::string text(16384, '\0');
    text.resize(fread(&text[0], 1, text.size() - 1, file));
    fclose(file);

    const char *logging = strstr(text.c_str(), "\"logging\"");
    const char *value = logging ? strchr(logging, ':') : nullptr;
    g_logging = value && strncmp(value + 1 + strspn(value + 1, " \t\r\n"), "true", 4) == 0;

    std::vector<int> all;
    ReadIdArray(text.c_str(), "\"all\"", all);
    for (int slot = 0; slot < kSlotCount; slot++) {
        // "slot_1".."slot_8" as players count them; save slots are 0..7.
        std::string key = "\"slot_" + std::to_string(slot + 1) + "\"";
        g_extras[slot] = all;
        ReadIdArray(text.c_str(), key.c_str(), g_extras[slot]);
    }
}

int CurrentSlot();

// --- Saved picks ---------------------------------------------------------------
// multi_accessory.sav (JSON text; .sav so tools sharing the mod's .json
// configs leave each player's picks alone, as Battle Styles' file): Steam
// account -> save slot (0..7) -> preset -> part set ids. A preset without an
// entry uses the config's list for its slot; an empty list means none.
//   {"76561198000000000": {"3": {"0": [835, 846], "2": []}}}

using PresetPicks = std::map<int, std::vector<int>>; // preset -> ids
using SlotPicks = std::map<int, PresetPicks>;        // save slot -> presets
std::mutex g_savedLock;
std::map<std::string, SlotPicks> g_saved; // every account in the file
std::string g_account;                    // the playing account, once a save is loaded
bool g_savedLoaded = false;

std::wstring SavedPath() { return g_modDir + L"multi_accessory.sav"; }

// The loaded save's Steam account (64-bit id as text), empty before one is
// loaded. From the save, not Steam: saves are per account.
std::string SteamAccount()
{
    char *saveSystem = *reinterpret_cast<char **>(g_exe + kSaveSystemRva);
    if (!saveSystem || memcmp(saveSystem + kSaveImage, "#SAV", 4) != 0) return {};
    uint64_t id = Field<uint64_t>(saveSystem, kSaveImage + kSaveImageSteamId) ^ kSteamIdXor;
    return id ? std::to_string(id) : std::string();
}

// Just enough JSON for the file's one shape.
struct JsonReader {
    const char *at;
    bool Eat(char c)
    {
        at += strspn(at, " \t\r\n");
        if (*at != c) return false;
        at++;
        return true;
    }
    bool Key(std::string &out) // "text" followed by ':'
    {
        if (!Eat('"')) return false;
        const char *end = strchr(at, '"');
        if (!end) return false;
        out.assign(at, end);
        at = end + 1;
        return Eat(':');
    }
    bool Ids(std::vector<int> &out) // [1, 2, ...]
    {
        if (!Eat('[')) return false;
        if (Eat(']')) return true;
        do {
            char *end = nullptr;
            long id = strtol(at, &end, 10);
            if (end == at) return false;
            if (id >= 0 && id < 0x8000) out.push_back(static_cast<int>(id));
            at = end;
        } while (Eat(','));
        return Eat(']');
    }
    // {"key": value, ...}, calling item(key) to read each value.
    template <typename Item> bool Object(Item item)
    {
        if (!Eat('{')) return false;
        if (Eat('}')) return true;
        do {
            std::string key;
            if (!Key(key) || !item(key)) return false;
        } while (Eat(','));
        return Eat('}');
    }
};

// Caller holds g_savedLock.
void LoadSaved()
{
    g_savedLoaded = true;
    FILE *file = nullptr;
    if (_wfopen_s(&file, SavedPath().c_str(), L"rb") != 0 || !file) return; // nothing picked yet
    std::string text;
    char chunk[4096];
    for (size_t read; (read = fread(chunk, 1, sizeof chunk, file)) > 0;) text.append(chunk, read);
    fclose(file);
    std::map<std::string, SlotPicks> saved;
    JsonReader reader{text.c_str()};
    bool ok = reader.Object([&](const std::string &account) {
        return reader.Object([&](const std::string &slotKey) {
            int slot = atoi(slotKey.c_str());
            return reader.Object([&](const std::string &presetKey) {
                int preset = atoi(presetKey.c_str());
                std::vector<int> ids;
                if (!reader.Ids(ids)) return false;
                if (slot >= 0 && slot < kSlotCount && preset >= 0 && preset < kPresetCount)
                    saved[account][slot][preset] = ids;
                return true;
            });
        });
    });
    if (ok) g_saved = saved;
    else Log("multi_accessory.sav unreadable - ignored until a pick changes");
}

// Caller holds g_savedLock. Written to a temporary file first so a crash
// never leaves half a file.
void WriteSaved()
{
    std::string text = "{";
    const char *accountComma = "";
    for (const auto &[account, slots] : g_saved) {
        if (slots.empty()) continue;
        text += std::string(accountComma) + "\n    \"" + account + "\": {";
        accountComma = ",";
        const char *slotComma = "";
        for (const auto &[slot, presets] : slots) {
            text += std::string(slotComma) + "\n        \"" + std::to_string(slot) + "\": {";
            slotComma = ",";
            const char *presetComma = "";
            for (const auto &[preset, ids] : presets) {
                text += std::string(presetComma) + "\"" + std::to_string(preset) + "\": [";
                for (size_t i = 0; i < ids.size(); i++) text += (i ? ", " : "") + std::to_string(ids[i]);
                text += "]";
                presetComma = ", ";
            }
            text += "}";
        }
        text += "\n    }";
    }
    text += "\n}\n";
    std::wstring path = SavedPath(), temporary = path + L".tmp";
    FILE *file = nullptr;
    bool ok = _wfopen_s(&file, temporary.c_str(), L"wb") == 0 && file;
    if (ok) {
        ok = fwrite(text.data(), 1, text.size(), file) == text.size();
        ok = fclose(file) == 0 && ok;
    }
    if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        Log("multi_accessory.sav: cannot write");
}

// The playing account's picks, loading the file on first use; null while no
// save is loaded (nothing is read or written then, so no pick lands on the
// wrong account). Caller holds g_savedLock.
SlotPicks *AccountPicks()
{
    if (g_account.empty()) {
        g_account = SteamAccount();
        if (g_account.empty()) return nullptr;
        Log("Steam account %s (from the save)", g_account.c_str());
    }
    if (!g_savedLoaded) LoadSaved();
    return &g_saved[g_account];
}

// The extras picked for a save slot's preset: the saved list, else the
// config's list for the slot.
std::vector<int> PickedExtras(int slot, int preset)
{
    if (slot < 0 || slot >= kSlotCount) return {};
    std::lock_guard<std::mutex> lock(g_savedLock);
    if (SlotPicks *picks = AccountPicks()) {
        auto presets = picks->find(slot);
        if (presets != picks->end()) {
            auto ids = presets->second.find(preset);
            if (ids != presets->second.end()) return ids->second;
        }
    }
    return g_extras[slot];
}

// Saves a save slot's picks for one preset (for the equip screen row).
void SetPickedExtras(int slot, int preset, const std::vector<int> &ids)
{
    if (slot < 0 || slot >= kSlotCount || preset < 0 || preset >= kPresetCount) return;
    std::lock_guard<std::mutex> lock(g_savedLock);
    SlotPicks *picks = AccountPicks();
    if (!picks) return;
    auto &saved = (*picks)[slot][preset];
    if (saved == ids) return;
    saved = ids;
    WriteSaved();
}

// The preset a save slot was last built from (0 if never).
int SlotPreset(int slot)
{
    return slot >= 0 && slot < kSlotCount ? InterlockedCompareExchange(&g_slotPreset[slot], 0, 0) : kMainPreset;
}

// The game copied loadout record `from` over `to` of the loaded CaC; its
// saved extras go along (none saved: `to` falls back to the config too).
void CopyPreset(int from, int to)
{
    int slot = CurrentSlot();
    if (slot < 0 || slot >= kSlotCount || from < 0 || from >= kPresetCount || to < 0 || to >= kPresetCount || from == to)
        return;
    std::lock_guard<std::mutex> lock(g_savedLock);
    SlotPicks *picks = AccountPicks();
    if (!picks) return;
    PresetPicks &presets = (*picks)[slot];
    auto source = presets.find(from);
    auto target = presets.find(to);
    if (source != presets.end()) {
        if (target != presets.end() && target->second == source->second) return;
        presets[to] = source->second;
    } else {
        if (target == presets.end()) return;
        presets.erase(target);
    }
    if (presets.empty()) picks->erase(slot);
    WriteSaved();
    if (to == kMainPreset) g_lobbyStale = g_lobbyCharaStale = true;
    Log("slot %d: preset %d -> %d, extras copied", slot + 1, from, to);
}

// --- BCS ---------------------------------------------------------------------

// Part set `partSet` of a BCS, null if it has none (same checks as 0x65c6c0).
char *PartSetData(char *bcs, int partSet)
{
    uint32_t tableOffset = bcs ? Field<uint32_t>(bcs, 0x18) : 0;
    if (!tableOffset || partSet < 0 || partSet >= Field<uint16_t>(bcs, 0x0c)) return nullptr;
    uint32_t partSetOffset = reinterpret_cast<uint32_t *>(bcs + tableOffset)[partSet];
    return partSetOffset ? bcs + partSetOffset : nullptr;
}

// The part a part set has for a part slot, null if none (= 0x298200).
char *PartOfPartSet(char *partSetData, int partSlot)
{
    uint32_t offsetsOffset = Field<uint32_t>(partSetData, 0x18);
    if (!offsetsOffset || partSlot < 0 || partSlot >= Field<uint16_t>(partSetData, 0x14)) return nullptr;
    uint32_t partOffset = reinterpret_cast<uint32_t *>(partSetData + offsetsOffset)[partSlot];
    return partOffset ? partSetData + partOffset : nullptr;
}

// Which body part slots a part set swaps (bit n = part slot n): its parts
// with flags 4 (wigs, helmets, hats) are shown instead of the base part.
// `selfHidden`: slots its own objects (or its parts on other slots) hide -
// a replacer there is a placeholder never seen in vanilla (MAM hat 835:
// Hair = model 0, the bare head with ears, hidden by the hat object's
// 0x20; HUM 918: Hair = model -1, same). A part's mask never hides its own
// slot (wig 612 hides 0x30 and still shows).
constexpr int kMaxPartSlots = 32;
constexpr size_t kPartHideMask = 0x1c, kPhysicsHideMask = 0x1c; // part / BCS physics entry
struct SlotUse {
    uint32_t replaced = 0;
    uint32_t selfHidden = 0;
    char *replacer[kMaxPartSlots] = {};
};

SlotUse SlotsUsed(char *partSetData)
{
    SlotUse use;
    for (int partSlot = 0; partSlot < kMaxPartSlots && partSlot < Field<uint16_t>(partSetData, 0x14); partSlot++) {
        char *part = PartOfPartSet(partSetData, partSlot);
        if (!part) continue;
        if (Field<uint32_t>(part, 0x18) & kPartReplacesBase) {
            use.replaced |= 1u << partSlot;
            use.replacer[partSlot] = part;
        }
        use.selfHidden |= Field<uint32_t>(part, kPartHideMask) & ~(1u << partSlot);
        uint32_t physicsOffset = Field<uint32_t>(part, 0x4c);
        for (int i = 0; physicsOffset && i < Field<uint16_t>(part, 0x4a); i++) {
            char *physics = part + physicsOffset + i * kPhysicsSize;
            if (Field<uint32_t>(physics, 0x18) & kPartReplacesBase) use.selfHidden |= Field<uint32_t>(physics, kPhysicsHideMask);
        }
    }
    uint32_t ownPhysicsOffset = Field<uint32_t>(partSetData, 0x1c);
    for (int i = 0; ownPhysicsOffset && i < Field<uint16_t>(partSetData, 0x16); i++)
        use.selfHidden |= Field<uint32_t>(partSetData + ownPhysicsOffset + i * kPhysicsSize, kPhysicsHideMask);
    return use;
}

// A part's file name string `index` (BCSPart, eternity_common BcsFile.h:
// offsets +0x38, from the part; 0 = none).
const char *PartFile(char *part, int index)
{
    uint32_t offset = Field<uint32_t>(part, 0x38 + index * 4);
    return offset ? part + offset : "";
}

// Same model, textures, flags and files (BCSPart +0x00..+0x37 minus the
// colour selector offset +0x14, plus the 4 file names): drawing both
// changes nothing. MAM/MAF hats 835 and Arale hat 843 both replace Hair
// with model 0 + MAM_000_Face_base and hang the hat itself as a physics
// object.
bool SamePart(char *a, char *b)
{
    if (memcmp(a, b, 0x14) || memcmp(a + 0x18, b + 0x18, 0x34 - 0x18 + 4)) return false;
    for (int i = 0; i < 4; i++)
        if (strcmp(PartFile(a, i), PartFile(b, i))) return false;
    return true;
}

uint32_t SameReplacers(const SlotUse &a, const SlotUse &b)
{
    uint32_t sameReplacer = 0;
    for (int partSlot = 0; partSlot < kMaxPartSlots; partSlot++)
        if (a.replacer[partSlot] && b.replacer[partSlot] && SamePart(a.replacer[partSlot], b.replacer[partSlot]))
            sameReplacer |= 1u << partSlot;
    return sameReplacer;
}

// An extra to show. Extras follow no replacement rules (user's call,
// retest 17): they never swap or hide a part of the CaC. On the resource
// path every part an extra would swap in is drawn on top instead
// (`stackedSlots`: its part set is preloaded into the resource's holder map
// and those parts registered in the skeleton at key 10+,
// Research_Notes\multi-accessory-wigs.md), and the hide masks of its
// objects are cleared (PhysicsInitHook). Earlier tries that hung the load
// and must not come back: attached objects on no part (part index -1,
// retests 7 + 8) and a part model as a made-up physics object (retest 6).
struct ExtraToShow {
    int id;
    uint32_t stackedSlots; // part slots drawn on top of the part the slot shows (0 = none)
};

// The picked extras that this race's BCS has, next to the equipped
// accessory: in pick order, without duplicates or ids the BCS lacks. A
// replacer identical to one the equipped accessory or an earlier extra
// already brings is not stacked again, nor a placeholder its own objects
// hide (`selfHidden`; drawing it put a bare MAM head over every wig, retest
// 18). Only the resource path stacks; the builder (equip screen) loads each
// extra like an accessory and puts the equipped accessory's holder last
// (BuilderLoaderHook), so the equipped wig/helmet keeps its slot there.
std::vector<ExtraToShow> ExtrasToShow(char *bcs, int equipped, const std::vector<int> &picked)
{
    std::vector<ExtraToShow> shown;
    if (!bcs) return shown;
    char *equippedData = PartSetData(bcs, equipped);
    SlotUse taken = equippedData ? SlotsUsed(equippedData) : SlotUse{};
    for (int extra : picked) {
        if (shown.size() == kMaxExtras) break;
        if (extra == equipped) continue;
        bool duplicate = false;
        for (const ExtraToShow &earlier : shown) duplicate |= earlier.id == extra;
        char *data = duplicate ? nullptr : PartSetData(bcs, extra);
        if (!data) continue;
        SlotUse uses = SlotsUsed(data);
        uint32_t stacked = uses.replaced & ~uses.selfHidden & ~SameReplacers(uses, taken);
        for (int partSlot = 0; partSlot < kMaxPartSlots; partSlot++)
            if (!taken.replacer[partSlot] && !(uses.selfHidden & (1u << partSlot))) taken.replacer[partSlot] = uses.replacer[partSlot];
        shown.push_back({extra, stacked});
    }
    return shown;
}

// --- Which CaC -----------------------------------------------------------------

char *SaveManager() { return *reinterpret_cast<char **>(g_exe + kSaveManagerRva); }

int CurrentSlot()
{
    char *manager = SaveManager();
    return manager ? reinterpret_cast<CurrentSlotFn>(g_exe + kCurrentSlotRva)(manager + 8) : -1;
}

const char *RecordName(char *record)
{
    return Field<size_t>(record, kRecordNameCapacity) >= 16 ? Field<const char *>(record, kRecordName) : record + kRecordName;
}

// Save slot of the CaC a record was made from, -1 if none: same name and
// same appearance values, so an NPC or online player named like one of our
// CaCs (lots are "PATROLLER") doesn't get its extras.
int SlotOfRecord(char *record)
{
    char *manager = SaveManager();
    const char *name = RecordName(record);
    if (!manager || !name || !*name) return -1;
    for (int slot = 0; slot < kSlotCount; slot++) {
        char *cac = manager + kSaveSlots + slot * kSaveSlotSize;
        if (strncmp(cac + kCacName, name, 64) != 0) continue;
        if (Field<int32_t>(cac, kCacValueA) == Field<int32_t>(record, kRecordValueA) &&
            Field<int32_t>(cac, kCacValueB) == Field<int32_t>(record, kRecordValueB))
            return slot;
        Log("  record \"%.64s\" has slot %d's name but other appearance values", name, slot + 1);
    }
    return -1;
}

// --- Online players' extras ----------------------------------------------------

// Extras online players sent (MemberUnpackHook), by name and appearance
// values (the record copies differ: table entry, stack temporaries).
using RemoteKey = std::tuple<std::string, int32_t, int32_t>;
std::map<RemoteKey, std::vector<int>> g_remoteExtras;
// Extras from tagged blocks (BlockCopyHook), by record look (kRecordLookSize).
std::map<std::string, std::vector<int>> g_remoteByLook;
std::mutex g_remoteLock;
std::atomic<bool> g_anyRemote{false};
// Looks whose extras changed after they were first seen (online test 7: an
// extras-only pick in the room arrived, but the room avatar's look was the
// same, so the game never reloaded it). LobbyCharaChangedHook forces one
// reload per look; under g_remoteLock.
std::set<std::string> g_staleLooks, g_seenLooks; // seen = any tagged block had it
std::atomic<bool> g_anyStaleLook{false};
constexpr size_t kMaxStaleLooks = 16; // one room's players, a few times over
constexpr size_t kMaxSeenLooks = 256; // a Conton lobby's ~40 players, a few times over
volatile LONG g_sendLogLines = 0, g_receiveLogLines = 0, g_remoteDrawLogLines = 0, g_sharedSeenLogLines = 0,
              g_blockCopyLogLines = 0;
// Diagnostics (under g_remoteLock): unpack caller + name pairs, drawn
// records (not our CaCs) and block copy caller + ids already logged once.
std::set<std::pair<uintptr_t, std::string>> g_unpacksSeen;
std::set<std::pair<int, std::string>> g_sharedSeen;
std::set<std::tuple<uintptr_t, uint32_t, uint16_t>> g_blockCopiesSeen;
// Send call + ids and drawn record + extras already logged: in test 5 one
// room entry repeated both ~40 times and used up their caps.
std::set<std::tuple<uintptr_t, uint16_t, uint16_t>> g_sendsSeen;
std::set<std::pair<int, std::vector<int>>> g_drawsSeen;
constexpr LONG kMaxOnlineLogLines = 40;
// A Conton lobby has ~40 players: their "other record" lines alone used up
// 40 lines before the room in online test 3.
constexpr LONG kMaxSharedSeenLogLines = 150;

bool OnlineLogLine(volatile LONG &lines, LONG cap = kMaxOnlineLogLines)
{
    return g_logging && InterlockedIncrement(&lines) <= cap;
}

// Lobby NPCs carry a message key as their name ("name_1011") or none; the
// diagnostics skip them so they don't use up the line caps.
bool IsLobbyNpcName(const std::string &name) { return name.empty() || name.compare(0, 5, "name_") == 0; }

RemoteKey KeyOfRecord(char *record)
{
    const char *name = RecordName(record);
    return {std::string(name ? name : "", name ? strnlen(name, 64) : 0), Field<int32_t>(record, kRecordValueA),
            Field<int32_t>(record, kRecordValueB)};
}

std::string RecordLook(const char *record) { return std::string(record, kRecordLookSize); }

// The extras an online player sent for this record (false if none).
bool RemoteExtras(char *record, std::vector<int> *picked)
{
    if (!g_anyRemote) return false;
    RemoteKey key = KeyOfRecord(record);
    std::lock_guard<std::mutex> lock(g_remoteLock);
    auto found = g_remoteExtras.find(key);
    if (found != g_remoteExtras.end()) {
        *picked = found->second;
        return true;
    }
    auto byLook = g_remoteByLook.find(RecordLook(record));
    if (byLook != g_remoteByLook.end()) {
        *picked = byLook->second;
        return true;
    }
    for (const auto &entry : g_remoteExtras)
        if (std::get<0>(entry.first) == std::get<0>(key) && OnlineLogLine(g_remoteDrawLogLines))
            Log("  online: record \"%.64s\" has a sender's name but other appearance values (%d %d, sent %d %d)",
                std::get<0>(key).c_str(), std::get<1>(key), std::get<2>(key), std::get<1>(entry.first), std::get<2>(entry.first));
    return false;
}

// The record a resource shows and the extras to draw on it: one of our CaCs
// (`slotOut` = its save slot, its picks) or an online player who sent theirs
// (`slotOut` = -1). False if neither.
bool ResourceExtras(void *resource, char **recordOut, int *slotOut, std::vector<int> *picked)
{
    *recordOut = nullptr;
    *slotOut = -1;
    void *table = *reinterpret_cast<void **>(g_exe + kRecordTableRva);
    int recordId = Field<int>(resource, kResRecordId);
    bool sharedId = recordId >= kLocalRecordIdsBelow && recordId < kSelectRecordIds; // NPCs, online players
    if (!table) return false;
    char *record = reinterpret_cast<FindRecordFn>(g_exe + kFindRecordRva)(table, recordId);
    if (!record) return false;
    *recordOut = record;
    if (!sharedId) {
        int slot = SlotOfRecord(record);
        if (slot >= 0) {
            *slotOut = slot;
            *picked = PickedExtras(slot, SlotPreset(slot));
            return true;
        }
    }
    if (g_logging) {
        // Diagnostic: each record drawn that isn't one of our CaCs (an online
        // player, on a shared or a local id), once, to compare with what
        // MemberUnpackHook stored.
        RemoteKey key = KeyOfRecord(record);
        bool firstSeen = false;
        if (!IsLobbyNpcName(std::get<0>(key))) {
            std::lock_guard<std::mutex> lock(g_remoteLock);
            firstSeen = g_sharedSeen.insert({recordId, std::get<0>(key)}).second;
        }
        if (firstSeen && OnlineLogLine(g_sharedSeenLogLines, kMaxSharedSeenLogLines))
            Log("online: other record %d \"%.64s\" (values %d %d) loaded, %s", recordId, std::get<0>(key).c_str(),
                std::get<1>(key), std::get<2>(key), g_anyRemote ? "looking up sent extras" : "nothing received yet");
    }
    if (!RemoteExtras(record, picked)) return false;
    bool drawSeen;
    {
        std::lock_guard<std::mutex> lock(g_remoteLock);
        drawSeen = !g_drawsSeen.insert({recordId, *picked}).second;
    }
    if (!drawSeen && OnlineLogLine(g_remoteDrawLogLines))
        Log("online: \"%.64s\" (record %d) draws %zu sent extra(s)", RecordName(record), recordId, picked->size());
    return true;
}

// Who the extras belong to, for the log.
std::string OwnerText(int slot)
{
    return slot < 0 ? "online player" : "slot " + std::to_string(slot + 1) + " preset " + std::to_string(SlotPreset(slot));
}

// The picks with extra column `column` (1 or 2) set to `id` (< 0 = none):
// the columns are the picks in order, kept without gaps or duplicates.
std::vector<int> WithColumn(const std::vector<int> &picks, int column, int id)
{
    int columns[kMaxExtras] = {-1, -1};
    for (size_t i = 0; i < kMaxExtras && i < picks.size(); i++) columns[i] = picks[i];
    columns[column - 1] = id < 0 ? -1 : id;
    std::vector<int> ids;
    for (int extra : columns)
        if (extra >= 0 && std::find(ids.begin(), ids.end(), extra) == ids.end()) ids.push_back(extra);
    return ids;
}

// --- Hooks -------------------------------------------------------------------

// Physics objects whose hide mask is cleared at init (PhysicsInitHook): the
// BCS entries of the extras' objects in the holder build running on this
// thread (set in ResPartPhysicsHook), and anything the builder loads while
// t_builderLoadsExtra (BuilderLoaderHook). Per thread, as both are filled
// and used inside one synchronous call.
thread_local std::vector<void *> t_extraPhysics;
thread_local bool t_builderLoadsExtra = false;

// After the game inits a physics object: an extra's object hides nothing
// (Android 13 hat / FRI hat 835 objects hide Hair, which took the equipped
// wig or helmet with it).
void __fastcall PhysicsInitHook(void *object, void *physics, int r8, int r9, void *a5, void *a6, int a7)
{
    reinterpret_cast<PhysicsInitFn>(g_exe + kPhysicsInitRva)(object, physics, r8, r9, a5, a6, a7);
    if (!object || !physics) return;
    if (t_builderLoadsExtra || std::find(t_extraPhysics.begin(), t_extraPhysics.end(), physics) != t_extraPhysics.end())
        Field<uint32_t>(object, kObjectHideMask) = 0;
}

// What the equip-screen builder loaded for the extras into its current
// buffer: the part holders after the equipped's (`equippedHolders` of them)
// and the slots each one stacks. Filled by BuilderLoaderHook, used by
// BuilderApplyHook; `stackedParts` = what that apply registered at keys 10+.
struct BuilderExtraHolder { void *holder; ExtraToShow extra; };
struct BuilderExtras {
    int buffer = -1;
    size_t equippedHolders = 0;
    std::vector<BuilderExtraHolder> holders;
    void *chara = nullptr;
    std::vector<char *> stackedParts;
};
std::map<void *, BuilderExtras> g_builders; // by builder
std::mutex g_buildersLock;
volatile LONG g_builderLogLines = 0;
constexpr LONG kMaxBuilderLogLines = 80; // a few lines per hovered accessory
bool BuilderLogLine() { return g_logging && InterlockedIncrement(&g_builderLogLines) <= kMaxBuilderLogLines; }

// Equip screen preview: after the game loads the equipped accessory, load
// each extra into the same buffer (undo the loader's buffer flip and re-arm
// it per call); BuilderApplyHook then stacks their replacer parts.
int __fastcall BuilderLoaderHook(void *builder, int partSet)
{
    auto loader = reinterpret_cast<BuilderLoaderFn>(g_exe + kBuilderLoaderRva);
    int bufferBefore = Field<int>(builder, kBuilderBuffer);
    int result = loader(builder, partSet);
    int buffer = Field<int>(builder, kBuilderBuffer);
    if (buffer == bufferBefore) return result; // the loader bailed out (busy or no BCS)
    char *resource = Field<char *>(builder, kBuilderResource);
    // The builder only ever shows the current CaC (equip screen: preset 0;
    // Presets panel: the preset it just built).
    int slot = CurrentSlot();
    std::vector<int> picked = PickedExtras(slot, SlotPreset(slot));
    // the accessory list is open for an extra column: show the hovered one there
    int column = g_pickColumn, preview = g_previewId;
    if (column > 0 && preview != kNoPreview) picked = WithColumn(picked, column, preview);
    // the buffer's part holders, a vector of pointers; the equipped's (0 or 1) is first
    char *holders = static_cast<char *>(builder) + kBuilderHolders + buffer * kBuilderBufferStride;
    auto holderCount = [holders] { return static_cast<size_t>(Field<void **>(holders, 8) - Field<void **>(holders, 0)); };
    BuilderExtras loaded;
    loaded.buffer = buffer;
    loaded.equippedHolders = holderCount();
    for (const ExtraToShow &extra : ExtrasToShow(resource ? Field<char *>(resource, kResBcs) : nullptr, partSet, picked)) {
        int state = Field<int>(builder, kBuilderLoadState);
        size_t holdersBefore = holderCount();
        Field<int>(builder, kBuilderBuffer) = 1 - buffer;
        Field<int>(builder, kBuilderLoadState) = 0;
        t_builderLoadsExtra = true;
        result |= loader(builder, extra.id);
        t_builderLoadsExtra = false;
        Field<int>(builder, kBuilderBuffer) = buffer;
        if (Field<int>(builder, kBuilderLoadState) == 0) Field<int>(builder, kBuilderLoadState) = state;
        // a part set with replacer parts gets one holder (objects-only ones none)
        for (size_t i = holdersBefore; i < holderCount(); i++)
            loaded.holders.push_back({Field<void **>(holders, 0)[i], extra});
        Log("equip screen: extra %d with equipped %d (slot %d preset %d)%s", extra.id, partSet, slot + 1, SlotPreset(slot),
            holderCount() > holdersBefore ? ", has a part holder" : "");
    }
    std::lock_guard<std::mutex> lock(g_buildersLock);
    BuilderExtras &entry = g_builders[builder];
    loaded.chara = entry.chara;
    loaded.stackedParts = std::move(entry.stackedParts); // still registered until the next apply
    entry = std::move(loaded);
    return result;
}

// Adds a part set to a resource's preload list (once), so 0x299ac0 builds,
// loads and later frees a holder for it in the resource's map.
void PreloadPartSet(void *resource, int partSet)
{
    int *&last = Field<int *>(resource, kResPreloadIds + 8);
    int *first = Field<int *>(resource, kResPreloadIds), *end = Field<int *>(resource, kResPreloadIds + 0x10);
    if (std::find(first, last, partSet) != last) return;
    if (last != end) *last++ = partSet;
    else reinterpret_cast<VectorInsertIntFn>(g_exe + kVectorInsertIntRva)(static_cast<char *>(resource) + kResPreloadIds, last, &partSet);
}

// The holder of a part set in a resource's map, null if none.
void *MapHolder(void *resource, int partSet)
{
    char *map = static_cast<char *>(resource) + kResPartSetMap;
    char *node = nullptr;
    reinterpret_cast<MapFindFn>(g_exe + kMapFindRva)(map, &node, &partSet);
    return node && node != Field<char *>(resource, kMapEndNode) ? Field<void *>(node, kNodeHolder) : nullptr;
}

// A resource's current skeleton, null before set-up.
void *ResourceSkeleton(void *resource)
{
    int index = Field<int>(resource, kResSkeletonIndex);
    return Field<void *>(resource, index < 0 ? kResSkeletonNoIndex : kResSkeletons + index * 8);
}

// One stacked extra's replacer part (from its preloaded holder) with its model.
struct StackedPart { int extraId, partSlot; char *part; void *model; };

// The stacked extras' parts of a CaC resource, in key order (10, 11, ...);
// parts without a model are logged and left out when `logMissing`.
std::vector<StackedPart> StackedParts(void *resource, char *record, const std::vector<int> &picked, bool logMissing)
{
    std::vector<StackedPart> parts;
    char *bcs = Field<char *>(resource, kResBcs);
    for (const ExtraToShow &extra : ExtrasToShow(bcs, Field<int16_t>(record, kRecordAccessory), picked)) {
        if (!extra.stackedSlots) continue;
        void *holder = MapHolder(resource, extra.id);
        for (int partSlot = 0; partSlot < kMaxPartSlots; partSlot++) {
            if (!(extra.stackedSlots & (1u << partSlot))) continue;
            char *part = holder ? reinterpret_cast<HolderPartFn>(g_exe + kHolderPartRva)(holder, partSlot) : nullptr;
            void *model = part ? reinterpret_cast<PartModelFn>(g_exe + kPartModelRva)(part) : nullptr;
            if (model) parts.push_back({extra.id, partSlot, part, model});
            else if (logMissing)
                Log("  extra %d part slot %d: no %s, not stacked", extra.id, partSlot, !holder ? "holder" : !part ? "part" : "model");
        }
    }
    return parts;
}

// The part a slot itself shows (the equipped wig is merged into the main holder).
char *ShownPart(void *resource, int partSlot)
{
    void *mainHolder = MapHolder(resource, Field<int>(resource, kResMainPartSet));
    return mainHolder ? reinterpret_cast<HolderPartFn>(g_exe + kHolderPartRva)(mainHolder, partSlot) : nullptr;
}

void ActivatePart(char *part)
{
    using ActivateFn = void(__fastcall *)(void *part);
    (*reinterpret_cast<ActivateFn *>(*reinterpret_cast<char **>(part) + kPartActivateSlot))(part);
}

// Points the stacked part's light and scene callbacks at `shown`'s, which
// Chara keeps up to date (see kPartCallbacksBegin). False if either list
// is too short.
bool ShareCharaCallbacks(char *stacked, char *shown)
{
    if (!stacked || !shown || stacked == shown) return false;
    auto entries = [](char *part, size_t *count) {
        char *begin = Field<char *>(part, kPartCallbacksBegin);
        *count = begin ? (Field<char *>(part, kPartCallbacksEnd) - begin) / kPartCallbackEntrySize : 0;
        return begin;
    };
    size_t stackedCount = 0, shownCount = 0;
    char *to = entries(stacked, &stackedCount), *from = entries(shown, &shownCount);
    if (stackedCount <= kSceneCallback || shownCount <= kSceneCallback) return false;
    for (size_t index : {kLightCallback, kSceneCallback})
        Field<void *>(to, index * kPartCallbackEntrySize) = Field<void *>(from, index * kPartCallbackEntrySize);
    return true;
}

// After the game sets up a resource's skeleton: register the stacked
// extras' replacer parts at keys 10+, so they draw skinned to the body next
// to the part the slot shows. They are activated with the game's own parts
// (ActivatePartSetHook), or here if the game already did its pass.
int __fastcall ResSkeletonHook(void *resource)
{
    int result = reinterpret_cast<ResSkeletonFn>(g_exe + kResSkeletonRva)(resource);
    char *record = nullptr;
    int slot = -1;
    std::vector<int> picked;
    if (!ResourceExtras(resource, &record, &slot, &picked)) return result;
    void *skeleton = ResourceSkeleton(resource);
    if (!skeleton) return result;
    auto registerModel = *reinterpret_cast<RegisterModelFn *>(g_exe + kRegisterModelImportRva);
    unsigned key = kFirstStackedKey;
    for (const StackedPart &stacked : StackedParts(resource, record, picked, true)) {
        int registered = registerModel(skeleton, stacked.model, key, 0);
        char *shown = ShownPart(resource, stacked.partSlot);
        bool gameActivated = shown && Field<void *>(shown, kPartToonUnit);
        Log("\"%.64s\" (record %d): extra %d part slot %d stacked at key %u (RegisterModel %d)%s", RecordName(record),
            Field<int>(resource, kResRecordId), stacked.extraId, stacked.partSlot, key, registered,
            gameActivated ? ", slot already activated - activating now" : "");
        if (registered && gameActivated) {
            ActivatePart(stacked.part);
            ShareCharaCallbacks(stacked.part, shown);
        }
        key++;
    }
    return result;
}

// Chara SetPartSet's activation of a part set's parts (0x297910, returns 1
// when the set was found): then activate the stacked parts, so their ramp
// units are built at the same point as the equipped wig's.
int __fastcall ActivatePartSetHook(void *resource, int partSet, void *charaParts)
{
    using ActivatePartSetFn = int(__fastcall *)(void *, int, void *);
    int result = reinterpret_cast<ActivatePartSetFn>(g_exe + kActivatePartSetRva)(resource, partSet, charaParts);
    char *record = nullptr;
    int slot = -1;
    std::vector<int> picked;
    if (!result || !ResourceExtras(resource, &record, &slot, &picked) || !ResourceSkeleton(resource)) return result;
    char **shownParts = static_cast<char **>(charaParts); // chara +0x370, just filled by the game
    for (const StackedPart &stacked : StackedParts(resource, record, picked, false)) {
        ActivatePart(stacked.part);
        char *shown = shownParts && shownParts[stacked.partSlot] ? shownParts[stacked.partSlot] : ShownPart(resource, stacked.partSlot);
        bool shared = ShareCharaCallbacks(stacked.part, shown);
        Log("\"%.64s\" (record %d): part set %d activated, extra %d part slot %d too, light/scene callbacks %s",
            RecordName(record), Field<int>(resource, kResRecordId), partSet, stacked.extraId, stacked.partSlot,
            shared ? "shared with the shown part" : "NOT shared (no shown part or short list)");
    }
    return result;
}

using CharaColourFn = void(__fastcall *)(char *chara, const char *materialName, const float *colour, int arg);
CharaColourFn g_charaColour = nullptr; // Chara vt[57] before our hook
volatile LONG g_colourLogLines = 0;
constexpr LONG kMaxColourLogLines = 40;

// Runs the game's colour pass on `parts` only: they go into the Chara's part
// array (the physics list emptied so those aren't coloured twice), then both
// are put back. Parts without a ramp unit yet are skipped.
void ColourParts(char *chara, const std::vector<char *> &parts, const char *materialName, const float *colour, int arg)
{
    void **array = reinterpret_cast<void **>(chara + kCharaParts); // parts, then physics objects
    void *saved[kCharaPartCount + kCharaPhysicsCount];
    memcpy(saved, array, sizeof(saved));
    for (size_t first = 0; first < parts.size(); first += kCharaPartCount) {
        memset(array, 0, sizeof(saved));
        for (size_t i = first; i < parts.size() && i < first + kCharaPartCount; i++)
            if (Field<void *>(parts[i], kPartToonUnit)) array[i - first] = parts[i];
        g_charaColour(chara, materialName, colour, arg);
    }
    memcpy(array, saved, sizeof(saved));
}

// The equip-screen builder's stacked parts on `chara` (BuilderApplyHook);
// false if `chara` isn't a builder's.
bool BuilderStackedParts(char *chara, std::vector<char *> *parts);

// The last colour of each material name per Chara, so parts the builder
// stacks after the game's colour pass get the CaC colours too (the pass runs
// when the CaC is set up, not after each apply, and a CaC may have no extras
// then). Every Chara is kept until the cache is full, then all but the
// builders' are dropped; a builder's entry goes with it (BuilderTeardownHook).
struct CachedColour { std::string materialName; float colour[4]; int arg; };
std::map<char *, std::vector<CachedColour>> g_colourCache;
std::mutex g_colourCacheLock; // taken before g_buildersLock, never the other way round
constexpr size_t kMaxColourCacheCharas = 16;

void CacheColour(char *chara, const char *materialName, const float *colour, int arg)
{
    if (!materialName || !colour) return;
    std::lock_guard<std::mutex> lock(g_colourCacheLock);
    if (g_colourCache.size() >= kMaxColourCacheCharas && !g_colourCache.count(chara)) {
        std::vector<char *> keep;
        {
            std::lock_guard<std::mutex> buildersLock(g_buildersLock);
            for (const auto &builder : g_builders) keep.push_back(static_cast<char *>(builder.second.chara));
        }
        for (auto it = g_colourCache.begin(); it != g_colourCache.end();)
            it = std::find(keep.begin(), keep.end(), it->first) == keep.end() ? g_colourCache.erase(it) : std::next(it);
    }
    std::vector<CachedColour> &colours = g_colourCache[chara];
    CachedColour entry{materialName, {colour[0], colour[1], colour[2], colour[3]}, arg};
    for (CachedColour &known : colours)
        if (known.materialName == entry.materialName && known.arg == arg) { known = entry; return; }
    colours.push_back(entry);
}

void ReplayColours(char *chara, const std::vector<char *> &parts)
{
    std::vector<CachedColour> colours;
    {
        std::lock_guard<std::mutex> lock(g_colourCacheLock);
        auto found = g_colourCache.find(chara);
        if (found != g_colourCache.end()) colours = found->second;
    }
    for (const CachedColour &cached : colours) ColourParts(chara, parts, cached.materialName.c_str(), cached.colour, cached.arg);
    if (BuilderLogLine())
        Log("equip screen: %zu cached colour(s) applied to %zu stacked part(s)", colours.size(), parts.size());
}

void ForgetColours(char *chara)
{
    std::lock_guard<std::mutex> lock(g_colourCacheLock);
    g_colourCache.erase(chara);
}

// Chara vt[57], the CaC colour pass (kCharaColourSlot): after the game's
// pass, run it again on the stacked parts - the builder's for its Chara
// (equip screen), else the resource path's.
void __fastcall CharaColourHook(char *chara, const char *materialName, const float *colour, int arg)
{
    g_charaColour(chara, materialName, colour, arg);
    std::vector<char *> parts;
    if (BuilderStackedParts(chara, &parts)) {
        CacheColour(chara, materialName, colour, arg);
        if (parts.empty()) return;
        ColourParts(chara, parts, materialName, colour, arg);
        if (g_logging && InterlockedIncrement(&g_colourLogLines) <= kMaxColourLogLines)
            Log("equip screen: colour '%.32s' applied to %zu stacked part(s)", materialName ? materialName : "", parts.size());
        return;
    }
    CacheColour(chara, materialName, colour, arg); // the equip screen's builder may stack on it later
    void *resource = Field<void *>(chara, kCharaResource);
    char *record = nullptr;
    int slot = -1;
    std::vector<int> picked;
    if (!resource || !ResourceExtras(resource, &record, &slot, &picked) || !ResourceSkeleton(resource)) return;
    std::vector<StackedPart> stacked = StackedParts(resource, record, picked, false);
    if (stacked.empty()) return;
    for (const StackedPart &part : stacked) parts.push_back(part.part);
    ColourParts(chara, parts, materialName, colour, arg);
    if (g_logging && InterlockedIncrement(&g_colourLogLines) <= kMaxColourLogLines)
        Log("\"%.64s\" (record %d): colour '%.32s' applied to %zu stacked part(s)", RecordName(record),
            Field<int>(resource, kResRecordId), materialName ? materialName : "", stacked.size());
}

bool BuilderStackedParts(char *chara, std::vector<char *> *parts)
{
    std::lock_guard<std::mutex> lock(g_buildersLock);
    for (const auto &builder : g_builders)
        if (builder.second.chara == chara) {
            *parts = builder.second.stackedParts;
            return true;
        }
    return false;
}

// Apply 0x65e290 attaches every holder of the buffer slot by slot, and a
// later holder takes the slot (retest 18: an extra's hat replaced the
// Devil-man head; retest 19's "equipped last" left the extra's wig unseen).
// The builder's Chara can't stack either, so the extras' holders are hidden
// from it (vector end moved back to the equipped's), and their replacer parts
// registered at keys 10+ on the Chara's skeleton after it, like the resource
// path does (ResSkeletonHook). Keys 10+ are emptied first: the apply frees
// the other buffer's holders, whose parts the previous apply registered, and
// the resource path's stacked wig from screen entry (it never changed with
// the picks).
int __fastcall BuilderApplyHook(void *builder)
{
    auto apply = reinterpret_cast<BuilderFn>(g_exe + kBuilderApplyRva);
    // the apply's own bail-outs (see kBuilderApplyRva): nothing is attached
    // or freed then, so nothing of ours changes either
    if (!Field<int>(builder, kBuilderApplyEnabled)) return apply(builder);
    if (!reinterpret_cast<BuilderFn>(g_exe + kBuilderReadyRva)(builder)) return 0;
    char *chara = Field<char *>(builder, kBuilderChara);
    void *skeleton = chara ? Field<void *>(chara, kCharaSkeleton) : nullptr;
    if (skeleton) {
        auto unregisterModel = *reinterpret_cast<UnregisterModelFn *>(g_exe + kUnregisterModelImportRva);
        for (unsigned key = kFirstStackedKey; key < kFirstStackedKey + kMaxExtras * kCharaPartCount; key++)
            unregisterModel(skeleton, key);
    }
    int buffer = Field<int>(builder, kBuilderBuffer);
    char *holders = static_cast<char *>(builder) + kBuilderHolders + buffer * kBuilderBufferStride;
    void **&holdersEnd = Field<void **>(holders, 8);
    void **holdersBegin = Field<void **>(holders, 0);
    void **fullEnd = holdersEnd;
    std::vector<BuilderExtraHolder> extraHolders;
    bool hide = false;
    {
        // not held over the apply: the game's colour pass (CharaColourHook) takes it
        std::lock_guard<std::mutex> lock(g_buildersLock);
        BuilderExtras &entry = g_builders[builder];
        entry.stackedParts.clear(); // unregistered above, freed by this apply
        entry.chara = chara;
        // only when the vector still holds exactly what BuilderLoaderHook saw
        hide = entry.buffer == buffer && !entry.holders.empty() &&
               static_cast<size_t>(fullEnd - holdersBegin) == entry.equippedHolders + entry.holders.size();
        for (size_t i = 0; hide && i < entry.holders.size(); i++)
            hide = holdersBegin[entry.equippedHolders + i] == entry.holders[i].holder;
        if (hide) holdersEnd = holdersBegin + entry.equippedHolders;
        if (!hide && !entry.holders.empty() && BuilderLogLine())
            Log("equip screen: apply with %zu extra holder(s) not stacked (holder list changed since the load)",
                entry.holders.size());
        if (hide) extraHolders = entry.holders;
    }
    int result = apply(builder);
    if (!hide) return result;
    if (Field<void **>(holders, 0) == holdersBegin) holdersEnd = fullEnd; // the next apply frees them with the rest
    if (!skeleton) return result;
    void *resource = Field<void *>(chara, kCharaResource);
    void *resourceSkeleton = resource ? ResourceSkeleton(resource) : nullptr;
    auto registerModel = *reinterpret_cast<RegisterModelFn *>(g_exe + kRegisterModelImportRva);
    char **shownParts = reinterpret_cast<char **>(chara + kCharaParts);
    std::vector<char *> stackedParts;
    unsigned key = kFirstStackedKey;
    for (const BuilderExtraHolder &held : extraHolders) {
        for (int partSlot = 0; partSlot < static_cast<int>(kCharaPartCount); partSlot++) {
            if (!(held.extra.stackedSlots & (1u << partSlot))) continue;
            char *part = reinterpret_cast<HolderPartFn>(g_exe + kHolderPartRva)(held.holder, partSlot);
            if (!part) continue;
            ActivatePart(part); // as the attach does before registering (0x27eb5c)
            void *model = reinterpret_cast<PartModelFn>(g_exe + kPartModelRva)(part);
            int registered = model ? registerModel(skeleton, model, key, 0) : 0;
            // the slot's shown part, or any the Chara shows (same light and draw buffer)
            char *shown = shownParts[partSlot];
            for (size_t i = 0; !shown && i < kCharaPartCount; i++) shown = shownParts[i];
            bool shared = ShareCharaCallbacks(part, shown);
            if (BuilderLogLine())
                Log("equip screen: extra %d part slot %d stacked at key %u (RegisterModel %d), callbacks %s%s", held.extra.id,
                    partSlot, key, registered, shared ? "shared" : "NOT shared",
                    resourceSkeleton == skeleton ? "" : " (Chara skeleton isn't its resource's)");
            if (registered) {
                stackedParts.push_back(part);
                key++;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_buildersLock);
        auto found = g_builders.find(builder);
        if (found != g_builders.end()) found->second.stackedParts = stackedParts;
    }
    if (!stackedParts.empty()) ReplayColours(chara, stackedParts); // CaC colours the game set before this apply
    return result;
}

// The builder goes away (its Chara and holders are deleted next).
void __fastcall BuilderTeardownHook(void *builder)
{
    char *chara = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_buildersLock);
        auto found = g_builders.find(builder);
        if (found != g_builders.end()) {
            chara = static_cast<char *>(found->second.chara);
            g_builders.erase(found);
        }
    }
    if (chara) ForgetColours(chara);
    reinterpret_cast<BuilderTeardownFn>(g_exe + kBuilderTeardownRva)(builder);
}

// The extras' physics objects (attached objects, plus a hat's own physics),
// loaded like the game's: 0x29a4d0 with the extra swapped into the record,
// then the part set's own physics list.
void __fastcall ResPartPhysicsHook(void *resource, void *entries, void *holder, int argD8, int argDc)
{
    auto partPhysics = reinterpret_cast<ResPartPhysicsFn>(g_exe + kResPartPhysicsRva);
    partPhysics(resource, entries, holder, argD8, argDc);
    t_extraPhysics.clear(); // a new holder build: the last one's objects are all added
    char *record = nullptr;
    int slot = -1;
    std::vector<int> picked;
    if (!ResourceExtras(resource, &record, &slot, &picked)) return;
    char *bcs = Field<char *>(resource, kResBcs);
    int16_t equipped = Field<int16_t>(record, kRecordAccessory);
    auto addPhysics = reinterpret_cast<AddPhysicsObjectFn>(g_exe + kAddPhysicsObjectRva);
    // entries: std::vector {first, last, end}; the game adds their objects after we return
    auto entryCount = [entries] {
        return (Field<char *>(entries, 8) - Field<char *>(entries, 0)) / kPhysicsEntrySize;
    };
    for (const ExtraToShow &extra : ExtrasToShow(bcs, equipped, picked)) {
        char *data = PartSetData(bcs, extra.id);
        if (extra.stackedSlots) {
            PreloadPartSet(resource, extra.id);
            Log("  extra %d stacks on part slots 0x%x: preloaded", extra.id, extra.stackedSlots);
        }
        size_t firstNew = entryCount();
        Field<int16_t>(record, kRecordAccessory) = static_cast<int16_t>(extra.id);
        partPhysics(resource, entries, holder, argD8, argDc);
        Field<int16_t>(record, kRecordAccessory) = equipped;
        for (size_t i = firstNew; i < entryCount(); i++)
            t_extraPhysics.push_back(Field<void *>(Field<char *>(entries, 0), i * kPhysicsEntrySize));
        for (int i = 0; i < Field<uint16_t>(data, 0x16); i++) {
            uint32_t offset = Field<uint32_t>(data, 0x1c) + i * kPhysicsSize;
            if (!Field<uint32_t>(data, 0x1c)) continue;
            t_extraPhysics.push_back(data + offset);
            addPhysics(holder, data + offset, -1, argD8, nullptr, nullptr, argDc);
        }
        Log("\"%.64s\" (%s, record %d): extra %d with equipped %d", RecordName(record), OwnerText(slot).c_str(),
            Field<int>(resource, kResRecordId), extra.id, equipped);
    }
}

// Online send: after the game packs one of our CaCs into a profile, put its
// extras into the appearance block's unused bytes (see kChannelFirst).
void *__fastcall ProfilePackHook(char *record, char *profile)
{
    void *result = reinterpret_cast<ProfilePackFn>(g_exe + kProfilePackRva)(record, profile);
    if (!record || !profile) return result;
    char *block = profile + kProfileAppearance;
    uint32_t firstBefore = Field<uint32_t>(block, kChannelFirst);
    uint16_t secondBefore = Field<uint16_t>(block, kChannelSecond);
    int slot = SlotOfRecord(record);
    if (slot < 0) return result;
    std::vector<int> picked = PickedExtras(slot, SlotPreset(slot));
    uint16_t ids[kMaxExtras] = {kChannelNone, kChannelNone};
    for (size_t i = 0; i < kMaxExtras && i < picked.size(); i++)
        if (picked[i] >= 0 && picked[i] < kChannelNone) ids[i] = static_cast<uint16_t>(picked[i]);
    Field<uint32_t>(block, kChannelFirst) = kChannelMagic | static_cast<uint32_t>(ids[0]) << 16;
    Field<uint16_t>(block, kChannelSecond) = ids[1];
    uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - g_exe - 5;
    bool sendSeen;
    {
        std::lock_guard<std::mutex> lock(g_remoteLock);
        sendSeen = !g_sendsSeen.insert({caller, ids[0], ids[1]}).second;
    }
    if (!sendSeen && OnlineLogLine(g_sendLogLines))
        Log("online send (call 0x%llx): \"%.64s\" slot %d preset %d, extras %d %d (bytes before %08x %04x)",
            static_cast<unsigned long long>(caller), RecordName(record),
            slot + 1, SlotPreset(slot), ids[0] == kChannelNone ? -1 : ids[0], ids[1] == kChannelNone ? -1 : ids[1],
            firstBefore, secondBefore);
    return result;
}

// Online receive: after the game unpacks a member's profile into a record,
// remember the extras it carries (none or no magic: forget the player's).
void *__fastcall MemberUnpackHook(char *member, char *record)
{
    uint32_t first = member ? Field<uint32_t>(member + kMemberAppearance, kChannelFirst) : 0;
    uint16_t second = member ? Field<uint16_t>(member + kMemberAppearance, kChannelSecond) : 0;
    void *result = reinterpret_cast<MemberUnpackFn>(g_exe + kMemberUnpackRva)(member, record);
    if (!member || !record) return result;
    bool tagged = (first & 0xffff) == kChannelMagic;
    std::vector<int> ids;
    for (uint16_t id : {static_cast<uint16_t>(first >> 16), second})
        if (tagged && id != kChannelNone) ids.push_back(id);
    RemoteKey key = KeyOfRecord(record);
    if (std::get<0>(key).empty()) return result;
    bool changed;
    {
        std::lock_guard<std::mutex> lock(g_remoteLock);
        auto found = g_remoteExtras.find(key);
        if (ids.empty()) {
            changed = found != g_remoteExtras.end();
            if (changed) g_remoteExtras.erase(found);
        } else {
            changed = found == g_remoteExtras.end() || found->second != ids;
            g_remoteExtras[key] = ids;
        }
        g_anyRemote = !g_remoteExtras.empty() || !g_remoteByLook.empty();
    }
    // Diagnostic: the first unpack of each caller + name is logged even when
    // untagged, so a test tells "never unpacked" from "bytes zeroed in transit".
    uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - g_exe - 5;
    bool firstSeen;
    {
        std::lock_guard<std::mutex> lock(g_remoteLock);
        firstSeen = !IsLobbyNpcName(std::get<0>(key)) && g_unpacksSeen.insert({caller, std::get<0>(key)}).second;
    }
    if ((changed || first || second || firstSeen) && OnlineLogLine(g_receiveLogLines))
        Log("online receive (call 0x%llx): \"%.64s\" (values %d %d) bytes %08x %04x -> %s%s",
            static_cast<unsigned long long>(caller), std::get<0>(key).c_str(), std::get<1>(key), std::get<2>(key), first, second,
            !tagged ? "not tagged" : ids.empty() ? "no extras" : ids.size() == 1 ? "1 extra" : "2 extras",
            changed ? " (changed)" : "");
    return result;
}

// Online receive: an appearance block copy whose source carries our tag
// (see kBlockCopyRva). The block is converted into a scratch record the way
// the game will convert it, and its extras kept under that record's look
// (none sent: forgotten). Untagged blocks (vanilla players, NPCs) change
// nothing. Our own sent blocks land here too, harmlessly: our CaCs' records
// are matched by save slot first.
bool IsTaggedBlock(const char *block)
{
    return (Field<uint32_t>(const_cast<char *>(block), kChannelFirst) & 0xffff) == kChannelMagic;
}

// Keeps a tagged block's extras under the look of `record` (the block
// converted by 0x655080).
void RememberTaggedBlock(const char *block, const char *record, uintptr_t caller, const char *what)
{
    uint32_t first = Field<uint32_t>(const_cast<char *>(block), kChannelFirst);
    uint16_t second = Field<uint16_t>(const_cast<char *>(block), kChannelSecond);
    std::vector<int> ids;
    for (uint16_t id : {static_cast<uint16_t>(first >> 16), second})
        if (id != kChannelNone) ids.push_back(id);
    std::string look = RecordLook(record);
    bool changed, firstSeen;
    {
        std::lock_guard<std::mutex> lock(g_remoteLock);
        auto found = g_remoteByLook.find(look);
        if (ids.empty()) {
            changed = found != g_remoteByLook.end();
            if (changed) g_remoteByLook.erase(found);
        } else {
            changed = found == g_remoteByLook.end() || found->second != ids;
            g_remoteByLook[look] = ids;
        }
        // a change to a look that came before (with or without extras): its
        // avatar may be drawn already and needs a reload; a first one doesn't
        if (g_seenLooks.size() >= kMaxSeenLooks) g_seenLooks.clear();
        bool seenBefore = !g_seenLooks.insert(look).second;
        if (changed && seenBefore) {
            if (g_staleLooks.size() >= kMaxStaleLooks) g_staleLooks.clear();
            g_staleLooks.insert(look);
            g_anyStaleLook = true;
        }
        g_anyRemote = !g_remoteExtras.empty() || !g_remoteByLook.empty();
        firstSeen = g_blockCopiesSeen.insert({caller, first, second}).second;
    }
    if ((changed || firstSeen) && OnlineLogLine(g_blockCopyLogLines))
        Log("online %s (call 0x%llx): extras %d %d for look (values %d %d)%s", what,
            static_cast<unsigned long long>(caller), ids.size() > 0 ? ids[0] : -1, ids.size() > 1 ? ids[1] : -1,
            Field<int32_t>(const_cast<char *>(record), kRecordValueA),
            Field<int32_t>(const_cast<char *>(record), kRecordValueB), changed ? " (changed)" : "");
}

char *__fastcall BlockCopyHook(char *dst, const char *src)
{
    char *result = reinterpret_cast<BlockCopyFn>(g_exe + kBlockCopyRva)(dst, src);
    if (!src || !IsTaggedBlock(src)) return result;
    alignas(16) char scratch[kScratchRecordSize] = {};
    reinterpret_cast<BlockToRecordFn>(g_exe + kBlockToRecordRva)(src, scratch);
    RememberTaggedBlock(src, scratch, reinterpret_cast<uintptr_t>(_ReturnAddress()) - g_exe - 5, "block copy");
    return result;
}

// Online receive: a block converted into a record in place (see
// kBlockToRecordCallRvas).
void __fastcall BlockToRecordHook(const char *block, char *record)
{
    reinterpret_cast<BlockToRecordFn>(g_exe + kBlockToRecordRva)(block, record);
    if (!block || !record || !IsTaggedBlock(block)) return;
    RememberTaggedBlock(block, record, reinterpret_cast<uintptr_t>(_ReturnAddress()) - g_exe - 5, "block convert");
}

// Every loadout build: remember which preset the save slot's loadout comes
// from, before the build, so the loads it starts see it.
void *__fastcall BuildLoadoutHook(void *character, void *out, int preset)
{
    char *manager = SaveManager();
    char *firstCac = manager ? manager + kSaveSlots : nullptr;
    char *cac = static_cast<char *>(character);
    if (firstCac && cac >= firstCac && (cac - firstCac) % kSaveSlotSize == 0 && (cac - firstCac) / kSaveSlotSize < kSlotCount) {
        int slot = static_cast<int>((cac - firstCac) / kSaveSlotSize);
        int record = preset < 0 || preset >= kPresetCount ? kMainPreset : preset;
        if (InterlockedExchange(&g_slotPreset[slot], record) != record) Log("slot %d: loadout from preset %d", slot + 1, record);
    }
    return reinterpret_cast<BuildLoadoutFn>(g_exe + kBuildLoadoutRva)(character, out, preset);
}

// Presets panel: after the game loads or saves a preset, copy the extras.
void __fastcall MysetCallbackHook(void *menu, const uint8_t *call)
{
    g_mysetCallback(menu, call);
    void *args = const_cast<uint8_t *>(call);
    if (Field<int>(args, kCallArgCount) < 2) return;
    int sendType = static_cast<int>(Field<double>(args, kCallArg0));
    int preset = static_cast<int>(Field<double>(args, kCallArg1));
    if (sendType == kMysetLoad) CopyPreset(preset, kMainPreset);
    else if (sendType == kMysetSave) CopyPreset(kMainPreset, preset);
}

// Chains a vtable slot (.rdata: no DRM wait). Another mod may have hooked it
// already; we call whatever was there.
template <typename Fn>
bool ChainVtableSlot(uintptr_t vtableRva, size_t slotIndex, Fn hook, Fn &previousOut)
{
    auto **slot = reinterpret_cast<void **>(g_exe + vtableRva) + slotIndex;
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtect)) return false;
    void *previous;
    do {
        previous = *slot;
        previousOut = reinterpret_cast<Fn>(previous);
    } while (InterlockedCompareExchangePointer(slot, reinterpret_cast<void *>(hook), previous) != previous);
    VirtualProtect(slot, sizeof(*slot), oldProtect, &oldProtect);
    return true;
}

// A menu's AS callback slot (Battle Styles may have hooked it already).
bool ChainUserCallback(uintptr_t vtableRva, UserCallbackFn hook, UserCallbackFn &previousOut)
{
    return ChainVtableSlot(vtableRva, kUserCallbackSlot, hook, previousOut);
}

// --- Equip screen row -----------------------------------------------------------
// The cloth tab's Accessory row has three columns (as3\Avatar.as): the
// equipped accessory, extra 1, extra 2. An extra column opens the game's own
// accessory list (request 210 with the column as its argument, vanilla -1);
// while it is open the equipped accessory stays put and the hovered or
// picked id goes to the column instead.

int MenuAccessoryCount(void *menu)
{
    return static_cast<int>(Field<int *>(menu, kMenuAccessoryIds + 8) - Field<int *>(menu, kMenuAccessoryIds));
}

int MenuAccessoryId(void *menu, int index)
{
    return index >= 0 && index < MenuAccessoryCount(menu) ? Field<int *>(menu, kMenuAccessoryIds)[index] : -1;
}

// The part set an accessory list entry (IDB item id) draws; -1 = none.
int AccessoryPartSet(int itemId)
{
    return itemId < 0 ? -1 : reinterpret_cast<ItemPartSetFn>(g_exe + kItemPartSetRva)(kItemTypeAccessory, itemId);
}

// The list entry that draws a part set (-1 = the "none" entry), or -1.
int MenuAccessoryIndexOf(void *menu, int partSet)
{
    const int count = MenuAccessoryCount(menu);
    for (int i = 0; i < count; i++)
        if (AccessoryPartSet(MenuAccessoryId(menu, i)) == partSet) return i;
    return -1;
}

// Avatar.as's layout report (maDiag): sendType 9000 + field, value * 100.
// Ours alone; never passed to the game.
constexpr int kDiagSendType = 9000, kDiagFields = 24;

void __fastcall EditCharaCallbackHook(void *menu, const uint8_t *call)
{
    auto *args = const_cast<uint8_t *>(call);
    if (Field<int>(args, kCallArgCount) >= 2) {
        int field = static_cast<int>(Field<double>(args, kCallArg0)) - kDiagSendType;
        if (field >= 0 && field < kDiagFields) {
            static const char *const names[kDiagFields] = {
                "row", "main x", "main y", "main scaleX", "bounds x", "bounds width", "bounds height", "main alpha",
                "extra x", "extra scaleX", "extra bounds width", "extra alpha", "narrowed (-1 = left vanilla): main scaleX",
                "main x", "extra x", "extra scaleX",
                // maFollowRows' last frame (it sets the row's y from rows 0/1)
                "later: main y", "later: row 0 y", "later: row 1 y", "later: row above y", "later: row below y",
                "later: main alpha", "later: extra x", "later: extra alpha"};
            Log("equip screen layout: %s = %.2f", names[field], Field<double>(args, kCallArg1) / 100.0);
            return;
        }
    }
    if (Field<int>(args, kCallArgCount) >= 2 && static_cast<int>(Field<double>(args, kCallArg0)) == kListAccessory) {
        int column = static_cast<int>(Field<double>(args, kCallArg1));
        g_previewId = kNoPreview;
        if (column == 1 || column == 2) {
            int &index = Field<int>(menu, kMenuAccessoryIndex);
            g_pickMainIndex = index;
            g_pickMain = MenuAccessoryId(menu, index);
            g_pickColumn = column;
            Field<double>(args, kCallArg1) = -1.0; // the game opens the list as for the equipped one
            // The list opens on (and ticks) the row's index (0x4fd2f9): point
            // it at the column's pick; ListCloseHook puts it back.
            std::vector<int> picked = PickedExtras(CurrentSlot(), kMainPreset);
            int pick = MenuAccessoryIndexOf(menu, column <= static_cast<int>(picked.size()) ? picked[column - 1] : -1);
            if (pick >= 0) index = pick;
            Log("equip screen: accessory list for extra %d (equipped item %d, opens on entry %d)", column,
                g_pickMain.load(), index);
        } else {
            g_pickColumn = 0;
        }
    }
    g_editCharaCallback(menu, call);
}

void RoomPushAfterPick();

// The list's hover / pick. Picking for a column: a hover goes to the game
// as it is (its item info follows the cursor; ModelRequestHook keeps the
// equipped accessory on the model), a decide gives the game the equipped id
// back (so the equipped accessory and its row stay as they are) and the
// column takes the value; picking the equipped one changes nothing. `value`
// is an item id; the picks are part sets.
void __fastcall ListApplyHook(void *menu, int value, int decided)
{
    auto apply = reinterpret_cast<ListApplyFn>(g_exe + kListApplyRva);
    int column = g_pickColumn;
    if (column == 0 || Field<int>(menu, kMenuListType) != kListAccessory) return apply(menu, value, decided);
    int main = g_pickMain;
    g_forceRebuild = true; // the game's loadout doesn't change, only ours
    if (!decided) {
        g_previewId = value == main ? kNoPreview : AccessoryPartSet(value);
        return apply(menu, value, 0);
    }
    g_previewId = kNoPreview;
    if (value != main) {
        int slot = CurrentSlot();
        int partSet = AccessoryPartSet(value);
        std::vector<int> ids = WithColumn(PickedExtras(slot, kMainPreset), column, partSet);
        SetPickedExtras(slot, kMainPreset, ids);
        g_lobbyStale = g_lobbyCharaStale = true;
        std::string text;
        for (int id : ids) text += " " + std::to_string(id);
        Log("equip screen: slot %d extra %d = item %d (part set %d), extras now:%s", slot + 1, column, value, partSet,
            text.empty() ? " none" : text.c_str());
    }
    apply(menu, main, 1);
    if (value != main) RoomPushAfterPick();
}

void __fastcall ListCloseHook(void *menu)
{
    if (g_pickColumn != 0) {
        g_forceRebuild = true; // drop a hovered extra the preview still shows
        // a hover left the row's index on the hovered entry (or the open on
        // the column's pick): back to the equipped one before close pushes it
        Field<int>(menu, kMenuAccessoryIndex) = g_pickMainIndex;
    }
    g_pickColumn = 0;
    g_previewId = kNoPreview;
    reinterpret_cast<ListCloseFn>(g_exe + kListCloseRva)(menu);
}

// Model rebuild request. While a column's list is open the equip screen's
// loadout (built from the row's index, which follows the cursor) gets the
// equipped accessory back. When an extra changed, the old copy's accessory
// is made to differ so the game reloads the accessory (and our extras).
int __fastcall ModelRequestHook(void *builder, void *loadout, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6)
{
    auto request = reinterpret_cast<ModelRequestFn>(g_exe + kModelRequestRva);
    // Only the equip screen's own model is touched. Retests 6 + 7: forcing
    // the accessory reload on another builder (the lobby avatar, then a save
    // select preview) left that model unloaded for good - the character
    // vanished, previews stopped loading, loading a save hung.
    if (reinterpret_cast<uintptr_t>(_ReturnAddress()) != g_exe + kEquipModelRequestReturnRva)
        return request(builder, loadout, a3, a4, a5, a6);
    // what the game puts there for the equipped one (its own default when
    // none, 0x4f951e), kept from the frames before the list opened
    if (g_pickColumn == 0) g_pickMainPartSet = Field<int>(loadout, kLoadoutAccessory);
    else Field<int>(loadout, kLoadoutAccessory) = g_pickMainPartSet;
    if (g_forceRebuild && Field<uint32_t>(builder, kBuilderBusy) <= 1 && g_forceRebuild.exchange(false)) {
        int accessory = Field<int>(loadout, kLoadoutAccessory);
        Field<int>(builder, kBuilderCachedA + kLoadoutAccessory) = ~accessory;
        Field<int>(builder, kBuilderCachedB + kLoadoutAccessory) = ~accessory;
    }
    return request(builder, loadout, a3, a4, a5, a6);
}

// Leaving the equip screen (0x4f8bc1) or the Presets panel (0x5067e3) sends
// the CaC's record to the lobby: 0x4657d0(lobby, record, 1) -> 0x465500
// queues an appearance message built by 0x654580 (static RE). Online lobby
// only: 0x4f8b80-0x4f8ba5 skip it unless [0x1585208] != 0, mode 0x46ee0 ==
// 0x14 and [0x158b378] != 0 (retests 6 + 7, offline: never reached). Retest 5: an
// extra-only change didn't show until character select - guess: the
// receiver rebuilds only when the appearance differs. So when the extras
// changed, a copy of the record with another accessory goes first, then the
// real one (unverified: whether the lobby reloads on that).
void __fastcall LobbyPushHook(void *lobby, char *record, int arg)
{
    auto push = reinterpret_cast<LobbyPushFn>(g_exe + kLobbyPushRva);
    if (record && g_lobbyStale.exchange(false)) {
        alignas(16) char copy[kRecordSize];
        memcpy(copy, record, kRecordSize);
        int16_t equipped = Field<int16_t>(record, kRecordAccessory);
        int slot = CurrentSlot();
        std::vector<int> picks = PickedExtras(slot, kMainPreset);
        // any other accessory: none, or (none equipped) the first extra
        int16_t other = equipped >= 0 ? -1 : static_cast<int16_t>(picks.empty() ? 0 : picks[0]);
        Field<int16_t>(copy, kRecordAccessory) = other;
        push(lobby, copy, arg);
        Log("lobby: sent slot %d with accessory %d, then %d, so the extras redraw", slot + 1, other, equipped);
    }
    push(lobby, record, arg);
}

// In an online room, send the CaC's record now that an extra changed, as
// leaving the equip screen does after a vanilla change (the receiver only
// takes room appearance updates from this send, online test 6). Goes through
// LobbyPushHook, so the accessory flip makes the receiver redraw.
void RoomPushAfterPick()
{
    if (!*reinterpret_cast<void **>(g_exe + kOnlineGlobalRva)) return;
    void *modeOwner = *reinterpret_cast<void **>(g_exe + kModeOwnerGlobalRva);
    if (!modeOwner || reinterpret_cast<GameModeFn>(g_exe + kGameModeRva)(modeOwner) != kModeRoom) return;
    void *lobby = *reinterpret_cast<void **>(g_exe + kRoomLobbyGlobalRva);
    void *table = *reinterpret_cast<void **>(g_exe + kRecordTableRva);
    if (!lobby || !table) return;
    char *record = reinterpret_cast<FindRecordFn>(g_exe + kFindRecordRva)(table, kPlayerRecordId);
    if (!record) return;
    Log("room: extras picked, sending the CaC's record");
    LobbyPushHook(lobby, record, 1);
}

// Offline, nothing is sent and leaving the equip screen only copies the
// loadout into the record, so an extras-only change left the lobby avatar
// as it was (retest 16). When the extras changed and this avatar shows the
// current CaC, its cached accessory is set to another value: the game's own
// check then reports a change and does its usual full reload, once.
// Online, another player's avatar is reloaded the same way when their sent
// extras changed (g_staleLooks, filled by RememberTaggedBlock).
int __fastcall LobbyCharaChangedHook(void *chara)
{
    auto changed = reinterpret_cast<LobbyCharaChangedFn>(g_exe + kLobbyCharaChangedRva);
    if (chara && g_anyStaleLook) {
        char *shown = Field<char *>(chara, kLobbyCharaRecord);
        bool stale = false;
        if (shown) {
            std::lock_guard<std::mutex> lock(g_remoteLock);
            stale = g_staleLooks.erase(RecordLook(shown)) > 0;
            g_anyStaleLook = !g_staleLooks.empty();
        }
        if (stale) {
            int16_t &cachedAccessory = Field<int16_t>(chara, kLobbyCharaCachedRecord + kRecordAccessory);
            cachedAccessory = static_cast<int16_t>(~Field<int16_t>(shown, kRecordAccessory));
            if (OnlineLogLine(g_remoteDrawLogLines))
                Log("room avatar: \"%.64s\"'s sent extras changed, reloading it", RecordName(shown));
        }
    }
    char *record = chara && g_lobbyCharaStale ? Field<char *>(chara, kLobbyCharaRecord) : nullptr;
    if (record) {
        // runs every frame in the lobby while stale: no logging check (SlotOfRecord logs name clashes)
        char *manager = SaveManager();
        int slot = CurrentSlot();
        const char *name = RecordName(record);
        char *cac = manager && slot >= 0 ? manager + kSaveSlots + slot * kSaveSlotSize : nullptr;
        bool isCurrentCac = cac && name && strncmp(cac + kCacName, name, 64) == 0 &&
                            Field<int32_t>(cac, kCacValueA) == Field<int32_t>(record, kRecordValueA) &&
                            Field<int32_t>(cac, kCacValueB) == Field<int32_t>(record, kRecordValueB);
        if (isCurrentCac && g_lobbyCharaStale.exchange(false)) {
            int16_t &cachedAccessory = Field<int16_t>(chara, kLobbyCharaCachedRecord + kRecordAccessory);
            cachedAccessory = static_cast<int16_t>(~Field<int16_t>(record, kRecordAccessory));
            Log("lobby avatar: slot %d's extras changed, reloading it", slot + 1);
        }
    }
    return changed(chara);
}

// After the game sends the accessory row: send the extra columns too. The
// game's text for a part set is had by pointing the row's index at the list
// entry that draws it for one more push, then the row is pushed again as it
// was. The list holds only the accessories the player has: a pick not in it
// (a config default) shows its part set number; no pick shows "---".
void __fastcall ItemPushHook(void *menu, int requestType)
{
    auto push = reinterpret_cast<ItemPushFn>(g_exe + kItemPushRva);
    push(menu, requestType);
    if (requestType != kListAccessory) return;
    static std::wstring texts[kMaxExtras]; // SetUserDataStr keeps the pointer
    static std::wstring loggedTexts[kMaxExtras];
    int slot = CurrentSlot();
    std::vector<int> picked = PickedExtras(slot, kMainPreset);
    int &index = Field<int>(menu, kMenuAccessoryIndex);
    const int equippedIndex = g_pickColumn != 0 ? g_pickMainIndex.load() : index; // a column's list moves the index
    const int count = MenuAccessoryCount(menu);
    std::vector<int> listPartSets(count);
    for (int i = 0; i < count; i++) listPartSets[i] = AccessoryPartSet(MenuAccessoryId(menu, i));
    for (size_t c = 0; c < kMaxExtras; c++) {
        int partSet = c < picked.size() ? picked[c] : -1;
        int found = -1;
        for (int i = 0; partSet >= 0 && i < count; i++)
            if (listPartSets[i] == partSet) { found = i; break; }
        if (found < 0) {
            texts[c] = partSet < 0 ? L"---" : L"#" + std::to_wstring(partSet);
            continue;
        }
        index = found;
        push(menu, requestType);
        char *text = reinterpret_cast<char *>(menu) + kMenuAccessoryText; // MSVC std::wstring
        size_t length = Field<size_t>(text, 0x10);
        texts[c].assign(Field<size_t>(text, 0x18) >= 8 ? Field<const wchar_t *>(text, 0) : reinterpret_cast<const wchar_t *>(text), length);
    }
    index = equippedIndex;
    push(menu, requestType);
    for (size_t c = 0; c < kMaxExtras; c++) {
        if (texts[c] == loggedTexts[c]) continue;
        loggedTexts[c] = texts[c];
        Log("equip screen: extra %d column text \"%ls\" (list of %d)", static_cast<int>(c) + 1, texts[c].c_str(), count);
    }
    auto setInt = reinterpret_cast<SetUserDataIntFn>(g_exe + kSetUserDataIntRva);
    auto setStr = reinterpret_cast<SetUserDataStrFn>(g_exe + kSetUserDataStrRva);
    for (int c = 0; c < static_cast<int>(kMaxExtras); c++) {
        setStr(menu, kUserDataExtraText + 2 * c, texts[c].c_str());
        setInt(menu, kUserDataExtraText + 2 * c + 1, count);
    }
}

// --- Menu edits ------------------------------------------------------------------

// Hands as3\Avatar.as (the row's columns) to the loader's UI library, which
// merges it with other mods' AVATAR edits (Battle Styles' style slot).
bool RegisterAvatarEdit()
{
    XV2UI ui;
    if (!ui.Load()) {
        Log("AVATAR.iggy: NOT patched - no extra accessory columns: the mod loader's UI library "
            "(XV2UserInterfaceLib.dll) is missing; update XV2 Mod Loader");
        return false;
    }
    char reason[256] = {};
    if (ui.EditScript("MultiAccessory", "ui/iggy/AVATAR.iggy", kMaAvatarAbc, sizeof(kMaAvatarAbc), kMaAvatarBaseAbc,
                      sizeof(kMaAvatarBaseAbc), kMaAvatarGameAbc, sizeof(kMaAvatarGameAbc), 0, reason, sizeof(reason)))
        return true;
    Log("AVATAR.iggy: NOT patched - no extra accessory columns: %s", reason);
    return false;
}

// --- Install -------------------------------------------------------------------

// Every RVA in this file is for game 1.27 (PE TimeDateStamp + SizeOfImage);
// the vtable writes are blind, so any other build must not be touched.
bool IsSupportedGame()
{
    const auto *image = reinterpret_cast<const uint8_t *>(g_exe);
    uint32_t pe = *reinterpret_cast<const uint32_t *>(image + 0x3C);
    return *reinterpret_cast<const uint32_t *>(image + pe + 8) == 0x6AAA4CB1 &&     // TimeDateStamp
           *reinterpret_cast<const uint32_t *>(image + pe + 24 + 56) == 0x1A33000;  // SizeOfImage
}

// "mov rax, target; jmp rax" within rel32 reach of the exe (as Training_QoL).
void *MakeJumpStub(uintptr_t nearAddress, void *target)
{
    unsigned char *page = nullptr;
    for (uintptr_t address = (nearAddress & ~uintptr_t(0xffff)) - 0x10000; address > nearAddress - 0x40000000; address -= 0x10000) {
        page = static_cast<unsigned char *>(VirtualAlloc(reinterpret_cast<void *>(address), 0x1000,
                                                         MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        if (page) break;
    }
    if (!page) return nullptr;
    page[0] = 0x48; page[1] = 0xB8;
    memcpy(page + 2, &target, 8);
    page[10] = 0xFF; page[11] = 0xE0;
    return page;
}

enum class Redirect { kNotYet, kDone, kFailed };

// Points the e8 call at `siteRva` (which must call `targetRva`) at `hook`
// (one jump stub per hook, shared by its call sites). kNotYet while the
// bytes don't match (Steam DRM still encrypting .text).
Redirect RedirectCall(uintptr_t siteRva, uintptr_t targetRva, void *hook)
{
    unsigned char *site = reinterpret_cast<unsigned char *>(g_exe + siteRva);
    int32_t relative = 0;
    memcpy(&relative, site + 1, 4);
    if (site[0] != 0xE8 || g_exe + siteRva + 5 + relative != g_exe + targetRva) return Redirect::kNotYet;
    static std::map<void *, void *> stubs; // hook -> stub; only the Start thread gets here
    void *&stub = stubs[hook];
    if (!stub) stub = MakeJumpStub(g_exe, hook);
    if (!stub) return Redirect::kFailed;
    int32_t newRelative = static_cast<int32_t>(reinterpret_cast<intptr_t>(stub) - reinterpret_cast<intptr_t>(site + 5));
    DWORD oldProtect = 0;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return Redirect::kFailed;
    memcpy(site + 1, &newRelative, 4);
    VirtualProtect(site, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return Redirect::kDone;
}

// Raises EditChara's user-data slot count to kSlotCountWanted unless another
// mod already raised it further (compare-exchange: never lowers it).
Redirect RaiseSlotCount()
{
    auto *code = reinterpret_cast<unsigned char *>(g_exe + kSlotCountCodeRva);
    if (memcmp(code, kSlotCountCode, sizeof(kSlotCountCode)) != 0) return Redirect::kNotYet;
    auto *count = reinterpret_cast<volatile char *>(g_exe + kSlotCountByteRva);
    if (static_cast<uint8_t>(*count) < kSlotCountVanilla || static_cast<uint8_t>(*count) > 0x7f) return Redirect::kFailed;
    DWORD oldProtect = 0;
    if (!VirtualProtect(const_cast<char *>(count), 1, PAGE_EXECUTE_READWRITE, &oldProtect)) return Redirect::kFailed;
    char seen = *count;
    while (static_cast<uint8_t>(seen) < kSlotCountWanted) {
        char before = _InterlockedCompareExchange8(count, static_cast<char>(kSlotCountWanted), seen);
        if (before == seen) break;
        seen = before;
    }
    VirtualProtect(const_cast<char *>(count), 1, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), const_cast<char *>(count), 1);
    return Redirect::kDone;
}

DWORD WINAPI Start(LPVOID)
{
    ReadConfig();
    Log("---- Multi Accessory v" MOD_VERSION " starting");
    bool any = false;
    for (int slot = 0; slot < kSlotCount; slot++) {
        std::string ids;
        for (int id : g_extras[slot]) ids += " " + std::to_string(id);
        if (!ids.empty()) Log("slot %d extras:%s", slot + 1, ids.c_str());
        any |= !ids.empty();
    }
    if (!any) Log("no default extras in multi_accessory.json (saved picks still apply)");

    g_exe = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!IsSupportedGame()) {
        Log("unsupported game build (this build targets 1.27) - nothing hooked");
        return 0;
    }
    Log("Presets panel callback: %s", ChainUserCallback(kEditMysetVtableRva, &MysetCallbackHook, g_mysetCallback)
                                          ? "hooked" : "NOT hooked - presets don't copy extras");
    bool menuEdited = RegisterAvatarEdit();
    bool rowHooked = ChainUserCallback(kEditCharaVtableRva, &EditCharaCallbackHook, g_editCharaCallback);
    Log("equip screen row: AVATAR edit %s, callback %s", menuEdited ? "registered" : "NOT registered",
        rowHooked ? "hooked" : "NOT hooked - extra columns pick for the equipped accessory");
    Log("CaC colour pass: %s", ChainVtableSlot(kCharaVtableRva, kCharaColourSlot, &CharaColourHook, g_charaColour)
                                   ? "hooked" : "NOT hooked - stacked wigs keep placeholder skin colours");
    struct Site { uintptr_t call, target; void *hook; Redirect state; };
    std::vector<Site> sites = {
        {kListApplyCallRva, kListApplyRva, reinterpret_cast<void *>(&ListApplyHook), Redirect::kNotYet},
        {kListCloseCallRva, kListCloseRva, reinterpret_cast<void *>(&ListCloseHook), Redirect::kNotYet},
        {kBuilderLoaderCallRva, kBuilderLoaderRva, reinterpret_cast<void *>(&BuilderLoaderHook), Redirect::kNotYet},
        {kResPhysicsCallRva, kResPartPhysicsRva, reinterpret_cast<void *>(&ResPartPhysicsHook), Redirect::kNotYet},
        {kActivatePartSetCallRva, kActivatePartSetRva, reinterpret_cast<void *>(&ActivatePartSetHook), Redirect::kNotYet},
    };
    for (uintptr_t call : kPhysicsInitCallRvas)
        sites.push_back({call, kPhysicsInitRva, reinterpret_cast<void *>(&PhysicsInitHook), Redirect::kNotYet});
    for (uintptr_t call : kResSkeletonCallRvas)
        sites.push_back({call, kResSkeletonRva, reinterpret_cast<void *>(&ResSkeletonHook), Redirect::kNotYet});
    for (uintptr_t call : kBuilderApplyCallRvas)
        sites.push_back({call, kBuilderApplyRva, reinterpret_cast<void *>(&BuilderApplyHook), Redirect::kNotYet});
    for (uintptr_t call : kBuilderTeardownCallRvas)
        sites.push_back({call, kBuilderTeardownRva, reinterpret_cast<void *>(&BuilderTeardownHook), Redirect::kNotYet});
    for (uintptr_t call : kBuildLoadoutCallRvas)
        sites.push_back({call, kBuildLoadoutRva, reinterpret_cast<void *>(&BuildLoadoutHook), Redirect::kNotYet});
    for (uintptr_t call : kItemPushCallRvas)
        sites.push_back({call, kItemPushRva, reinterpret_cast<void *>(&ItemPushHook), Redirect::kNotYet});
    for (uintptr_t call : kModelRequestCallRvas)
        sites.push_back({call, kModelRequestRva, reinterpret_cast<void *>(&ModelRequestHook), Redirect::kNotYet});
    for (uintptr_t call : kLobbyPushCallRvas)
        sites.push_back({call, kLobbyPushRva, reinterpret_cast<void *>(&LobbyPushHook), Redirect::kNotYet});
    sites.push_back({kLobbyCharaChangedCallRva, kLobbyCharaChangedRva, reinterpret_cast<void *>(&LobbyCharaChangedHook),
                     Redirect::kNotYet});
    for (uintptr_t call : kProfilePackCallRvas)
        sites.push_back({call, kProfilePackRva, reinterpret_cast<void *>(&ProfilePackHook), Redirect::kNotYet});
    for (uintptr_t call : kMemberUnpackCallRvas)
        sites.push_back({call, kMemberUnpackRva, reinterpret_cast<void *>(&MemberUnpackHook), Redirect::kNotYet});
    for (uintptr_t call : kBlockCopyCallRvas)
        sites.push_back({call, kBlockCopyRva, reinterpret_cast<void *>(&BlockCopyHook), Redirect::kNotYet});
    for (uintptr_t call : kBlockToRecordCallRvas)
        sites.push_back({call, kBlockToRecordRva, reinterpret_cast<void *>(&BlockToRecordHook), Redirect::kNotYet});
    Redirect slotCount = Redirect::kNotYet;
    // Steam DRM decrypts .text after start: retry for up to 2 minutes.
    for (int attempt = 0; attempt < 1200; attempt++, Sleep(100)) {
        if (slotCount == Redirect::kNotYet) slotCount = RaiseSlotCount();
        bool pending = slotCount == Redirect::kNotYet;
        for (Site &site : sites) {
            if (site.state == Redirect::kNotYet) site.state = RedirectCall(site.call, site.target, site.hook);
            pending |= site.state == Redirect::kNotYet;
        }
        if (!pending) break;
    }
    Log("equip screen user-data slots: %s", slotCount == Redirect::kDone ? "raised" : "NOT raised - extra columns stay empty");
    int loadoutHooked = 0, pushHooked = 0, packHooked = 0, unpackHooked = 0, copyHooked = 0, convertHooked = 0;
    for (const Site &site : sites) {
        if (site.target == kBlockCopyRva && site.state == Redirect::kDone) { copyHooked++; continue; }
        if (site.target == kBlockToRecordRva && site.state == Redirect::kDone) { convertHooked++; continue; }
        if (site.target == kBuildLoadoutRva && site.state == Redirect::kDone) { loadoutHooked++; continue; }
        if (site.target == kItemPushRva && site.state == Redirect::kDone) { pushHooked++; continue; }
        if (site.target == kProfilePackRva && site.state == Redirect::kDone) { packHooked++; continue; }
        if (site.target == kMemberUnpackRva && site.state == Redirect::kDone) { unpackHooked++; continue; }
        Log("call at 0x%llx: %s", static_cast<unsigned long long>(site.call),
            site.state == Redirect::kDone ? "hooked" : site.state == Redirect::kFailed ? "hook failed" : "never matched 1.27");
    }
    Log("loadout builder calls hooked: %d of %d (unhooked ones don't track the preset)", loadoutHooked,
        static_cast<int>(_countof(kBuildLoadoutCallRvas)));
    Log("accessory row pushes hooked: %d of %d (unhooked ones leave the extra columns stale)", pushHooked,
        static_cast<int>(_countof(kItemPushCallRvas)));
    Log("online profile packs hooked: %d of %d, unpacks: %d of %d (unhooked ones don't send / receive extras)", packHooked,
        static_cast<int>(_countof(kProfilePackCallRvas)), unpackHooked, static_cast<int>(_countof(kMemberUnpackCallRvas)));
    Log("appearance block copies hooked: %d of %d, converts: %d of %d (unhooked ones don't receive extras)", copyHooked,
        static_cast<int>(_countof(kBlockCopyCallRvas)), convertHooked, static_cast<int>(_countof(kBlockToRecordCallRvas)));
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        wchar_t path[MAX_PATH];
        DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        g_modDir.assign(path, length);
        g_modDir.resize(g_modDir.find_last_of(L"\\/") + 1);
        // Off the loader lock: the hook retries must not block DllMain.
        HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}

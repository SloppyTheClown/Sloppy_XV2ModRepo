# Battle styles: equip screen, style list and menu rows

Covers: BattleStyles' menus - the Battle Style slot on the equip
screen (AVATAR.iggy, exe EditChara / EditList, AVATAR_LIST in mode 3),
style list search and favorites, the character select skill popup and
lobby Presets panel rows, and the FFDec roundtrip bug (character select
hang). Overview: battle-styles.md; character select search:
chara-select-search.md.

## Equip screen UI (phase 3)
The Customize / equip screen (cloth + Super Soul + skills) is **AVATAR.iggy**
(class `action_script.Avatar`, same AS as character creation) driven by the
exe class EditChara; the item list popup is AVATAR_LIST (EditList).
Vanilla AVATAR.iggy (cpk, no loose copy): 101471 bytes, subfiles
(1, 100152 @64) and (0, 1255 @100216), ABC at 0xf158, size 38209,
FNV-1a64 0x58659141e5ce1f73; index_walk ends at 100152, so the LOBYHUD
append method works (ui-iggy-and-textures.md).

### Avatar.as
* `edit_menu.btn_1..btn_9` (EditMenuItemNum 9). EditMenuTabInfoList index 7
  (ModeTypeCustomize) = [[EditMenuClothInfoList, SendTypeRequestMenuCusCloth
  107, TabText_ClothEquip], [EditMenuSkillInfoList, 108, ...]]; the cloth list
  is [Top, Bottom, Gloves, Shoes, Accessory, Material, Talisman(=Super Soul)]
  -> 7 buttons, btn_8 is free under Super Soul.
* EditMenuItemInfoList[type] = [header msg id, itemType (0 Part), ReceiveData
  user-data idx, ReceiveNum idx, RequestType (sendType), active]. Talisman:
  type 18, data 25 / num 26, sendType 211. ReceiveTypeNum = 96 user-data
  slots; EditMenuTypeNum 34.
* Part slot: header via StringList (msg id, "^" splits icon^name), text =
  GetUserDataString(data), num <= 0 -> blank/inactive.
* Opening a list: SetUserDataValidFlag(data,false), CallbackUserData("user",
  RequestType, -1), wait each frame for GetUserDataValidFlag(data) (the exe
  re-sending that slot), then back to the menu.

### Exe (1.26.01 RVAs)
* OggyBase (vtable 0xf98020): wrapper +0x40, user-data vector +0x160..0x170
  of 16-byte {dirty, type, value}; ctor 0x2c6e40(this, mode, count) sizes it.
  SetUserDataInt **0x2c81a0**(this, idx, int), SetUserDataStr
  **0x2c8240**(this, idx, const wchar_t*) - bounds-checked, store the
  POINTER (string must stay alive). AS "user" calls land in vtable +0x88.
* EditChara (vtable **0x10a0098**): ctor 0x4f67a0 passes the slot count with
  `BA 02 00 00 00 44 8D 42 5E` at 0x4f67cf -> **byte 0x4f67d7** = 0x5e
  (96 = 2 + 0x5e); patch it to give AS more slots (.text, after DRM).
  Slot 17 (+0x88) = 0x4f72c0: argc at call+0x10, arg0 double at +0x28, arg1
  at +0x48; calls 0x4fa690(this, sendType, value), jump table 0..0xe6,
  anything else ignored.
  Fields: +0x6c0 EditList*, +0x6b8 state machine (0x652680(sm, state,0,0)),
  +0x1f4 mode, +0x2f8 open list's sendType (0x192 = none), +0x328+k*0x20
  wstrings kept for item names.
* List opener template 0x4fcac0 (face parts): param {int default;
  vector<0x28 entry {wstring name (SSO cap 7), int valid=1 @0x20, int @0x24}>}
  -> 0x503970(list, &param) copies it (0x503220), sets list user data 1 =
  count; mode 8 sets the cursor via 0x503a50(list, idx); +0x2f8 = sendType;
  state from mode: 3->0x27, 1/5->0x21, 2/6->0x28, 8->0x35+(sendType!=0x192),
  4->0x34, else (7 = Customize) -> 5.
* EditList (vtable 0x10a0ab0): entries +0x1f8, default +0x1f0, result
  +0x210, cursor +0x214; GetResult 0x503940, HasResult 0x503950, Reset
  0x503960; user handler 0x5038f0 (send 1 = select idx, every cursor move =
  preview). Flags +0x1b4: 0x6696c0 open bit, 0x6696d0 closed bit.
* Poll **0x4fd9d0**(EditChara) (entry `53 48 83 EC 20` + `mov rbx,rcx`):
  with a result, switches on +0x2f8-200 (0..0x15); out of range goes to the
  tail 0x4fdbaa: if closed -> 0x4fb5f0(this, +0x2f8) (refresh that item's
  user data; unknown types do nothing), +0x2f8 = 0x192, back to the menu
  state.
* Loaded save slot: int at [[0x15545b0]] + 8 + 0x44.
* **The AVATAR_LIST (+0x6c0) exists only in modes 1, 2, 5, 6, 8** (ctor
  check at 0x4f6ab6: `lea eax,[rcx-1]; test eax,~5; je create; cmp ecx,8;
  jne skip` - the jne at 0x4f6ac3). The in-game Customize (mode 3) has none:
  opening the style list through it crashed the game on click. Fix: nop
  that jne (every mode gets the hidden list; dtor 0x4f6f10 frees it; its
  ctor 0x503700 loads via the generic 0x2c7f40). Its users: ctor, dtor,
  0x4f7320, template, 0x4fd190, update 0x4fd460, poll - all null-checked
  except the openers.
* Equipment lists (cloth + Super Soul, sendType 206..212 -> 0x4fc7b0) use a
  different item list at +0x6d0 (0x5405d0 adds item ids) and state 0x33 in
  mode 3 / 5 otherwise. The face-part template's mode-3 state 0x27 is the
  main edit state the poll's close tail returns to (0x4fdc1b).
* Cloth push 0x4fb3f0(menu, customize) refreshes slots 206..212; called at
  menu start (0x4f7bb0) and for requests 103/107. Hooking only the 103/107
  callback left the Battle Style slot grey until the tab was re-shown.

### Style list search (AVATAR_LIST, Item Search step 3)
The loader's search box (`equipment_search`, shared with Item Search)
filters the style list while it is open. Found by reading the exe and
`action_script.AvatarList` (FFDec export of AVATAR_LIST.iggy).

EditList (the AVATAR_LIST object, 1.26.01 RVAs):
* 0x503970(list, ListParam*) set contents: clears flags 1/2/4 at +0x1b4,
  +0x1f0 = default, copies the entries, result +0x210 = -1, user data
  0 = default, 1 = count, then 0x503ba0 sends the rows (strings 2..9,
  valid flags 10..17 per row block).
* 0x503a50(list, idx): +0x214 and user data 18 = the X button's entry.
  The game's opener calls it (0x4fcda3); without it the style list keeps
  the last list's X entry.
* Flags +0x1b4: 1 exited (0x6690e0), 2 cancelled (0x6690a0, result =
  +0x1f0), 2|4 decided (0x6690c0). Never refill while one is set.
* AS -> C++ handler 0x5038f0: send 0 = request rows (0x503ba0 from the
  given start), send 1 = cursor move, result = idx if entries[idx].valid -
  **no bounds check**, so the entry count must never shrink below what the
  AS still thinks it has. 0x503940 GetResult, 0x503950 HasResult (>= 0).
* User data (19 slots, SetUserDataInt 0x2c81a0): 0 ListSelect (start
  cursor + the check mark in updateList), 1 count, 2..9 strings, 10..17
  valid flags, 18 ListDefault (X entry). The valid flags live in the AS's
  `Callback.m_user_data` arrays; C++ cannot read them back.

The vanilla script reads the count only in startList, so a refilled list
does not redraw. Rebuilt `BattleStyles\as3\AvatarList.as` (served by
the DLL only over the vanilla ABC, `ClaudeTools\iggy\menu_abc.py avatarlist`):
startList clears only the count's valid flag, stashes slot 18 (X entry)
and adds an ENTER_FRAME check; when the count arrives again (not while
waiting for rows) it takes slot 18 as the new cursor, puts the X entry
back, SetItemNum/SelectList/requestList/sendListSelect. FFDec roundtrip
pcode diff: only the edited methods change.

C++ side (`style_menu.cpp`): Default and the equipped style always stay
listed (a cancel returns the default entry, which must be the equipped
one); no match shows everything with the note "No style matches"; on a
query change (XV2Search_Changes) refill via 0x503970, cursor kept on its
style (else entry 1) via user data 18 + result. Note set every frame while
the list is open, "" once when it closes. Config `style_search`.

Confirmed in game (2026-10-09): live filtering, pick/cancel/X, the query
survives opening and closing the style list and the item lists in between,
notes right, item lists unchanged.

### Favorite styles (2026-10, confirmed in game)
Same idea as Item Search's favorites (ui-iggy-and-textures.md "Item
lists"). `favorite_styles.sav` in the mod folder: `{"<steam id>":
["GOK", ...]}`, per account (not per slot or preset). FilterStyles orders
Default, then favorites, then the rest; Default stays entry 0 (X button and
the lost-cursor fallback use it) and the list still opens on the equipped
style. Favorites are handed over as a second ListEntry set named
`"<name>\t★"`, only while the rebuilt AvatarList.as is served (it
needs `style_search`): its updateList calls showFavorite, which splits on
the tab and sets the row's htmlText to the escaped name in the row colour
plus a gold star. Toggle in HkListPoll: F (no Ctrl, game window in front)
or XInput left stick click (0x40, XInputGetState from the xinput DLL the
game loaded), edge-detected, not while typing in the box, list not ended;
then RefreshStyleList keeps the cursor on the toggled style. Opening the
list marks the key as held, so the press that opened it never toggles.
FFDec roundtrip: only updateList (+ showFavorite) differs from the earlier
search build.
In game (2026-10-09) no press toggled anything: the log had "favorite
toggle: F or the left stick click" (the poll reached FavoritePressed) but
no "favorite style" line and no favorite_styles.sav; the cause isn't known
from the code. The DLL then logged why each press did nothing; the next
try (2026-10-10) logged "favorite: cursor -1 is outside the 219 shown
styles" for every press. Cause: EditList's result +0x210 holds a cursor
move only until the game's list poll 0x4fd9d0 reads it, then is -1 again,
so reading it later in the frame (after the original poll) never sees the
cursor. The search refresh had the same bug, hidden by its fallback to the
equipped style. Fix: the poll hook reads HasResult/GetResult before the
original poll and keeps the last move (`g_shownCursor`, set to the default
entry when the list opens and to the new cursor after a refresh); the
toggle and the refresh use it. Confirmed in game (star, order, persistence, search).

## Skill popup / Presets panel UI
Both panels get a "Battle Style" row; the text rides on a string the game
already sends (appended after a tab), so no user-data count patches.
* **Character select skill popup**: CHARASELE.iggy, class
  `action_script.CharaSele` (vanilla ABC 46381 bytes, FNV-1a64
  0x18865d7ce449251c). `inact_skill.skill01..skill09` (09 = Super Soul:
  `sys_com01` header, `sys_skill` name); updateSkill reads user data 17
  (ReceiveType_TarismanNameStr). Exe class SelectCharacter (vtable
  0x10be328, AS callback slot 17 = 0x5e1630 -> 0x5e46d0(menu, sendType,
  value)). 0x5e57a0(menu) (single caller 0x5e4988) fills the popup's user
  data for the character under the cursor and sets 17 from the wstring at
  +0x4c0. Fields: +0x438 CMS chara id (< 0 none; CaC = 100..109, as
  0x290000 `add ecx,-0x64; cmp ecx,9`), +0x440 picked preset, +0x448
  costume, +0x450 CaC save slot. Mod: post-hook 0x5e57a0 (prologue `48 8B
  C4 55 41 54 41 55 ...`, 6 bytes stolen) appends "\t<style>" to 17 for a CaC.
* **Lobby Presets panel** (pause menu > Customize > Presets): AVATAR_SET.iggy,
  class `action_script.AvatarMyset` (vanilla ABC 17146 bytes, FNV-1a64
  0xd3e089487c3d84b6), exe EditMyset (vtable 0x10a1098). Rows
  `sys_sb_a/b_01..07` (equipment: label/value), `sys_sk_a/b_01..08`
  (skills). sendType 0 (RequestEquipData, arg 1 = preset) -> 0x506710 sets
  user data 2..8 from the record's equipment (vector +0x200) and 9..16
  from its skills (vector +0x218; 16 = evasive = ReceiveType_SkillStrEnd),
  then 0x64fa20. Mod: after the original, "\t<style>" appended to 16 for
  that preset of the loaded slot.
* AS string arguments: CallbackUserDataString("user", type, text) reaches
  the slot-17 callback with arg 1 (+0x48) a **UTF-8 char*** instead of a
  double. The rebuilt AS sends its panel layout once as sendType 9901
  (logged by the mod, not passed on) - for fixing placement without a
  debugger.
* AS side: rows are made at runtime (copied TextFields, as LobbyMap.as);
  Presets tries the free slot under Super Soul first, else under the last
  skill with the window backing stretched (growBackgrounds). Sources:
  `BattleStyles\as3\CharaSele.as`, `CharaSele_XV2Patcher.as`,
  `AvatarMyset.as`, built by `ClaudeTools\iggy\menu_abc.py`.
* **xv2patcher's CHARASELE**: xv2ins 4.7 writes a loose
  `data\ui\iggy\CHARASELE.iggy` whose CharaSele calls an XV2Patcher class
  (ABC 47686 bytes, FNV-1a64 0xaa2bcacb32cbd1fb). Every player with
  xv2patcher has it, so the mod carries a rebuild of both (menu_abc.py
  `charasele_patcher`, read from the loose file; the DLL tries each
  variant). The edits are the same; only the `_locN_` names differ (the
  edit script matches them by regex).
* **Squeezed copied text**: Utility.SetAutoSizedText/SetAutoSizedhtmlText
  store the field's first width/align/x on the field's parent clip
  (`user_add_property_default_width/_align/_x`) and later set scaleX =
  defaultWidth / textWidth when wider. A runtime copy of a row clip has no
  such properties, so the first text set fixes its width - the copied
  value field was empty (12px wide), squeezing the style name to a line.
  copyRow now copies the three properties from the original row.
* **Row backings** (Presets): each row has a plain MovieClip behind it at
  x=54, 2px above the text (`_CMN_M_B_plt_list01` scaled 14.12 + a Shape;
  instance15 backs sys_sk_*_08). copyRowBacking finds the smallest such
  clip covering the row. **Dead end (2026-10-06, both builds crashed):**
  copying it. Build 1 made each part with `new part.constructor()` and
  drew plain Shapes into a BitmapData; build 2 only did
  `new part.constructor()` (skipping Shapes). Both times the script
  stopped silently inside the copy (the layout line before it logged, the
  note after it never did), and pressing Y (show the preset's details)
  went blank, hung and crashed the game. Common factor: constructing
  `_CMN_M_B_plt_list01` - a `_CMN_` shared symbol, presumably imported
  from the common menu swf, which Iggy apparently cannot instantiate at
  runtime (the game's own `new icon.constructor()` copies are of the
  movie's own symbols). So the row has no box; the window still grows
  by a row. A box would need another way (an existing backing moved, or
  the Graphics API - untested in Iggy). The
  window (instance0, 560x570) already reaches past the new row, so
  growBackgrounds now always grows by the added row's height.
* **FFDec roundtrip bug - the character select hang (2026-10-06)**: with
  the xv2patcher variant served, a training match with two CaCs hung at
  Start (one core at 100%, no battle loader log lines).
  `ClaudeTools\threadpc.py` (samples the busiest thread's RIP via
  SuspendThread/GetThreadContext, no debugger) put it inside
  iggy_w64.dll's AS interpreter. FFDec decompiles the cursor-skip loops
  (changeUD, changeVar) as `do {...continue;...} while(_loc4_ =
  this.checkUnlockChara(_loc2_), !_loc4_);` and its compiler **drops the
  `!` of the comma condition**, so the loop runs while the character is
  unlocked - forever with everything unlocked. changeCurrentPlayer calls
  changeUD when the next player's cursor sits on an already-picked CaC
  (the two-CaC case). The original bytecode's `continue` (picked CaC)
  jumps to the loop top without the unlock check; edit_charasele.py
  rewrites the loops to `while(true) {...; if(unlocked) break;}`, checked
  in the recompiled p-code (`ffdec -format script:pcode -export script`).
  That fix alone still hung: a **second loss** in changeCurrentPlayer's
  `do { switch(checkPlayerTeamType(index)) {...} } while(!found)`. The
  original is `case PlayerTeamTypeOwn: default: found = true` (lookupswitch
  default == the Own case target); FFDec drops the `default:`, so an
  index outside every team loops forever. edit_charasele.py adds it back;
  with both fixes the two-CaC training match starts (confirmed in game,
  xv2patcher variant).
  Found with `ClaudeTools\iggy\pcode_flow.py` (canonical control-flow walk of
  each method, original vs rebuilt p-code) plus listing every
  `lookupswitch`: after both fixes the only differing methods are the
  edited updateSkill and compiler noise (`while(true)` -> `pushtrue;
  iftrue`, `int(x)` -> `convert_i`, a dropped `Boolean()` on a Boolean
  return, changeLR/changeUD/setSelectChara checked equivalent by hand).
  Lesson: before trusting a roundtripped class, grep the decompile for
  comma expressions in conditions (`while(... = ..., ...)`), compare every
  `lookupswitch` default/case list with the original, and run
  pcode_flow.py; no comma conditions in Avatar.as / AvatarMyset.as /
  LobbyMap.as.
  **Switches compared (2026-10-14, `ClaudeTools\iggyoundtrip_check.py
  <ffdec-cli.jar> [avatar|myset|avatarlist|lobbymap]`)**: vanilla ABC from
  the cpks vs the rebuilt one in the generated header, each exported to
  p-code; pcode_flow walk + every lookupswitch's default and cases resolved
  to the code they run (jumps followed). Avatar, AvatarMyset, AvatarList,
  LobbyMap (General_QoL): no lost branch. FFDec does drop every case whose
  target is the default's (usually the trailing `break` case) - harmless,
  that case runs the default anyway; the CharaSele bug was the opposite
  (the default lost, pointing at a real case). Avatar `waitStart` checked
  by hand (Wait -> return, else Start), LobbyMap `createEventIcon` by hand
  (`ifnlt` -> `ifnge` + swapped branches, same logic). Other flow
  differences are the mods' own edits. pcode_flow.py now also reads
  private methods (`PrivateNamespace(null,"1")`; LobbyMap's were skipped).

## Combo List (pause menu)

Not built from the moveset/BCM: Menu::PopupCombo (POPUP_COMBO.iggy, AS3
class PopupCombo) holds two fixed pictures, "var1" and "var2" (var1 adds
A1_6..9, A2_1..3, B2_2..4 - the longer chains). User data: 0 = has-var2 flag
(ReceiveTypeFlagHasVar2), 1 title, 2 charge text, 3 ComboA, 4 ComboB, 5/6
icons X/Y, 7 end, 8 sync icons.

Exe: the battle pause handler 0x16fef0 (rdi = [0x1554460] battle manager)
gets the local player index via 0x92050(manager + 0x7c), the mob
[manager + idx*8 + 0x3a58], its CMS entry [mob + 0x3f0] (written at 0xd3534,
Mob vtable slot 16, via 0x2c5460 on the mob's chara id), and reads
`movzx edi, byte [entry + 0x14]` at 0x1701a3 (default 1 without an entry);
that goes to the setup 0x58c020 as its 5th argument (call 0x1701ef, the only
caller). CMS +0x14 is Xv2CoreLib's "LoadCamDist" u16: low byte 1 only for
DMG, DM2, MRN, CL2 and ZMD (0x1601); CaC races 100..107 read 0 (var1).

So every style already showed the right (generic) list except those five.
Fix: Battle Styles redirects the call at 0x1701ef and, for a styled CaC
mob (StyleCharaOfMob - same resource/slot match as the effects), passes
the style's CMS +0x14 byte. Built + installed 2026-10-14, not seen in game.
Found by static analysis of the decrypted 1.26.01 image + FFDec of
POPUP_COMBO.iggy.

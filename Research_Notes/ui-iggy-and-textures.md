# UI: Iggy menus, .iggytex/.iggyted textures, EMB, msg text

Covers: menu title art, the Iggy texture container formats, how to tell
which texture numbering an .iggy uses, EMB image files, and how menu text is
looked up. Context: General_QoL's replay-NPC menu title and the Revamp
bridge's texture renumbering (revamp-on-1.26.md). Game 1.26.01.

## Menu titles
* Headers: CMNTITLE.iggy (mc_menutitle). The big title is a picture picked by
  a number ("MainTitleType"); the line under it is text
  (menu_title_text_*.msg, e.g. HED_FTS_00_000 = "Future Saga", the only entry
  with that text). Changing the msg changes only the small line.
* CMNTITLE_<lang>.iggytex: 14 DDS textures. Texture 14 (768x1536 DXT5) = newer
  titles, one per band; "Future Saga" = block rows 254..272, cols 0..96, same
  in all 13 languages (gap rows 253/273 alpha <= 3). Texture 1 = header icons
  (time machine icon).
* Iggy blends premultiplied: alpha 0 with the colour kept STILL SHOWS. Blank =
  zero the whole 16-byte DXT5 block. General_QoL does that; not yet confirmed
  in game - if the title still shows, the texture is not read through the
  hooked request's vt+0x20 (the log does show cmntitle_en.iggytex edited, and
  memory held only the edited copy).
* Other UI: QUEST.iggy = quest menu frames only; STORETITLE / CMNTITLE_TTL /
  CMNTITLE_BATTLE are other screens.
* Preview: stdlib DXT5 decode -> PNG with zlib (no PIL);
  `XV2_RevampBridge\tools\ddspng.py`.

## Formats
* .iggytex / .iggyted (same format): magic "oggytex\0", u32 count at 8, then
  per texture {index, size|0x80000000, offset|0x80000000, 0} from 16, each a
  DDS. Writer: header 16 + 16/texture, textures at 128-byte alignment, no
  tail - reproduces vanilla byte for byte. An entry with size 0 / offset 0
  references another file's texture; its high u16 names the file (0xFFFF =
  held here). Match references by that, not by order.
* A texture-only mod using BC7 (DX10 DDS, dxgiFormat 98) works on Windows
  but not under Proton: the game loads Iggy textures via d3dx11_43 and
  Wine's builtin cannot decode BC7 (Revamp's button icons). Fix and
  details: loader-and-steam-deck.md.
* .iggy: header subfile count u32 at 0x1C, entries {id, size, size, offset}
  from 0x20. It refers to textures by id, so a texture-only mod breaks when
  an update renumbers them.
* Which numbering an .iggy uses (`XV2_RevampBridge\tools\iggyids.py`): it
  holds a table of named characters, 24-byte records {u64 name offset
  (rising), u64 character id (falling), u64 0}; texture ids are character
  ids. Compare with each version's vanilla .iggy. Never assume a loose .iggy
  is the mod's era - xv2ins 4.7 writes its own (CHARASELE, STAGESELE,
  ITEMLIST_DETAILS, POPUP_PAUSE, 1.26-numbered; embedded in xv2ins.exe with
  tags XV2_PATCHER_AS3_V16/V14/V05/V09, `XV2_RevampBridge\tools\exeiggy.py` extracts them).
* Shared pieces: POPUP_TRAINING / POPUP_PAUSE / POPUP_SKILLSEL import symbols
  by name from MENU/COMMON/swf/CMN_MENU_BATTLE.swf and msg/sysfont01.swf.
* 1.25 -> 1.26 renumbered: TITLE_<as|eu|jp|us>_<lang> (23 -> 24 textures, new
  id 110, e.g. 53 -> 120), CMN_MENU_BATTLE_<lang> (68, all renumbered; old 45
  redrawn = new 14), sysfont01 (504 -> 505: one shared icon inserted - the
  "race icons off by one"), LOBYHUD, ITEMLIST_DETAILS (24 -> 26), STAGESELE
  (id 5 redrawn). Maps: `XV2_RevampBridge\src\iggymaps.txt`.
* EMB (ui\texture, ui\sprite): "#EMB", count u32 at 0x0C, entry table at [0x18]
  ({u32 offset relative to the entry, u32 size}), name table at [0x1C]
  (absolute u32 string offsets). HCI rows {chara, costume, state1, state2,
  EmbIndex, ...} u16; EmbIndex indexes ui\texture\<code>.emb (per character).
  `XV2_RevampBridge\tools\embdiff.py` compares a mod's emb with vanilla by name.

## Editing an .iggy's ActionScript (AS3)
* Movie subfile (id 1) header field +0x98 = self-relative u64 to the AS3
  block {u64 1, u32 ABC size, ABC (magic 10 00 2e 00)} (LOBYHUD, LOBYMENU,
  LOBYMENU_HUD). Subfile 0 (right after) is the pointer index.
* Method that works offline: wrap the ABC in a minimal SWF (FileAttributes
  + DoABC + End), FFDec `-export script` to read it, `-replace <in> <out>
  <package.Class> <file.as>` to recompile one class, take DoABC back out.
  FFDec recompiles from its own decompile: diff a re-decompile against the
  original - only the edited class should change (it rewrote one labelled
  loop in LobbyMap.createEventIcon, same behaviour).
* Subfile 0 (index) is byte code iggy_w64.dll (0x59860..0x59bb1) runs to
  walk the movie start to end, turning relative u64s into pointers (1 =
  null); its stream ends at the index size from the subfile table. Opcodes
  in `ClaudeTools\iggy\lobyhud_abc.py` (index_walk simulates it; vanilla LOBYHUD,
  LOBYMENU, LOBYMENU_HUD walk exactly to the movie size). The AS3 block is
  walked as `fd 0f 01 00 02` (15 bytes, pointer at +0) + `ff <rest>`.
* Re-packing without moving anything: append the new block at the end of
  subfile 1 (16-aligned), point +0x98 at it, append `fe <gap-1>`,
  `fd 0f 01 00 02`, `ff <rest>` to the index, fix subfile 1's size fields
  (0x24/0x28) and subfile 0's sizes and offset (0x34/0x38/0x3C). The old
  block stays as dead bytes.
* Dead end: appending the block WITHOUT the index commands crashed the game
  as the map loaded (iggy_w64.dll+0x16f0 reads [block+8] walking the AS3
  block chain; the unwalked block's +0 kept its raw 1 and was followed as
  a pointer). With the commands it loads and runs in game; a runtime
  TextField draws in the legend's font, and npc+0x42 is the shown name.
* `ClaudeTools\iggy\lobyhud_abc.py` does all of this for LOBYHUD;
  `ClaudeTools\iggy\menu_abc.py` for BattleStyles' AVATAR, CHARASELE and
  AVATAR_SET iggys, whose C++ side is the generic
  `BattleStyles\src\iggy_abc.cpp` (`ReplaceIggyAbc`, output
  byte-identical to the Python one). FFDec drops comments, so a
  re-decompile differs from the source only by those.
* Passing new text to an iggy without more user-data slots: append it to
  a string the exe already sends, after a tab, and split it in the AS
  (battle-styles-ui.md "Skill popup / Presets panel UI"). SetUserDataStr
  keeps the pointer: the buffer must live on. A string argument of
  CallbackUserDataString arrives at the callback as a UTF-8 char* at
  +0x48 (where a number arg is a double).
* File-request paths use backslashes (`data\ui\iggy\AVATAR.iggy`): treat
  '\' and '/' alike when matching a path suffix.
* AS user data beyond the vanilla count: the menu object's ctor sizes the
  user-data vector (OggyBase ctor 0x2c6e40(this, mode, count)); patch the
  count where the menu's own ctor passes it (EditChara: byte 0x4f67d7,
  battle-styles-ui.md "Equip screen UI").
* Adding to a multi-mode menu (EditChara): first read the live mode
  (`ClaudeTools\battle_styles\editchara.py`) - AS lists and exe sub-objects
  differ per mode (the in-game Customize is mode 3 and has no AVATAR_LIST).
  Fill new slots wherever the game fills its own (the push at menu
  start), not only on a request, or they start grey.

## Lobby map (LOBYHUD.iggy, classes LobbyHUD / LobbyMap)
* The exe sends lines through user-data slots ReceiveType_MapInfoStart+i:
  `Event:id=<icon 1..22>,x=<%>,y=<%>`, `BArea:`, `BeaCon:`, `Start`, `End`,
  `PageL`, `PageR`. LobbyHUD.parseMapInfo splits on `:` `,` `=` and
  parseInt()s every value (a text value becomes NaN). x/y are percent of
  point1..point2 on map_inact; icons are copies of `map_inact.icn_<id>`.
* Exe side: 0x3e9890 walks the lobby NPC list (manager 0x15747c0) and calls
  0x5767c0(map, [npc+0x1c], type [npc+0x34c], npc+0x2d0 position); type ->
  icon id by jump table 0x577380 (2->1, 3->15, 4..9->2..7, 0x10->8, ...,
  0x64/0x65 from npc flags 0x10/0x20 -> 9/10); [npc+0x1c] 2..12 -> fixed
  map position slots (0x577500). The line is "Event:" + sprintf
  "id=%d,x=%d,y=%d" appended with std::string::append (0x7a740) at 0x576904.
* NPC object: +0x42 and +0x142 wide strings (0x4aa1d0(npc, 0/1)); the OPC
  name plate prints +0x42 with %ls - it is the NPC's shown name (confirmed
  on the map). Undiscovered mentors get no map line at all. Mentor icons: classes po_icn_sensei /
  po_icn_sensei2 ("sensei" = mentor); which icon id they are was not
  established (the iggy's display list was not decoded) - the AS3 tests
  the class instead.
* Legend text: lobby_hud_text LBY_HUD_02_000..016 (Instructor = 002,
  Instructor Event = 006).

## msg text
* data\msg\*.msg: UTF-16 entries; the game looks text up by entry NAME, not id
  (mods renumber ids freely). Edits: append the new UTF-16 string and repoint
  the line record; `'` -> `&apos;` like the game's text. 13 languages.
* A line record's 4th u32 is its parameter count: with 0 the game shows
  "%ls"/"%d" as is. Keep it when rewriting a file (see revamp-on-1.26.md).
* Compare msg files with `XV2_RevampBridge\tools\msgdiff.py` (an ASCII token
  check misses them - names are mostly UTF-16).

## Item lists (Menu::ItemList)
Equipment lists (torso ... Super Soul), shops: one Menu::ItemList object,
vtable 0x10aa508; slot 9 = per-frame update 0x5417e0 (rcx = list). It keeps
ticking while hidden (Customize screen: state 1, 0 entries), so a list stays
"active" for the whole equipment screen. Used by ItemSearch; probe:
`ClaudeTools\itemlist_probe.py`.
* Fields: +0x1f0 mode (15 = equipment, also for Super Souls), +0x1f4 state
  (2 = open and idle, 1 = hidden), +0x1f8/+0x200 all entries, +0x210/+0x218/
  +0x220 shown entries (begin/end/capacity), +0x228 category, +0x22c sub,
  +0x234 cursor (entry index), +0x23c top visible entry (written by the page
  pusher), +0x2f4 rows-changed flag, +0x318 preview owner (EditChara) or 0,
  +0x528/+0x52c sort (cmd 12 at 0x544805 cycles +0x52c).
* Entries: 0xb0-byte POD (push_back 0x544190 copies with movups), +0 type,
  +4 id (-1 = "None").
* Functions: 0x543090(list, wstring* out, shownIndex) row name as shown;
  0x542640(list) shown = all filtered by category + sorted (no reload);
  0x546730(list, 0) reloads the all array from the inventory (and rebuilds);
  0x545340(list, topRow) sends the visible rows to Iggy; 0x545980(list) row
  icons (Iggy int variables 0x3f..0x49); 0x2c81a0(menu, variable, int) sets
  one Iggy int variable (6 = selected entry, 7 = selected row on screen,
  from the cursor move at 0x5447a8); 0x541370(list) character preview.
* Command handler 0x544220 (Iggy -> game, jump table 0x544860): cmd 1 =
  category change (0x54426f: rebuild, 0x5465e0 find page, push page,
  cursor). That path does NOT reset Iggy's selected entry/row (the Iggy
  side started the change). Refreshing an open list from the game side has
  to do what the sort refresh 0x542580 does: variables 6 and 7 = 0, icons,
  push page 0, variable 2 = 0, cursor 0, (modes 9/10: 0x5452d0 + 0x5410c0),
  preview, +0x2f4 = 1. With only the category-path tail the rows did not
  change on screen, and a shortened list with Iggy's selection past its end
  is the likely cause of the menu ignoring input after a search (seen in game;
  the fix is not confirmed yet).
* **Calling that sequence from Lua does not redraw the rows** (seen in game:
  the preview changed - cursor 0 = "None", the CaC showed naked - but the
  rows stayed). ITEMLIST.iggy's script (action_script.ItemList) is
  pull-based: rows are only redrawn by its own requestList flow (requestList
  -> waitOutList -> sends RequestList/SelectIndex/ListPosition ->
  waitSetupList/checkSetupList -> changeListPage/initList -> waitShowList,
  which adds checkKey + updateNewFlagNotify). The game's sort path works only
  because the script started it.
  * Game -> script int variables (ReceiveType_*): 0 ModeType, 1 RequestEnd,
    2 RequestResume, 3 Zeny, 4 ListType, 5 ListElemMax, 6 ListSelectList,
    7 ListButtonStart, 8 ListNameStart... Script -> game (SendType_*): 0
    RequestList, 1 RequestSort, 4 SelectIndex, 6 DecideIndex, 11
    ListPosition, 12 PauseList. Setters 0x2c81a0/0x2c81f0/0x2c8240 (int /
    float / string) store the value, valid flag = 1, menu +0x54 dirty.
  * Game-side refresh after a reload: 0x5488b9 (reload 0x546730, rebuild,
    clamp cursor/top, variables 6/7, push page, preview) - also no variable
    2, so it relies on the script asking too.
  * The fix (ItemSearch): a patched ItemList script
    (`ItemSearch\as3\ItemList.as`, built by `ClaudeTools\iggy\itemlist_abc.py`,
    shipped as `ItemList.abc`; item_search.lua has the loader swap it into
    the game's own ITEMLIST.iggy as it loads - `files.ReplaceIggyScript`,
    vanilla ABC 59410 bytes / FNV-1a 64 0x8dd6dfe18ce480c6; older versions
    installed a loose `data\ui\iggy\ITEMLIST.iggy`). checkSearchRefresh,
    called first in updateNewFlagNotify (every idle frame, not while paused):
    when variable 2 is valid and holds 0x5EA0000 | entry, it clears the flag,
    SetItemNum(variable 5), SelectList(entry, -1), sets
    m_restore_pos_change_page_init_list and calls requestList; the game then
    answers as for any page change. The Lua side only shortens the shown
    array and writes variables 5 and 2 (entry = where the previously
    highlighted item now is, else 0).
  * FFDec roundtrip of this script: pcode_flow flags initListBase,
    waitResumeList, selectMixMaterial, changeRecipeMaterial, isCancelExit;
    all equivalent encodings (`iftrue` -> `not; iffalse`, `equals; iffalse`
    -> `ifne`, `decrement` -> `pushbyte 1; subtract`, a switch's default
    pushed as -1 instead of an extra case). No source fixes needed.
  * Live redraw confirmed in game. Never refresh while the game handles a
    key: a forced refresh on the Enter that also reached the game (decide)
    left Customize stuck - EditChara (vtable 0x10a0098) +0x2f8 = 0xce (a
    list open; 0x192 = none) while the ItemList was hidden (+0x1f4 = 1), no
    input worked (probe: `ClaudeTools\battle_styles\editchara.py`). Item Search now
    refreshes only when the query changes.
  * **Favorites (2026-10):** star (the game font has ★), F / L3 toggle,
    persistence, search and favorites-first seen in game. The refresh tag:
    `0x5E000000 | favoriteCount << 14 | 0x2000 (they follow one leading
    row, "None") | entry` (mask 0x7F000000 for the tag, count bits 14-23 up
    to 0x3FF, bit 13 the offset, entry bits 0-12 up to 0x1FFF; the old
    0x5EA0000 form is gone). The script keeps `m_favorite_first` /
    `m_favorite_count` (count reset in startList and before a sort request)
    and changeListPage appends a gold `<font color="#FFC800">★</font>` to
    rows in that range (SetButton's name is htmlText). The Lua side orders
    the shown array None, matching favorites (by "type:id", whole 0xb0
    entries, game order), the rest. Per Steam account in
    `mods\ItemSearch\favorites.sav` (account from the save image,
    battle-styles-loading-and-online.md "Saves"). Toggle: F (no Ctrl) or the left stick click
    (the loader's `isPadButtonPressed(PAD_LEFT_THUMB)`, XInput through the
    game's own xinput DLL), Customize modes only, list idle: first 15 (equipment, Super Souls); the
    skill lists searched but F/L3 did nothing (seen in game 2026-10-10), so
    now 14-16 (the script's ModeType_CustomizeItem/Skill/Costume) - the
    skill lists' own mode value not probed; confirmed in game. Offline
    check: `ClaudeTools\itemsearch\run.bat`.
  * **Bug seen in game: stars only on a list's first opening, and moving
    from a favorite jumped to the item's natural place.** endList does not
    remove updateNewFlagNotify, so the closed list's script kept running
    checkSearchRefresh and took the tag meant for the next opening; startList
    then reset the count, and waitSetupList put the game's cursor (variable
    6, the item's natural index) into ListBox.m_select_list, which
    SelectNext/SelectPrev move from. Also the equipment screen's ItemList
    keeps ticking while hidden (+0x1f4 = 1), so the Lua side saw no new list
    and sent nothing when the game kept the arranged array. Fix (not seen in
    game yet): `m_search_ready`, true only while checkKey listens (set at its
    two addEventListener sites, cleared at every removal: endList,
    requestList, decideList, pauseList, finalize), gates checkSearchRefresh;
    Lua sends the tag again after the list was hidden (state 1 -> 2) and
    always writes the cursor +0x234 = the tag's entry.

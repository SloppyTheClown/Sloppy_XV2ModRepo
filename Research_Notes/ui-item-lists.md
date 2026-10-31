# UI: item lists (Menu::ItemList)

Covers: Menu::ItemList in memory and its ITEMLIST.iggy script - the shown array, row redraw, the vtable slot used by Item Search.
Split out of `ui-iggy-and-textures.md` (2026-10-22).

## Item lists (Menu::ItemList)
Equipment lists (torso ... Super Soul), shops: one Menu::ItemList object,
vtable 0x10b86a0; slot 9 = per-frame update 0x5422d0 (rcx = list). It keeps
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
* Functions: 0x543b80(list, wstring* out, shownIndex) row name as shown;
  0x543130(list) shown = all filtered by category + sorted (no reload);
  0x546730(list, 0) reloads the all array from the inventory (and rebuilds);
  0x545e30(list, topRow) sends the visible rows to Iggy; 0x545980(list) row
  icons (Iggy int variables 0x3f..0x49); 0x2c82a0(menu, variable, int) sets
  one Iggy int variable (6 = selected entry, 7 = selected row on screen,
  from the cursor move at 0x5447a8); 0x541370(list) character preview.
* Command handler 0x544d10 (Iggy -> game, jump table 0x544860): cmd 1 =
  category change (0x544d5f: rebuild, 0x5465e0 find page, push page,
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
    ListPosition, 12 PauseList. Setters 0x2c82a0/0x2c81f0/0x2c8340 (int /
    float / string) store the value, valid flag = 1, menu +0x54 dirty.
  * Game-side refresh after a reload: 0x5488b9 (reload 0x546730, rebuild,
    clamp cursor/top, variables 6/7, push page, preview) - also no variable
    2, so it relies on the script asking too.
  * The fix (ItemSearch): a patched ItemList script
    (`ItemSearch\as3\ItemList.as`, built by `ClaudeTools\iggy\itemlist_abc.py`,
    shipped as `ItemList.abc` + `ItemList_base.abc` (FFDec merge base) +
    `ItemList_game.abc` (vanilla, 59410 bytes / FNV-1a 64
    0x8dd6dfe18ce480c6); since 2026-10-19 item_search.lua registers it
    with `uilib.EditScript`, which merges it into the menu as it loads
    (offline `xv2ui merge`: 12 methods changed, 11 traits added, 0
    conflicts, "same semantics" as ItemList.abc; files in
    `ClaudeTools\uilib_test\itemlist\`). Before: `files.ReplaceIggyScript`
    over the vanilla script only; older versions installed a loose
    `data\ui\iggy\ITEMLIST.iggy`). checkSearchRefresh,
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
    left Customize stuck - EditChara (vtable 0x10ae228) +0x2f8 = 0xce (a
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

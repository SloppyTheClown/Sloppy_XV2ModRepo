# Character select search + favorites (CharaSearch)

Covers: CharaSearch (now chara_search.lua; was CharaSearch.dll) - the Lua
port, why it first was a DLL,
how it shares CHARASELE's script and the SelectChara callback with
Battle Styles, the grid as a view of the game's list, the AS <-> DLL
channel (9902/9904, user data 40758), favorite stars, name matching,
xv2patcher's user-data slot mapping. Was part of Battle Styles; its
menu notes are in battle-styles-ui.md.

State (2026-10-10): split out of Battle Styles, NOT all of it seen in
game - see the bullets below.

**Ported to Lua, `CharaSearch\chara_search.lua`, 2026-10-15** (installed on
the PC, NOT seen in game). The three blockers below were solved in the
loader library instead (`sdk\modloader_lib.lua` "menus", `files`):
1. Callback -> Lua `menus` table (Lib\src): `menus.HookCallback(name,
   vtable, slot, {[sendType]="number"|"string"}, mutex)` chains the vtable
   slot (same mutex as Battle Styles, `XV2Mods_SelectCharaCallbackHook`),
   copies the taken messages into a queue (AS strings copied during the
   call) and passes every other sendType on. `menus.Messages` drains it
   each frame; `menus.LastSeen(name, 9904)` <= 0.1 s = grid open. The
   answer: `menus.SetUserDataString(name, -1, text)` is written at the
   menu's next taken message (the AS sends 9904 every frame and waits up
   to 30 frames; the Lua answer arrives ~2 frames later). The write is the
   same as the DLL's WriteUserDataString (never the game's SetUserDataStr,
   xv2patcher stops the game).
2. Names -> `files.ReadGameFile("msg/proper_noun_character_name_en.msg")`,
   parsed in Lua on the first list.
3. One script per menu -> `ReplaceIggyScript` now keeps one script per
   original (vanilla 0xb52d / 0x18865d7ce449251c -> `CharaSele.abc`,
   xv2patcher 0xba46 / 0xaa2bcacb32cbd1fb -> `CharaSele_XV2Patcher.abc`;
   both written raw by `ClaudeTools\iggy\menu_abc.py`, sizes/hashes as
   before so Battle Styles still accepts them). Battle Styles'
   `CharaSearchActive()` now asks `XV2Files_Owner("ui/iggy/charasele.iggy")
   == "CharaSearch"` (the script's LUA_NAME), falling back to the old DLL
   export. Paths are matched lower case with '/'.
Chain order: if Battle Styles serves CHARASELE first, the loader's status
for the menu reads "NOT patched" - expected. Offline:
`ClaudeTools\charasearchtest\run.bat` (script with stubbed menus/search,
real names through the library: filtering by name/code/"CaC", no-match
note, favorites toggle/save/reload, padding). The old C++ source is kept
outside the repo. The section below describes the DLL version.

**Moved to its own DLL, `CharaSearch\` (CharaSearch.dll), 2026-10-10.**
Battle Styles keeps only its style row and the layout dump. Split confirmed
in game (user, 2026-10-10); a tidy-up pass is still owed.

**Why a DLL and not a Lua script** (the user preferred Lua; the loader's Lua
API, XV2ModLoader README "Lua scripts", can't do these):
1. **Answering the menu's AS callback.** CharaSele.as asks for the grid order
   and toggles favorites through the SelectChara callback (slot 17, ids
   9902/9904) and reads the answer from the menu's user data in the same
   call. Lua scripts run once per frame and can only read/write memory or
   patch code (`WriteExec`); there is no way to run Lua when the game calls
   a function, so nothing can answer synchronously. The call also passes AS
   strings (search text, favorite keys) that must be copied out during it.
2. **Character names.** Searching by name needs `chara_<code>_<costume>`
   from the msg files inside the cpks. The loader has no cpk/msg reader for
   Lua; the DLL reuses Battle Styles' C++ CPK + MSG readers.
3. **One script per menu.** `files.ReplaceIggyScript` swaps a menu's ABC
   only when it matches one vanilla size + hash, one entry per menu. Battle
   Styles also rebuilds CHARASELE (style row), so the two must be combined,
   over vanilla or xv2patcher, depending on whether Chara Search is loaded.
   That needs code on both sides (the `CharaSearch_Active` export and the
   variant tables), not a single hash-gated swap.
To move it to Lua later, the loader would need: a Lua hook for a menu's AS
callback (with user data + string arguments), a msg/cpk lookup, and a way
for two mods' scripts on one menu to be combined (or one served variant per
installed set).
* Only one script can replace CHARASELE's. CharaSearch serves the search-only
  script over vanilla or xv2patcher. Battle Styles serves style row + search
  (`charasele_search_abc.h` / `charasele_patcher_search_abc.h`) when
  `CharaSearch_Active()` (export, 1 once both CharaSearch hooks are in;
  found by GetModuleHandleW + GetProcAddress) is true. It accepts vanilla,
  xv2patcher and CharaSearch's two outputs (`charasearch_ids.h`), so the
  file hooks' order doesn't matter. CharaSearch, given anything else, passes
  it through and logs it. The file hooks chain under `XV2Mods_FileRequestHook`.
* Both hook the SelectChara AS callback (vtable 0x10be328 slot 17, vanilla
  0x5e1630) under the mutex `XV2Mods_SelectCharaCallbackHook`. CharaSearch
  takes 9902/9904 and Battle Styles takes 9901 (layout dump). Each passes on
  the rest.
* Favorites: `mods\CharaSearch\favorite_characters.sav`. On start it copies
  Battle Styles' old one if it has none yet (CopyFileW, fail-if-exists).
* Scripts are built with `ClaudeTools\iggy\charasele\build_charasele.py` and
  `ClaudeTools\iggy\menu_abc.py`. Check them with pcode_flow.py as before.

The grid (CharaSele.as, both variants) gets the loader's search box and
favorites; order: the loaded CaC, favorites, the rest. Built by a second
stage after edit_charasele.py: `ClaudeTools\iggy\charasele\charasele_favorites.py
chara_search.as <as3 file>` (inserts chara_search.as's functions).
* The grid is a **view** of the game's list (`m_master_list`, as
  recieveList built it). Everything the game indexes by list position
  reads by master index (`masterIndex(display)`): image strings
  `ReceiveType_ImageStrStart + i` (startMain's faces, setImage) and the
  unlock flags in checkUnlockVar (`i * CharaVarIndexNum + var`). The only
  list numbers sent back to the game (SelectListIndex, RequestNewInfo, the
  "new" icon check) go through `CharaListShiftCount[display]`, which
  setView permutes with the view - so the game never sees our order.
* Channel: AS sends 9902 (string, once: token + per entry `key\tcodes`;
  key `CODE_mid`, `HUM@<avatar slot>`, empty for padding) and 9904 (int,
  every frame from main: master index under the cursor). The DLL answers in
  user data **40758** = `ReceiveType_CustomList_PartnerJudge_End`
  (ReceiveType_Num - 1; the game only uses the start of that range):
  `token\t<shown masters | *>\t<favorite masters>`, via SetUserDataStr
  (pointer kept: a ring of 8 strings). 9902 is answered at once; waitStart
  waits up to 30 frames for it so the grid opens in order.
* Own CaC: the first `HUM` entry whose VarTypeAvatarSlotID equals
  ReceiveType_AvatarSlotID; else the first HUM, else entry 0.
* Applied only while the grid is idle (no player change / decide / page
  scroll); picks keep their character (decided ones stay even when
  filtered out), a vanished undecided cursor goes to the first unlocked
  entry after the CaC. Faces of entries the grid opened without are made
  on first show (faceFor).
* Star: a text field copied from `cha_parameter.sys_charaName` with
  `<font color="#FFC800">★</font>`, on the nest clip (setImage empties
  chara_img); also on the cursor icon `cha_select_cur.chara_sel`. The first
  star logs the icon's layout ("CHARASELE icon": chara_icn 128x64, mask
  -44,-29 89x59, chara_img 205x102). **Don't size or place it by its
  measured size:** right after htmlText is set Iggy reports nonsense (6x4
  once, 30x34 another time). Fixed box: font size 16, left-aligned, 28x23
  (size seen fine in game). **Place it by chara_icn, not chara_img:** Iggy
  gives chara_img no `mask`, so its bounds are the whole 205-wide face
  bitmap (-102..103): bottom-right put stars over the next icon,
  left-middle half a tile left of their own - in both cases most were
  hidden under neighbouring icons or the page arrow (looked like "saved
  favorites get no star"). `setTextFormat` after htmlText also overrides
  the `<font color>` with the format's colour (the name's white): the
  format carries 0xFFC800. Placement by chara_icn, gold: seen in game.
  Saved favorites still got no star (stars made later did, and a reload
  lost them too). First guess, `copyTextField` copying sys_charaName's
  fade-in alpha 0: star now sets alpha 1, didn't help in game. The log
  showed the cause: at open the icon (nest_charaselect0N) has **no
  children yet** - "CHARASELE icon layout" listed only the star. The
  timeline adds chara_img/chara_icn afterwards, above the star, and
  `setChildIndex(star, numChildren-1)` on later redraws did not lift it
  in Iggy. Now: no star until chara_icn exists, and topmost by
  removeChild + addChild. Didn't help in game either. (The layout log is
  written before the star is positioned: its x/y aren't the final ones.)
  18 % in from chara_icn's left edge (was 10 %): seen in game.
  User clue: saved favorites' stars showed only while scrolled onto the
  dimmed edge column. A temporary per-icon dump (instance, label, children,
  star x/y/alpha/visible, getChildByName) every changed frame showed the
  code state was right everywhere: the 9 main-view favorites' stars were
  there, visible, alpha 1, topmost, at -41,-12 - and not drawn. The ones
  that drew were on the edge column's icons (#0-#2), whose stars were made
  later, while scrolling. So stars made during the screen's fade-in don't
  draw, whatever their properties; `copyTextField` copies sys_charaName's
  format/filters at that moment. Fix: `settleFavoriteStars` (poll, once per
  screen, when the name field's alpha reaches 1) remakes every existing
  star and logs "CHARASELE stars remade" with the old and new format. Not
  seen in game yet.
* Matching (now CharaSearch chara_search.cpp Poll): each variation's code, its
  `chara_<CODE>_000` name from proper_noun_character_name_en.msg, "CaC" for
  HUM. Nothing matching shows all with the note "No character matches".
  `favorite_characters.sav`, same JSON as favorite_styles.sav.
* pcode_flow.py: only the edited methods (waitStart, startMain, main,
  setImage, updateCharaIconUnit, checkUnlockVar, inits) differ besides
  the known noise; lookupswitch counts unchanged (4).
* **xv2patcher maps user-data slots:** its new_chara.xml hooks the
  setters (send_int/flag/string, `resolve_target`) and maps every slot
  the game passes from the vanilla layout (ReceiveType_Num 20563,
  CharacterMax 256) to its own (40759, CharacterMax 850). The first try
  passed 40758 and xv2patcher stopped the game on entering the character
  select ("ResolveCode: Cannot map value 0x9f36"); the vanilla slot 20562
  stopped it too ("0x5052") - the hook maps by the caller's return address
  and the slot's range, and ReceiveType_CustomList_PartnerJudge_End isn't
  in any range it maps. Fix: don't call the setter. SetUserDataStr
  (0x2c8240, read from the image past xv2patcher's entry jmp) only does
  `entry.value = text; entry.type = 6; entry.set = 1; menu[+0x54] |= 1`
  for a slot inside the +0x160/+0x168 vector; the DLL does the same
  writes (WriteUserDataString) on the vector's last entry (= ReceiveType_Num
  - 1 in either layout). Confirmed in game (no popup).
* Seen in game: CaC first then favorites, favorites kept across visits,
  the search box (0.35/0.03/0.30) fine.

## Second player's starting cursor (2026-10-15)
Seen in game (Lua version): after the first pick, the next player's cursor
sat on the last grid entry or on the first CaC that isn't the loaded one,
not on the next free entry. Cause (from the script, as3\CharaSele.as):
`waitUnlock` gives every player a starting entry before the grid is
reordered - the first unlocked one in the game's own list order, CaCs
skipped for players other than the own one unless
ReceiveType_FlagSelectAvatar (or a custom list) allows them. `setView`
then kept each pick on its character, so player 2 started wherever the
game list's first entry landed in our order. `changeCurrentPlayer` only
moves the cursor (changeUD down) off a CaC that is already picked.
Fix: `resetStartPicks()` (as3\chara_search.as), run once after the first
answer in `startCharaSearch`: the same rule over the shown grid, for every
player but the own one. Built into CharaSearch's two .abc files and Battle
Styles' combined scripts (charasearch_ids.h updated). Seen in game: now
consistent, but always the top of the grid - wanted is the next entry after
the one just picked (solo Training: player, then opponent).

Second fix: after a decide, main's m_flag_change_player block calls
`changeCurrentPlayer(true)`, which only loads the next player's stored
start (`m_select_info[player][SelectInfoTypeListIndex]`). The stage
(charasele_favorites.py) now wraps it: `decidedPick` reads the deciding
player's cursor position (setSelectChara keeps it in m_select_info) before
the index moves, `startAfterDecided` sets the next player's start to
`nextStartPick(that)` - the next entry after it (wrapping) that is unlocked,
not padding, and not a CaC unless avatars are allowed. Going back (cancel,
param false) keeps the earlier player's own pick. Installed on the PC, not
seen in game.

# Multi accessory: the Presets panel (AVATAR_SET.iggy)

Built 2026-11-01 for game 1.27; **not seen in game yet** (the layout below is
from panel coordinates, not from a look at the screen).

Save/load was already there: `MysetCallbackHook` copies a slot's picks between
preset n and preset 0 on sendType 2 (load) / 3 (save). This adds the display:
rows "Accessory 2" and "Accessory 3" under the panel's equipment rows, showing
the extras of the preset under the cursor.

## How the text gets there
`ReceiveType_Num` fixes the panel's user-data count, so no new slots: same
trick as Battle Styles' row (`battle-styles-ui.md`). After the game's callback
handles sendType 0 (fill the panel from preset n; 0x5071f0 sets user data 2..8
to the equipment names), the DLL replaces user data 6 (accessory, 2 + 4) with
`<game's text>\t<extra 1>\t<extra 2>`, and `as3\AvatarMyset.as` splits it.
* The DLL only appends after the script sent sendType 9100
  (`SendType_MaScriptReady`, from `maMakeRows`, never passed to the game), so a
  panel without our rows never shows a tab, whatever happened to the edit.
* Part set -> name. Picks are saved as part sets. Raw accessory IDB:
  `0x86aa60([0x1568308], 6)` (0x61aa00's table maps item type 4 to file 6): +8
  count, +0xc first record, records of 0x310, u16 id at +0, MODEL at +0x30. The
  first record with that MODEL gives the item id (items sharing a model give
  the first one's name; checked against 0x61af70). No record: `#<part set>`.
* Id -> text: `0x507070(menu, wstring *out, 4, id)`, the function 0x507840 uses
  for the panel's own accessory name. It sets up `out` itself (no init needed),
  id < 0 gives the game's "none" text, and 0x678a00 never returns null. A long
  name's buffer comes from the game's allocator, so it isn't freed; each id is
  asked once and kept.

## Layout
The panel sits at 30,30 in a root at 100,-1 in menuwin at 339,74, so its window
starts at y 103 on screen, 570 tall (596 with Battle Styles' row: ends at 699
of 720). Two more rows at 24 each would run off the screen, so `maMakeRows`:
* moves every panel child with y below the last equipment row down by 48
  (skill header, skill rows, their boxes), grows the window (the child covering
  first row to last skill) by 48;
* moves the panel up 48: its bottom stays put, its top goes to y 55. Whether
  anything sits at y 55-103 at x 469-1029 is what the in-game test must show.
* Rows have no box (`_CMN_M_B_plt_list01` can't be constructed at runtime).

## With Battle Styles
Calls sit where Battle Styles' edit doesn't touch, so the instruction merge
takes both: `maMakeRows` between `initEquipList`'s two loops (so it always runs
before `makeStyleRow`, at the function's end), `maShowExtras` between
`changeEquipList`'s two loops. Row names start with `sys_` so Battle Styles'
`overlapsRows` sees them at y 260 and falls back to "row under the skills"
(copied from the moved sk_08, window grown again).
Offline: `ClaudeTools\iggy\myset_merge_check.py` (both orders + each alone),
`roundtrip_check.py <jar> ma_myset`.

# Iggy display lists (sprites, placements, frames)

Covers: how an .iggy movie stores a sprite's timeline (PlaceObject /
ShowFrame / Remove / FrameLabel tags), the record layouts by flags, what
has to change to add a button (clone a placement) or resize a box
(matrix), and the AVATAR.iggy edit_menu / editmenu facts the Skills page
row needs. Static analysis of 1.27 AVATAR.iggy; the clone/matrix edit is
not yet confirmed in game.

Offsets are **movie offsets** (the movie subfile starts at file +0x40 in
AVATAR.iggy; `ClaudeTools\iggy_dl\iggywalk.py` finds subfiles and walks
the pointer index). Pointers are 64-bit, self-relative in the file and
listed in the index (ui-iggy-and-textures.md); a value of 1 = null.

## Sprite definition (char type 03 ff)
* u16 +0x28 frame count, u16 +0x2a **depth count** (highest depth + 1),
  +0x38 first tag pointer, +0x40 last tag pointer.
* Tags are a singly linked list: +0 next pointer (1 = end), byte +8 type.

| Type | Tag |
|---|---|
| 1 | ShowFrame (ends a frame) |
| 2 | PlaceObject (place or move) |
| 3 | Remove |
| 5 | FrameLabel |

## PlaceObject records
Same for every variant: matrix 6 x f32 at +0x10 (a b c d tx ty), u64 depth
at +0x28, character pointer at +0x30. Byte +9 = flags, which set size and
name location:

| Flags | Size | Use | Name |
|---|---|---|---|
| 0x41 | 0x70 | first placement with name | ptr +0x40 -> inline UTF-16 at +0x60 (16 bytes) |
| 0x07 | 0x80 | move with extra data | - |
| 0x05 | 0x60 | move | - |
| 0x21 | 0x80 | import-style placement | ptr +0x50 |

Other flags were not seen in AVATAR's edit menus; an editor should refuse
them.

## Adding a placement ("clone")
Append a copy of each source PlaceObject (with its index fields), set its
depth to the old depth count, bump the depth count, shift tx/ty, rename
it, and link it right after its source (copy.next = source.next,
source.next = copy; fix the last-tag pointer). Frames stay the same
because the ShowFrame tags are untouched. Implemented as the UI library's
display edit (`XV2UI_EditDisplay`, `clone`/`matrix` commands; offline
`xv2ui display`, check `ClaudeTools\iggy_dl\check_display.py`).
Unverified: whether the runtime needs anything beyond the list and depth
count (no other per-depth table was found).

## AVATAR.iggy edit menu
* **edit_menu** sprite (def 0x90c8, id 45, 19 frames, depth count 442):
  frame 1 "wait" places btn_9..btn_1 at x 32, y 375/340/306/272/238/204/
  170/136/102 (row pitch 34), depths 1, 56, ..., 441. Labels out = 6,
  out_comp = 10, in = 15. Frames 7-10: 0x07 moves; 11-19: 0x05 moves;
  every row moves by the same offset in a frame; no Remove tags.
* **editmenu** sprite (dc90, id 55, 6 frames, depth count 445) holds the
  backing at depth 1 (`_CMN_M_B_plt_win01`, 512 px tall), edit_menu at
  depth 2 (frame 3 moves it to y -42), the tab at 444. Depth 1 per frame:
  f1 0x21 [1.41, 0.822, -30, -30], f2 0x05 [1.41, 0.928], f3 0x21 [1.41,
  0.879, -30, -130], f5 0x21 [1.71, 0.82224, -30, -30], f6 0x21 [1.21,
  0.812, -30, -130]. Frame 5 = the Skills page.
* One more row: `clone edit_menu btn_9 btn_10 0 34` and frame 5 scale
  d + 34/512 (`matrix editmenu @1 5 * * * +=0.06641 * *`; `+=` is relative,
  so Ki Blast Override and Battle Styles each add a row: 0.95505 with both,
  checked offline). The box is a timeline placement (per-frame matrix), so
  the script can't resize it for good: the next frame's tag sets it again.
* A cloned button also needs what the frame scripts do to it: editmenu sprite
  dc90's frames 1,2,3,5,6 run `stop(); for(i=1; i<=9; i++)` over `btn_i` to
  stop btn_off, btn_on.ef, sys_cont(_imtg) and sys_header(_imtg) on the page's
  frame (6 frames, one per page). The 9 is hardcoded, so unstopped btn_10 was
  short (btn_off frame 1 is 608 wide, not 762), blank and flickering, with
  sys_header/sys_cont looping and replacing their `sys` TextField. Fixed in AS
  (`kiBlastSyncRow`), after the editmenu gotoAndStop and at the head of
  setupEditMenuItemBase. Depth and tag order were not the cause.
* AS side: `Avatar.EditMenuItemNum` (9) bounds the button loops,
  `getButtonEditMenu` and `new ButtonList`; KiBlastOverride uses literal
  10 there and leaves EditMenuItemNum (colour tables depend on it).

Tools: scratchpad-style dumps `sp1.py` (sprite tags) and `sprites.py`
(find a sprite by placement name) were used; `iggywalk.py` keeps the
reusable parts. `ClaudeTools\iggy_dl\sprite_tags.py` lists a sprite's tags and
`abc_names.py` prints ABC constant pool strings and multinames.

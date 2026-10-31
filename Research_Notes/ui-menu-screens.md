# UI: individual menu screens

Covers: notes on specific menus: the lobby map (LOBYHUD), the stat radar chart (CMN_CHART), the attributes screen shared by two mods.
Split out of `ui-iggy-and-textures.md` (2026-10-22).

## Lobby map (LOBYHUD.iggy, classes LobbyHUD / LobbyMap)
* The exe sends lines through user-data slots ReceiveType_MapInfoStart+i:
  `Event:id=<icon 1..22>,x=<%>,y=<%>`, `BArea:`, `BeaCon:`, `Start`, `End`,
  `PageL`, `PageR`. LobbyHUD.parseMapInfo splits on `:` `,` `=` and
  parseInt()s every value (a text value becomes NaN). x/y are percent of
  point1..point2 on map_inact; icons are copies of `map_inact.icn_<id>`.
* Exe side: 0x3e9b60 walks the lobby NPC list (manager 0x15747c0) and calls
  0x577310(map, [npc+0x1c], type [npc+0x34c], npc+0x2d0 position); type ->
  icon id by jump table 0x577380 (2->1, 3->15, 4..9->2..7, 0x10->8, ...,
  0x64/0x65 from npc flags 0x10/0x20 -> 9/10); [npc+0x1c] 2..12 -> fixed
  map position slots (0x577500). The line is "Event:" + sprintf
  "id=%d,x=%d,y=%d" appended with std::string::append (0x7a740) at 0x577454.
* NPC object: +0x42 and +0x142 wide strings (0x4aab00(npc, 0/1)); the OPC
  name plate prints +0x42 with %ls - it is the NPC's shown name (confirmed
  on the map). Undiscovered mentors get no map line at all. Mentor icons: classes po_icn_sensei /
  po_icn_sensei2 ("sensei" = mentor); which icon id they are was not
  established (the iggy's display list was not decoded) - the AS3 tests
  the class instead.
* Legend text: lobby_hud_text LBY_HUD_02_000..016 (Instructor = 002,
  Instructor Event = 006).

## Stat radar chart (CMN_CHART.iggy, class _CMN_CHART)
* One shared symbol in CMN_CHART.iggy (46714 bytes, ABC 2047 bytes, hash
  0x98335341d032597c). CHARASELE and PLAYERDATA_CHART import
  "../../COMMON/swf/CMN_CHART.swf" at runtime (PLAYERDATA_CHART has roots
  `_CMN_CHART_root{,_charasele,_clothes,_myset,_slot}`), so patching
  CMN_CHART's own ABC reaches every chart. AVATAR_FACULTY only has
  `_CMN_CHART_icn` (its row icons, frame = stat 1..6) - an earlier note
  that it imports the chart was a false positive of the "_CMN_CHART"
  search; the Attributes screen's chart is probably PLAYERDATA_CHART's.
* `_CMN_CHART` is top level (no package), a timeline that only stop()s at
  frames 17 and 23; children color_fill_before/after, color_line_after,
  icn_1..6 (+ icn_Narw arrows), plt, dammy, dammy_def, fill_dammy. The
  polygon is moved from outside (game/host menu), not by this class.
  icn_5 / icn_6 = Strike / Ki Blast Supers (same frames as the Attributes
  rows).
* The axis labels (HEA KI STM ATK STR BLA) are images: txt_cmn_radarct /
  txt_cmn_radarct_ex in cmn_chart_<lang>.iggytex, not text fields - new
  text has to be a runtime TextField in a font the movie already has.
* Graphics drawing API: iggy_w64.dll carries the whole native method table
  (moveTo, lineTo, curveTo, lineStyle, lineGradientStyle, beginFill,
  beginGradientFill, beginBitmapFill, endFill, clear, drawRect,
  drawRoundRect, drawCircle, drawEllipse) plus the JointStyle / CapsStyle /
  LineScaleMode / SpreadMethod constants and the render path
  "(iggy/render/object)iggy_draw_Graphics" (static search of the DLL's
  strings, 2026-10-15). Not yet seen drawing in game - the first use is
  Combined Supers' Supers box and chart link below.
* Combined Supers' chart link: `CombinedSupers\as3\_CMN_CHART.as` (built
  by `ClaudeTools\iggy\menu_abc.py cs_chart`; menu_abc treats a class
  name starting with "_" as top level) adds a bounded ENTER_FRAME wait in
  the constructor (icons present and still for 3 frames, at most 90),
  then a Sprite: two interlocked gold links rotated along the icn_5-icn_6
  edge at its middle, and "SUP" outside it (font copied from the first
  TextField a bounded breadth-first search under root finds; stroke-drawn
  letters when there is none). Served through the loader's
  ReplaceIggyScript while the supers are combined.

## Attributes screen: one script, two mods
* AVATAR_FACULTY.iggy (79065 bytes, ABC 14221 bytes, hash
  0xe1bda563c2c786aa), class action_script.AvatarStatus. General_QoL (free
  attribute points) and Combined Supers (Supers box) both change it, so
  there is one source, `General_QoL\as3\AvatarStatus.as`, built once by
  `menu_abc.py qol_faculty` into both mods' `src\avatar_faculty_abc.h`.
* Feature flags: the constant string "XV2FACULTY:00" - char 11 =
  General_QoL ('1' free points), char 12 = Combined Supers ('a' additive
  total = sum, 'h' highest = max, '0' off). The DLLs rewrite the characters
  in place (same length, so the ABC layout and the iggy patch stay valid).
* Serving, either hook order:
  - General_QoL (own file hook, iggy_abc.cpp PatchAvatarFaculty): vanilla
    -> shared ABC with its flag set; a file whose active ABC holds the
    marker -> copy with its flag set (left alone when already '1').
  - Combined Supers (loader ReplaceIggyScript, menu_scripts.cpp) registers
    two originals: vanilla -> "0"+mode, and General_QoL's "10" copy (hash
    computed at runtime from the embedded ABC) -> "1"+mode.
  - Both are checked offline by `General_QoL\tests\run.bat` (maptest
    faculty: vanilla, again, and over Combined Supers' "0a" copy).
* Supers box (AS side): initEditor blanks btn6's name and gives btn5
  "Supers"; a bounded ENTER_FRAME (rows' bounds unchanged 3 frames, at most
  90) then draws a Sprite on stacusmenu: gold rounded-rect trim around both
  rows (union of their visible children's bounds, without ef_decide and the
  arrows) with a short chain on each side at the seam; a "Supers" label
  and a gold total field, both copied from the rows' own fields
  (SaveSync's saveSyncCopyField way). The rows' values stay editable,
  sys_nmb dimmed to 0.45 (0.8 while selected). The total reads user data
  9 + 10 (ReceiveType_ParamDataStart 5 + rows 4, 5) and updates in
  updateEditorButton / changeStatusSelect; endMenu's waitEnd and
  TestDestroy take it all back out.

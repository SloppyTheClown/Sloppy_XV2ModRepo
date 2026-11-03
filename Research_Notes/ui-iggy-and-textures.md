# UI: Iggy menus, .iggytex/.iggyted textures, EMB, msg text

Covers: menu title art, the Iggy texture container formats, how to tell
which texture numbering an .iggy uses, EMB image files, and how menu text is
looked up. Context: General_QoL's replay-NPC menu title and the Revamp
bridge's texture renumbering (revamp-on-1.26.md). Game 1.26.01.

Moved to their own files (2026-10-22):
* `ui-item-lists.md`: Item lists (Menu::ItemList)
* `ui-iggy-display-list.md`: sprite timelines (PlaceObject records), adding a button / resizing a box by display-list edit
* `ui-menu-screens.md`: Lobby map (LOBYHUD.iggy, classes LobbyHUD / LobbyMap); Stat radar chart (CMN_CHART.iggy, class _CMN_CHART); Attributes screen: one script, two mods

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
  zero the whole 16-byte DXT5 block. General_QoL does that (since
  2026-10-19 as a UI library image edit of texture 14, file hook as
  fallback); not yet confirmed in game - if the title still shows, the texture is not read through the
  hooked request's vt+0x20 (the log does show cmntitle_en.iggytex edited, and
  memory held only the edited copy).
* Other UI: QUEST.iggy = quest menu frames only; STORETITLE / CMNTITLE_TTL /
  CMNTITLE_BATTLE are other screens.
* Preview: stdlib DXT5 decode -> PNG with zlib (no PIL);
  `XV2_RevampBridge\tools\ddspng.py`.

## Formats
* .iggytex / .iggyted (same format): magic "oggytex\0", u32 count at 8, a
  u32 at 12 (16 in every game file), then per texture {u16 id, u16 file,
  u32 size, u32 offset, u32 0} from 16, each a DDS. Sizes/offsets are plain
  values - they often end in 0x80 only because of the 128-byte alignment,
  there is no flag bit. `file` is 0 for a held texture in every cpk
  iggytex checked (3832 entries, 2026-10-19) - not 0xFFFF. The UI library
  keeps a replaced texture's entry as it was and writes 0 for one it adds
  (it wrote 0xFFFF until 2026-10-19 - then General_QoL's output differed
  from its old whole-file edit at CMNTITLE 0xE2-0xE3; now identical).
  Writer: header 16 + 16/texture, textures at 128-byte alignment, no tail -
  reproduces vanilla byte for byte. Size 0 / offset 0 = a reference to
  another file's texture. Match references by id, not by order.
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
* Index byte code in full (disassembled iggy_w64.dll 0x59860..0x59bb1,
  fields 0x590a0 / 0x592a0; checked over 194 game .iggy files + 58 iggytex/iggyted with
  XV2UserInterfaceLib's `xv2ui roundtrip`: 0 failures, 24423 method bodies
  re-encoded):
  * Starts with a type table: u8 count, per type {u8 size, u8 field count,
    (u8 offset, u8 kind) per field}.
  * Commands: `00..7f` one struct of that type; `80..bf t` (op-0x7f) structs
    of type t; `c0..cf` skip 2*(op-0xbf) bytes; `d0..df n` n+1 values of
    kind (op & 15) (2 = 8-byte pointer, 4 = u16, 5 = u32, 6 = u64);
    `fd size k (off kind)*k` one ad-hoc struct; `fe n` skip n+1; `ff u32`
    skip u32; `fc x` nothing.
  * Field kinds: 2 = self-relative u64 -> absolute pointer (1 = null);
    4/5/6 endian swaps (no-ops on PC); 3 bit-reverse (other platforms);
    8..11 = replaced by a runtime u64 (context[0][k-8]); 12..15 = runtime
    byte (context[1][k-12]); 7 nothing. Game files use only 2, 8 and 12.
  * Survey: 496,912 pointers (156,794 null), all targets inside the movie;
    ops t 167k, c 56k, tn 23k, fe 7.3k, fd 802, ff 754, d2 191.
  * Index subfiles are capped at ~16 KB: gasha (3), kokuzouko (2),
    lobymenu_bg (3), result_je (2) have several id-0 subfiles after the
    movie. Each is a complete stream (own type table) walking from movie
    offset 0; chunks 2+ start with `ff <where the previous ended>`. The
    last one ends at the movie size.
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
  count where the menu's own ctor passes it (EditChara: byte 0x4f72b7,
  battle-styles-ui.md "Equip screen UI").
* Adding to a multi-mode menu (EditChara): first read the live mode
  (`ClaudeTools\battle_styles\editchara.py`) - AS lists and exe sub-objects
  differ per mode (the in-game Customize is mode 3 and has no AVATAR_LIST).
  Fill new slots wherever the game fills its own (the push at menu
  start), not only on a request, or they start grey.

## Several mods editing one menu script (XV2UserInterfaceLib merge)
Before: each mod shipped a whole ABC, and mods that shared a menu shipped
hand-combined copies, picking one at run time (Battle Styles + Chara
Search on CHARASELE). Now `XV2ModLoader\UILib\src\formats\abc_merge.cpp`
(`MergeScript`) applies each mod's edit in turn over the current script.
The offline check is `xv2ui merge <game.iggy> [-base <mod base>] <mod.abc>... [-expect x.abc]`.
* **Matching by name, never by index.** An edited ABC renumbers its
  pools. Methods are compared as canonical text: operands resolved,
  branches as instruction numbers, debug ops ignored.
* **Private namespaces.** The game's are named "" and FFDec's
  "action_script:CharaSele", so the canonical form ignores private
  namespace names. Pair them via a trait both sides have.
* **FFDec noise (found the hard way).** FFDec recompiles every method of
  a class it touches (`iftrue` -> `not` + `iffalse`, extra `convert_i`,
  methods renumbered: waitStart is method 7 in the game, 231 in Battle
  Styles'). Against the game's script, every method of an FFDec-made mod
  looks changed: 124 for Battle Styles, 10 false conflicts for Chara
  Search.
  * Fix: a **mod base**, the game's ABC with the same class recompiled
    unchanged (`ClaudeTools\iggy\ffdec_base.py`).
  * "Did the mod change it": mod vs mod base.
  * "Did an earlier mod change it": current vs the game's script.
  * With the mod base, Battle Styles shows 5 real method changes and 18
    added traits.
* **Trait ids of added traits.** The game's compiler numbers static
  traits (slot ids 1..n; disp ids where a class has static methods) and
  leaves instance traits at 0. A merge that left added consts at 0 among
  numbered ones and kept two mods' own disp ids (both 3..) gave a blank
  CHARASELE in game (2026-10-19, likely cause - fix not seen in game yet).
  Now each added trait gets max+1 of its id space in that list (slots /
  consts / classes / functions share one, methods / getters / setters the
  other), 0 when the list numbers nothing. `xv2ui traits` prints the ids.
* **Both changed one method.** diff3 per instruction (`diff3.cpp`; keys =
  op + resolved operands, debug ops stripped).
  * Branch targets are moved through the diff's position maps.
  * The mod's new registers are shifted past the ones the earlier mod
    added (getlocal0-3 / setlocal0-3 become getlocal / setlocal).
  * Two pure inserts at one place keep both, the earlier first.
  * It gives up (the later mod wins, reported as a conflict) on: a changed
    signature, try blocks, activation traits, or dead-code raw targets.
* **Result.** Battle Styles then Chara Search on CHARASELE: 0 conflicts
  (static init and changeCurrentPlayer merged at instruction level),
  "same semantics" as the hand-combined `kCharaSeleSearchAbc`. Reverse
  order: only the static init's constant order differs.
  * `-expect` with an FFDec-made expected file counts "FFDec form there,
    game form here" as the same (`CompareScripts` game + base).
* **XV2Patcher's CHARASELE** (xv2ins 4.7's loose `data\ui\iggy\CHARASELE.iggy`,
  267589 bytes, ABC 0xba46 / 0xaa2bcacb32cbd1fb; the cpk's is 0xb52d /
  0x18865d7ce449251c): merging over it needs the patcher's own FFDec base
  (`ffdec_base.py` over the patcher's ABC), not the vanilla one. Battle
  Styles then Chara Search: 0 conflicts, "same semantics" as the old
  `kCharaSelePatcherSearchAbc`; reverse order: static init constant order
  only (2026-10-19, `ClaudeTools\uilib_test`).
* **Menu versions.** One edit can't cover both CHARASELEs, so a mod
  registers one script per version, each with its `game` (the ABC it was
  built over): the library keeps them per owner + path (`ScriptEdit
  ::variants`) and the stage uses the one whose `game` equals the menu's
  original script byte for byte; else one registered without `game`; else
  the first, merged over its own `game` (status says "made for another
  version of the menu"). The same owner + `game` again replaces that one.
  `menu_abc.py` writes all three per version for the CHARASELE menus
  (`UILIB_MENUS`): Lua mods get `<name>.abc`, `<name>_base.abc`,
  `<name>_game.abc`; DLL headers `k<prefix>Abc/BaseAbc/GameAbc`.
* **In game: the loader's file pipeline.** `XV2UserInterfaceLib.dll`
  (`UILib\src\ui_lib.cpp`) registers each mod's edit as an
  `XV2Files_AddStage` stage (`Lib\src\file_patches.cpp`): on every read of
  the menu, stage N gets the .iggy as stages 0..N-1 left it plus the
  game's original, takes the script out, `MergeScript(game, current, mod,
  modBase)`, puts it back (`IggyDocument::SetScript` + `Write`). The
  3-way "game" is the original's script unless the mod passes the one it
  built from (needed when the original isn't vanilla, e.g. XV2Patcher's
  CHARASELE: otherwise the patcher's edits count as "the game" and a
  method the mod also changed silently loses them). Results are cached per
  input (hash, size), so a menu that reopens is not merged again. Offline
  equivalent: `xv2ui merge ... -o out.iggy` (round-trips). Menus confirmed
  working in game (user, 2026-10-18). First users (2026-10-19, not seen in
  game): Battle Styles (`XV2UI_EditScript`, owner "BattleStyles") and Chara
  Search (`uilib.EditScript`, "CharaSearch") on CHARASELE, replacing the
  hand-combined scripts. A DLL that needs to know whether its edit reached
  the menu reads `XV2Files_Status`: "served: ..." with one stage (then
  `XV2Files_Owner` says whose), "owner: served: ...; owner: ..." with several.

## Several mods editing one texture file (XV2UserInterfaceLib images)
* Texture files go per image, not per file: a mod registers the images it
  replaces or adds (`XV2UI_SetImage`, Lua `uilib.SetImage`); its stage
  (`UILib\src\formats\image_edit.cpp ApplyImages`) parses the current
  .iggytex/.iggyted/.emb, swaps or appends only those, writes it back with
  the formats' writers (byte-identical round-trip, textures 128-aligned,
  EMB files 64-aligned). Mods touching different images all apply; the same
  image from two mods: the later wins, noted in loader.log ("changed by an
  earlier mod" = current differs from the game's original there).
* Names: iggytex = the texture id the .iggy uses (low u16; the entry is
  written with file 0, so a reference to another file's texture turns
  into one held here); EMB = entry name, "#n" for EMBs without a name table
  (every ui\texture and ui\sprite EMB checked so far has names).
* Offline: `xv2ui images <game file> name=img... [-then ...] -o out`;
  `ClaudeTools\uilib_test\images\run.bat` stacks two mods on CMNTITLE_en,
  apl.emb and advertise.emb (names dropped) and checks every untouched
  image stays the game's. Built 2026-10-19, not seen in game (no user yet).
* First user: General_QoL's "Future Saga" blanking (texture 14 of every
  CMNTITLE_<lang>, built from the game's texture at start; its
  `tests\run.bat` "titles" compares with `xv2ui images`).
* Not covered: two mods editing different regions of one texture (e.g.
  title bands inside texture 14) - that needs a DDS block-level merge.

## msg text
* data\msg\*.msg: UTF-16 entries; the game looks text up by entry NAME, not id
  (mods renumber ids freely). Edits: append the new UTF-16 string and repoint
  the line record; `'` -> `&apos;` like the game's text. 13 languages.
* A line record's 4th u32 is its parameter count: with 0 the game shows
  "%ls"/"%d" as is. Keep it when rewriting a file (see revamp-on-1.26.md).
* Compare msg files with `XV2_RevampBridge\tools\msgdiff.py` (an ASCII token
  check misses them - names are mostly UTF-16).
* Layout (Xv2CoreLib's MSG_File, checked on all 7315 cpk .msg by
  `xv2ui roundtrip`, byte-identical): "#MSG"; u16@4 = 256 -> UTF-16 names,
  u16@6 = 1 -> UTF-16 text; u32 count@8, names@12 (16-byte {offset, length,
  size, unknown}), ids@16 (u32), line lists@20 (8-byte {count, offset}),
  total lines@24, line records@28 (16-byte {offset, length, size,
  parameters}); then all names, then all texts, 0-terminated. A file with
  no entries (41 in the cpks, e.g. qx_target01_*) has every offset 0.
  Reader/writer: `XV2ModLoader\UILib\src\formats\msg_file.cpp`.
* Several mods editing one msg: XV2UserInterfaceLib message edits
  (`SetMessage`, per entry; `XV2ModLoader\NEXT_STEPS.md` "Message edits").

# XV2 Mod Loader

**Version 1.2.0** · for Dragon Ball Xenoverse 2 **1.27** (Steam)

A small mod loader for Dragon Ball Xenoverse 2. It loads DLL mods and Lua
scripts from their own folders instead of the game's `bin` folder, and comes
with two launchers to switch between modded and unmodded play in one click.

The loader itself (`winmm.dll`) only loads things: everything else - the
in-game menu, search boxes, game file edits - is in its libraries in
`XV2ModLoader\Lib`.

Works alongside XV2Patcher.

---

## Installing

Put the files in the game folder like this:

```
DB Xenoverse 2\
    bin\
        winmm.dll                  <- the loader
    XV2ModLoader\
        loader.json
        Lib\
            lua54.dll              <- Lua, for Lua scripts
            XV2ModLoaderLib.dll    <- in-game menu, search boxes, game file edits
            XV2UserInterfaceLib.dll <- lets several mods edit the same menu
        mods\
            <ModName>\             <- one folder per mod
                <ModName>.dll      <- and/or Lua scripts (*.lua), see below
            _LuaExample\           <- example Lua script, switched off
        Launch Modded.exe
        Launch Unmodded.exe
        Add To Steam.bat           <- optional, see below
```

To install a mod, put its folder in `XV2ModLoader\mods`. To switch one off
without removing it, put `_` at the start of its folder name.

> **Mods run with full access.** A mod (DLL or Lua script) is a program that runs inside the game
> with the same permissions as the game itself. It can change anything in the
> game, and it can read, change or delete your files. The loader does not check
> or limit what a mod does. Only install mods from people you trust.

To uninstall, delete `bin\winmm.dll` and the `XV2ModLoader` folder.

Updating from a version without `Lib`: copy the new `Lib` folder in too -
without it Lua scripts don't run and the menu and search boxes are missing
(`loader.log` says which file is missing).

## Launching

Both launchers are in the `XV2ModLoader` folder.

* **Launch Modded.exe** — turns your mods on and starts the game with them.
* **Launch Unmodded.exe** — turns every mod in `bin` off and starts the game
  normally through Steam, with EasyAntiCheat, for online play.

"Every mod in `bin`" means every DLL there that is not one of the game's own —
the loader, XV2Patcher and anything else. They are renamed to
`<name>.dll_disabled`, and **Launch Modded** renames them back. Nothing is
deleted.

Both launchers refuse to run while the game is open.

### In your Steam library

Double-click **Add To Steam.bat** to add both launchers to Steam as non-Steam
games, with the game's own library art (cover, banner, background, logo) and
icon. It asks to close Steam first and starts it again when done.

* It does this for every Steam account that has logged in on the PC.
* It unblocks the downloaded files (launchers, mods, loader). Windows blocks
  downloaded programs it does not know, and when Steam starts a blocked
  launcher, nothing happens.
* Shortcuts to the launchers that are already there are kept (name, play time)
  and just get the art. Shortcuts to launchers in the game folder itself,
  where older versions put them, are pointed at the new ones.
* Custom art you set for the game itself is used over Steam's.
* The previous `shortcuts.vdf` is saved as `shortcuts.vdf.bak`.

To take them out of Steam again, with their art, run from the `XV2ModLoader`
folder:

```
"Add To Steam.bat" remove
```

### Steam Deck / Linux

EasyAntiCheat does not run on Linux, so only modded play applies. Add
`XV2ModLoader/Launch Modded.exe` (or `bin/DBXV2.exe`) as a **non-Steam game**, set it to use
Proton, and give it these launch options so Proton uses the loader's
`winmm.dll` instead of its own:

```
WINEDLLOVERRIDES="winmm=n,b" %command%
```

If you also use XV2Patcher, add its DLL to the same list:
`WINEDLLOVERRIDES="xinput1_3,winmm=n,b" %command%`.

If button icons are garbled or menus come up blank (mods with BC7 textures,
e.g. Revamp), copy `C:\Windows\System32\d3dx11_43.dll` from a Windows PC
into the game's `bin` folder and add it to the list too:
`WINEDLLOVERRIDES="xinput1_3,d3dx11_43,winmm=n,b" %command%`.

---

## Settings

`XV2ModLoader\loader.json`. If it is missing, the loader writes the default one
— and creates the `XV2ModLoader\mods` folder — the next time the game starts.

| setting | default | meaning |
|---|---|---|
| `logging` | `false` | `true` writes `loader.log` next to it, listing every mod DLL that loaded or failed to, and everything Lua scripts print. |
| `lua_console` | `false` | `true` opens a console window next to the game showing what Lua scripts print. **Don't close that window: closing it closes the game.** Set this back to `false` to get rid of it. |

If your `loader.json` is from an older version, add the `lua_console` line
yourself: the loader never overwrites an existing `loader.json`.

---

## Lua scripts

A mod can also be one or more Lua scripts (Lua 5.4): put the `.lua` files in
the mod's folder, like a DLL. The same rules apply: `_` at the start of the
folder name switches it off. Scripts use the same functions as
[LuaBackend](https://github.com/Sirius902/LuaBackend) scripts, plus the
memory functions of Cheat Engine's Lua under their CE names.

`mods\_LuaExample\example.lua` shows the basics. To try it, rename the folder
to `LuaExample`, set `lua_console` to `true` and start the game.

### Writing a script

```lua
function _OnInit()      -- once, when the game shows its first frame
    ConsolePrint("hello", 1)
end

function _OnFrame()     -- every frame, before it is shown
end
```

A script needs at least one of the two. Both run on the game's render thread,
so the game waits for them: keep `_OnFrame` short. If a script has an error,
the error and where it happened are printed (console and `loader.log`); a
script whose `_OnFrame` fails is stopped until the game is restarted.

Every script runs on its own: scripts don't share variables. `require("name")`
loads `name.lua` from the mod's `io_packages` folder. Lua's `io` and `os`
libraries are available.

`sdk\modloader_lib.lua` describes every function of the `ui`, `search`,
`files` and `uilib` tables (what it takes and returns). Add the `sdk` folder to your
editor's Lua language server (`Lua.workspace.library`) to get them as
completions and hints.

### In-game menu and overlay

```lua
function _OnMenu()      -- draws this script's section of the ~ menu
    if ui.Button("Heal") then ConsolePrint("healed", 1) end
end

function _OnOverlay()   -- windows over the game (menu open, or ui.ShowOverlay())
    if ui.Begin("My window") then ui.Text("hello") end
    ui.End()
end
```

The `ui` table has text (plain, disabled, wrapped, coloured, outlined),
buttons, checkboxes, sliders, number and text inputs, dropdowns, tree nodes,
collapsing headers, tooltips and windows (Dear ImGui underneath). Each
widget returns its new value; `sdk\modloader_lib.lua` lists them all.

### Memory functions

**LuaBackend style** - `address` is an offset from the start of `DBXV2.exe`.
Pass `true` as the last argument (or use the name ending in `A`, e.g.
`ReadIntA`) to give a full address instead.

| function | |
|---|---|
| `ReadByte/ReadShort/ReadInt/ReadLong/ReadFloat/ReadBoolean(address)` | read 1/2/4/8 bytes, a float, a bool |
| `WriteByte/.../WriteBoolean(address, value)` | write one; read-only memory is unlocked for the write |
| `ReadArray(address, length)` / `WriteArray(address, {bytes})` | bytes as a table |
| `ReadString(address, length)` / `WriteString(address, text)` | raw bytes as a string (no end marker written) |
| `GetPointer(address, offset)` | the pointer stored at `address`, plus `offset` |
| `WriteExec(offset, {bytes})` | write into the exe's code |
| `ULShift32(value, shift)` | 32-bit left shift |
| `ConsolePrint(text, type)` | print; `type` 0..3 adds MESSAGE/SUCCESS/WARNING/ERROR |

**Cheat Engine style** - `address` is a full address, or text in CE's form:
`"DBXV2.exe+1A2B"` (hex), `"[DBXV2.exe+1A2B]+10"` (the pointer stored there,
plus 0x10).

| function | |
|---|---|
| `readSmallInteger/readInteger/readQword/readPointer/readFloat/readDouble(address)` | read; `nil` if the address can't be read |
| `writeSmallInteger/.../writeDouble(address, value)` | write (read-only memory is unlocked for it); `true` if it worked |
| `readBytes(address, count, asTable)` / `writeBytes(address, {bytes})` | bytes |
| `readString(address, maxLength, wide)` / `writeString(address, text, wide)` | text, read up to the first 0 (writing adds no 0); `wide` = UTF-16 |
| `getAddress(text)` / `getAddressSafe(text)` | text to address (`getAddressSafe` gives `nil` instead of an error) |
| `AOBScan(pattern, flags)` | find a byte pattern (`"48 8B ?? 05"`) in `DBXV2.exe`; a list of hex addresses |
| `AOBScanUnique(pattern, flags)` / `AOBScanModuleUnique(module, pattern, flags)` | the first match as an address, or `nil` |
| `isKeyPressed(key)` | `true` while the key is held (a `VK_*` value, or a letter like `"F"`) and the game has the focus |
| `isPadButtonPressed(button)` | this loader's own: `true` while a controller button is held and the game has the focus; `button` is one of `PAD_A`, `PAD_B`, `PAD_X`, `PAD_Y`, `PAD_START`, `PAD_BACK`, `PAD_DPAD_UP/DOWN/LEFT/RIGHT`, `PAD_LEFT_SHOULDER`, `PAD_RIGHT_SHOULDER`, `PAD_LEFT_THUMB`, `PAD_RIGHT_THUMB` (stick clicks); any connected controller |

`flags` is CE's: `"+X"` only code, `"+W"` only writable memory, `"-W"` only
read-only, etc.

**Values set for every script:** `BASE_ADDR` (where `DBXV2.exe` starts in
memory), `SCRIPT_PATH` (the mod's folder), `LOAD_PATH` (`<mod>\io_load`),
`LUA_NAME` (the script's file name), `GAME_ID`, `ENGINE_VERSION` (5) and
`ENGINE_TYPE` (`"BACKEND"`).

LuaBackend's Discord functions (`InitializeRPC`, `UpdateDetails`, ...) do
nothing here, and `SetHertz` has no effect: scripts always run once per frame.

### Game files

A script can have the loader's library change a game file as the game reads it, so
nothing in `data` has to be replaced. Game version 1.27 only.

| function | |
|---|---|
| `files.ReplaceFile(path, file)` | serve the whole of `file` (relative to the mod's folder, read once when called, at most 512 MB) instead of the game file `path` (e.g. `"msg/proper_noun_en.msg"`). One mod per file: a second mod's replacement is refused. `true`, or `false` and the reason |
| `files.ReplaceIggyScript(path, abcFile, vanillaSize, vanillaHash)` | swap the ActionScript of the menu `path` (e.g. `"ui/iggy/ITEMLIST.iggy"`) for the compiled ABC in `abcFile` (relative to the mod's folder). Only done when the menu's current script is the one given by its ABC size and FNV-1a 64 hash, so a menu another mod changed is left alone. `true`, or `false` and the reason |
| `files.Status(path)` | what happened when the game last read that file (`"served: ..."`, `"NOT patched: ..."`), or `nil`; `files.IggyScriptStatus` is its old name |
| `files.Owner(path)` / `files.Owners(path)` | the first mod changing that file (`nil` if none) / all of them, in order |
| `files.Remove(path)` | drop this script's change of `path`; `false` if it had none |
| `files.ReadGameFile(path)` | a game file's bytes as the game finds it (loose `data\` file first, then the `.cpk`s; other mods' changes not applied) and where it came from (`"loose"` or the cpk's name); `nil, "not found"` |

Call `ReplaceIggyScript` in `_OnInit`: menus are read when they open. One
script per menu; calling it again (or F1) replaces that script's entry. To
have several mods change one menu, use `uilib` below instead.

### Menus and menu textures (several mods at once)

`uilib` (from `Lib\XV2UserInterfaceLib.dll`; `nil` without it) lets any
number of mods edit the same menu. Each mod ships its whole edited copy of
the menu's ActionScript (the AS3 ABC, e.g. saved from FFDec); the library
merges only what that mod changed into the menu as the mods before it left
it, so every mod's edits apply. Changed and new methods, traits, classes and
scripts are applied, removals are not. When two mods change the same method,
the edits are merged if they don't overlap, otherwise the later mod's wins;
`loader.log` lists each case. Texture files work the same way, one image at
a time, and text (`.msg`) files one entry at a time.

| function | |
|---|---|
| `uilib.EditScript(path, abcFile, baseFile, gameFile, order)` | register your edit of the script of the menu `path` (e.g. `"ui/iggy/CHARASELE.iggy"`). `abcFile`: your edited ABC, relative to the mod's folder. `baseFile` (optional): the script you edited, when it isn't the game's byte for byte (FFDec recompiles the classes it saves). `gameFile` (optional): the game script `baseFile` comes from, for a menu that comes in versions (XV2Patcher ships its own CHARASELE) - call once per version and the one matching the menu the game loads is used. `order` (optional, default 0): lower runs earlier. `true`, or `false` and the reason |
| `uilib.SetImage(path, name, imageFile, order)` | replace one image of the `.iggytex` texture file `path` (e.g. `"ui/iggy/CMNTITLE_en.iggytex"`), or add it when missing. `name` is the texture id the menu uses (`14`); `imageFile` is a DDS, relative to the mod's folder. Other images of the file, and other mods' images, stay; the same image from two mods: the later wins |
| `uilib.RemoveImage(path, name)` | drop one of your images of `path` |
| `uilib.SetMessage(path, name, text, order)` | set the text of the entry `name` of the `.msg` file `path` (e.g. `"msg/lobby_text_en.msg"`, `"name_4293"`), or add the entry when missing. `text` is plain UTF-8. Other entries, and other mods' entries, stay; the same entry from two mods: the later wins |
| `uilib.RemoveMessage(path, name)` | drop one of your entries of `path` |
| `uilib.edit_display(path, commands, order)` | edit the timeline of the `.iggy` menu `path`: `commands` is text, one per line, `clone <sprite> <source> <name> <dx> <dy>` (a copy of a placement, e.g. one more button; does nothing when `<name>` exists) and `matrix <sprite> <target> <frame> <a> <b> <c> <d> <tx> <ty>` (`*` keeps a value, `+=<n>` adds to it). `<sprite>` is a placement's instance name or `#<id>`; `<source>`/`<target>` a placement name or `@<depth>`. Your earlier commands for the path are replaced |
| `uilib.Remove(path)` | drop your edit of `path` (its script, display list, or all your images or messages of it) |

Register in the script's main body or `_OnInit` (the game reads some files,
like `lobby_text`, during boot: an edit registered after that misses the
read); F1 drops the edits and the
script makes them again. `files.Status(path)` shows what happened the last
time the game read the file.

### Search boxes

The loader's library draws a search box over the game for any mod that asks for one.
The player clicks it or presses Ctrl+F to type (the game gets no key presses
meanwhile); Enter or a click elsewhere ends typing, Esc undoes the edit.
`id` is any text - start it with your mod's name so boxes don't collide.

| function | |
|---|---|
| `search.Show(id, {hint, x, y, width, note})` | show the box this frame: call it from `_OnFrame` every frame it should be up; it hides by itself when you stop. `x`, `y`, `width`: fractions of the game's picture. `note`: a greyed-out line under the box (`""` removes it; leave `note` out to keep the current one, e.g. when two mods share a box) |
| `search.Query(id)` | the typed text in lower case |
| `search.Matches(id, name)` | `true` when `name` contains it, ignoring case (A-Z), or the box is empty |
| `search.Changes(id)` | goes up by one each time the text changes: filter again when it does |
| `search.Typing(id)` | `true` while the player types in the box |
| `search.Focus(id)` / `search.Clear(id)` / `search.SetText(id, text)` | put the keyboard in the box / empty it / replace its text |

---

## For mod authors

A mod is an ordinary 64-bit DLL. The loader calls `LoadLibrary` on every
`.dll` in each mod folder, in alphabetical order of folder and then file name,
shortly after the game starts. Your `DllMain` runs then; do any lasting work on
a thread of your own.

* A mod's own dependencies are looked up in its folder first.
* Find your folder from your own module path (`GetModuleFileName` on your
  `HMODULE`), and the game folder from the running exe (`GetModuleFileName(nullptr, …)`
  gives `<game>\bin\DBXV2.exe`).
* The game's code is encrypted by Steam's DRM until shortly after start. If
  you patch it, check that the bytes you expect are there first, and retry
  until they are.
* A DLL mod can use the loader's search boxes too: copy
  `sdk\xv2_search.h` into your mod (it explains the calls; they match the
  Lua ones). Any thread may call them.
* A DLL mod can edit menus and menu textures alongside other mods the same
  way as `uilib`: copy `sdk\xv2_ui.h` (`XV2UI_EditScript`, `XV2UI_SetImage`,
  `XV2UI_SetMessage`, `XV2UI_EditDisplay`, ...).

### Libraries

Before any mod, the loader loads every `.dll` in `XV2ModLoader\Lib` (in
alphabetical order, `_` names skipped). A library that exports
`ModLoaderLib_Init` is handed the loader's services: its log, the Lua scripts
(add functions to them, call their callbacks, unload them) and the frame
hook. `sdk\modloader_lib.h` describes them; `Lib\src` is the bundled library
and the example to follow. A library's own dependencies are looked up in
`Lib` first.

---

## Building

Visual Studio 2019 or newer with the C++ desktop workload, and CMake. Lua 5.4
(MIT licence) is included in `third_party\lua`.

```
build.bat              build into build\Release
build.bat clean        clean build
build.bat install      build and install into the game
build.bat uninstall    remove the loader, its libraries and launchers (mods and settings are kept)
```

`install` finds the game in Steam's library. If yours is elsewhere, set
`XV2_GAME` to the game folder first:

```
set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
```

---

## What it touches

* **Game code and memory:** nothing, unless a mod folder holds a Lua script
  or a DLL mod asks for a search box. Then it redirects one DirectX function
  (the one that shows each frame) to run the scripts and draw its menu and
  boxes first; scripts read and write whatever they are written to.
  When a script uses `files`, it also hooks the game's file reading (the same
  hook General QoL and Battle Styles use; they work alongside).
* **Files it reads:** `loader.json`, `Lib` and the mod folders.
* **Files it writes:** none, unless `logging` is on (Lua scripts can write
  files themselves). The launchers rename DLLs in `bin` as described above.
* **Network:** none.

`winmm.dll` is loaded by the game and by several of the game's own DLLs, so it
passes every Windows `winmm` function straight through to the real one, and
exports nothing else.

---

## Credits

* [Lua 5.4](https://www.lua.org) by Lua.org, PUC-Rio (MIT licence), in
  `third_party\lua`, runs the scripts (built as `Lib\lua54.dll`).
* [Dear ImGui](https://github.com/ocornut/imgui) 1.92.4 by Omar Cornut and
  contributors (MIT licence), in `third_party\imgui`, draws the in-game menu
  (its core and DirectX 11 renderer, in `Lib\XV2ModLoaderLib.dll`).
* [LuaBackend](https://github.com/Sirius902/LuaBackend) by Sirius902: the script
  functions (`ReadInt`, `WriteFloat`, `_OnInit`/`_OnFrame`, ...) follow its API
  so its scripts carry over.
* [Cheat Engine](https://github.com/cheat-engine/cheat-engine) by Dark Byte: the
  CE-named memory functions (`readInteger`, `AOBScan`, `getAddress`, ...)
  follow its Lua API.
* The Iggy (`.iggy`) file layout behind `files.ReplaceIggyScript` was found
  by reverse engineering the game's menu files.
* [XV2-Tools / Xv2CoreLib](https://github.com/LazyBone152/XV2-Tools) by
  LazyBone: the library's game file reader follows its CPK reader (based on
  CriPakTools) for the `.cpk` archive layout and CRILAYLA compression, and
  its `MSG_File` for the `.msg` text layout.
* [eternity_common](https://github.com/eterniti/eternity_common) by eterniti:
  its `IggyFile` and `EmbFile` were the reference for the `.iggy` container
  and `.emb` layouts in `UILib\src\formats`. The rest of the Iggy movie
  (the index, `.iggytex`/`.iggyted`) was found by reverse engineering the
  game's menu files.
* Adobe's [ActionScript Virtual Machine 2 (AVM2) Overview](https://www.adobe.com/content/dam/acom/en/devnet/pdf/avm2overview.pdf):
  the ABC (AS3 bytecode) format the UI library reads, writes and merges.
* [JPEXS Free Flash Decompiler (FFDec)](https://github.com/jindrapetrik/jpexs-decompiler)
  by Jindřich Pečínka (GPL-3.0): studied for how it rebuilds AS3 methods, which
  the UI library's merge allows for (scripts edited with it are merged).
* `sdk\modloader_lib.lua` uses the annotation format of
  [Lua Language Server](https://luals.github.io/wiki/annotations/) by sumneko
  (LuaLS) so editors can read it.

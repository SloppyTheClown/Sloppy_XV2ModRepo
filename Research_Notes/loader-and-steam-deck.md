# Mod loader, launchers, Steam shortcuts, Steam Deck / Proton

Covers: why the loader is a winmm.dll proxy, how it loads mods, the
Launch Modded/Unmodded exes and "Add To Steam.bat", config embedding, and
running the game modded on the Steam Deck. Project: XV2ModLoader.

## winmm proxy
* EOSSDK, iggy and xgcore in bin\ also import winmm (15 functions) and load
  the proxy too, so all 181 winmm exports must exist: forwarders to System32
  (`src\winmm_exports.h`, generated). The absolute-path forwarders resolve to
  Wine's builtin winmm on Proton.
* Loader loads `<game>\XV2ModLoader\mods\<Mod>\*.dll` on a thread after
  DllMain; folders starting `_` are skipped; `loader.json` "logging".
* Combined Supers also builds a standalone winmm.dll (COMBINED_STANDALONE,
  config in `<game>\XV2CombinedSupers\`) - release artifact only, never
  installed.
* Missing configs are written on startup (CREATE_NEW, never overwrites). The
  text is the shipped .json itself, compiled in by CMake via
  `src\default_config.h.in` -> `build\generated\default_config.h`, so they
  cannot drift. Same for every mod.
* With "logging": false a mod writes NO files.

## Core / library split (2026-10-12; offline test passes, installed on the PC, not seen in game)
* winmm.dll = load + chain only: System32's exports exactly (181 functions,
  180 names + nameless ordinal 2 - it was never trimmed; the only extras
  were the 9 `XV2Search_*`, now gone), Lib + mod DLL loading, the Lua
  runtime (LuaBackend/CE API) and the Present hook.
* `XV2ModLoader\Lib\*.dll` load before mods (sorted, `_` skipped,
  ALTERED_SEARCH_PATH). A library exporting
  `bool ModLoaderLib_Init(const ModLoaderHost*)` gets the host table
  (`sdk\modloader_lib.h`, version 1: log, script output, Lua registrars,
  extra callback names, unload handlers, ForEachScript, CallScriptFunction,
  frame handlers, EnsureFrameHook).
* `Lib\XV2ModLoaderLib.dll` (`Lib\src`): ImGui overlay, Lua `ui`/`search`/
  `files`, iggy ABC swap + file-request hook, `XV2Search_*` exports
  (`sdk\xv2_search.h` now finds them with GetModuleHandle("XV2ModLoaderLib.dll")
  - Battle Styles and Chara Search rebuilt).
* `Lib\lua54.dll`: Lua built LUA_BUILD_AS_DLL but compiled as C++ (/TP), so
  its exports are C++-mangled - only our own builds link to it. winmm.dll
  delay-loads it after loading it by full path; without it scripts are
  skipped with a log line.
* Still XV2-specific in the core: only the `<game>\bin -> <game>\XV2ModLoader`
  folder (loader.cpp DllMain); the exe base and GAME_ID come from the running
  exe. The launchers and "Add To Steam" are XV2-only.

## Lua scripts (added 2026-10-06; runs in game on the PC, 2026-10-07)
* Seen in game (PC, LuaProbe): hook installed ~1 s after start, `_OnInit` at
  ~3 s, `_OnFrame` 58 per second at the title, GAME_ID CB0C9306, MZ read ok.
* Closing the Lua console window ends the game (Windows kills a console's
  process; no crash dump, no event log entry - looked like a startup crash).
  The close button is now removed and Ctrl+C ignored; F2 hides it.
* Started from Steam (non-Steam shortcut) the game quit ~3.5 s in, at the
  first frame, with no crash dump or event log entry; fine when started
  directly. Not the console (the overlay only hooks D3D). Cause, found
  with a vectored exception handler + a trace of the first Present calls:
  C00000FD (stack overflow) in our hook. Steam's overlay patches the first
  instruction of dxgi's Present (`48 89 5C 24 10` -> `E9 rel32`; the vtable
  slot still held our hook) and, after drawing, calls Present through the
  vtable - our hook - so our call to Present looped overlay -> us -> overlay
  (every call's return address was gameoverlayrenderer64.dll+93E7D).
  Fix (confirmed through Steam 2026-10-07, title screen, 58 fps): a nested
  call (or one after another hook replaced ours in the slot) goes to dxgi
  past the patch - its first instructions copied from dxgi.dll's file
  (vtable slot RVA -> file offset, rebased; then the function's first 32
  bytes), whole instructions until 14 bytes are covered (push / mov / lea /
  sub rsp only, no rip-relative), then `FF 25` to the rest. Logged as
  "Present's code is patched by another program ... calling past it".
  Dead end: reading only the unpatched Present address from disk and
  calling it - its code is the patched one.
* Overlay (Shift+Tab), the removed close button and F2 confirmed by the user
  2026-10-07 (Steam start).
* Game "Not Responding" after alt-tabbing between game and console
  (2026-10-07): `cdb -pv -p <pid> -c "~*k 12; q"` (non-invasive, safe on a
  live game) showed the render thread in WriteConsoleA from our hook, under
  gameoverlayrenderer64's Present. A text selection in a console (QuickEdit:
  any click into it; or Mark) pauses every write until it ends. Fix: QuickEdit
  off, all console output goes through a bounded queue to a writer thread
  (dropped and counted when full), io.write/stdout through a pipe drained by
  its own thread, F2 uses ShowWindowAsync. Not yet seen in game.
* A first-chance C0000005 at CombinedSupers.dll+2EC1 ~3 s after the title
  shows up with an exception logger; the game carries on (handled).
* `mods\<Mod>\*.lua` run inside winmm.dll (`src\lua_scripts.cpp` runtime,
  `src\lua_api.cpp` functions). The API is LuaBackend's (Sirius902/LuaBackend,
  engine v5: `_OnInit`/`_OnFrame`, `Read*/Write*[A]`, `GetPointer`, `WriteExec`,
  `ConsolePrint`, globals `GAME_ID`/`BASE_ADDR`/`SCRIPT_PATH`/...), rebuilt
  from its documentation - LuaBackend is GPLv3, no code copied. Plus CE-style
  names (`readInteger`, `writeBytes`, `getAddress("DBXV2.exe+1A2B")`,
  `AOBScan*`). Lua 5.4.9 is vendored in `third_party\lua`, compiled as C++.
* Frame hook: IDXGISwapChain::Present, vtable slot 8, patched in the vtable
  of a swap chain made on a hidden window (all swap chains share it; Steam
  overlay interplay above). Only
  installed when a .lua file exists. `DXGI_PRESENT_TEST` presents are
  skipped. Not verified: that Present runs on XV2's logic thread, and
  behaviour under DXVK on the Deck.
* `_OnInit` runs on the first frame, not at load, so the DRM has decrypted
  .text (exe-and-tooling.md).
* LuaBackend differences on purpose: a script with neither handler is
  skipped (LuaBackend aborted the whole load); WriteExec unprotects the
  address it writes (LuaBackend protected the wrong one); Discord RPC
  functions are stubs; AOBScan only scans one module image (exe by default);
  GetHertz/SetHertz do nothing (one run per game frame).
* FindFirstFile `*.dll` also matches `x.dll_disabled` via the 8.3 short name;
  both listings re-check the extension (`HasExtension`).
* Offline check: `ClaudeTools\luatest\run.bat` (fake game folder, a D3D11 host exe,
  scripts that exercise every function, error/skip cases). In-game probe:
  `ClaudeTools\luatest\ingame\LuaProbe` (prints GAME_ID, MZ check, frame rate).
* Shipped example: `XV2ModLoader\examples\_LuaExample\example.lua` (read-only,
  same checks as the probe); `build.bat install` copies it to
  `mods\_LuaExample` (off) unless `_LuaExample` or `LuaExample` exists.
* README leaves out the F1/F2 hotkeys until they're seen working in game.

## Why our own loader / other loaders (tested 2026-10-06, PC)
* XV2Patcher can't host our mods: Epatches only patch bytes or hook into
  functions already in xinput1_3.dll, and xv2patcher.ini has no option to
  load an extra DLL.
* The mods are loader-agnostic (DllMain -> thread, config next to the DLL via
  GetModuleFileName, hooks retry for the DRM). Ultimate ASI Loader v9.7.4 x64
  as `bin\winmm.dll` with the mods as `bin\scripts\*.asi` (+ json/sav): all
  four loaded and hooked ~760 ms after start, confirmed in game.
* But XV2Patcher did not load at all in that setup (no new
  `XV2PATCHER\xv2_log.txt`). xinput1_3.dll is imported by xgcore.dll (not the
  exe), which also imports winmm; UAL contains XInput proxy code. Cause not
  investigated (next step: list the game's modules while running). Reverted
  to our loader.

## Launchers
* "Launch Unmodded.exe": renames every non-vanilla bin DLL to
  `*.dll_disabled`, starts `steam://rungameid/454650` (EAC). Vanilla bin DLLs:
  EOSSDK, iggy, oo2core, render_pass, steam_api, xgcore.
* "Launch Modded.exe": renames back, starts `bin\DBXV2.exe`, stripping Steam*
  env vars first (an inherited SteamAppId made the game relaunch via
  START.exe = EAC).
* Both live in `<game>\XV2ModLoader\` (find the game in their folder or the
  parent); `build.bat install` deletes old game-root copies.
* "Add To Steam.bat": edits binary `userdata\<id>\config\shortcuts.vdf` for
  every loginusers.vdf account (Steam must be closed - it rewrites the file on
  exit), reuses existing entries, copies 454650's librarycache art to
  `grid\<appid>{p,,_hero,_logo}`. "remove" undoes. `XV2_STEAM` overrides the
  Steam folder (tested against a copy).

## Steam Deck / Proton (verified 2026-09-25)
* The stock Steam entry launches EasyAntiCheat, which does not run on Linux:
  use a non-Steam shortcut to `bin/DBXV2.exe`, Proton Experimental.
* Launch options: `WINEDLLOVERRIDES="xinput1_3,d3dx11_43,winmm=n,b" %command%`
  (XV2Patcher is xinput1_3.dll, NOT dinput8).
* Debug: `PROTON_LOG=1` -> `~/steam-454650.log`; `+loaddll` lines say native
  vs builtin per DLL (builtin for a DLL in bin/ = Proton did not take the file).
* "Add To Steam.sh" (in XV2ModLoader, bash + embedded python3): the Deck's
  Add To Steam. Shortcuts to `bin/DBXV2.exe`: Modded `xinput1_3,d3dx11_43,winmm=n,b`,
  Unmodded `xinput1_3,winmm=b` (builtin = no loader/patcher; not yet tried in
  game). Names end "(Deck)": the PC's same-named shortcuts also show on the
  Deck as Remote Play entries. Reuses (and renames) any shortcut to that exe (name, else winmm=n/b in its
  options) keeping its appid and extra options, sets Proton Experimental in
  `config/config.vdf` CompatToolMapping (unsigned appid) only where none is
  set, copies grid art. Closes Steam (`steam -shutdown`); Game Mode restarts
  it itself. `XV2_STEAM=<copy>` tests on a copy without touching Steam
  (done 2026-10-04: add, re-run, remove, VDF round-trip byte-exact).
  Copying from Windows (tar) drops +x: chmod `XV2ModLoader/*.sh` on the
  Deck. "Launch Unmodded" leaves `bin\*.dll_disabled`: copy the enabled
  name to the Deck.
* BC7 textures + Proton's d3dx11 (found and fixed 2026-10-04, confirmed in
  game on the Deck): Revamp's button icons (`revamp_user_interface/buttons/sysfont01_ab*`,
  93 BC7 = DXGI 98) and CHARASELE_en.iggytex (1 BC7) are BC7; vanilla UI
  never is. The game loads Iggy textures through d3dx11_43, and Wine's
  builtin one logs `fixme:d3dx:d3dx_pixel_format_id_from_dxgi_format
  Unhandled DXGI format 0x62` (109x in PROTON_LOG). Deck symptoms with
  files byte-identical to the PC: garbled controller button icons, blank
  character select that takes no input (PQ), emotes in the online lobby
  freeze the player for the emote's length (emote freeze: not tied to BC7,
  followed up separately).
  Fix: copy a Windows `System32\d3dx11_43.dll` (DirectX June 2010
  runtime) to the Deck's `bin/`, Modded override `xinput1_3,d3dx11_43,winmm=n,b`
  (d3dx11_43 must come before winmm=: Add To Steam.sh finds the Modded
  shortcut by "winmm=n").
  Dead ends: the 13:44 session ran on a different Steam account and never
  mounted the DLC cpks (second "files were removed in ram" batch, toc 0x224c8
  = data_d4_5_xv1 etc.) - not the cause per the user. `iggypairs.py` flags
  the bridge-converted textures as "old layout" though they work on the PC.
  `CHARASELE_JE_*` are not referenced by the exe.
  Format census: parse .iggytex entries (ui-iggy-and-textures.md) and read
  each DDS's FourCC / DX10 dxgiFormat at +128.
* Sync: the Deck needs the same XV2ModLoader, XV2PATCHER, `data\` and bin
  DLLs as the PC (plus d3dx11_43.dll, above); logs and `*_saved.txt` are per
  machine.

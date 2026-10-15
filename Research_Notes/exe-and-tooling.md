# DBXV2.exe, globals, hooking, live-game tooling

Covers: the encrypted exe and decrypted images, global singletons, when code
can be patched, the shared file-request hook every data-editing mod uses,
debugging methods that work / don't, and the probe scripts in `tools\`.
Game 1.26.01; addresses are DBXV2.exe RVAs.

## The exe
* `DBXV2.exe` on disk is encrypted by the Steam DRM wrapper (`.bind` section,
  entry point RVA `0x19E5310`, signature `0xC0DEC0DF` at `0x19E5543`; `.text`
  entropy 8.0). Static analysis needs a decrypted image:
  `XV2_RevampBridge\tools\dumpimage.py` runs the game and saves one (flat
  image, file offset == RVA). An old 1 MB-chunk dump from the first sessions
  is in `ClaudeTools\_claude_xv2mod\dump\` (`cat chunk_*.bin > img.bin`).
* Static-analysis scripts for an image: `ClaudeTools\exe\` (xref, rawref, da, parent,
  who/RTTI, vt, ptr, imports, exports, strs, vcallnear - see its README.txt; needs
  capstone and `XV2_RevampBridge\tools\codematch.py`).
* Module base was identical across restarts (`0x7FF78E790000`).
* Steam's DRM decrypts `.text` at startup: code patches must wait for it (the
  mods retry hook install for 60 s). `.rdata` (vtables) can be patched
  immediately.
* The 1.25.02 -> 1.26.01 global/function map: see revamp-on-1.26.md.

## Global singletons (1.26.01)
| global | what |
|---|---|
| `[0x15545b0]` | save / character manager ("mgr"): slot int at mgr+8+0x44, per-character SYSFLAGs, quest progress, ACCFLAGs, options (save-flags-and-unlocks.md) |
| `[0x1554458]` | quest/progress manager: list +0xb10, QXD lookup via +0x928, voice player via +0x948 |
| `[0x1554490]` | Sound::System (sound.md) |
| `[0x1554448]` | save/system singleton Revamp's patches use (old 1.25 `[0x1681C08]`); +0x68 = decrypted save image (save-flags-and-unlocks.md) |
| `[0x1554480]` | demo (cutscene) system |
| `[0x156b408]` | Battle::CameraManager; `[0x15712e8]` Lobby::CameraManager (boot-and-camera.md) |
| `[0x15544d8]` | runtime chara table (code at entry+4) |

## File request hook (shared by the file-editing mods)
* Every data file is read through a request object: vtable rva `0xf9b270`,
  path inline at +8, inner data object at +0x128 (vtable `0xf8dfa0`: data
  +0x48, size +0x38). Consumers call request vt+0x20 (data) and vt+0x38 (size).
* The mods repoint vt+0x18/+0x20/+0x30/+0x38 (.rdata, patched immediately) and
  return their own edited copy. .acb and .iggytex loads go through it too.
* Chaining: whatever is already in vt[3]/[4]/[6]/[7] is called through, under
  the named mutex `XV2Mods_FileRequestHook`, with a PE-header version check -
  load order does not matter. No mod checks for another.
* Cache keyed by path; a reload of an identical file reuses the edited copy
  after re-hashing (FNV-64) the original, so game writes are noticed.
  `lobby_text_en.msg` (6.6 MB) reloads several times a session.
* Files are caught ~0.6 s after launch (quest/skill files included).
* Also in the chain: BattleStyles (menu iggys) and the loader itself
  (`XV2ModLoader\src\file_patches.cpp`, Lua `files.ReplaceIggyScript`), which
  hooks only once a script registers a file - after launch, but before any
  menu iggy it serves is read.

* **Chain cost (fixed 2026-10-11):** every hook's size slot called the next
  mod's data AND size (twice), and its data slot the next size, before
  checking the path. Down six mods (e.g. Battle Styles -> Chara Search -> another
  file mod -> General_QoL -> SaveSync -> loader) one game size call became ~600 calls
  (D(n)=D+S+1, S(n)=2S+D+1). Online, the host's workers poll requests every
  frame, so the hosting PC lost fps. Rule: **check the path first**, and
  only fetch data/size for a file the mod edits (`Serve`/`ServeSize` in each
  mod; General_QoL also when `log_files` is on). Seen in game 2026-10-12:
  frameprobe went from 26-50 fps (median 42, 64 of 90 windows under 55)
  to median 60 (4 of 178 windows under 55; the rest of the hitches are
  100-500 ms spikes in clusters, probably loads/transitions - not checked).

## Debugging - what works
* Cheat Engine's GUI "Find out what writes to this address" works. Hardware
  breakpoints set from CE Lua did not (the Lua calls were wrong, not EAC).
* `ClaudeTools\watchread.py`: attaches as a debugger for a while, hardware
  watchpoint DR0 on a 4-byte value, logs readers/writers. A **second debugger
  attach in one game session crashed the game** - avoid.
* Polling known structures read-only (sndwatch, qolprobe, sysflags) is cheap
  and safe - prefer it.
* Never do unbounded work on a timer or tick: an AOBScan / memory sweep inside
  a CE timer tick hung Cheat Engine completely. Errors inside a CE timer kill
  it silently (wrap in pcall).
* Exact-value scans for a damage number found HUD/scoreboard values, not the
  real ones (stats-and-combined-supers.md "Dead ends").

## tools\ (all read-only unless noted)
* `ringfind.py <hitsA> <hitsB>` - Combined Supers' rbx ring + attacker structs
* `hpwatch.py [s]` - HP changes from Combined Supers' status file, 4x/s
* `memtable.py` / `cpkscan.py` - find the CaC level table (memory / archives)
* `shader_dis.py <shader_*.emz> [name.xpu ...]` - list / disassemble
  adam_shader entries (offline; see costumes-and-models.md "Shaders")
* `emm_params.py` - flag-like EMM param names + TOON_UNIF_SCROLL params over
  all chara .emm in the cpks (offline)
* `bcs_partset.py <RACE> <id>...|--physics` - print BCS part sets / tally
  physics object kinds from the installed (or cpk) BCS (offline; costumes-and-models.md)
* `bcs_physics_dyt.py` - HUF physics objects by flags x own `.dyt.emb` x
  parent part model (offline; costumes-and-models.md)
* `emb_texture_stats.py <cpk name | .emb> ...` - format + mean colour/alpha of
  every texture in chara .emb files and the .emm's MatCol/MatScale (offline)
* `idb_compare.py <file.idb> <id> <id>` - diff two installed IDB entries by
  u16 word, plus entry count and duplicate ids (offline)
* `exe\dump_running.py <out.bin>` - read-only dump of the running
  DBXV2.exe's decrypted image (SizeOfImage from PE +0x50), for static
  disassembly at RVAs without attaching a debugger
* `sysflags.py` - loaded character's SYSFLAGs (first 256 bytes only)
* `save\sav_steamid.py [sav ...]` - decrypt DBXV2.sav offline, print its Steam id;
  `save\steamid_probe.py` - find that id / the save image in the running game
* `save\sav_mentors.py <sav>` - per character: mentor friendship/gauge/flags + cleared lessons (save-flags-and-unlocks.md).
* `save\quest_states.py <sav ...>` - per character: cleared/total records for TPQ TMQ BAQ TCQ HLQ RBQ.
* `save\restore_quest_progress.py <current> <backup> <out>` - WRITES a save: merges a backup's quest records into the current save and fixes the header checksums (data-files-and-xv2dataedit.md).
* `gamefiles\qxd_set_quest_count.py <qxd> <n>|--restore` - lowers a QXD's quest count (backup `.count<old>`); used for the 101-quest HLQ (data-files-and-xv2dataedit.md).
* `gamefiles\qxd_clean_revamp.cs` (csc x64; args: lib dir, vanilla qxd, installed qxd, out, quest prefix, chara codes) - vanilla QXD + only Revamp's quests/characters, via the bridge's Xv2CoreLib.
* `snapdiff.py save|diff` - bit diff of the save manager's first 0x4000 bytes
* `lobbyloop.py` - lobby scene-init checks vs the loaded character's flags
* `loopwatch.py [s]` - timestamped SYSFLAG / quest / slot changes
* `flagclear.py [--slot N] [--set] ranges` - WRITES SYSFLAGs (bisecting)
* `fix_save_editor_flags.py [--undo]` - patches Save Editor 3.5.0's SysFlags.xml
* `qolprobe.py [s]` - General_QoL inputs: slot, volume options, categories
* `sndwatch.py [s]`, `acbcues.py` - which sound plays (sound.md)
* `watchread.py` - hardware watchpoint (see above)
* `threadpc.py [samples] [thread id]` - where a hung game spins: busiest
  (or the given) thread's RIP and stack code addresses as module+offset
  (suspend/get-context, no debugger attach)
* `threadcpu.py [seconds]` - per-thread CPU % with each thread's start module
* `regionmap.py` - committed private RW memory by protection flags (what
  Combined Supers' scan walks)
* `nearexport.py <dll> <rva>...` - nearest preceding export name
* `frameprobe\frameprobe.lua` - loader Lua frame-time logger (hitches > 25 ms
  + 10 s summaries to `XV2ModLoader\frameprobe.log`); install as
  `mods\FrameProbe\`, F1 to load
* `stackwalk.py <tid> [samples]` - real call stacks of one thread via
  dbghelp StackWalk64 (suspend/get-context; run without `python -I`, it
  imports threadpc)
* Main game thread in battle (2026-10-11, online, PC hosting, 26-50 fps):
  waits in xgcore's job system (fn 0xd33e0 hands jobs out with
  ReleaseSemaphore, waits at xgcore+0xd357c; workers start at
  xgcore+0xd6953), not in Present; GPU 26% at idle clocks, CPU ~10%. The
  workers spent ~1/3 of their samples in the mods' file-request hook chain
  (callers DBXV2+0x7fb3c3 <- 0x1d5d81/0x1d5872, and 0x1b5057, inside
  xgcore+0xd271b jobs) - see "File request hook" below for the cause.
* `ClaudeTools\iggy\lobyhud_abc.py <ffdec-cli.jar> [--iggy out]` - compiles
  General_QoL\as3\LobbyMap.as into LOBYHUD.iggy's AS3 via JPEXS FFDec
  (`ClaudeTools\ffdec\ffdec-cli.jar`, run with java 21) -> src\lobyhud_abc.h;
  `ClaudeTools\iggy\menu_abc.py <ffdec-cli.jar> [menu ...]` the same for
  BattleStyles' menus (avatar, charasele, charasele_patcher = xv2ins's
  loose CHARASELE.iggy, myset)
* `ClaudeTools\iggy\itemlist_abc.py <ffdec-cli.jar>` - ItemSearch\as3\ItemList.as
  -> ItemSearch\iggy\ItemList.abc, prints the vanilla ABC's size + FNV-1a 64;
  `--from-iggy <ITEMLIST.iggy>` takes the ABC out of a patched iggy (no FFDec)
* `ClaudeTools\luatest\iggy_abc_check.bat <vanilla iggy> <expected iggy>` - offline:
  the loader's ReplaceIggyAbc on vanilla ITEMLIST.iggy + ItemList.abc must
  give the expected bytes, and refuse an already patched file
* `ClaudeTools\luatest\run.bat` - offline check of the loader's Lua API (incl. `files`)
* `ClaudeTools\gamefilestest\run.bat` - offline: the loader library's
  `XV2Files_ReadGameFile/FindGameFile/ReadGameFilePart` on the PC's game
  (loose + cpk entries, compressed tails): whole reads compared with
  XV2DataEdit's cpk.py (`reference.py`), part reads with the whole file
* `ClaudeTools\itemlisttest\run.bat` - offline: the loader library's
  `XV2Files_ReadGameFile` + Debug Menu's runtime item list parser on the
  real game files (junctions to the PC's `data` and `cpk`); prints group
  counts, `DUMP=<file>` writes every entry
* `ClaudeTools\itemsearch\run.bat` - offline check of ItemSearch's search +
  favorites against a simulated item list (`favorites_test.lua`)
* `ClaudeTools\qxd_chara_unlocks.py [game]` - every quest's QXD Chara_Unlock
  entries (loose data first, then the cpks)
* `itemlist_probe.py` - the open Menu::ItemList's fields and shown entries
  (ui-iggy-and-textures.md "Item lists")
* `threadmap.py` - which game thread runs the loader / Present / Iggy ticks
  (stack sampling, no debugger)
* `ClaudeTools\iggy\pcode_flow.py <orig.pcode> <rebuilt.pcode> [method ...]` -
  methods whose control flow an FFDec decompile -> recompile changed
  (export both with `ffdec-cli -format script:pcode -export script`)
* `ClaudeTools\iggy\roundtrip_check.py <ffdec-cli.jar> [avatar|myset|avatarlist|lobbymap]` -
  vanilla (cpk) vs rebuilt (generated header) menu ABC: pcode_flow walk +
  every lookupswitch's cases resolved to their code; run after editing a menu .as
* `ClaudeTools\iggy\charasele\edit_charasele.py` - decompiled CharaSele.as ->
  BattleStyles' as3 CharaSele sources (style row + FFDec loss fixes)
* `ClaudeTools\gamefiles\` - lobby/quest file decoders and the mods' offline file
  check (README.txt there); `ClaudeTools\ticktest\` - Combined Supers' Tick() offline
* `ClaudeTools\_claude_xv2mod\` - the original Cheat Engine prototype (Lua) of
  Combined Supers + the old image dump. Historical reference.

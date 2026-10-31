# XenoKit (Tools\XenoKit) - local additions

Covers: the local changes on top of LazyBone's XenoKit clone - the Effect
Library (grid, labels, click-to-isolate), the external back buffer that lets
another window show the viewport, the Akeytsu export/import menu, and the local
build helper. Game internals
are in the other topic files; this one is only about the tool.

Moved to their own files (2026-10-22):
* `xenokit-akeytsu.md`: Akeytsu export/import (2026-10-21); State (2026-10-22)

## Where the code is
* `XenoKit\Engine\Vfx\EffectLibrary\EffectLibraryScene.cs` - the grid scene:
  cells, actor pool, camera framing, label screen positions, isolation.
* `XenoKit\Engine\Vfx\EffectLibrary\EffectLibraryCatalog.cs` - vfx_spec.ers
  listing, EEPK loading, label text.
* `XenoKit\Engine\Vfx\EffectLibrary\EffectLibraryEffectUsageIndex.cs` - the
  BAC/BDM scan behind "which skills/characters use this effect" (built once per
  session).
* `XenoKit\Windows\EffectLibrary\EffectLibraryWindow.xaml(.cs)` - the window:
  paging, labels, click handling.
* `XenoKit\Engine\Viewport.EffectLibrary.cs` - swaps the viewport into the
  library while the window is open (saves/restores stage, camera, playback).
* `MonoGameWpfInterloop\MonoGame.Framework.WpfInterop\D3D11Host.cs` -
  `SetExternalBackBufferSize` (marked "Local addition") renders the viewport at
  the library preview's pixel size.
* Akeytsu menu (see "Akeytsu export/import" below):
  `XenoKit\MainWindow.Akeytsu.cs` (menu commands, dialogs),
  `XenoKit\Editor\Akeytsu\AkeytsuExchange.cs` (scene -> XkScene, XkScene ->
  EAN keys), `AktBridge\` (the .NET 9 exe that writes/reads the .akt),
  `AktBridge\Interchange\AkeytsuInterchange.cs` (the XKAK file both sides read;
  linked into XenoKit.csproj). Upstream edits: the menu in `MainWindow.xaml` and
  the link in `XenoKit.csproj` (both commented "Local").

## Gotchas found the hard way
* **The shown picture is mirrored on X.** The viewport's last pass
  (`LB_AxisCorrection`, `ExtShaders\AxisCorrection.hlsl`) samples the image at
  `1 - u`. Anything projecting a world point onto a screen overlay (the effect
  labels) must flip X after the view-projection divide, or it moves the wrong
  way when the camera pans. The TextRenderer/overlays drawn after the
  correction are not mirrored.
* **WpfMouse captures the mouse on the preview image** (`CaptureMouseWithin`,
  true by default, so drags can leave the image). With capture, overlaid WPF
  elements never receive clicks - WpfMouse's own comment says so - so the
  labels could not be clicked at first. `BeginEffectLibrary` sets
  `CaptureMouseWithin = false` and `EndEffectLibrary` restores it; label clicks
  are hit-tested in the window's `PreviewMouseLeftButtonDown`
  (`labelCanvas.InputHitTest`) so they keep working if capture comes back.
* **A TextBlock built from Runs hit-tests as a Run.** When the vanilla/mod tag
  made the labels use `Inlines`, `labelCanvas.InputHitTest` returned the `Run`,
  not the `TextBlock`, so the click handler's `is TextBlock` check failed and
  clicking a label did nothing (isolation and the details panel both stopped).
  `GetClickedLabel` now walks the inline's `Parent` chain;
  `ClaudeTools\effects\click_hittest_probe.cs` checks it (hit = Run, resolved =
  TextBlock with the cell Tag).
* **Stopping a cell drawing** is done by not updating it: assets only set
  `DrawThisFrame` in their update and the render system clears it after a
  draw. Clear the flag on the assets/actor as well for the first frame, when
  the cell is hidden before it was ever drawn. Particle emission draws check
  their owning `ParticleSystem.DrawThisFrame`, and particles only reach the
  batcher from their update, so leaving the update out hides the whole cell.
* **Do not re-sort a DataGrid's collection from inside an ID cell edit.**
  The Camera/Animation tab ID cells commit on LostFocus; the old `ChangeAnimId`
  called `SortEntries()` in the binding setter. Sorting replaces the rows, the
  DataGrid raises SelectionChanged, and `AnimListBox_SelectionChanged` ->
  `UpdateProperties()` -> `Items.Refresh()` runs while the DataGrid is still in
  its EditItem transaction. WPF then throws `InvalidOperationException:
  'Refresh' is not allowed during an AddNew or EditItem transaction.` (the
  empty camera selection also made the edited row jump). Both tabs now sort in
  `Dispatcher.BeginInvoke(..., DispatcherPriority.Background)` and keep the
  edited animation selected. `ClaudeTools\camera-id-test` reproduces the old
  failure with a minimal DataGrid.

## Vanilla/mod effects and where an effect is used (2026-10-19)
* **Vanilla check.** The library now marks each effect `Vanilla` or `Mod`. The
  default game files are the ones inside the cpks; the loose `data\` folder is
  what XV2Patcher's `loose_files` and mod installers (Revamp included) override,
  so the check reads the EEPK with
  `FileManager.GetBytesFromGame(path, onlyFromCpk: true, raiseEx: false)` and
  only parses its header/effect table (`EffectLibraryCatalog.ParseEffectIds`,
  same "offset != 0" and effect-index match rules as `EEPK_File.Load`). An effect id missing from the
  vanilla EEPK (or an EEPK missing from the cpks altogether) is `Mod`. Reading
  ids instead of loading the whole EEPK keeps it to one small allocation per
  selected EEPK; all 346 vanilla EEPKs are in the cpk list Xv2CoreLib reads.
  Measured with `ClaudeTools\effects\effect_library_probe.cs`: BTL_CMN 420/525
  vanilla (105 Revamp effects), BG_LBY 114/116 (2), D01_10 2/4 (2).
* **Where an effect is used.** The ERS entry carries the owner, so no BAC scan is
  needed: table 2 id = CMS character id (`GetCharacterName`, CMS code as the
  detail), table 10 = a z-assist ultimate (folder `{ID2}_{chara}_{skill}`, CUS
  Ultimate with `id1 = 5000 + ID2`; shown as Ultimate Skill + character), table
  4 = demo, 0 = common effect set, 1/11 = stage. Skill tables 3/5/6/7/9/12 (not
  in the 1.26 ERS) are also mapped through CUS for completeness.
  `EffectLibraryCatalog.GetUsages` describes only the **selected** entry: several
  character entries share one file (VGT.eepk is also the fallback for RSB, NIL,
  CaC SYM) and listing those would be misleading.
* **Z-assist ERS paths are virtual.** Table 10 entries say
  `zasst/{folder}/{folder}.eepk`, but no `vfx/zasst` file exists; the EEPK is the
  ultimate skill's `skill/ult/{folder}/{folder}.eepk`. `310_CL3_KMH` has no EEPK
  of its own and uses `000_GOK_CKM`'s through its CUS `EepkPath`
  (`skill/ULT/000_GOK_CKM/000_GOK_CKM.eepk`). The library resolves this in
  `GetZAssistEepkPath`; before that all 16 z-assist entries failed to load. The
  group is named "Z Assist" (table id 10) and "Demo" (4) instead of "Type N".
* **Offline check.** `ClaudeTools\effects\effect_library_probe.cs` calls the
  built `EffectLibraryCatalog.BuildGroups` / `LoadVanillaInfoAsync` /
  `GetUsages` through XenoKit.exe (extract Costura first), printing all 16
  z-assist owners and the vanilla/mod counts above. Two other probes were used
  to get there: `eepk_vanilla_compare.py` diffs every loose EEPK against its cpk
  copy (effect ids and part counts), and `ers_owner_probe.cs` prints the ERS
  type 2/10 owner mapping through CMS/CUS.

## Effect-level users: which skills/characters actually play an effect (2026-10-19)
* **Why.** The ERS owner above only says who *owns the EEPK*. The common sets
  (BTL_CMN 0/1/2 are the hit sparks, 40 Fly_Wind, 45 the block lines, 80-83 the
  skill lines, ...) are pointed at by hundreds of movesets and skills, so the
  old details panel could only say "used by many characters/skills" for them.
  The BAC/BDM effect entries themselves name the EEPK type, EEPK id and effect
  id, so the real users can be read from the game files without guessing.
* **Index.** `Engine\Vfx\EffectLibrary\EffectLibraryEffectUsageIndex.cs` builds,
  once per session on a background task started when the library opens:
  every CUS skill's BAC + BDM, every CMS character's moveset BAC + BDM, and
  `chara/CMN/CMN.bac` + `.bdm`. Each BAC type 8 entry and each BDM effect slot
  is stored as (eepk type, effective eepk id, effect id) -> the skill/character
  that plays it. 1.26: 1451 BACs + 1338 BDMs scanned (shot BDMs included),
  ~1 s warm and a few seconds cold; 1171 EEPK keys and 5487 effect targets.
  Off-flagged (`EffectFlags.Off`) entries and negative effect ids are skipped.
* **Which id an entry means.**
  * Common (type 0): `SkillID` is the common EEPK id (`0` = BTL_CMN).
    `UseSkillId=false` with `65535` (and the occasional `65535` left with
    `UseSkillId=true`) means common 0; XenoKit's `Files.GetEepkFile` also
    passes 0 for those entries.
  * Skill EEPK types (3/5/6/7/9/12): `SkillID` 0 or `UseSkillId=false` means
    the BAC's own skill. Every ultimate's BAC stores 0 (000_GOK_CKM has CUS id
    5000 but its own effects say 0), so 0 -> self is not optional.
  * Character (type 2): the moveset's own EEPK. In a *skill's* BAC it is the
    user's character EEPK, so those are not attributed to anybody.
  * Stage (1/11) and demo (4) effects are chosen by the loaded stage / demo
    sequence, not by a BAC, so they stay ERS-level only.
* **Shared EEPK files.** Lookup uses every `vfx_spec.ers` entry with the same
  EEPK path. Story slots borrow files: NIL and RSB use `chara/VGT/VGT.eepk`, so
  VGT's entry lists Nail and Raspberry as users too. That is the sharing the
  old ERS-owner code avoided as "misleading"; at effect level it is accurate.
  Z-assist entries resolve through *every* ultimate skill whose EEPK path is
  that file (310_CL3_KMH borrows 000_GOK_CKM's), looked up as `6:{ID1}`.
* **Panel.** Shows the first 5 users, then "Click to show all N"; clicking the
  box expands into a 260 px scroll viewer ("Click to show fewer" collapses).
  Named characters come first, then skills, then unnamed story slots (910,
  9O6, ...). Skills show their owning character and `ID {ID1}` so two skills
  with the same name stay distinct. The box is hit-testable now, so a label
  under it cannot be clicked; the window's click handler checks the box first
  and lets clicks inside the expanded scroll bar through to the scroll bar.
* **Not covered.** BSA effect slots (projectiles) are not scanned. BDM has no
  `UseSkillId` flag: a skill EEPK id of 0 there is treated as the owning skill.
  Demo/stage effects are not attributable at all.
* **Probe.** `ClaudeTools\effects\usage_index_probe.cs` (compile against the
  extracted build, like the other probes): prints index build time/size,
  BTL_CMN effects 0..89 with their users, the VGT 2:16 sharing and the
  z-assist resolution.

## Build
* `Build XenoKit.bat` / `Launch XenoKit.bat` (`LocalBuild\`), Release net48 only.
  The dependencies multi-target `net10.0-windows`, so
  `LocalBuild\net48-only.targets` drops that target for this build
  (`DirectoryBuildTargetsPath`); no .NET 10 SDK needed.
* MSBuild 2022 x64. `MSB4216` / `MSB4028` (`GenerateResource` task host) is an
  environment restriction (e.g. a sandbox blocking the task host), not code.

## State (2026-10-23)
* Camera/animation ID edit fix (2026-10-23): swapping IDs in the Camera tab
  used to throw `'Refresh' is not allowed during an AddNew or EditItem
  transaction.` (the synchronous sort, see "Gotchas found the hard way"). Both
  the Camera and Animation tabs now defer the sort until the cell edit has
  finished and re-select the edited animation, so the row no longer jumps.
  Rebuilt into `XenoKit\bin\Release\net48`; not yet tried in the editor.
* Idle "invisible exception" (2026-10-21): after XenoKit sat idle, an empty
  "Exception Thrown" prompt. Its text (read with UI Automation's TextPattern on
  the prompt's Document control - the window draws nothing) was
  `UCEERR_RENDERTHREADFAILURE (0x88980406)` from `HwndTarget.UpdateWindowPos ->
  DUCE.Channel.SyncFlush`: WPF's render thread died, so nothing (the prompt
  included) renders again. No GPU driver reset (no 4101) in the System log; the
  PC's monitors sleep after 1 h (`powercfg` VIDEOIDLE 3600), it has 3 monitors +
  a Parsec virtual display, and XenoKit was on the left monitor - monitors
  dropping off -> windows moved -> sync with the viewport's shared D3D11 surface
  fails (likely cause, not proven). Fix built into bin (2026-10-21), not yet
  seen past a monitor sleep: `D3D11Host.DisplayPower.cs` in MonoGameWpfInterloop registers
  GUID_CONSOLE_DISPLAY_STATE and on "off" stops rendering + detaches the
  D3DImage back buffer, on "on" recreates it; `ExceptionHelper` shows this one
  error in a plain Win32 MessageBox (GDI) since the WPF prompt can't draw. If it
  still happens after the fix, the fallback is `RenderOptions.ProcessRenderMode
  = SoftwareOnly` (slower).
  It happened again (2026-10-25) when every monitor loses power at once: no
  "display off" notification is sent, the displays just vanish. Dead end: WPF's
  AppContext switch
  `Switch.System.Windows.Media.ShouldRenderEvenWhenNoDisplayDevicesAreAvailable`
  alone (set in `App`'s static constructor, still there) did not stop it. Now
  `App`'s static constructor also sets `ProcessRenderMode = SoftwareOnly`; the
  viewport still shows because D3D11Image attaches with enableSoftwareFallback
  (WPF copies each frame to system memory). Built; not yet checked that the
  viewport speed is acceptable or that it survives a power cut.
* Effect Library: label positions confirmed in the editor after the X-mirror
  fix.
* Click-to-isolate (highlight one label, hide every other cell, click again to
  show the page) confirmed working in the editor.
* Fixed (built, not yet seen in the editor): setting a character as the primary actor
  while the library was open froze every cell. `SceneManager.SetActor` ends in
  `SceneManager.Stop()` -> `Viewport.IsPlaying = false`, and no library path set
  it back (only the auto-load branch, which doesn't run once an actor exists).
  The window now subscribes `SceneManager.ActorChanged`: slot 0 changes rebuild
  the page with the new actor and set `IsPlaying = true` again.
* Fixed (built, not yet seen in the editor): a primary actor change also left every
  cell showing a copy of the *old* actor, and those copies were always built
  from the plain BCS part set - a CaC came out as the default body. The pool is
  keyed on the actor's part set now and `CharaPartSet.CopyAppearanceFrom` copies
  the live actor's parts/colours into the copies.
* Fixed (built, not yet seen in the editor): an EEPK that fails to load only said
  "Could not load <path>"; the real reason (`EMB_Index is out of range`, missing
  `.ptcl.emm`) now comes from `EffectLibraryCatalog.LoadEepkAsync` into the
  status line (hover for the full text).
* Auras from Revamp+NT (`vfx/cmn/BTL_AURA.eepk`, 134 ptcl.emb textures with
  128..133 used) could not load at all - Xv2CoreLib's 128 effect-texture limit,
  see effects-and-vfx.md. Fixed there (255), which needs XenoKit rebuilt.
* Vanilla/Mod tags and the click details panel (parts + effect-level usage list)
  are built (2026-10-19); the user confirmed clicking labels works again after
  the Run -> TextBlock fix. The panel sits under the preview at the bottom left;
  it only takes clicks when its list can expand, so labels under it stay
  clickable for the short lists.
* Fixed (confirmed by the user in the editor, 2026-10-19): the vanilla/mod tag
  turned each label into `Inlines`, so WPF hit testing returned a `Run` and the
  click handler ignored the click - click-to-isolate and the details panel both
  stopped. The hit element is now resolved to its TextBlock by `GetClickedLabel`;
  verified with `ClaudeTools\effects\click_hittest_probe.cs` (Run -> TextBlock,
  cell Tag).
* Effect-level usage list (2026-10-19): the details panel now lists every skill
  and character whose BAC/BDM references the selected effect, first 5 with
  "click to show all N" (see "Effect-level users" above). Built, not yet seen in
  the editor; index/probe numbers are in that section.
* Offline check: `ClaudeTools\effects\eepk_load_probe.ps1` loads EEPKs through
  the same Xv2CoreLib build XenoKit uses and prints OK/effects or the exact
  exception (run it with Windows PowerShell; Z: is a "network location" to .NET,
  so it copies the DLLs to %TEMP% and loads them from bytes). `-DllDir` checks a
  freshly built Xv2CoreLib without installing it. Costura embeds the
  dependencies in XenoKit.exe and deletes the loose copies from bin, so when bin
  has no DLLs the probe runs `ClaudeTools\effects\extract_costura.ps1` and uses
  the assemblies taken out of the exe (exactly what the app runs with).
* Building in the agent sandbox: `Build XenoKit.bat` needs `-m:1` plus
  `-p:GenerateResourceMSBuildRuntime=CurrentRuntime
  -p:GenerateResourceMSBuildArchitecture=CurrentArchitecture` (MSBuild's worker
  nodes and the GenerateResource task host use named pipes, which the sandbox
  blocks - MSB4216/MSB4028). The app must be closed or the final copy fails
  (MSB3027: XenoKit.exe in use). A full `-t:Rebuild` lets Costura clean bin down
  to XenoKit.exe/pdb/config + the shader folders - that is normal, the
  dependencies are inside the exe.

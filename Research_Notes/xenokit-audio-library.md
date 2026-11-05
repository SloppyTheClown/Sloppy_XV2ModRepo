# XenoKit Audio Library (Tools\XenoKit)

Covers: the "Audio Library" window - which ACB files and cue ids exist, which
AcbType reaches each file, a preview, and who plays each cue. Opened from the
same menu as the Effect Library.

## How a sound reference resolves
A sound in a BAC (type 11), BDM (`Type0Entries`) or BSA (type 7) is only an
AcbType plus a cue id, so the type picks the one file the id is looked up in
(`Files.GetAcbFile`, `Editor\Files\Files.Lookup.cs`):

| BAC/BDM | BSA | Name | File |
|---|---|---|---|
| 0 | 0 | Common SE | `sound/SE/Battle/Common/CAR_BTL_CMN.acb` |
| 2 | 1 | Character SE | user's CSO `SePath`, `sound/SE/Battle/Chara/` |
| 3 | - | Character VOX | user's CSO `VoxPath`, `sound/VOX/Battle/Chara/[en/]` |
| 10 | 3 | Skill SE | the skill's `CAR_BTL_{letter}{ID2:D3}_{ShortName}_SE`, or CUS `SePath`, `sound/SE/Battle/Skill/` |
| 11 | - | Skill VOX | name starts with the skill's prefix (or CUS `VoxPath`) and ends in the voice code + `_VOX`, `sound/VOX/Battle/Skill/[en/]` |

* A skill reaches Common, its user's character SE/VOX and its own SE/VOX, and
  nothing else: another skill's SE is out of reach, whatever its cue ids are.
* On a character moveset, types 10/11 mean the moveset's own files.
* XenoKit's engine uses the English voice files.
* No type points at `sound/SE/Sys`, `SE/Demo`, `SE/Battle/Field`, `VOX/Demo`,
  `VOX/Quest`, `VOX/Sys` or `BGM`: the window lists them under "Other" so their
  ids aren't mistaken for usable ones.

## Undocumented BAC AcbType values
The game data holds BAC type 11 records with other numbers. Xv2CoreLib's enum
has no name for them and `GetAcbFile` returns null, so XenoKit plays nothing
for them (the exe's handling of 4 and 5 is below). Counts with Revamp
installed, 2026-11-03:
4 = 1422 (e.g. Goku's moveset, cue 0), 34 = 47, 35 = 38, 22 = 12, 23 = 12,
57 = 9, 52 = 4, 98 = 4, 28 = 4, 29 = 4, 26 = 2, 53 = 2, 256 = 2, 55 = 1, 5 = 1.
That is about 1.2% of all sounds. Several appear on base-game skills (Spirit
Bomb 22/23, Crusher Volcano 34/35), though the scan can't tell whether those
records are vanilla or Revamp's. The values look like bit flags on top of
the known types (34 = 2 + 0x20, 35 = 3 + 0x20), but that is a guess from the
numbers, not checked in the exe.

### What the exe does with them (game 1.27, traced 2026-11-04)
Sound play function 0x116585 (flags in r15, AcbType in r14d); slot lookup 0x117a60
is a switch on AcbType (jump table at 0x117d70):

| AcbType | Bank slot |
|---|---|
| 0 | Common SE: global at 0x157fa70, +8 |
| 2 / 3 | Character SE / VOX: `[rsi+8]` / `[rsi+0xc]` |
| 4 | global +0xc, **ground-material sounds**: the `cmp r14d, 4` block at 0x116595 casts a ray down (0x1441f0), returns silently with no ground under the character, and adds a material offset to the cue id (0x30a180). That's the footsteps, and why it has 1422 uses |
| 5 | global +0x14, bank not identified |
| 6 / 7 / 8 | fixed slots 0x140 / 0x13f / 0x1a2 |
| 1, 9 | none |
| 10 | Skill SE, see below |
| 11 | Skill VOX, same logic with +0x34, +0x850 |
| 12 | 0x13c / 0x13d if the index is 4 / 5 |

**Skill SE needs an entry from a skill slot's BAC** (exe trace only, **not tried in game**).
Type 10 (0x117bce) takes the index from `[rsi+0x178]` or 0xee9b0(chara+0x11d0), which returns
chara+0x364; -1 or above 8 means **no sound**. 0xf3da0 starts a BAC entry: 0xf5c00 resets +0x364
to -1, then it compares the entry's BAC file with the 9 slots at [+0x50..+0x90] and a match sets
the slot. So it isn't "a skill is in use" but "the running entry belongs to a slot's BAC". An after
BAC is none of the 9 (+0x98 is just past them, probably the after BAC, unconfirmed), so its entries
have no Skill SE bank. Matches the data: all 9 after BACs in the cpks and a modded install have
0 Skill SE / Skill VOX events.

To play new sounds from an after BAC: Character SE (AcbType 2). It resolves through the owner's CSO
`SE Path`, so an installer can merge the CSO rows of the races the skill allows, pointing them at
own-named copies of the race banks + new cues. Nothing vanilla is overwritten; LB merges rows by
chara + costume and tracks them for uninstall. An x2m can't edit CSO rows. No installer can merge
cues into `CAR_BTL_CMN` (LB's AudioPackage only takes BGM + AutoVoice), and shipping that whole bank
would overwrite every other mod's copy, so don't.

Flag bit 8 (StopWhenParentEnds): the path at 0x116861 keeps up to 4 handles at rdi+0x98
(16-byte stride, type at +0xa0, cue at +0xa4) and **skips a sound whose type + cue is
already listed**, so a looping entry doesn't stack it. Bit 2's path at 0x11675e uses the
list at rdi+0x58, bit 0x100's rdi+0x180.

Values above 12 (the 22, 34, 35... records) are still not read.

BSA: the enum only names 0/1/3 and suspects 2 and 4 are voice types.
`BsaType7ViewModel.GetBacAcbType` turns any other number into Common, so the
scan reports those as unknown instead of using it.

## Where the code is
* `XenoKit\Engine\Audio\AudioLibrary\AudioLibraryCatalog.cs` - lists ACBs (loose
  + cpk, deduped by hand: `GetFilesInDirectory`'s own duplicate check compares a
  relative path with a `data/` one), names owners from CSO/CUS, loads cue tables,
  vanilla/mod check.
* `...\AudioLibraryUsageIndex.cs` - the once-per-session "Used by" scan.
* `XenoKit\Windows\AudioLibrary\` - the window. `MainWindow.AudioLibrary.cs` -
  the menu command.
* `AudioEngine.StopPreviewCues` and `CueInstance.IsPreview` - stop previews
  without cutting the scene's sounds. `PreviewCue` now removes a cue whose
  `Init` throws.

## What the columns can't tell you
* **Current move group** re-resolves when the window regains focus, using the
  selected move and the primary actor.
* **Origin** compares cue ids and names with the cpk copy. A mod that swaps the
  audio under the same id and name reads as Vanilla.
* **Used by** has no entry for a skill's own Character SE/VOX records (type 2/3):
  they depend on who equips the skill. They're counted as
  `UserDependentSounds`. Skills' VOX keys ignore the voice, so all voice variants
  of one skill share their users. BDM `Type1` entries (old format) have no named
  audio fields and aren't scanned.
* **Previews** refuse Switch and ComboSequential cues (XenoKit's engine doesn't
  support them, the game may) and cues with no tracks.

## Upstream bug, not fixed
`ACB_Wrapper.Load` (Xv2CoreLib\ACB\ACB_Wrapper.cs:128) swaps the arguments of
its `.awb` path (`"{0}/{1}.awb", FileNameWithoutExt, DirectoryName`), so it never
finds an external AWB. The library avoids it by calling `ACB_File.Load` and
wrapping the result.

## Probe
`ClaudeTools\effects\audio_library_probe.cs` (compile against the extracted
build, like the other probes): group counts, files with no owner, scan size,
unknown AcbType values with an example, and load results per group (all SE, a
150-file sample of the large groups) plus spot checks.

Last run, 2026-11-03: 0 load errors, 0 missing AWBs, no duplicate paths. Catalog
229 ms, usage scan about 400 ms over 3540 files. Files per group: Common 1,
Character SE 117, Character VOX 408, Skill SE 782, Skill VOX 10567, Other 1348.
Only about 1.4% of the Skill VOX files have no owner in CUS.

## State
Window and play-on-select confirmed in the app 2026-11-03. Fix for stuck loops
built the same day, not yet tried in the app. Shared audio output and the female
majin voices built 2026-11-05, not yet tried in the app (OPEN_ITEMS.md).

## Stuck looping previews (engine bugs, fixed locally)
Some previews kept looping after Stop or a new selection. A wave with HCA loop
data repeats until its player is terminated, and `KillCue` only terminates what
is in `AudioPlayers`. Found by reading the code; which one a given cue hit was
not measured. Three ways out of that:
1. **Action_Play children weren't previews.** An ACB action track starts another
   cue (often the loop part of a sound) through the normal `PlayCue`, so
   `StopPreviewCues` skipped it. Children now inherit the preview flag.
2. **Stopped while loading.** Players joined `AudioPlayers` after their async
   load, so a cue killed mid-load left them to start anyway. They now join first;
   `WorldAudioPlayer.Terminate` during a load waits for `Init` and never plays.
3. **Later tracks of a sequence** (`TryPlaySeq`) never joined the list at all.
   Side effect of the fix: Sequential/Shuffle cues now play one track after
   another. Before, all tracks after the first started together. This also changes
   scene playback.

Changing the selection now always stops previews, even if the new cue won't play.

## Crash while arrowing through cues (2026-11-05, cause not proven)
One crash, no dump: `AccessViolationException` in `WeakObjectHashtable+WeakKeyComparer.Equals`, under
`ItemPeersStorage` <- `DataGridItemAutomationPeer` <- `UpdateLayout` <- `DataGrid.OnArrowKeyDown`. That code is all
managed, so a bad object reference there suggests a damaged GC heap, and the stack only shows who noticed.

**Suspect:** a WaveOut per sound. NAudio 2.4.0's `WaveOutBuffer.Dispose` frees the pins on header and buffer before
`waveOutUnprepareHeader` and `waveOutClose`, so winmm is handed addresses the GC may have moved.

**Probe:** `ClaudeTools\effects\Run_audio_churn_probe.ps1 old|new|control <seconds>`, silent sounds, forced compacting
GCs, `COMPlus_HeapVerify=1`. Old pattern: **no crash in 10,903 sounds over 180 s**, so the crash was **not reproduced**
and this cause is unconfirmed. New pattern: no crash in 11,458 sounds. The `control` mode (method table overwritten on
purpose) dies with 0xC0000005, so gross damage is caught; a narrow race or subtle damage could still be missed. The probe
has no WPF grid, no UI Automation client, no D3D and no real audio data.

**Changed anyway:** `SharedAudioOutput` keeps one `WaveOutEvent` + mixer open for the session (converted to 48 kHz
stereo, paused after 5 s idle, reopened if the device fails), so that teardown no longer happens per sound. Sound
start latency drops from 800 ms buffers to 150 ms. This affects scene playback too.

**If it crashes again, look at:** the UI Automation path (why a UIA client is active at all), and native code outside
audio (D3D11 interop, `Engine\Shader\Buffers.cs`). `HCA.DecodeToWav` is VGAudio; Xv2CoreLib's HCA folder has no unsafe
code, VGAudio itself was not checked. Get a dump this time.

## CaC voices
The CaC VOX switch in `Xenoverse2.cs` had no `MAF` case, so a female majin got no voice files (fixed locally). Checked
with `ClaudeTools\effects\Run_cac_vox_probe.ps1`: races 100, 101, 104, 106, 107 each get 30 banks (15 voices, en + jp), all
with cues. The `MAP` case can't run because the guard stops at 107; left alone.

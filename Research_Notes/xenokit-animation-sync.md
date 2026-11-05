# XenoKit: Sync Animations To Banks (local addition, 2026-11-05)

Animation tab, skill only: select animations, press **Sync** next to Add/Edit/Delete. A window has one row per
bank the game's races use (Male: Human, Saiyan; Female: Human, Saiyan, Majin; Majin male; Namekian; Frieza race). Each
ticked row fits the selection to the row's "Fit to" race and merges it into the chosen bank.

## Why a fit and not a copy
A skill's banks hold the same ids for different bodies. Pasting between them keeps the pelvis keys of the
source's leg length, so the character floats (source legs longer) or sinks (shorter). Leg lengths from the
fitter, in the race EANs' embedded skeletons:

| Race | Leg | Scale from HUM/SYM |
|---|---|---|
| HUM, SYM | .851 | x1.000 |
| HUF, SYF | .681 | x.800 |
| NMC | 1.023 | x1.202 |
| FRI | .622 | x.731 |
| MAM | .595 | x.699 |
| MAF | .701 | x.824 |

## Which banks exist
Vanilla skill EANs in the 1.27 cpks: no suffix 589, `_f` 306, `_mam` 299, the rest are character codes
(`_vgt`, `_frz`...). No `_hum`, `_huf`, `_sym`, `_syf`, `_nmc`, `_fri` or `_maf` exists, so **whether the game
loads those is untested**. XenoKit's loader takes the bank code from the 4th `_`-split part of the file name.

All female CaCs share F, so F holds one fit. HUM = SYM and HUF = SYF fit byte for byte (checked on Refinement's Main),
so those choices are equivalent; MAF's legs are about 3% longer than HUF's (.701 vs .681), so with F fitted to one, the
other's pelvis is about .02 off. "Fit to" defaults to the first race the CUS lock allows: HUF for an any-race skill, MAF for
Refinement. How visible that 3% is in game is not tested. Two rows moved onto one bank by hand are still refused.

## How it works
* `XenoKit\Windows\EAN\EanBankSync.cs` (no UI): `Fit` saves clones of the selection into a new EAN with the source
  bank's header + skeleton, runs the fitter, loads the result. `MergeIntoBank` replaces by id (other ids stay),
  inserts new ids in sorted order, or creates the bank. Everything is one composite undo.
* `XenoKit\Windows\EAN\EanBankSyncWindow.xaml(.cs)`: the rows. It fits everything before merging anything, so a failure
  leaves all banks untouched. Ticks by default only: a race of the row allowed by the CUS race lock, bank exists, not the source.
* The fitter is Battle Styles' `ean_fit.cpp` in skill mode, through `ean_race_fit.dll` (`Tools\EanRaceFit`,
  next to `XenoKit.exe`; `LocalBuild\build.bat` builds and copies it). A DLL rather than a C# port so there stays one
  fitter; rather than the exe so there are no temp files or process (and no sandbox trouble writing them).
* Target skeleton: takes the race's, unless the bank's other animations use bones the race lacks; then it stays and
  the report warns.

## Gotchas
* **The source bank's embedded skeleton sets the scale.** A bank made with XenoKit's Add button has the CMN skeleton, so
  fitting from one gives a wrong scale. The report prints the leg ratio (`leg a -> b`): check it.
* `EAN_File.SaveToBytes` removes nodes without a skeleton bone from the objects it is given: pass clones.
* `EAN_File.AddEntry(id, ...)` replaces the same id without an undo, so the merge doesn't use it.
* `EAN_Animation.LinkEskData` is internal; `EAN_File.LinkEskData()` is the public way to relink.

## Check
`ClaudeTools\eanbanksynctest\run.ps1`: a partial selection (first + last, leaving an id hole) gets the same keys as
`ean_race_fit.exe`'s whole-file fit; only the selected ids change; undo and redo return the bank byte for byte; a
missing id and a missing bank are created and undone; HUM/SYM and HUF/SYF fits are identical. Passed 2026-11-05 (MAF). The
window itself is not covered. In game: F fitted to MAF has the right height (first version of the window); the merged rows are untried.

## Credits
Fitter: found by reverse engineering the game (Battle Styles). EAN handling: LazyBone's Xv2CoreLib.

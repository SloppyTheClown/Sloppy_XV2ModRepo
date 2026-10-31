# Cross-game effects (other games' VFX in XV2)

Covers: whether effect files from other games (Naruto Storm Connections, JoJo
All-Star Battle R) can be used in XV2; the two engines' architectures (CC2
NUCC/XFBIN vs Dimps EEPK); which games share which; what is actually reusable
(textures, parameters) and the tool path. Research only - nothing installed or
tested in game. Also: porting a Sparking! ZERO (UE5) ultimate's animation,
camera and an effect to XV2 by rebuilding them (section "Sparking! ZERO ->
XV2").

Moved to their own files (2026-10-22):
* `sparking-zero-port.md`: Sparking! ZERO -> XV2 (Angry Explosion)

## Verdict

* XV2's effect pipeline is **EEPK** (`#EPK`) plus loose side assets, loaded from
  `data\vfx\...` and `data\skill\...`. There is no XFBIN/NUCC reader in the game
  and no `.xfbin` in the effect tree (counts below).
* Naruto Storm (incl. Connections) and JoJo ASB/ASBR/EoH are CyberConnect2's
  **NUCC** engine: **XFBIN** containers (`NUCC` magic), page/chunk model, effect
  data inside `.xfbin` chunks. Different container and different particle model.
* So the files are **not drop-in**. They move between CC2 games (with repacking),
  not into XV2. No EFB/NUCC -> EEPK converter is known, and Xv2CoreLib has no
  xfbin reader. What can be reused is the **textures** (NUT/DDS -> EMB), the
  **mesh** of mesh-based effects (EMD -> EMO in the EEPK Organiser, see below)
  and the look/timing, when the effect is rebuilt in EEPK.

## XV2 side (checked against the local 1.26.01 install)

* Header `#EPK` (`EEPK_File.cs`, `EEPK_SIGNATURE = 1263551779`, header 24 bytes,
  effect part 100 bytes, asset container 48 bytes, asset 12 bytes; version 37568
  in the current files).
* An EEPK is an asset-container table plus an effect table. Containers:
  `EMO`, `PBIND`, `TBIND`, `CBIND`, `LIGHT`; the effect's parts reference those
  assets (position/rotation flags, animation, bone direction, loop flags).
* Side assets beside the EEPK: `.emp` (particle), `.etr` (trace), `.ema`
  (animation), `.emo` (model), `.ecf`, `.emm` (material), `.emb` (texture)
  (`Xv2CoreLib.EffectContainer.EffectContainerFile.SupportedFormats`).
* Install layout: `data\vfx\{chara,cmn,stage,...}\<name>\<name>.eepk` +
  `<name>.ptcl.emb/.emm`, `<name>.pbind.emb`, `<name>.trc.*`, `.light.emb`;
  skills: `data\skill\<type>\<id>_<name>\<id>_<name>.eepk`.
* `data\vfx` file counts in 1.26.01: 226 `.eepk`, 1406 `.ema`, 1249 `.emb`,
  891 `.emm`, 605 `.emo`, 23 `.ecf`, **0 `.xfbin`**.
* Character -> EEPK mapping comes from `vfx_spec.ers` (XenoKit's Effect Library
  catalog reads it).
* Effect texture cap is 128 per `ptcl.emb` (the game's signed `EmbIndex` read at
  `xgcore+0x4c119`); the current fix is the AuraOptimizer texture merge, with
  `EffectTextureLimit`'s 255 patch as the opt-in alternative
  (`effects-and-vfx.md`).

## CC2 side (from the format docs and the ASBR Steam file list)

* XFBIN = magic `NUCC`, big-endian header (version 121 for ASB/EoH/ASBR), then
  `nuccChunkIndex` and pages of chunks (`nuccChunkBinary`, `nuccChunkModel`,
  `nuccChunkTexture`, `nuccChunkParticle`, `nuccChunkTrail`, ...). Chunk paths
  are stored inside (e.g. `cmnparam/bin/230/...`). CC2's NU engine is used across
  the Storm and JoJo games (JoJo wiki "NUCC" page).
* ASBR (Steam): content is in `data\launch\*.cpk` and `data\patch\patch*.cpk`;
  loose data under `data\...`. Effect-related files seen in the file list:
  per-character `<char>eff1.xfbin` (e.g. `4fgm01eff1.xfbin`) in `spc`,
  `spc\effectprm.bin.xfbin`, `spc\damageeff.bin.xfbin`, and `data\sprite.eft`
  (effect sprite/texture bank).
* Storm series: the effect assets live under `data\effect\efb` as `.xfbin`
  (reported by the user; matches the per-character `eff1.xfbin` layout in ASBR).
* Tools: `xfbin_lib` (Python, unpack/repack pages/chunks), XFBIN Parser++ (ASBR
  focus, WIP, chunk -> JSON), XFBIN Texture Tool / Blender XFBIN importer
  (NUT/DDS), NSC Toolbox (Storm 4/Connections param tables, incl.
  `effectprm.bin.xfbin`), plus a CRI CPK extractor for the `data.cpk`.

## Games by engine

| Engine | Games | Effect files usable in XV2? |
|---|---|---|
| Dimps (EEPK) | Dragon Ball Xenoverse (1) - same engine family, unverified; XV2 itself/DLC | only candidate, needs checking |
| CC2 NUCC (XFBIN) | Naruto Storm 1/2/Generations/3/4/Connections; JoJo ASB, Eyes of Heaven, All-Star Battle R; Asura's Wrath (unverified) | no - different engine |
| Other | FighterZ (ArcSys), Kakarot (UE), Hinokami Chronicles (UE), Sparking Zero (UE5), Battle of Z (Artdink) | no (files); Sparking! ZERO's animations, cameras and textures can be read and rebuilt - see below |

Storm <-> JoJo is the real cross-game pool: same container, so effects can be
unpacked/rebuilt between those titles, but their effect IDs, bone names and
texture sets still differ per game.

## What an actual port involves

1. Own the game, extract its CPK (CRI).
2. Unpack the XFBIN pages/chunks (`xfbin_lib`, XFBIN Parser++).
3. Textures: NUT -> DDS -> EMB (Xv2CoreLib / XenoKit texture import).
4. Read the effect (timing, colours, emitter counts, trails) and rebuild it as
   EMP/ETR/EMA/... inside an EEPK - XenoKit's Effect Library previews EEPKs,
   EEPK Organiser edits them; usually fastest to start from the closest XV2
   effect and re-texture/re-time it.
5. Wire it up: skill CUS/BAC effect entry (`EepkType`, `EffectID`), or the
   character's own EEPK in `data\vfx\chara\<id>`; ship as x2m/LB package.

## The `.emo` route (what the modders mean)

`"If you can convert them to .emo then yes"` is accurate for **geometry**, and
the tooling is already in the EEPK Organiser: the EMO asset tab's "Add EMD file"
is literally titled *EMD + ESK -> EMO conversion*
(`EEPK Editor\View\EepkEditor.Asset.cs`, `AssetEmoAddEmdFile`, calling
`EMO_File.ConvertToEmo`). It merges one or more EMDs plus their EMB/EMM (and an
optional DYT) into a single EMO asset + merged EMB + merged EMM.

So the real chain for an imported effect mesh is:

    source mesh -> EMD + ESK + EMM + EMB -> EMO inside an EEPK

Constraints in that code (why it is not a one-click Storm-import):
* Requires an **ESK skeleton**; EMO parts link to its bones (`EMO_Part.LinkedBone`).
* Each EMD needs its EMM/EMB beside it with the same basename, plus optional
  `<name>.dyt.emb`; one DYT per EMD if any.
* The target EMO asset must not already contain an EMB/EMM/EMO.
* Character-shader materials must be swapped for VFX ones - `TOON_UNIF...` ->
  `TOON_UNIFfx_VFX_DFDna_FCM` - because "regular character shaders do not work
  as effects (game crashes)" (comment in `ConvertToEmo`).
* All textures merge into one EMB, capped at `MAX_EFFECT_TEXTURES` (128)
  (`effects-and-vfx.md`).
* Animated EMOs are supported: EMA (an EAN -> EMA conversion button sits in the
  same tab, marked "TODO: Make worky"), plus the EEPK's own part animation;
  XenoKit plays them (`VfxEmo`, `EmaAnimationPlayer`).

What it does **not** carry over is the particle behaviour: EMP is a different
system (emitters, curves, colour-over-life) and nothing converts an EFB
particle definition to an EMP. "Any effect can work" therefore means "any
effect can be *rebuilt*", with mesh + textures carried across and the emitter
setup re-authored.

## The JoJo stand moveset video

A stand that follows and punches is **model + skeleton + animation + moveset**
work, all of which XV2 has natively: EMD/ESK geometry, EAN/BDM animation,
BAC/BCM for the moves, EEPK for auras/hits - the same class of work as
`ModelSwaps` and `BattleStyles`, plus hand-authored effects. If the stand is
only ever visible during attacks it is likely EMO geometry inside those EEPKs;
if it is persistent it is a costume/accessory or an EMO part bound to a
character bone. Either way it demonstrates the ceiling is effort, not the file
format; it is not evidence that ASBR/Storm files load.

## Dead ends (don't re-check)

* "XV2 loads xfbin": no reader in `DBXV2.exe`/`xgcore.dll`, and no `.xfbin`
  under `data\vfx`.
* SteamDB depot browsing: 403 + explicit no-API notice; not a source.
* JoJo wiki search for "efb"/"effect": not documented; the file list lives in
  `All-Star Battle R Files.json` (linked from `Category:ASBR_Files`).
* grep.app code search: 429 (Vercel checkpoint).

## State (2026-10-18, CC2 research)

Research only. Nothing bought, downloaded or installed. Community claims seen
so far: "convert them to .emo then yes" (matches the EEPK Organiser's
EMD+ESK -> EMO conversion, so it is about reusing the geometry) and "any effect
can work, you can make JoJo stands" with a video of a stand moveset (native
EMD/ESK/EAN/BAC/EEPK content, not an import). Neither claim implies ASBR/Storm
files loading as-is.

If a mod/claim is found, check what it actually ships: an `.eepk` (recreated
effect) versus an `.xfbin` (would be ignored by XV2).

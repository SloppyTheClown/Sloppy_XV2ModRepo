# Battle styles: custom styles from moveset mods

Covers: Phase 5 - styles taken from installed moveset mods (Gun Fu Rush confirmed).
Split out of `battle-styles.md` (2026-10-22).

## Phase 5: custom styles from moveset mods (2026-10-14, Gun Fu Rush confirmed in game)
Goal: a moveset mod that replaces a CaC race's (or a roster character's)
moveset installs as an extra style instead, listed under every `styles`
setting, nothing vanilla replaced.

**Converted styles' CMS entries can vanish (2026-10-18):** every converted
style was missing in game (log: `custom style 910 - HUM line: "910" is no
CMS moveset with its files - skipped`) while its files, style xml, ERS
entries and LB tracker were all still installed: the loose
`system/char_model_spec.cms` had none of the LB-added entries (ids 400+).
It happened twice: between the Gun Fu Rush install and the next ones
(2026-10-14 - LB's AutoCharaID then handed 400/401 out again, to Kick
Tempest and Muay Thai) and again around 2026-10-16 18:21, when the xv2ins
system file set (cso, psc, aur, sev, cml, pup) was rewritten; the CMS was
saved again by an LB install at 19:10. Likely cause (user): xv2ins was
left open while the LB installers ran, and its later installs wrote back
the system files it had loaded at start, without LB's entries. Not proven,
but it fits both dates. Avoid it by closing xv2ins before running an LB
installer and reopening it afterwards. Restored with
`ClaudeTools\battle_styles\restore_style_cms.py` (entries from each
installer's `.installinfo`, ids from the LB trackers, clashing ids moved to
the next free one and the tracker updated; dry run without `--write`).
The double-click wrapper `Restore_Style_CMS.bat` reads copies of the 7
installers in `ClaudeTools\battle_styles\installers\` and skips entries
that are already there. ERS type-2 sub-entries are a table indexed by CMS
id (Xv2CoreLib ERS\Parser.cs), so on a clash move the style without ERS
entries. A DLL self-heal (adding missing entries in memory) is parked: the
user worries it could break something. Its risks: the healed id may clash
with an id that xv2ins or LB hands out later, and it may not match the
ERS index.

**LB needs folder entries in the .installinfo zip** (first in-game try,
Gun Fu Rush): the converted installer added its CMS/ERS/CMN merges but no
JUNGLE1 file, so the style's EAN was missing and the DLL left it out
("910 left out - chara/910/910.ean not found"). LB's `JungleCheck`
(Install.cs) tests `ZipManager.Exists("JUNGLE1/")` = `ZipArchive.GetEntry`,
which only finds an explicit directory entry; Python's zipfile writes
none. The converter now adds an entry for every folder
(`add_folder_entries`), as LB's own packer does.

**Any input (2026-10-14, offline only):** the converter also reads x2m
characters, bare moveset files and archives (`read_mod`). Findings:
* Windows' own `%SystemRoot%\System32\tar.exe` (bsdtar/libarchive 3.8.4)
  lists and extracts .rar (rar4 + rar5) and .7z; Git Bash's GNU tar can't.
  7-Zip/WinRAR are not on the PC.
* x2m `<Entry>` (eternity CmsEntryXV2) -> CMS binary fields: U_10 = I_16
  (+0x10), LOAD_CAM_DIST, U_16 = I_22, U_18 = I_24, U_1A = I_26, U_1C =
  I_28; I_08 always 0. CHARACTER = BCS, AI = BAI, STR_50 = BDM. Paths
  without "../" are the x2m's own folder (not installed) -> HUM's.
* An x2m model may drop a prop the moveset animates: Dark King Fu (USV)'s
  XFD keeps XBF's a_x_sword1-4 in ESK + EAN but ships no sword mesh (its
  Face_forehead is face marks; XBF's sword is `XBF_000_Face_forehead.emd`),
  so its style fits "prop bones but no mesh skinned to them".
* x2m `<CsoEntry SE=...>` may name a roster bank (Dark King Fu: XBF's) and
  `CHAR_EEPK` a roster EEPK (chara/XBF/XBF.eepk); the JUNGLE then still
  ships an unused CAR_BTL_<own>_SE.
* CSO: "#CSO", count +8, table +0xc, 32-byte entries (chara id +0,
  costume +4, SE name offset +8).
* **cpk keys include `data/`** (`data/system/char_model_spec.cms`); the
  converter's `Game` had looked them up data-relative and silently read
  nothing from the cpks (Gun Fu worked only through Revamp's loose files).
* Download names: Xenoverse Mods prefixes an upload id
  (`1729306997_Kick-Tempest-MAF.rar`, `1789314554921-Kaido_Final.rar`);
  race moveset mods are often just `MAF.ean` + `MAF_PLAYER.bac/bcm/bdm`
  at the archive root.

* **Style XMLs** `<game>\data\battle_styles\styles\*.xml`:
  `<BattleStyle code="910" name="Gun Fu Rush"><Race race="HUM" moveset="910"/>...`.
  `StyleMenu_MovesetFor` picks the line: exact race -> `ANY` -> same sex ->
  first line. Each line's moveset must be a CMS entry with its files; a CMS
  code used only by custom styles is not listed on its own.
* **Converter** `BattleStyles\tools\moveset_to_style.py` (wrapper
  `Convert_Moveset (Drag installers here).bat`): reads an LB installer (`.installinfo` / its exe)
  or loose `data` folders, takes every option of every step, groups
  identical race variants (Gun Fu Rush: HUM=SYM, HUF=SYF) and gives each
  variant a new CMS code ("9" + 2 base36 chars from crc32(name), skipping
  CMS codes and other style xmls). Writes `_Releases\Battle Style - <name>\`
  with the mod's own LB exe renamed + a new `.installinfo`:
  JUNGLE1 = `chara/<NEW>/<NEW>.ean/_PLAYER.bac/bcm/bdm`, `vfx/chara/<NEW>/`,
  `sound/SE/Battle/Chara/CAR_BTL_<NEW>_SE.acb`, the style xml; CMS + ERS
  entries merged; other merges (BSA, shot BDM) kept as is; the mod's own
  CMS/CSO/ERS edits dropped.
* **CMS entry:** numbers copied from the source entry (I_16, I_22..I_28 ...);
  moveset fields (EAN, BAC, BCM, BDM) = the new code's own files; every
  other field points back at the source: the getter maps value "X" to
  `chara/<code>/X` and "../A/B" to `chara/A/B`, so `../HUM/HUM` etc.
* **LB facts (source in `ClaudeTools\XV2-Tools\LB Mod Installer`):** CMS Entry ID
  and ERS SubEntry ID take bindings: `{AutoCharaID=(400;999), SetAlias=X}`
  then `{GetAlias=X}` (ERS sub-entries go through a second binding pass,
  Install.cs InstallSubEntries). **CSO Chara_ID is an int - no binding**, so
  the converter writes no CSO entry and the DLL finds a style's SE bank by
  name: `StyleSeBankOf` = CSO bank, else `CAR_BTL_<code>_SE` when the acb
  exists. JUNGLE1 is copied into data as-is after the Files. The installer
  identifies a mod by Name: running it again uninstalls.
* **Packed effect files:** an EEPK's .emp/.etr files live inside its
  `<name>.pbind.emb` / `.tbind.emb` containers (Gun Fu Rush's 8 .emp are in
  `HUM.pbind.emb`), so the converter's missing-asset check looks inside them.
* **Side installers:** Gun Fu Rush ships optional "Effects Installer for CaCs"
  (race ERS entries) and "Projectiles Installer" (CMN.bsa + shot.bdm merges)
  for drag-and-drop users; the main installer already carries the same files.
* Not seen in game: LB 3.27's CMS/ERS/BSA/BDM merges on 1.26, fit + SE merge
  + effects with a custom code, online with a custom style (the CMS id comes
  from AutoCharaID, so it may differ between two PCs; the code is the same).

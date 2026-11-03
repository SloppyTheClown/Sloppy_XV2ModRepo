# Changelog

The version is in `VERSION`; it goes up with every release.

## 0.1.0 - 2026-11-02

* First release, for game version 1.27. Needs XV2 Mod Loader 1.2.0 or newer.
* Customize > Skills: a Ki Blast Override row opens a list of the loaded ki
  blast skills (Default and Race Default first), without roster blasts that only
  copy a standard type. Every blast works on any race. Picks are saved per Steam
  account, save slot and preset; the Presets panel copies them.
* With a blast or Race Default picked, the super soul no longer changes the
  ki blast; transformations still do. Default leaves the game's behaviour.
* The Presets panel and the character select's skill popup show the equipped
  blast in a Ki Blast Type row.
* Blasts are listed by name: who uses them plus the type, for example
  "Kid Buu - MAS" or "Hit - Power". A mod's blast shows the character it belongs
  to or the character using it, and is always listed.
* A search box on the list (`blast_search`, on by default).
* Blasts with animations only for their owner are converted for CaCs automatically,
  including ones from mods; `converted_eans.txt` lists the files for uninstalling.

# Changelog

The version is in `VERSION`; it goes up with every release.

## 0.1.0 - 2026-10-31

* First release, for game version 1.27. Needs XV2 Mod Loader 1.1.0 or newer.
* Equip screen: the cloth tab's Accessory row has two more columns (left /
  right), each opening the accessory list to pick an extra; picks are saved
  per save slot and preset in `multi_accessory.sav`. The menu edit goes
  through XV2 Mod Loader's UI library, so it stacks with Battle Styles'.

* Up to two extra accessories per save slot from `multi_accessory.json`,
  drawn in the equip screen, save select, character select, Conton City,
  battles and cutscenes (local only).
* Conflicts: an extra that would replace a body part the equipped
  accessory (or an earlier extra) replaces is skipped.

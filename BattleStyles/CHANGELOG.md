# Changelog

The version is in `VERSION`; it goes up with every release.

## 1.1.1 - 2026-11-02

* The Battle Style row and hooks no longer stall for minutes at start-up when
  another mod (Ki Blast Override) has already made the same list patch.
* With XV2 Mod Loader 1.2.0 or newer, the Skills page's box grows by one row
  for the Battle Style row.

## 1.1.0 - 2026-10-31

* For game version 1.27 (was 1.26.01). Needs XV2 Mod Loader 1.1.0 or newer.
* Works next to other mods that edit the Customize menu (Multi
  Accessory): the menu's user-data count is only ever raised, and the menu
  callbacks are chained safely when another mod hooks them at the same time.

## 1.0.0 - 2026-10-15

* First versioned release (everything in the README).

# Changelog

The version is in `VERSION`; it goes up with every release.

## 1.1.0 - 2026-10-31

* For game version 1.27 (was 1.26.01). Needs XV2 Mod Loader 1.1.0 or newer.
* The partner switch moves to **D-pad up** (`toggle_button`).
* New: **D-pad down** (`photo_button` / `photo_key`) turns on the game's
  photo camera (the free camera of the online battle photo mode) and holds
  both characters still; press it again to go back. Pausing turns it off.
  The battle keeps running. The battle HUD is hidden while it is on
  (`photo_hide_hud`, default on).
* The buttons are now ignored only while the game is paused, so switching
  should also work during ultimates.

## 1.0.1 - 2026-10-20

* The toggle no longer fires while the game is paused (D-pad down moved
  the Training menu's cursor and switched at the same time).
* Switching back restores the partner's own AI mode before Training
  re-applies its CPU setting, so a "CPU AI" partner always fights again.
  Changing the CPU setting while switched no longer crashes the game on
  unpause (it switches back first).

## 1.0.0 - 2026-10-20

* First release: D-pad down (configurable, plus an optional keyboard key)
  hands your controller and the camera to the Training partner and back.

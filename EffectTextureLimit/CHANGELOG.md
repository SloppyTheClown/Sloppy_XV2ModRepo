# Changelog

The version is in `VERSION`; it goes up with every release.

## 1.1.1 - 2026-10-31

* For game version 1.27 (was 1.26.01). Needs XV2 Mod Loader 1.1.0 or newer.
* Fix: `"raise_limit": true` was never read, so 1.1.0 never raised the
  limit.

## 1.1.0 - 2026-10-18

* Off by default: the limit is only raised with `"raise_limit": true` in
  `effect_texture_limit.json`. Files with more than 128 textures break other
  tools; merging textures (EEPK Organiser's Optimize Textures) is the
  recommended fix. A config without the key counts as off.

## 1.0.0 - 2026-10-15

* First versioned release (everything in the README).

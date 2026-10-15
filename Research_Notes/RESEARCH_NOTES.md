# Research notes - index

Game 1.26.01 internals, file formats, how they were found, dead ends, and
per-mod state. Addresses are DBXV2.exe 1.26.01 RVAs unless a DLL is named.

**Open only the file for the area you are working on.** Each file starts
with a "Covers:" line; don't read the others unless that one points there.

| File | Read when the task involves |
|---|---|
| [exe-and-tooling.md](exe-and-tooling.md) | patching/hooking the exe, global singletons, the shared file-request hook, debugging methods, decrypted images, any `tools\` probe script |
| [stats-and-combined-supers.md](stats-and-combined-supers.md) | attack stats, HP, level table, PSC body types, buffs, damage, the battle character struct, Combined Supers / Height |
| [qol-mods.md](qol-mods.md) | General_QoL: features, config keys, tests, what is confirmed |
| [lobby-quests-and-shops.md](lobby-quests-and-shops.md) | lobby NPCs (TNL/TSD/TSR), NPC ids, quest-list menus + DLC check, QXD requirements, item shop availability, TP medal shop |
| [save-flags-and-unlocks.md](save-flags-and-unlocks.md) | save manager layout, SYSFLAG/ACCFLAG, quest progress, what unlocks shops/desks/flight/teleports/rifts, level cap, per-CaC save data + merging slots (SaveSync), inventory/skill lists in memory + giving items/unlocking skills (debug menu item getter) |
| [sound.md](sound.md) | which cue an event plays, voice banks, ACB edits, volume options/categories |
| [ui-iggy-and-textures.md](ui-iggy-and-textures.md) | menu titles, .iggy/.iggytex/.iggyted, texture renumbering, EMB images, msg text lookup, item lists (Menu::ItemList) |
| [boot-and-camera.md](boot-and-camera.md) | logos, intro movie, title, the renderer camera, culling, camera zoom |
| [revamp-on-1.26.md](revamp-on-1.26.md) | XV2_RevampBridge, porting Epatches between game versions, Revamp content conflicts |
| [data-files-and-xv2dataedit.md](data-files-and-xv2dataedit.md) | .cpk archives, QXD/CUS/IDB/BDM/BCM layouts, skill/item names, XV2DataEdit |
| [loader-and-steam-deck.md](loader-and-steam-deck.md) | XV2ModLoader, launchers, Add To Steam, Proton / Steam Deck |
| [lobby-physics-and-debug-menu.md](lobby-physics-and-debug-menu.md) | the lobby player's Havok (hknp) body, collision filter/layers, noclip, DebugMenu |
| [battle-styles.md](battle-styles.md) | BattleStyles hub: goal, EquipableMovesets reference mod, CMS path getter, phase plan + state, style names, which movesets are listed. Its table points to the topic files: `battle-styles-fit.md` (EAN/BAC/BDM fit to race), `-sound-and-effects.md` (SE bank merge, style EEPK, cue limits), `-props.md` (held props), `-loading-and-online.md` (setup list, online sync, presets/sidecar), `-ui.md` (equip slot, style list, menu rows, FFDec bug) |
| [chara-select-search.md](chara-select-search.md) | CharaSearch: character select search + favorites, CHARASELE script/callback shared with Battle Styles, xv2patcher's user-data slot mapping |
| [effects-and-vfx.md](effects-and-vfx.md) | EEPK/particles/effect textures, the 128 ptcl.emb texture limit (xgcore signed index, one-byte fix), Xv2CoreLib's MAX_EFFECT_TEXTURES, New Transformations vs Revamp BTL_AURA |
| [new-transformations.md](new-transformations.md) | New Transformations skills: battle HUD stage names ("???"), Hair Change face/hair swap, BCS colour indices vs Revamp, XV2_NewTransformationsBridge |
| [costumes-and-models.md](costumes-and-models.md) | costume x2m by script (x2m.xml, BCS part sets, IDB), EMD/EMM/EMB via the XML serializer, character shaders; accessories as physics objects (keep the hair), XV2_ClownNose; local x2m sources/tools, checking an installed x2m |

## Adding to these notes
* Put a finding in the topic file it belongs to (new file only for a new
  area; then add a row here). Keep "Covers:" accurate.
* Record facts, the method that worked and the dead ends - not session
  narrative. Mark anything unverified as such.
* Per-mod state ("confirmed / not tested") goes in that mod's topic file;
  the short open-items list lives in CLAUDE.md.

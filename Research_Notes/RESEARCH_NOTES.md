# Research notes - index

Game 1.27 internals, file formats, how they were found, dead ends, and
per-mod state. Addresses are DBXV2.exe 1.27 RVAs unless a DLL is named (converted
from 1.26.01 with the map in exe-and-tooling.md "Porting to a new game build";
byte listings and revamp-on-1.26.md / new-transformations.md stay 1.26.01).

**Open only the file for the area you are working on.** Each file starts
with a "Covers:" line; don't read the others unless that one points there.

| File | Read when the task involves |
|---|---|
| [exe-and-tooling.md](exe-and-tooling.md) | patching/hooking the exe, global singletons, the shared file-request hook, debugging methods, decrypted images, any `tools\` probe script |
| [xenokit.md](xenokit.md) | Tools\XenoKit local additions: Effect Library (grid, labels, click-to-isolate, which skills/characters use an effect), external back buffer, local build helper |
| [xenokit-animation-sync.md](xenokit-animation-sync.md) | XenoKit's Sync Animations To Banks: fitting skill animations to other bone structures, leg lengths, which banks vanilla skills use |
| [xenokit-akeytsu.md](xenokit-akeytsu.md) | XenoKit's Akeytsu menu: .akt export/import, IK, cameras, its state |
| [xenokit-audio-library.md](xenokit-audio-library.md) | XenoKit's Audio Library: how AcbType + cue id resolve to a file, undocumented AcbType values, its state |
| [stats-and-combined-supers.md](stats-and-combined-supers.md) | attack stats, HP, level table, PSC body types, buffs, damage, the battle character struct, Combined Supers / Height |
| [qol-mods.md](qol-mods.md) | General_QoL: features, config keys, tests, what is confirmed |
| [lobby-quests-and-shops.md](lobby-quests-and-shops.md) | lobby NPCs (TNL/TSD/TSR), NPC ids, quest-list menus + DLC check, QXD requirements, item shop availability, TP medal shop |
| [save-flags-and-unlocks.md](save-flags-and-unlocks.md) | save manager layout, SYSFLAG/ACCFLAG, quest progress, what unlocks shops/desks/flight/teleports/rifts, level cap, mentor progress, Shenron wishes |
| [savesync.md](savesync.md) | per-CaC save data, merging slots offline and in game (SaveSync) |
| [save-inventory-and-skills.md](save-inventory-and-skills.md) | inventory/skill lists in memory, giving items/unlocking skills (debug menu item getter) |
| [sound.md](sound.md) | which cue an event plays, voice banks, ACB edits, volume options/categories |
| [ui-iggy-and-textures.md](ui-iggy-and-textures.md) | menu titles, .iggy/.iggytex/.iggyted, texture renumbering, EMB images, msg text lookup, editing AS3, merging several mods' menu script/texture edits |
| [ui-iggy-display-list.md](ui-iggy-display-list.md) | an .iggy sprite's timeline (PlaceObject/ShowFrame records), adding a button or resizing a box by display-list edit, AVATAR edit_menu/editmenu layout |
| [ui-item-lists.md](ui-item-lists.md) | item lists (Menu::ItemList), Item Search |
| [ui-menu-screens.md](ui-menu-screens.md) | specific menus: lobby map (LOBYHUD), stat radar chart (CMN_CHART), attributes screen |
| [boot-and-camera.md](boot-and-camera.md) | logos, intro movie, title, the renderer camera, culling, camera zoom |
| [revamp-on-1.26.md](revamp-on-1.26.md) | XV2_RevampBridge, porting Epatches between game versions, Revamp content conflicts |
| [data-files-and-xv2dataedit.md](data-files-and-xv2dataedit.md) | .cpk archives, QXD/CUS/IDB/BDM/BCM layouts, skill/item names, XV2DataEdit |
| [loader-and-steam-deck.md](loader-and-steam-deck.md) | XV2ModLoader, launchers, Add To Steam, Proton / Steam Deck |
| [lobby-physics-and-debug-menu.md](lobby-physics-and-debug-menu.md) | the lobby player's Havok (hknp) body, collision filter/layers, noclip, DebugMenu |
| [battle-styles.md](battle-styles.md) | BattleStyles hub: goal, EquipableMovesets reference mod, CMS path getter, phase plan + state. Its table points to the topic files: `battle-styles-roster.md` (style names, unlocked/giants, DLC characters, broken movesets), `-custom.md` (Phase 5: styles from moveset mods), `-fit.md` (EAN/BAC/BDM fit to race), `-sound-and-effects.md` (SE bank merge, style EEPK, cue limits), `-props.md` (held props), `-loading-and-online.md` (setup list, online sync, presets/sidecar), `-ui.md` (equip slot, style list, menu rows, FFDec bug) |
| [chara-select-search.md](chara-select-search.md) | CharaSearch: character select search + favorites, CHARASELE script/callback shared with Battle Styles, xv2patcher's user-data slot mapping |
| [effects-and-vfx.md](effects-and-vfx.md) | EEPK/particles/effect textures, the 128 ptcl.emb texture limit (xgcore signed index, one-byte fix), Xv2CoreLib's MAX_EFFECT_TEXTURES, New Transformations vs Revamp BTL_AURA |
| [cross-game-effects.md](cross-game-effects.md) | effects from other games: CC2 NUCC/XFBIN (Storm, JoJo) vs XV2's EEPK - which games share an engine, what can be reused (textures only), the extraction/port path |
| [sparking-zero-port.md](sparking-zero-port.md) | Sparking! ZERO (UE5) paks via CUE4Parse, retargeting its animations/cameras onto XV2, rebuilding a Niagara effect as EMP, WindIN, x2m skill id placeholders (Angry Explosion) |
| [new-transformations.md](new-transformations.md) | New Transformations skills: battle HUD stage names ("???"), Hair Change face/hair swap, BCS colour indices vs Revamp, XV2_NewTransformationsBridge |
| [awoken-skills.md](awoken-skills.md) | awoken skills/transformations: CUS model/skillset swap, skillset presets, after BAC/BCM paths, files-loaded flags, fixed-path files in an x2m (JUNGLE), Purification's parts, XV2_PurificationControlled |
| [training-qol.md](training-qol.md) | Training_QoL: per-slot input bindings, human player pads, the pad-reassign/disconnect check, local player override, battle camera follow target + setter, Body Change swap |
| [costumes-and-models.md](costumes-and-models.md) | costume x2m by script (x2m.xml, BCS part sets, IDB), EMD/EMM/EMB via the XML serializer, character shaders; accessories as physics objects (keep the hair), XV2_ClownNose; local x2m sources/tools, checking an installed x2m |
| [multi-accessory.md](multi-accessory.md) | wearing up to 3 accessories at once (phase 0: loader/storage traced, cap 4 objects): accessory loading, extra save/equip slots, sending extras online, blockers |
| [multi-accessory-phase0.md](multi-accessory-phase0.md) | multi accessory phase 0 detail: model builder, CharaResourceBase part/physics paths, physics cap, record layout |
| [ki-blast-type.md](ki-blast-type.md) | the CaC's charged ki blast type: race/super soul pick 0x618d30 and its callers, loadout +0xa2, online record, type names, equip-mod plan |
| [multi-accessory-presets.md](multi-accessory-presets.md) | multi accessory in the Presets panel: extras' rows, part set -> item name, panel layout, merging with Battle Styles' row |
| [multi-accessory-wigs.md](multi-accessory-wigs.md) | stacking a 2nd part per slot (wigs/helmets): Chara part slots, hide mask, xgcore Skeleton model map, plan |

## Adding to these notes
* Put a finding in the topic file it belongs to (new file only for a new
  area; then add a row here). Keep "Covers:" accurate.
* Record facts, the method that worked and the dead ends - not session
  narrative. Mark anything unverified as such.
* Per-mod state ("confirmed / not tested") goes in that mod's topic file;
  the short open-items list lives in the workspace's OPEN_ITEMS.md.
* Keep each file under ~20 KB: when one grows past that, split a
  self-contained section into its own file and add a row here.

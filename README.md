# Sloppy XV2 Mod Repo

Mods for Dragon Ball Xenoverse 2 (game version 1.26.01), built around a small
mod loader. Each folder is its own project with a README that explains what
it does and how to install it.

## Installing

Install [XV2 Mod Loader](XV2ModLoader/README.md) first; every other mod here
runs on it. Then add the mods you want - each README has the steps. Release
packages are on the [Releases](../../releases) page.

## Mods

| Mod | What it does |
|---|---|
| [XV2 Mod Loader](XV2ModLoader/README.md) | Loads DLL mods and Lua scripts from their own folders; an in-game menu; launchers for modded/unmodded play |
| [Battle Styles](BattleStyles/README.md) | Equipable movesets for created characters |
| [Chara Search](CharaSearch/README.md) | Search and favorites in the character select |
| [Combined Super Stats](CombinedSupers/README.md) | Supers and ultimates scale off Strike + Ki Blast Supers combined |
| [Debug Menu](DebugMenu/README.md) | Noclip and an item getter in the loader's menu |
| [Effect Texture Limit](EffectTextureLimit/README.md) | Effect texture files can hold 255 textures instead of 128 |
| [General QoL](General_QoL/README.md) | Unlocks and quality-of-life changes, each switchable |
| [Item Search](ItemSearch/README.md) | A search box over the item lists |
| [SaveSync](SaveSync/README.md) | Shares unlock progress between your created characters |

[Research_Notes](Research_Notes/RESEARCH_NOTES.md) holds what was found
about the game's internals while making them.

## Versions

Every mod has its own version in its `VERSION` file and a `CHANGELOG.md`;
release packages are named `<Mod>-<version>.zip`.

## Licence

Public domain ([The Unlicense](LICENSE)): use any of it for anything.
Bundled third-party code keeps its own licence (listed in [LICENSE](LICENSE)).

## Credits

Each mod's README lists the tools, libraries, references and community work
it builds on.

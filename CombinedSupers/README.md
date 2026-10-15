# Combined Super Stats

A Dragon Ball Xenoverse 2 mod that makes **super and ultimate attacks scale off
your combined Strike Supers + Ki Blast Supers investment**, instead of each
attack type using only its own stat.

Vanilla pushes every build toward specialising in one or the other. This
removes that pressure without simply making everyone stronger: the combined
total has a soft cap with diminishing returns. It can also take body type out
of the equation, so Weight and Height become purely cosmetic.

It applies to every Custom Character (CaC) in a fight, at any level —
yours, other players', and NPCs. Roster characters are not changed.

For game version **1.26.01**. Works alongside XV2Patcher.

---

## Installing

There are two versions. Use one, not both.

### With XV2 Mod Loader

Put the `CombinedSupers` folder in the loader's `mods` folder:

```
DB Xenoverse 2\
    XV2ModLoader\
        mods\
            CombinedSupers\
                CombinedSupers.dll
                combined_supers.json      <- its settings
```

To uninstall, delete the `CombinedSupers` folder. For playing online with
EasyAntiCheat, and for Steam Deck setup, see XV2 Mod Loader.

### Standalone

1. Put `winmm.dll` in the game's `bin` folder, next to `DBXV2.exe`.
2. Make a folder called `XV2CombinedSupers` in the game folder, next to `bin`,
   and put `combined_supers.json` in it:

```
DB Xenoverse 2\
    bin\
        winmm.dll
    XV2CombinedSupers\
        combined_supers.json      <- its settings
```

To uninstall, delete `bin\winmm.dll` and the `XV2CombinedSupers` folder.
Remove `winmm.dll` before playing online with EasyAntiCheat.

On Steam Deck, add `bin/DBXV2.exe` as a non-Steam game, set it to use Proton,
and give it these launch options:
`WINEDLLOVERRIDES="winmm=n,b" %command%` — with XV2Patcher,
`WINEDLLOVERRIDES="xinput1_3,winmm=n,b" %command%`.

---

**Everyone playing together needs the mod and the same `combined_supers.json`.**
The settings change the numbers, so mismatched setups disagree about damage.

---

## Configuration

`combined_supers.json`, next to the DLL. Changes take effect within a second,
no restart needed. If the file is missing, the mod writes the default one the
next time the game starts.

Transformations and super souls are **never** combined. 

Gear penalties count, the same way the game counts them. With `stats:
additive`, a penalty on one stat is shared across like any other investment,
so a character with a large penalty on one side can end up slightly below
vanilla on the other, while gaining more on the penalised side.

---

## How it works

The game multiplies super damage by the attacker's Strike Supers or Ki Blast
Supers stat at two places in its code. The mod redirects both to multiply by a
per-character factor first. **Your stats themselves are never changed** — the
character sheet, basic attacks and defence all see the real values; only super
and ultimate damage sees the combined one.

A background thread finds every CaC in the fight and works out their build
from their live stats. Every CaC stat follows

```
stat = G(level) * (1 + body type + points/500)
```

where `G` comes from the game's level table and the body-type value from the
game's own `data/system/parameter_spec_char.psc`. From that the mod recovers
each character's points and gear — whatever their level, and whether or not a
transformation or super soul is active — and computes their factor.

Everything it uses is something every player's game already has: race, body
type, level and the live stats. Two players running the mod compute exactly the
same numbers for every character. It never reads save files.

Characters are identified as a fight loads. In the first fight after starting
the game this can take until a character's first super, so that one super can
land partly unboosted. Every later fight is boosted from the first hit.

---

## Building

Visual Studio 2019 or newer with the C++ desktop workload, and CMake.

```
build.bat              build both versions into build\Release
build.bat clean        clean build
build.bat install      build and install the loader version into
                       XV2 Mod Loader's mods folder
build.bat uninstall    remove the loader version (settings are kept)
```

`install` finds the game in Steam's library. If yours is elsewhere, set
`XV2_GAME` to the game folder first:

```
set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
```

---

## What it touches

* **Game code:** 16 bytes — the two multiply instructions — restored when the
  game closes.
* **Game memory it writes:** only health, and only with `height` on. Current
  health is raised only if it was full at that moment, so it never heals anyone
  mid-fight.
* **Files it reads:** its own `combined_supers.json` and the game's
  `parameter_spec_char.psc`.
* **Files it writes:** none, unless `logging` is on.
* **Network:** none. **Save files:** none.

The standalone `winmm.dll` is loaded by the game and by several of the game's
own DLLs, so it passes every Windows `winmm` function straight through to the
real one and only adds the mod on top.

---

## Credits

Game data formats (`.psc`, `.pup`) documented by eterniti's `eternity_common`
and LazyBone152's `XV2-Tools`.

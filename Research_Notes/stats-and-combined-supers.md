# Character stats, damage and Combined Supers

Covers: how a CaC's attack stats and HP are built (level table, PSC body
types, points, gear, buffs), the battle character struct, how
CombinedSupers finds characters and solves their investment, Height,
what was verified against damage, and the dead ends. Game 1.26.01.

Goal of the mod: supers/ultimates scale off Strike Supers + Ki Blast Supers
combined (soft cap past a threshold), for every CaC, deterministic across
players.

## The stat model (CONFIRMED 2026-09-26, Lv1..199)

    stat = G(level) * (1 + psc + points/500)      HP: same with 1000*G (points = HEA + gear)

* `G(level)` = the level table (below). A point is G/500 at any level.
* `psc` = the body-type value from parameter_spec_char.psc.
* QQ Bang point = 10 attribute points; Time Patroller Battle Gear piece = 2.5
  points (e.g. 4 pcs x (+1 HEA, -1 BLA)). Gear can be negative (seen: -6 ki,
  -1 strike).
* Examples: Lv1, 0 pts: stat = 0.21*(1+psc), HP = 210*(1+psc). Lv199, 0 pts:
  basic 3.75, strike = ki = 3.5625, HP 4650. Lv10 + 1 STR: strike 0.7101020
  -> 0.7110986 (predicted 0.711099). Old Majin HP 3000*(1+.55+150/500) = 5550
  (HEA 100 + Bang 5x10).
* SUPERSEDED: the first model, `stat = intercept + 2.5*psc` (basic 2.70,
  basic_ki 2.70, strike 2.50, ki_blast 2.75) and `HP = 3900 + 3000*psc`, was
  fitted at Lv199 only: 2.5 = G(199), 3000 = HP(199), and the extra
  "intercepts" were the test characters' QQ Bang +5 BLA (+0.25) and HEA 100
  (+900). It is wrong at other levels / builds.

## The level table
Found in game memory 2026-09-25 (not in loose data, the exe dump, or
uncompressed in any .cpk; `ClaudeTools\memtable.py` re-finds it live). 199 records
of 128 bytes (32 floats), one per level:

    f0 HP base (=1000*G)  f1,f3 ? (300 -> 1000)  f2 ? (3.8 -> 4.2)  f4 22.5
    f7..f10  G for basic, basic_ki, strike, ki_blast
    f11..f14 1.0 x4 (defences?)   f15..f18 0.85 0.80 0.85 0.85 (the k of the
    struct's speed-looking fields)   f19.. constants 18000 600 100 30 1440 3 5 ...

G(1)=0.21 G(2)=0.2466135 G(10)=0.4983172 G(50)=1.15 G(99)=1.5 G(199)=2.5.
A second copy has identical G up to Lv140 but other HP/speed columns and
G(199)=2.0 - probably a non-CaC table. Full dump: `ClaudeTools\leveltable_dump.txt`;
compiled in as `src\level_table.h`.

## parameter_spec_char.psc (body types)
`data\system\parameter_spec_char.psc`, parsed by `src\psc.cpp`. Format from
eternity_common `DBXV2/PscFile.h` and XV2-Tools `Xv2CoreLib/PSC/PSC_File.cs`:

    0x00  "#PSC", endian 0xFFFE, header size 20   0x08 num_entries (160)
    0x10  num_configs (2)
    0x14  num_configs blocks of num_entries x {char_id, num_specs, unused} (12 B)
          then all spec entries packed, config-major
    spec_base = 20 + 12*num_entries*num_configs

Spec entry 200 bytes (196 before 1.20; psc.cpp derives which from the file
size). Fields: +0x18 health, +0x50 basic, +0x54 basic_ki, +0x58 strike,
+0x5C ki_blast, +0x70 ground_speed.

* CaC races = char_id 100..107 (Human M/F, Saiyan M/F, Namekian, Frieza,
  Majin M/F), 12 specs each = height*3 + weight. Both configs identical for
  CaCs (config 0 used). 770 of 922 spec entries have strike != ki (vanilla).
* Weight changes ONLY basic/basic_ki/strike/ki; Height ONLY health and speed.
* strike + ki sum to the same total across weight tiers, so "best tier for
  each stat" and "add the other across" are the same number. A literal max()
  would leave MIDDLE Weight short (0.125 instead of 0.250).
* Height table (best health always h3, best speed always h0):

      race        health h0..h3                speed h0..h3
      Human M/F   .450 .475 .525 .550          +.050 +.025  .000 -.050
      Saiyan M    .375 .400 .450 .475          +.050 +.025  .000 -.050
      Saiyan F    .350 .375 .425 .450          +.050 +.025  .000 -.050
      Namekian    .525 .550 .600 .625          +.025  .000 -.025 -.075
      Frieza      .350 .375 .425 .450          +.100 +.075 +.050  .000
      Majin M     .550 .575 .625 .650          -.025 -.050 -.075 -.125
      Majin F     .350 .375 .425 .450          +.075 +.050 +.025 -.025

  Equalising gains: health x1.054..1.061, speed x1.10..1.11.

## Buffs: powerup_parameter.pup
16-byte header, 85 entries of 152 bytes. Offsets (installer labels F_14...):
0x00 id, 0x04 cmn_effect, 0x08/0x0C super_soul_1/2, 0x10 HEA, 0x18 KI,
0x1C ki_recovery, 0x20 STM, 0x34 ATK, 0x38 basic_ki, 0x3C STR, 0x40 BLA,
0x44..0x50 normal/ki_blast/super_strike/super_ki dmg, 0x54..0x60 speeds,
0x68 duration (float). Buffs are additive fractions on a 0.025 grid, so a
live multiplier M is 1 + n*0.025. Super souls are PUP entries. Example: M 1.55
= 0.15 awoken + 0.20 soul + 0.20 soul again on transformation.

## The battle character struct
Pointer in `rbx` at the damage hook:

    +0x04 race / CMS id (100..107 CaC)   +0x14 PSC spec (height = /3, weight = %3)
    +0x24 always 0x3D (PSC unk_14)        +0x48 level
    +0x50 current HP   +0x54 max HP
    +0x88 basic  +0x8C basic_ki  +0x90 strike  +0x94 ki_blast   (live, buffed)
    +0x98..0xA4 the four defences (NOT multipliers - misled once)
    +0x78..0x84 stay 100 in every state; no stored buff multiplier anywhere

* Fingerprint (36 bytes from +0x04): race, 0, race, ?, spec, ?, 5, 5, 0x3D.
  The old 12-byte `05 05 3D` was too generic. A freed struct stops matching
  race/spec and is skipped.
* Scanning finds COPIES of a character; the damage code's `rbx` is a different
  struct. Only the ring (* in the status file) holds live battle HP: in a 1v1
  the * Majin went 5550 -> 5183 while its scanned copy stayed 5550. Write HP
  to the * struct.
* Past ~+0x300 the struct is instance state (pointers, AI, buffers).
  +0x250/+0x254/+0x280/+0x2b0 change with Height = body size / hitbox.
* The battle struct grew 0x20 above +0xC4 from 1.25.02 to 1.26.01.

## How the mod finds characters
* Damage caves push {rbx, live_s, live_k} into a ring and multiply by a
  per-character factor (one mulss). Ring-sourced pointers are adopted with a
  loose check; scanned ones need the full fingerprint.
* Full scan ~2.1 s (2.6 GB); with a 5% duty cap it runs every ~42 s. A fight's
  structs (live + copies) come from one ~16 MB allocation ("arena"), so known
  arenas are rescanned every second (3 arenas, 4 ms). Only the first fight
  after launch can open partly vanilla (no arena known yet; the ring learns
  the attacker on its first hit - a multi-hit opener blends).
* FPS drops while hosting online (2026-10-11, unconfirmed suspect): measured
  in a 6-player online session the full scan had grown to 2.57 s every
  51.5 s (arenas: 6, 9.3 ms/s). `ClaudeTools\regionmap.py` (VirtualQueryEx from
  outside) showed the scan's filter passes 3.70 GB: 3.6 GB PAGE_READWRITE
  plus 101 MB in 215 regions of PAGE_READWRITE|PAGE_WRITECOMBINE (0x404) -
  GPU-driver memory, uncached to read. `Protect & PAGE_READWRITE` doesn't
  exclude WC/NOCACHE. Not yet tied to the hitches by a frame-time measurement.
  Fix installed on the PC 2026-10-11, not seen in game: WC/NOCACHE regions skipped; the
  full scan reads only pages in the working set (QueryWorkingSetEx, 4 MB per
  query; arenas are still read straight through) and sleeps 1 ms per 32 MB;
  the scan thread runs at THREAD_PRIORITY_BELOW_NORMAL. Check the new
  "scan full" time in combined_supers_status.txt in an online session.
  In game (same night, online): full scan 386-393 ms every ~8 s, arenas
  0.1-7 ms. The match stayed choppy (26-50 fps on every frame, not periodic),
  so the scan was not the cause - see exe-and-tooling.md "Main game thread
  in battle".
* 64-slot table; ring-adopted attackers evict a scan-only copy when full (a
  raid pool showed 20 identical copies of one Saiyan).

## The solver (half-point lattice, in units of G)
* base = G*(1+psc), RATE = G/500; solve `live = M*(base + RATE*k)` for both
  stats with one shared M on the 0.025 grid; k on a half-point lattice (Time
  Patroller gear gives half points - the "13.5-point character"). Feed the
  minimum values seen. F is a ratio, so G cancels.
* Raids HALVE stats (strike 0.6751 -> 0.3376). The solver tries M < 1 when
  nothing fits at M >= 1 and picks the SMALLEST investment among fits (PSC
  values are multiples of 0.025, so ratios like 4:5 alias: M .5 +30/+80 also
  fits as M .625 -118.5/-78.5).
* F = softcap(U + share*(other stat's investment)) / U, per character and
  buff-independent, so transformations/souls need no handling.
* Points and gear are ONE total (see the multiplayer rule): telling them apart
  needs the save record, which other players' clients do not have.
* Unsolved = vanilla damage, printed as UNSOLVED with Lv, hp, live, base, off.

## Caps
* The game clamps attributes + gear at base + 1.000 (200 points) - from ONE
  observation (Majin BLA 200 + a +5 Bang read exactly base+1.000). At the cap
  the Bang tier is unobservable. Does not affect the mod (it reads live stats).
* The mod's soft cap: `soft_cap_points` (200) and `falloff` (0.35).

## Height (health only)
* Bonus = 1000*G*(best_psc - own_psc), ADDED to the character's own max HP on
  the * struct; current HP raised only when it was full (raised once - a
  persistent HpMark). Revive uses the new max (KO revive 162 = 20% of 810).
  height.lua compared against the original max every tick and healed hits
  smaller than the bonus - the C++ deliberately differs.
* Verified: Namekian h1 Lv10 772 -> 810, Lv20 Namekian +54.8, Saiyan h0 +250.
* SPEED is not implemented. The struct's +0x128/+0x12C/+0x13C/+0x140 =
  k*(1+psc_speed) fit exactly but writing them (even 10x) changed nothing -
  derived/display values. Speed is not in this struct; finding it needs a
  watchpoint on position/velocity while moving. ~10% effect, no number on
  screen to verify against -> shipped health-only.

## Multiplayer
* Rule: only inputs every client sees identically (race, PSC spec, level, live
  stats). In an online lobby only the local player's 8 save records were in
  memory; other players' builds are not readable -> the save is out.
* PC + Deck raids: identical tot/U/F and HP on both, including a fresh-launch
  vs warm comparison - HP is synced/consistent, no desync seen. A stranger's
  character (Saiyan M -50/+4) solved fine.
* Online LOBBY has no CaC battle structs. You cannot ready up alone in an
  online raid; a 1v1 with an unmodded stranger risks desyncing them.

## Verified against damage (always re-baseline against the SAME opponent:
damage depends on the target's defences; one prediction missed by 4/3 because
the baseline was taken against another race)
* OG Majin Gamma vs Evil Hercule 5118 = 3982 x 1.2854 exactly (current model).
* Older Lua runs: Majin w0 STR100/BLA200 Bang +5/+5, F 1.2000/1.1190, Gamma /
  Bending predicted = actual at M 1.0/1.2/1.55 (4522/8563, 5426/10276,
  7008/13273); Saiyan F 100/200 Gamma x1.1740 (pred 1.1743), Bending x1.0488;
  negative gear Frieza x0.99068 (pred 0.99024), Namekian x0.936; QQ Bang
  STR+0 -> +5 moved strike +0.25 and Gamma 3543 -> 3768.
* Baseline for scale (STR 0 / BLA 130 Majin): Bending 8532, Gamma 3707,
  Godly Display 38429, Super Vanishing Ball 19768 - skill base damage
  dominates over the stat.

## Decisions (user's calls)
* Negative investment shared across may push F below 1 - kept deliberately.
* Buffs never combined. Default height "highest".
* Points and gear are one "stats" total; the equipment axis was removed.

## Dead ends (do not repeat)
* No data-only solution: each super resolves to SP_ATK or SP_SHOT_ATK; no
  BAC/BDM/CUS field expresses "both". (Untested idea: BDMSubEntry.damage_type,
  u16 at sub-entry +0, may pick the scaling stat or the damaged resource.)
* Quest-script stat dispatcher `0x7E5100` (names at .rdata `0x11003CC`: LV, HP,
  GIRYOKU, KIRYOKU, ATK, SHOT, SP_ATK, SP_SHOT_ATK, ..._DAMAGE, MOVE_SPEED...)
  writes a stats object at chara+0x40 (+0x154 level, +0x170 SP_ATK, +0x174
  SP_SHOT_ATK...). That object is NOT the battle character: 9999 written into
  64 such blocks changed nothing (physics/animation data).
* Attack-power function `0x7D8FA6`/`0x7D8FE8`: never executes. The 12 per-field
  writes to stats+0x170: zero hits in battle.
* On-screen damage number = scoreboard (writers 0x1BEBCA/0x1BEBE8, derived
  from an HP delta in 0x1B993F). Value-scanned "HP" = the HUD bar lerp
  (0x1E1470, [rbx+0x22C/0x230]). True HP is not findable by value scan.
* CaC save record (attributes block): stride 0xC6C8 in one arena; name at
  -0x74, equipment ids -0x1C..-0x10, level -0x0C, exp -0x08, HEA/KI/ATK/STR/
  BLA/STM +0x00..+0x14, more ids +0x18..+0x24, super soul +0x2C. No pointer to
  it anywhere; scanning for it by structure matches huge parts of memory and
  crashed CE. Only usable for the local player - see the multiplayer rule.
  (Revamp's patches reach the same records at [0x1554448]+0x1E8B4 x 0xC6C8.)

## Testing
* `ClaudeTools\ticktest\run.bat` feeds Tick() measured values (build the mod first).
* Measurements used: Namekian "Bromaciamm" h1 w1 (Lv1/2/9/10/20/50, +1 STR,
  TP gear), old Majin h3 w0 (QQ +5, +50/+180 incl. gear), Lv199 Saiyans.
* Comparing to the Lua prototype (`ClaudeTools\_claude_xv2mod`): same U and F for
  the same character proves the port; the damage ratio proves the hook.

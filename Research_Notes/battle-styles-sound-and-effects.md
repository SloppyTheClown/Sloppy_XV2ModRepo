# Battle styles: character SE bank, effects and cue limits

Covers: BattleStyles build 7 - the race + style character SE bank
merge (`src\acb_merge.cpp`) and its load hook, the style's character
EEPK registration and the effect-owner / SpawnEffect hunt (resource slot
= battle player index), and the two cue limit crashes in merged banks.
Overview: battle-styles.md; the BAC/BDM side of the fit:
battle-styles-fit.md; ACB format: sound.md "Merging two banks".

## Build 7: merged character SE bank + style EEPK (2026-10-11; sounds confirmed in game, effects still missing)
* **Sounds (`src\acb_merge.cpp` MergeAcb):** the race's CAR_BTL_*_SE bank
  with the style bank's cues appended under ids after the race's highest
  (format and writer rules: sound.md "Merging two banks"). The fitted BAC's
  AcbType-2 records go to the new ids (`BacFit::CharacterSe::Remap`); race
  cues keep their ids, so Turn Giant's NMC cues still play. Written once per
  session to `data\sound\SE\Battle\Chara\BattleStyles\<STYLE_SE>.<RACE>.acb`
  (own folder, no ".." in the game path).
* **Bank load (exe 1.26.01):** 0x298c70 (CharaResource SE load; only caller
  0x11405e in 0x113d40, so on the battle loader's thread) calls
  0x3630a0(sound manager, slot, "SE/Battle/Chara/<CSO SE>", 0) at 0x298e14
  (code at 0x298e0c `45 33 c9 8b d3 48 8b cd e8 87 a2 0c 00`); the game adds
  "data/sound/" and ".acb". rdi = the resource (chara +0xd0, costume +0xd4,
  slot +0xc8). Redirected through a cave that does `mov r9, rdi` first;
  for a styled CaC whose path ends in its own CSO bank the hook passes
  "SE/Battle/Chara/BattleStyles/<STYLE_SE>.<RACE>". Without the hook the BAC
  drops the sounds instead (battle-styles-fit.md "Build 6").
* **AisacControlMap:** every CAR_BTL_*_SE cue has the same one-byte map 02
  (ACF control id), so it is copied as is (Xv2CoreLib does the same).
* **Offline:** `ean_fit_test.exe <game> <out> ZMS:NMC:CAR_BTL_ZMS_SE:CAR_BTL_NMC_SE`
  merges too; ZMS:NMC 9 cues as 11..19 after 11, all 39 BAC sounds remapped
  (light combo 300-303 -> cue 11 KenFuri); ZBN:MAM 6 cues as 15..20, 4
  remapped. `ClaudeTools\battle_styles\acbmergecheck.py MERGED RACE STYLE`: race
  cue ids/names/audio bytes unchanged, style cues in order with their audio.
* **Style EEPK:** 0x113d40(holder, {chara, costume, ..}, 0) loads one
  character's files; the battle player loader calls it at 0x16c484 (code at
  0x16c47a `45 33 c0 48 8d 55 c7 48 8b cb e8 b7 78 fa ff`) and 0x16c7b3
  (0x16c7a8 `45 33 c0 48 8d 54 24 40 49 8b ce e8 88 75 fa ff`). After it,
  for a styled CaC the mod registers the style's character EEPK like the
  loader does at 0x16c761: 0x956320(*(base+0x1554608), 2, style chara, 0, 1,
  1) (entry `48 8b c4 57 41 54 41 55 41 56 41 57`; it looks an id up first,
  0x957090), once per player and style, and remembers the holder.
* **Effect owner:** Battle::MobVfxOwner vtable 0xf5deb0 slot 14 (rva
  0xf5df20) = 0x121370: mob [owner+8] ([mob+0xa20] when [mob+0x15c0] ==
  0x34), returns [mob+0xbc] (= resource +0xd0) or -1. xv2patcher's VfxFix
  patches that code, so the slot is chained. The hook returns the style's
  chara id when the mob's holder ([mob+0x4d0], 0xc2e9c) is a remembered
  styled resource. Race BACs and both Turn Giant BACs use no character-EEPK
  effect, so the race loses nothing.
* **The owner hook alone did nothing in game** (registration logged, no beam
  sword): SpawnEffect **0x956ef0**(manager, request, type, id, flags, a6, a7;
  callers include 0xc3067/0xc30a8, 0x9b1d9, 0x9edc7) looks the container up
  by (type, id) at 0x956fc6 (0x957160) and asks the owner ([request+8]
  ->slot 14, 0x956fa4) **only when id < 0** (0x956f8d; type 1 always
  asks). BAC effects pass the mob's chara id themselves. Fix: an entry hook
  (7 bytes `4c 8b dc 49 89 5b 08` replayed) swaps a character-EEPK id for
  the style's when [request+8] is a MobVfxOwner (vtable 0xf5deb0) of a
  styled mob. Found by `ClaudeTools\exe\vcallnear.py` (virtual +0x70 calls in
  0x940000..0x9a0000). The holder match was checked statically: the loader
  puts the 0x113d40 holder in the player's map (0x1696f0 at 0x16c4b5 /
  0x16c7a3, map at +0x3978 + player*16), 0xe7940 reads it back with
  0xe2e60 and passes it to the mob setup 0xc2b10, which stores it at
  +0x4d0.
* **The SpawnEffect hook did nothing in game either** (2026-10-11, NMC CaC
  with ZMG, light combo + grab: no beam sword, no grab light). The log of that
  session (09:39) has "character effect hooks installed" and "player 0 chara
  104 ... style ZMG (chara 142) registered", but **never** the swap line
  "character effect of race chara %d spawned from style chara %d's EEPK". So
  HkSpawnEffect never swapped: one of its checks fails, and the log can't
  say which. Suspects, in the order to check:
  1. ZMG's BAC effects don't reach 0x956ef0 with type 2 at all (another
     spawn function, or the BAC EepkType is mapped to a different manager
     type before the call; the 0x956ef0 callers 0xc3067/0xc30a8 weren't
     traced back to the BAC type-8 handler).
  2. The id passed isn't the CaC's chara id (IsCac fails) - e.g. the BAC
     path passes something already resolved, or the owner's id via another
     route.
  3. [request+8] isn't a MobVfxOwner (vtable 0xf5deb0) for BAC effects.
  4. The holder at [mob+0x4d0] isn't the pointer passed to 0x113d40 (the
     static proof may have followed the wrong map entry), or the mob is the
     transformed body.
  Next step: a diagnostic build that logs the first ~40 SpawnEffect calls
  (type, id, owner vtable rva, holder, remembered holders) - bounded, not
  per tick - then read the log after one light combo. Also confirm
  0x956ef0 is hit at all (the hook itself is logged as installed).
  Not tried yet: checking whether ZMG's EEPK actually got registered under
  142 (0x957090 lookup after the register call), and whether xv2patcher's
  VfxFix changes this path.
* **Diagnostic build (2026-10-11, installed, not run yet):** `trace ...`
  lines in battle_styles.log (logging only, fixed budgets): `trace load
  site` (holder -> *holder per styled load), `trace register` (0x957090
  (manager, 2, id, flag 0/1) containers for style + race chara after
  registering), `trace spawn (any|chara|styled)` (caller rva, type, id ->
  swapped id, owner vtable, mob, [mob+0xbc], [mob+0x4d0], styled match),
  `trace owner` (slot 14 calls answering a CaC id). Static: 0x956ef0 has
  ~120 call sites; the only other (type, id) lookups (0x957160) are
  0x955970/0x9559e0/0x955f50/0x9560a0/0x956d10. 0x956ef0 first calls an
  optional manager callback [manager+0x90](request, type, id, ...).
  Side note: mod.cpp's kLastCac is 107, the exe's CaC range is 100..109
  (only 8 CaC races exist; 108/109 look provisioned, unused - kept at 107).
* **Diagnostic result (2026-10-11, NMC + ZMG, Training):** suspect 4. BAC
  character effects come from **0xf9788** into 0x956ef0 with type 2, **id
  -1** (so the id swap in HkSpawnEffect can never apply; the owner path
  does it), owner = MobVfxOwner, [mob+0xbc] = 104, and the owner slot 14
  is asked from 0x956fae as expected. But [mob+0x4d0] (0x...64fd0) was none
  of the four holders passed to 0x113d40 (0x...64e90/654d0/656b0/66330):
  the mob keeps the **copy** the loader puts in the player's map (0x1696f0),
  whose first field is the resource (0xc2e90 reads [[+0x4d0]]+0xd0 as the
  chara id). Register check: (2, 142, flag 0) container exists after
  registering, flag 1 none; race 104 has no character container at all.
  Fix (build installed, not seen in game yet): remember `*holder` (the
  resource) and match it against `[[mob+0x4d0]]`. Trace lines kept, now with
  the resource and the spawn's lookup flag ([request+0x58] >> 1 & 1) - if
  effects still don't show, check that flag is 0 (only flag 0 registered).
* **Resource match failed too (2026-10-11 run 2):** lookup flag 0 (fine),
  but the mob's resource 0x...71640 was none of the four `*holder`s
  (0x...70740/71a00/71dc0/739e0) - same pool, stride 0x3c0, so the mob
  gets its own CharaResource from somewhere not traced (0x113d40 only
  fills an existing [holder]: `cmp [rsi], 0; je` at 0x113dea, and
  0x298c70 at 0x11405e gets that same [rsi], so the SE hook sees the same
  four). Race chara 104 has **no** (2, 104) container, so a CaC's
  character effects find nothing anyway and mapping to the style is safe.
  Build 3 matches: same resource, else same resource slot (+0xc8; the SE
  load uses slot*2+3 as the player's sound slot), else the race's only
  style when one style is loaded for that race (an unstyled CaC of the
  same race as a styled one would then get the style id - harmless only
  while race BACs spawn no character effects). Trace lines now log the
  slot of the loaded resources and of the mob's.
* **Confirmed in game (2026-10-11 run 3): ZMG's beam sword shows on the NMC
  CaC.** Log: loaded resources player 0 all slot 0; mob resource
  0x...4df40 (again not a loaded one) slot 0; owner answers 104 -> 142.
  Player 0 had slot 0, so this run can't tell the slot rule from the
  only-style fallback; slot == player index is not proven yet. To check:
  a styled CaC as a player other than 0 (2+ CaCs in one battle, or online)
  and compare the `trace load` / `trace owner` slots.
* **Two CaCs in Training (run 4):** MAM + ZBN as player 0, NMC + ZMG as
  player 3; both styles' effects correct. Loaded resources: player 0 slot
  0, player 3 slot 3, so **resource +0xc8 = battle player index**. (The
  owner trace's 24-line budget was spent in earlier fights, so the mob's
  slot for player 3 wasn't logged; both races differ, so the fallback
  could also have matched.) Build 4: the
  only-style fallback only when the mob's slot is unreadable (an unstyled
  CaC of a styled one's race keeps its race id); owner trace now LogOnce
  per resource + answer. Seeing ZMG effects on player 3 with it proves
  the slot rule on the mob side. If the mob's resource
  origin is ever needed: mob setup 0xc2b10's r14 caller chain, 0xe7940 /
  0xe2e60.
* **Slot rule confirmed (run 5, build 4):** same two-CaC Training fight;
  the mob-side owner trace logged `chara 104 -> 142, resource
  0x...4f430 slot 3` (again the mob's own resource, not a loaded one), and
  ZMG's beam sword + grab light showed on player 3. So the mob's resource
  +0xc8 = battle player index too, and the slot match does the work.
  Afterwards all `trace ...` lines (and the diagnostic FindEffects
  0x957090 call) were removed; the owner hook now logs one line per slot
  and answer ("slot N: character effects of race chara X from style chara
  Y's EEPK"). The SpawnEffect id swap stays in (it never applies: BAC
  passes id -1, so the owner hook does the work).
* **Sounds confirmed in game** (2026-10-11): ZMG on NMC, ZBN on MAM, no
  Turn Giant bank spam, Turn Giant keeps its sfx.
* **Open:** whether the registered style EEPK is released at battle end
  (unregistration not found; the lookup stops it piling up within a
  session); whether loose ACBs load from a new folder (xv2ins installs loose
  ACBs, so expected); the CaC still uses its own voice bank (AcbType 3 cues
  are the race VOX's, as before).
* **Link pitfall:** one ean_fit_test.exe link came out with a null import
  (crash at start, call to 0 in the CRT's LoadLibraryExW); deleting the exe
  and relinking fixed it - check the test runs before trusting a build.

## Cue limit crash (merged SE bank) (2026-10-11, built, NOT yet seen in game)
Crash 21:28:41 (and very likely the 20:15 one): exe 0x9bba67, in CRI sound
playback (stack BattleEventKey_Snd 0x363670 -> criAtomExPlayer start ->
command interpreter 0x9cbca0 -> 0x9b814c -> 0x9bb918). Players: P0 NMC +
VTO, P1 NMC + PR1, whose SE bank is CAR_BTL_FIO_SE -> merged
`CAR_BTL_FIO_SE.NMC` (FIO.NMC also existed at 20:00).
* 0x9bb918(list, freeNodes, item) pushes a playback onto a cue limit work
  area; with no free node it steals the list head, which was garbage.
  0x9b814c gets the work area as `[[r13+0x50]+0x13a8] + index*0x30` (list
  0x18 + free node list 0x18; nodes from `+0x13b0`), index and limit from
  the command's parameters. The interpreter's jump table (0x9cdc88, codes
  0..0x81) sends codes **0x0049 and 0x004f** to 0x9ccd2d ("Ignore the cue
  limit information" warning on old ACB versions): u16 limit, u16 work
  index (+ u8 for 0x4f).
* The array is sized by the ACB header's `NumCueLimitListWorks` /
  `NumCueLimitNodeWorks`. CAR_BTL_FIO_SE has (1, 1) and a 0x4f cue (limit 1,
  work 0); every CaC race bank has (0, 0), and the merge kept the race
  header - so FIO's cue indexed past an empty array.
* Survey (scratch cuelimitscan.py over every cpk .acb): character banks with
  cue limits FIO, FIT, TPO (0x4f) and GBR, GKS, TM1, TSH (counts only);
  CAR_BTL_CMN has 217; many skill banks. Node works = sum of the limits
  (S1210_TWN: 4 lists, limits 1+2+2+1 = 6 nodes).
* **Fix (`src\acb_merge.cpp`):** style 0x49/0x4f work indices move up by
  the race's NumCueLimitListWorks, and both header counts become race +
  style. Offline: FIO.NMC (1, 1), TPO.NMC (2, 2), acbmergecheck OK.
  **Incomplete** - see the next section.

## Cue limit work buffer (2026-10-12, confirmed in game: Training with the GAL style no longer crashes)
Crash in Training as soon as the CPU (player 3, HUM CaC + GAL style) attacked:
`strlen(NULL)` in the CRI play-start logging 0x9af298, character SE (BAC
type 2) cue 53 = GAL cue 43 in the merged `CAR_BTL_GAL_SE.HUM`.
* The lookup chain: 0x997a50(acb, cue id) -> name; 0x9978c0 id -> row by a
  binary search of the CueTable (0x9d1290 -> 0x9d4214, CueTable at
  acb+0x1f0), then 0x9d0fd4 row -> name by a linear scan of the CueNameTable
  (acb+0x288) for CueIndex (column 0 CueName, 1 CueIndex); returns 0 when
  not found, and 0x9af298 strlens it anyway, so any missing id/name crashes.
* The merged file passed all of that offline (ids sorted 0..54, name table
  columns like vanilla, every row named). The real difference: the top
  table's **`CueLimitWorkTable` is the work areas' memory**, stored zeroed in
  the file and sized 8 + 32 per list work + 32 per node work (every cpk
  character bank with works fits it: FIO 1+1 = 72 bytes, TPO 2+2 = 136, GBR
  14+14 = 904; GAL 45+45 = 2888). The merge summed the counts (45, 45) but kept
  HUM's empty buffer, so the cue limit writes went past it into the loaded
  bank (most likely over its tables - the NULL name). FIT.MAM had the same
  flaw. `NumCueLimit` is 0 in every bank.
* **Fix (`src\acb_merge.cpp`):** after summing, the buffer is set to that
  many zero bytes. Offline: GAL.HUM 2888, FIT.MAM 72, acbmergecheck OK.
  Check every bank: `ClaudeTools\battle_styles\acbcuelimit.py` (no arguments = all
  cpk banks + the installed merged ones). The not-yet-seen PR1 -> FIO fix
  above needs this too.
* The tail merge (same build) is unrelated: the fitted files only load for
  styled players, and the crash was in sound playback.
* Perf: the effect-owner hook (slot 14) called LogOnce (vsnprintf + locked
  std::set lookup) on every effect of a styled CaC when logging is on; now a
  per-slot InterlockedExchange decides when to log.

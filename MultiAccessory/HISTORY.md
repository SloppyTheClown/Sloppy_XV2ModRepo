# Multi Accessory - history

Superseded test logs moved out of NEXT_STEPS.md.

## Equip row / wig stacking retests 5-13 (2026-10-25/26)

  **Retest 5:** row level with the others (not into Feet), also after a
  tab switch; "Accessory" on the first box only; an extra's list checks
  that column's pick (or none), the info pane follows the cursor; hover of
  an extra previews it on the model, cancel goes back, pick keeps it; the
  equipped accessory still shows while doing so; after leaving the menu
  the lobby avatar shows the new extras without character select. Send the
  `later:` lines (main y should equal row below y - step, and != 228 if
  the timeline moves rows).
  **Retest 7:** (game hung in retest 6; PC .sav still has slot 8 = 612 +
  2005) load that NMC CaC: no hang, lobby avatar shows 2005 but not 612
  (log `extra 612 swaps a part slot ... not shown`); leave the equip screen
  after a pick: lobby look updates (log `lobby:` line or not); wig + hat
  835 / glasses: both show, head intact; FRI helmet + hat fine.
  **Retest 8:** pick an extra, leave the equip screen, back to title: save
  select previews all load, loading a save works (no hang); hover/pick of
  an extra still redraws the equip-screen model; extras show in the lobby
  after character select. Slot 8 now has 980 (Angel Set) + 831: both draw?
  **Retest 9:** save select previews all load (FRI helmet CaC: no hat, log
  `extra 835 ... not shown`), save loads; then retest 8's list (equip-screen
  hover/pick redraw, back to title still fine, 980 + 831 on slot 8).
  **Retest 10 (wig stacking test build, resource path only):** on a CaC
  that wears a wig/hat, pick a second wig as an extra (e.g. slot 7's 612).
  Log on load: `extra N stacks on part slots 0x..: preloaded`, then
  `extra N part slot S stacked at key 10 (RegisterModel R)` (R = 1/0?) or
  `no holder/part/model, not stacked`. Look at save select preview, lobby,
  battle: does the 2nd wig draw (overlapping the first, skinned, coloured)?
  Shadow? No hang on load, back to title, character swap, battle exit. The
  equip screen still skips it ("not shown") - expected for now.
  **Retest 11 (wig colours):** same 3-wig CaC (slot 7: 1410 + 612 + 1406):
  each stacked wig in its own colours, steady when the camera turns; check
  save select, lobby, battle, back to title, battle exit (no crash/hang).
  Log unchanged (`stacked at key 10/11 (RegisterModel 1)`).
  Result 2026-10-26: colours mostly right outside battle, but skin-coloured
  wig parts (Namekian antenna) bright green everywhere except battle, and
  hair/nose black at some angles in previews (save select, equip screen,
  character select; Conton fine). Battle still uses the first slot wig's
  ramp. Title and back fixes it. All three wigs (612, 1406, 1410) are
  DytRampsFromTextureEmb (flag 4) hair parts.
  **Retest 12 (diagnostic build, installed 2026-10-26):** same CaC; visit
  save select, lobby, character select, one battle. Send the
  `MultiAccessory.log` lines: `stacked at key`, the `equipped accessory's
  part` / `stacked, before activate` / `stacked, after activate` dumps, and
  every `controller:` line (+ `before`/`after` dumps). They show whether
  vt47's 0x29e020 reaches the stacked parts before or after our vt+0x18
  activation, which controller it gives them vs the equipped wig's part,
  and how the fields differ. Every log now holds one session (last one in
  `<name>.prev.log`).
  Result 2026-10-26 (log 03:50-03:52): no `controller:` line at all, so
  vt47 / 0x29e020 never runs on these screens (dead end, hook removed).
  Activation does prepare a ramp unit (+e0 set, from a pool: 612 always
  gets the same unit, 1406 the next one; in battle +ec = 1, a different
  unit type) - except the 03:52:24 builds (record 0, after battle,
  several loaded at once), where +e0 stayed null. The equipped-part dump
  said "none": it looked up 1410 as its own holder, but the equipped
  accessory is merged into the main holder (key res +0xd4).
  **Retest 13 (diagnostic build, installed 2026-10-26):** same visits;
  send the `stacked at key` lines and the `slot's own part` / `stacked,
  before/after activate` / `slot's own part, after` dumps. They compare
  the equipped wig's +c0 (ramp emb), +e8/+ec (mode), +104 (main cluster)
  and +e0 (ramp unit) with ours, and show whether our activation changes
  the equipped wig's unit.
  Result 2026-10-26 (log 04:05-04:07): at skeleton set-up the equipped
  wig is not activated yet (+e0 null, +fc -1; same +e8/+ec/+104/set as
  ours, own +c0), and ours doesn't touch it. So we activated earlier than
  the game does (Chara SetPartSet 0x27e6e0 -> 0x297850 -> 0x29cf60, after
  the holders are ready); the 04:06:14 builds again got +e0 null.

## State log up to retest 17 (2026-10-25/26)

* Installed on the PC (probe uninstalled). PC test config: `all: [835]`
  (hat), `slot_6: [846]` (glasses), logging on.
* **Confirmed in game:** CaCs without headwear get the hat; Hercule-hair
  CaC keeps its hair (835 skipped); nose + hat + glasses all show; no
  "other appearance values" lines (CaC match works).
* **Confirmed:** FRI helmet CaC (2035) keeps head + helmet, no hat (rule
  `SlotsUsed` / `Conflicts`: skip an extra that replaces a slot the
  equipped/earlier extra replaces or hangs physics on, or hangs physics on
  a slot they replace). Hat-less, nose, glasses combos still fine.
* **Lost by that rule, fix built + installed (untested):** Majin "Puu"
  (Arale hat 843) + hat 835. On MAM/MAF both replace Hair with the *same*
  part (model 0, `MAM_000_Face_base`) and hang the hat as a physics object;
  the probe drew both. New: `SamePart` - a slot both replace with an
  identical part (BCSPart +0..+0x37 minus +0x14, + 4 file names) doesn't
  conflict. Check tool: `ClaudeTools\multiacc_slotuse.py <bcs> <ids>`.
* **Phase 2 save built + installed (untested):** picks per account ->
  save slot -> preset in `multi_accessory.sav` (hand-parsed JSON, .tmp +
  MoveFileExW); `PickedExtras(slot, preset)` falls back to the config list.
  Preset tracking via the 38 redirected 0x64fa20 calls (`BuildLoadoutHook`,
  `g_slotPreset`); Presets panel load/save copies the entry
  (`MysetCallbackHook`, chains Battle Styles). `SetPickedExtras` is ready
  for the UI (unused so far, so nothing writes the .sav until a preset copy
  of an existing entry). Readme unchanged: no visible change yet.
* **Equip screen row built + installed (untested), with Battle Styles
  rebuilt + installed** (slot-count raise + CAS vtable chain): AS edit
  `as3\Avatar.as` registered with `XV2UI_EditScript` (owner
  "MultiAccessory"); DLL hooks in dllmain.cpp "Equip screen row" (details +
  unknowns in multi-accessory.md Plan step 3). Picks write preset 0 with
  `SetPickedExtras`. Limits: an extra not in the player's accessory list
  shows the list's first entry's text; when the equipped slot is empty,
  picking "none" for an extra counts as cancel (can't clear that way).
* **Equip row first test (2026-10-25):** hooks all in (log), three columns
  selectable, column picks saved; but (1) the list's values are IDB *item*
  ids, not part sets, so extra 1 = item 61 drew part set 61 (a gas mask)
  and extra 2 = item 121 drew nothing; (2) the row looked blank (an empty
  column's game text is ""). **Fixed, built + installed (untested):** item
  id -> part set via 0x6172d0(4, id) for hover/pick, the column text finds
  the list entry by part set; empty column shows `---`, a pick the player
  doesn't own (config default) shows `#<part set>`; the log prints each
  column's text when it changes. PC `multi_accessory.sav` emptied (held the
  bad 61/121).
* **Equip row retest 2 (2026-10-25):** data side correct (picks saved as
  part sets, character select draws the extras and keeps them), but on the
  equip screen (1) the row is invisible - the cursor still hits three
  boxes; (2) neither hover nor pick of an extra updates the preview, nor
  does leaving the menu. Cause of (2), static: model request 0x659140
  compares the 0xb0-byte loadout with its cached copy and returns 5 (no
  rebuild) when equal - an extra-only change never differs. **Fixed, built
  + installed (untested):** `ModelRequestHook` on the 8 calls of 0x659140
  scrambles the cached copy once after any apply/close of an extra column
  (`g_forceRebuild`). (1) unknown cause; guess: bounds measured mid-
  animation (scale 0/NaN). AS now skips narrowing (row stays vanilla) when
  the measured bounds look wrong, and sends its numbers to the log
  (`equip screen layout: <field> = <value>`, user-data type 9000+n,
  handled + swallowed in `EditCharaCallbackHook`). Remove the diag once
  the row shows.
* **Equip row retest 3 (2026-10-25):** row shows Torso/Legs/Hands/Feet, a
  blank gap, QQ Bang; extra columns not selectable. Log: first pass
  narrowed but `extra scaleX = 0.11` - the column loop read `main.scaleX`
  after column 0 shrank it (extras at 1/9, 1/27 width); later passes (tab
  switches) measured `bounds width = 0` -> left vanilla -> `m_ma_row = -1`
  -> no columns. `main alpha = 0` on every pass (narrowed mid-appear).
  **Fixed, built + installed (untested):** scale read once; the box's
  own-space left/width kept from the first good measurement
  (`m_ma_box_left/width`); narrowed buttons forced to alpha 1 (guess: a
  script-moved timeline clip loses its fade-in); `maDiagLater` logs
  `later: ...` 30 frames after a narrowing (columns vs an untouched row).
  ABC regenerated (`menu_abc.py <ffdec.jar> ma_avatar`), roundtrip check
  clean (waitStart's shared default target only).
* **Equip row retest 4 (2026-10-25):** three boxes show, but (1) a third of
  a row too high (into Feet): `main y = 228` was read mid slide-in, the
  timeline then moves the other rows; (2) every column said "Accessory:
  name"; (3) an extra's list opened with the main accessory checked + its
  info pane, unchanged while scrolling; (4) no preview on hover/pick of an
  extra; (5) elsewhere the look only updated after character select.
  Static RE: 0x659140 reloads the accessory only when loadout **+0x28**
  (accessory part set) differs from the cached copy's +0x28 (cmp
  0x6595c8; the old dword-0 scramble did nothing); the equip screen calls
  it every frame from 0x4f8cde with a loadout built from the main index
  (+0x2cc); apply 0x4f8df0 never requests the model; list open 0x4fc7b0
  checks the entry at the index +0x2bc. **Fixed, built + installed
  (untested):** AS `maFollowRows` sets the moved buttons' y (row 0 + i *
  row step) and alpha every frame for 30 frames; extra columns have no
  header and their name starts at the header's x; the DLL opens an extra's
  list on that column's pick (index swapped, restored on close), hover goes
  through apply(…, 0), the 0x4f8cde loadout gets the hovered/picked extra
  at +0x28 while an extra column is open (main's value kept for the rest),
  the cache's +0x28 at +0x168/+0x244 is scrambled after a pick, plus once
  for the next non-equip-screen request (`g_forceOtherRebuild`; unknown
  whether lobby avatars use 0x659140). ItemPush checks the main index as
  equipped. Diag fields 16-23 = the follow's last frame.
* **Equip row retest 5 (2026-10-25):** main box level + aligned, one
  "Accessory" label, three boxes; list check mark / info / preview follow
  the cursor and picks. But (1) extra boxes' y wrong (2nd below the last
  row, 3rd a row lower): `maFollowRows` used the button index (7/8) as the
  row; (2) leaving the equip screen kept the old look until character
  select; (3) user: too restrictive (glasses / hats skipped next to wigs),
  wants any extra to show, wigs stacking. **Fixed, built + installed
  (untested):** (1) extra columns follow the accessory row (`m_ma_row`),
  ABC regenerated, roundtrip clean; (2) `LobbyPushHook` on the lobby
  appearance sends 0x4f80e1 / 0x505d03: when preset 0's extras changed
  (`g_lobbyStale`, set by a column pick or a Presets load) a record copy
  with another accessory is sent first, then the real one; (3) no extra is
  skipped any more: one that clashes over a slot is "loose" - its objects
  hang on part index -1 (nothing hidden), its replacer parts (wigs,
  helmets) load as a made-up physics entry (`LooseReplacer`: file
  `<race>_<model>_<slot>`, bone per slot, e.g. b_C_Head). Equip-screen
  preview of a loose wig: replaces the equipped wig instead of stacking
  (the builder still loads it as a part; objects get index -1).
* **Equip row retest 6 (2026-10-26):** boxes all on the Accessory row.
  NMC CaC, equipped 831, extras 612 (wig) + 2005: leaving the equip screen
  built the lobby avatar with `loose model NMC_612_Hair on b_C_Head`, then
  the character vanished, no movement, re-entering the equip screen stayed
  blank (load never finished). No `lobby:` line at all (LobbyPushHook not
  reached; the 01:37 rebuild had the new extras anyway, via
  `g_forceOtherRebuild`). Cause (files, not proven in game): physics objects
  ship their own small .esk (`hum_980_hair.esk`, `nmc_831.esk`: b_C_Base,
  x_x_SkinRoot, one bone); a part model (`nmc_612_hair` emd/emb/emm) has
  none and is skinned to the body. **Fixed, built + installed (untested):**
  `LooseReplacer` removed; a clashing extra that swaps a slot itself (2nd
  wig/helmet) is skipped (`extra N swaps a part slot ... not shown`);
  loose hats/glasses unchanged. Wig stacking needs another way (RE).
* **Equip row retest 7 (2026-10-26):** NMC CaC loads fine (612 skipped,
  2005 shown); picked extra 2 = 980. Leaving the equip screen: lobby avatar
  unchanged, no `lobby:` line. Back to title: save-select previews stopped
  loading (slot 8 built twice at 01:49:37, then slots 7/6/3), loading a
  save hung. Cause (static + log): 0x4f80e1 / 0x505d03 push only in the
  *online* lobby (0x4f80a0-0x4f80c5: mode 0x46ee0 == 0x14 + net object), so
  offline nothing consumed `g_forceOtherRebuild` until the first save-select
  preview, whose accessory got force-reloaded -> stuck. Retest 6's hang was
  the same on the lobby avatar (not LooseReplacer, which is gone anyway).
  **Fixed, built + installed (untested):** `g_forceOtherRebuild` removed;
  `ModelRequestHook` touches only the equip screen's request (0x4f8cde).
  Offline lobby avatar now updates after character select (known gap; how
  the offline lobby avatar is rebuilt is unknown - needs RE, *not* by
  scrambling another builder's cache).
* **Retest 8 (2026-10-26):** fresh boot, save select: previews never
  loaded, UI gone (soft lock), no equip screen visit. Log's last build in
  both retest 7 and 8: `"Refridgerator" (slot 3): extra 835 (loose) with
  equipped 2035` - so the hang is **loose objects** (attached physics added
  with part index -1), not `g_forceOtherRebuild` (retest 7's diagnosis was
  wrong; retest 6's loose 612 build hung too). **Fixed, built + installed
  (untested):** "loose" removed everywhere; any extra that `Conflicts` with
  the equipped / an earlier extra is skipped again (log `extra N shares a
  part slot ... not shown`) = the rule confirmed before retest 5. Showing
  clashing extras waits on the wig RE.
* **Retest 9 (2026-10-26): passed.** Save-select previews load, saves
  load; FRI helmet CaC (2035) has no hat (`extra 835 shares a part slot
  ... not shown`, as designed); retest 8's list all passed (equip-screen
  hover/pick redraws, back to title fine, 980 + 831 on slot 8). Clashing
  wig picks (612, 1409-1419, 778...) log "not shown" on hover.
* **Retest 10 (2026-10-26): wig stacking works** outside the equip screen
  (1410 equipped + 612 + 1406: all draw, skinned, placed right; save select,
  lobby, battle; no hangs). Bug: stacked wigs use the equipped wig's colour
  ramp (colours shift with view angle). Cause + fix in multi-accessory-wigs.md
  (part never *activated*, vt+0x18, so its DynToon ramp unit isn't prepared).
  **Fixed, built + installed (untested).** Equip screen still shows only
  the main (builder path, not done).
* Known gaps: preset stays "last built" until a -1 build resets it to 0; a
  recreated CaC in a slot inherits that slot's picks (as Battle Styles);
  sold accessories aren't dropped from picks yet.
* Not in the repo README's mod table yet (add once tested).

### Waiting on the user (as of retest 17)
* Puu: Arale hat + hat 835 both draw again (no `extra 835 skipped` with
  equipped 843 in the log).
* FRI helmet still no hat; Hercule hair (1406) still keeps its hair.
* Phase 2 smoke test: extras still show everywhere as before (config
  fallback); log has `Presets panel callback: hooked` and `loadout builder
  calls hooked: 38 of 38`; picking presets in battle character select logs
  `slot N: loadout from preset P`; no crash in the Presets panel.
* **Equip row** (Customize -> cloth tab): log has `equip screen row: AVATAR
  edit registered, callback hooked`, `user-data slots: raised`, `accessory
  row pushes hooked: 9 of 9`, both mods' AVATAR served (loader log). Row
  shows three narrow columns with readable text; left/right move between
  them, up/down leave from any column; Battle Styles' slot on the Skills
  page still works. Pick in column 2: hover previews the extra, decide
  saves (`slot N extra 1 = item I (part set P)`), equipped accessory
  unchanged, cancel keeps the old pick, "none" clears it. Leaving the tab
  restores the layout. Retest after the item-id fix: both extra columns
  show their names (log `column text "..."`), both picks draw. If the main
  column's text is blank too, it's the AS narrowing (maNarrowText), not data.
  Retest 4 (after the scale/cache/alpha fix): three boxes side by side on
  the Accessory row, all selectable (also after switching tabs and back)?
  hover/pick of an extra redraws the model (untested since retest 2's
  force-rebuild)? Send the `equip screen layout:` lines incl. the `later:`
  ones (later main/extra alpha should be 1, extra scaleX 0.33, x unchanged
  from the narrowing; if x/scale moved back, the menu timeline animates
  the buttons and the narrowing must move to the buttons' children).
  Retests 5-13 (steps and results): `HISTORY.md`. Retest 13 found the
  stacked parts were activated before the game's own pass (0x297850).
  **Retest 14 (fix, installed 2026-10-26):** stacked parts are activated
  right after 0x297850 (call 0x27e726 hooked, `ActivatePartSetHook`); in
  `ResSkeletonHook` only if the slot's own part is already activated.
  Same CaC: save select, lobby, character select, battle, back to lobby;
  check the stacked wigs' colours (antenna skin colour, no black at
  angles, battle colours right). Send the `part set N activated` lines
  with their two dumps (the equipped wig's +e0 should now be set too) and
  any `slot already activated` line.
  Result 2026-10-26 (log 19:08-19:11): colours mostly right now and the
  equipped wig's +e0 is set (same fields as ours). Still wrong: antenna +
  forehead green everywhere; save/character select black at some angles;
  battle hair turns white at a few angles. Cause of the green found
  statically: CaC colours are written only by Chara vt[57] 0x27d7d0 into
  the 10 parts at chara +0x370 (see `multi-accessory-wigs.md` "CaC colour
  pass"), and stacked parts aren't there.
  **Retest 15 (fix, built 2026-10-26):** Chara vtable slot 57 hooked
  (`CharaColourHook`): after the game's pass it reruns the pass with only
  the stacked parts in the array (physics list emptied), then restores.
  Install with `Build_Install.bat` (game closed). Same visits: save select,
  lobby, character select, battle, back to lobby.
  1. Log start: `CaC colour pass: hooked`.
  2. Antenna + forehead skin-coloured in all four places (expected).
  3. Black angles at save/character select, white hair angles in battle:
     note whether they're gone, and where they remain.
  4. Send the `colour '...' applied to N stacked part(s)` lines (up to 40;
     expect SKIN_ / HAIR_ etc. names) and any crash/hang.
  Result 2026-10-26: skin colours right everywhere; black/white at angles
  remained. Cause (static, `multi-accessory-wigs.md` "Per-part draw
  callbacks"): a part's light + chara sub-buffer callbacks are filled by
  Chara only for the parts at +0x370.
  **Retest 16 (fix, installed 2026-10-26):** `ShareCharaCallbacks` points
  a stacked part's light/scene callbacks at the shown part's (in
  `ActivatePartSetHook`, and `ResSkeletonHook` when already activated).
  Part dumps removed. Same visits: save select, lobby, character select,
  battle (turn the camera round the CaC), back to lobby.
  Result 2026-10-26: **passed** - stacked wigs shade right everywhere.
  Still open: halo 831 in the last slot didn't render (skipped as a clash
  with wig 1410), lobby avatar didn't update after leaving the equip
  screen offline (no rebuild of record 0 logged).
  **Retest 17 (fixes, built 2026-10-26, not installed yet):**
  - Clash rule rewritten on hide masks (`CanShow`, research note
    "Physics-only hats"): objects hung on a slot never clash; one part set
    hiding a slot the other replaces does. Offline
    (`multiacc_slotuse.py --equipped`): HUM/NMC/FRI halo over 1410/1406/
    2035 shown; hat 835 still skipped on 1406 and FRI 2035; 612 still
    stacks; NMC 835 now stacks on 1410 (its object hides nothing) -
    new case: a stacked part set with a physics object.
  - Offline lobby avatar: `LobbyCharaChangedHook` (call 0x48fd83 of the
    lobby avatar's change check 0x4887f0) sets the cached accessory to
    another value once when the extras changed (equip row pick / Presets
    load) and the avatar is the current CaC -> the game's own full reload.
    Log: `lobby avatar: slot N's extras changed, reloading it`.
  Once the layout is confirmed: remove the `maDiag` code (sendType 9000+n).

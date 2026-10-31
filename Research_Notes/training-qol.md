# Training QoL - switching control/camera to the Training partner

Covers: who controls which battle slot (per-slot input bindings, human player
pads, local player override), the battle camera's follow target and its
setter, the game's Body Change swap, and the tests run for Training_QoL.
DBXV2.exe 1.26.01 RVAs. Probes: `ClaudeTools\trainingprobe.py` (read-only),
`trainingswap.py`, `trainingbodychange.py`, `trainingpoke.py` (write).

## Goal
A Training-only toggle (D-pad up since 1.1.0; D-pad down = photo camera) that moves the player's controller and
the camera to the Training partner/opponent (slot 3) and back, without
swapping stats or team, offline only.

## Battle manager `[exe+0x15681c0]`
* `+0xbc` = 1 in Training; `+0xc0` online flag (0x156190 returns it).
* Slots `+0xe0 + i*0x1f0` (14): `+0` chara id, `+0x14` control type (cached at
  spawn - writing it live does nothing), `+0x18` team, `+0x24` bit0 = CPU.
* `+0x1c00` local player override slot (-1 = none). Read by the local index
  function 0x92050 (`rcx = mgr+0x7c`, `[rcx+0x1b84]`); 0x14d100(mgr) = local
  player's Mob. Setting it alone to 3 brought up a stuck "A Controller was
  disconnected" prompt (needed a game restart): the disconnect check finds
  human player 0's Mob through it and slot 3's binding had no pad.
* `+0x3a58 + i*8` Mob of slot i (Mob `+0x44` = slot index).
* `+0x3ad0 + i*8` input binding of slot i (no vtable). Each Mob holds the same
  pointer at `[[mob+0x4e8]+0xd48]` (getter 0xc8bc0).
  * `+0x130` pad (0-7, 8 keyboard, 9 any pad, -1 none), `+0x1c8` human
    player number - setter 0x1fbe30(binding, player, pad).
  * `+0x190` device kind (<4 = 0x849820(player), 4 = pads 4-7, 5 keyboard,
    6 any pad) - setter 0x1fbe00(binding, kind, slot), which refreshes
    through 0x1fb6d0 when the pad is >= 0. **Use the setters**: writing the
    fields directly broke control (see Tests).
  * Live, Training, partner on CPU: slot 0 = pad 9 / kind 6 / player 0,
    slot 3 = pad -1 / kind 0 / player 1.

## InputMgr `[exe+0x15543f8]`
* `+0x8c8` last active pad (0x2f3e80 sets it and `+0x8cc` = 1).
* `+0x8d0 + player*0x14` pad of human player 0/1 (setter 0x2f4410 also copies
  capabilities to `+0x8d4`). Live: player 0 = 9, player 1 = -1.

## Pad check / reassign 0x15ad90 (rcx = battle manager)
Runs while `[mgr+0x4a90]` and `[mgr+0xa8]` are set. Checks GetStatus (xgcore
`Pad::GetStatus`, IAT 0xf4ec08) of each human player's pad (9 = any of 0-7, 8
= keyboard via 0x2f4bf0); on a missing pad shows the "connect controller"
prompt. Reassign (0x15af00..0x15afe5): needs as many distinct connected pads
0-7 as human players, then per player 0x154c30(mgr, player) -> Mob -> slot ->
binding, 0x1fbe30, 0x1fbe00, 0x2f4410, finally 0x16f420([mgr+0x4a90], pad).
So the partner on "Player Controls" with one controller always prompts.

## Camera
CameraManager `[exe+0x157f168]`, `[cam+8]` = director (CameraBase), director
`+0x90` = followed Mob (getter 0xb9210). Writing `+0x90` directly does not
stick (reverts). Setter **0xb8ca0(cameraManager, mob)** (-> 0xad1a0(director,
mob)) works: the camera followed the partner (2026-10-20).

## Body Change 0x158de0(mgr, slotA, slotB)
The Ginyu swap (offline; online it sends a packet). Swaps the slot records
(`+0xe0` blocks, incl. team/control), each Mob's `+0x2300`, the binding
pointers (`+0x3ad0` and `[mob+0x4e8]+0xd48`), the local override `+0x1c00`
(0x1591b1 / 0x15913b), and moves the camera with 0xb8ca0 (0x15971c). It
swaps stats too - rejected by the user.

## Control mode (human vs CPU)
* Mob `+0x50` = control mode, 0 human / 1 CPU (3 = another CPU kind).
  Setter **0xd9930(mob, mode)** (calls vfunc `[mob+0xc8]` on a human<->CPU
  change). Read through 0xc9340; Battle::Command (vtable 0xf7c018,
  `[mob+0x4e8]`) recomputes its own CPU flag `Command+0x14` from it every
  frame in 0x1f76f0 (0x1f773e: mode 1/3 -> 1, 0/2 -> 0).
* **Training forces the partner's mode every frame**: MenuTraining's update
  0x192db0 calls 0x192ec0 at 0x192ea5 (also from 0x191fc3 on a setting
  change, 0x192914). 0x192ec0 reads the partner's CPU Status setting
  `[0x196940([this+0xc8], 0)]`, partner Mob = `[this+0x28]`:
  0 -> 0x2a6f90; 1 (Player Controls) -> 0xd9930(mob, 0) + rebinds the
  partner's binding to pad 0x43150()->vfunc+0x68 via 0x1fb6d0; 2 (CPU) ->
  0xd9930(mob, 1) + AI setup; 3 -> 0xd9930(mob, 3) once per value.
  So a mode written raw or with the setter reverts within a frame.
  `trainingcontrol.py hold A B` NOPs the call at 0x192ea5 and sets the modes.

## Tests (2026-10-20, Training, partner on CPU unless noted)
1. Swap binding fields `+0x130/+0x190` in place + write director `+0x90`:
   "controller disconnected" prompt (cleared with A), no character
   controllable, camera stayed on the player. Fields restored by hand, control
   not re-checked - **later tests may have been run on broken input**.
2. Swap binding pointers + override 3 + camera setter: camera followed the
   partner, nothing controllable.
3. Body Change via remote thread: camera on the partner (free camera), still
   no control. Undone (called again, override reset to -1).
4. Partner on "Player Controls", one controller: prompt; setting player 1 /
   slot 3 to pad 9 by hand cleared it, still no control.

2b. Test 2 again on a clean game: no prompt, camera on the partner, nothing
   controllable.
5. Test 2 + swap Mob `+0x50`: R2 showed the partner's skill list (so the
   local player really moved), but no movement; the partner's mode went
   back to 1 within a second (the Training apply above).
6. Test 5 + `trainingcontrol.py hold 0 0` (apply call NOPed, both modes 0):
   modes stay 0, Command+0x14 = 0 for both. **Works** (user, 2026-10-20):
   full control of the partner, "as if that's who I picked"; the original
   character just stands there (mode 0 with no pad = no AI, accepted).

## Working mechanism (test 6)
Toggle on: swap the binding pointers (`mgr+0x3ad0` slots and each Mob's
`[[mob+0x4e8]+0xd48]`), override `+0x1c00` = partner slot, camera via
0xb8ca0, partner mode 0 via 0xd9930, and stop 0x192ec0 running (the 0x192ea5
call NOPed in the probe; the mod skips it). Toggle off: swap back, override
-1, camera back, let 0x192ec0 run again (it re-applies CPU Status itself)
- but restore the partner's own mode first (see below: status 3 does not
re-apply it).

## CPU Status entry and the Training menu (2026-10-20, static)
* The CPU Status 0x192ec0 applies is entry `[MenuTraining+0xac]` of the
  table `[+0xc8]`: 0x196940(table, 0, index) -> 24-byte entry, `+0` status,
  `+4` level (0x196940 bounds-checks the upper end only, signed).
* Status 3 ("CPU AI" level) applies only when the level differs from the
  cache `[+0xbc]` (reset to -1 whenever the status is not 3): mode 3 via
  0xd9930, AI params 0x701b0/0x70060/0x701d0, then 0x1fb6d0(partner
  binding, 0, slot, 0, 0). A mode changed by someone else is not undone
  while the level stays the same.
* Choosing an entry in the pause menu: handler 0x6097ef (training =
  `[mgr+0xd540]`) calls MenuTraining slot 9 0x191f00(training, index) (sets
  `+0xac`, clears `+0xb4/+0xb8` and flag bits on both Mobs, calls 0x192ec0
  at 0x191fc3), then 0x18c5d0 / 0x18c550 / 0x18c590 / 0x191f20 for the
  other menu options (none touch the local player).
* 0xd9930(mob, mode): modes {0,2} human-like, {1,3} CPU-like; only a change
  between the groups calls vfunc `[mob+0xc8]` (Mob 0xd5e30: resets
  `+0x2730`, `+0x2840..+0x2860`).
* Pause: MenuTraining's per-frame update 0x192db0 (so 0x192ec0 too) runs
  while the menu is open. 0x155ec0(mgr) returns 1 while `mgr+0x24` lacks bit
  0x40 or has bit 1, or `[exe+0x15544d0]` (0x2c4fa0) / `[exe+0x1554480]`
  (0x431a0) report busy, or `[[exe+0x15681b8]+0xc8]` bit 2; the Training
  HUD 0x193330 and 0x1937a0 skip themselves on it. Training_QoL uses it as
  "paused": confirmed to cover the pause/Training menu (2026-10-20).
* 1.0.0 crashed on unpause after a CPU AI entry was picked while switched;
  no dump, exact cause not found. 1.0.1 (switch back restores the partner's
  saved mode before the apply + the pause guard) no longer crashes
  (confirmed 2026-10-20).

## Pause flag (2026-10-20, static)
* `mgr+0x24` bit 0 = battle paused (pause menu up); getter 0x156180, used by
  the pause controller (0x16eca1, 0x16f011, ...) and by PhotoArtEvent, which
  only enters photo while it is set. 0x155ec0 is broader (bit 0x40 missing,
  0x2c4fa0 / 0x431a0 busy, `[[0x15681b8]+0xc8]` bit 2) - one of those may
  cover ultimates/cinematics (not measured), so 1.1.0 guards with bit 0 only
  and logs 0x155ec0 changes beside it.
* Pause controller `[mgr+0x4a90]` (ctor 0x16e970, made in 0x155570);
  `+0x68` = PopupPause (0x671c70) while open (0x170760 creates, 0x16fcd0
  frees).

## Battle photo camera (2026-10-20, static)
The lobby's photo mode (LobbyPhotoArt, update 0x40e150) has a battle twin,
`Battle::PhotoArtEvent` (vtable 0xf75030, ctor 0x18bd60, init 0x18cc20,
update 0x18e290 = state machine on `+0x18c`), created in battle setup
0x15b050 **online only** (`[mgr+0xd548]`) and driven from the pause menu
(0x170536). Creating one offline was rejected: its init changes control
modes (0xd9930(partner, 1)) and loads sound banks. What it does is small:
* Cameras: CameraManager `[0x157f168]`, 0xb9ec0(camMgr, index) -> camera,
  index 9 = `Battle::CameraPhoto` (update 0x79e390 -> 0x7924f0 reads the pad
  itself through `[0x1554400]`: move `+0x3a0..+0x3a8`, rotate `+0x30/+0x34`,
  zoom `+0x48` clamped to `+0x644..+0x648`). Switch with 0xb8c70(camMgr,
  index, &zeroInt) (index <= 9).
* Enter (0x18e4e1..0x18e4d5): only if camera 0's vfunc `+0xc8` returns
  nonzero; 0xb8c70(camMgr, 9, &0); 0xd9990(mob, 0) on both characters
  (event `+0x20`, `+0x28`); then 0x18df00 turns input on with
  0x79f430(cam9, 0) (byte `cam+0x654` = input off).
* Exit (0x18f502..0x18f518): same camera-0 check, 0xb8c70(camMgr, 0, &0);
  0xd9990(mob, 1) on both; 0x79f430(cam9, 1).
* 0xd9990(mob, enable): enable 0 sets Mob `+0x78` bit 0x10 and
  0x1f61e0(command, 0) clears Battle::Command's input buffers
  (`+0x10` = enabled) - the character ignores its pad.
* 0x7924f0 zeroes the camera's input when `cam+0x654` is set, when
  `[mgr+0xd524]` != 0, or when `[mgr+0xd548]` (the online event) exists and
  its state is not 2 - offline that pointer is null.
* HUD: the online event sets `mgr+0xd528` (0x157d20) on entry; the HUD draw
  callbacks 0x1637f0/0x163af0 skip on it **only online** (`+0xc0`). They
  also skip on `mgr+0x6f28`, but that field is read all over battle code
  (Mob update 0xd5750 etc.) - not touched. Offline fix (Training_QoL
  1.1.0, `photo_hide_hud`): the check order in both callbacks is
  `cmp [rcx+0x6f28],0; jne skip; cmp [rcx+0x4a58],0; je skip;
  cmp [rcx+0xc0],0; je draw; cmp [rcx+0xd528],0; jne skip` - the
  `je draw` (74 0d at 0x16381e and 0x163b1b) is nopped and the DLL sets
  `mgr+0xd528` itself. d528 is read nowhere else; written only by the ctor
  0x151c08 (zero) and 0x157d20 (online event), so the patch changes nothing
  while it is 0. Not used: the HUD's own mask `hud+0x23a40` (default 0x5f,
  setter 0x1b8d50(hud, mask, set); Training 0x191f34 uses bits
  0x100/0x200; the event clears 0x40 every frame).
* Photo UI (not done, a bigger job): the event owns `+0x198`
  PhotoCameraMenu (new 0x270, ctor 0x795b30 loads data/msg/photo_text;
  setup 0x2c7460(menu, 0x17, 0), 0x795f10(menu, 1/0) shows/hides, help
  text 0x796600, 0x797110 / 0x7982e0 call AS functions through vfunc
  +0x130 (photoCameraSetUIVisible, photoCameraSetCenterUIVisible,
  photoCameraSetHelpText, photoCameraSetCameraBarArrowPosition,
  photoCameraShutterAnimation), per-frame vfunc +0x48(dt)); `+0x1a0`
  PhotoDecoMenu (0x79d750, stamps); `+0x1a8` 0x798980; `+0x1b0` the capture
  object 0x4e8530 (shutter, saves to the album). The init 0x18cc20 also
  loads sound banks and msg files. The camera menu alone would advertise a
  shutter that does nothing unless the capture is driven too.
* Not found: a way to freeze the battle without the menu (dt 0x2a6fb0 is 0
  while `[0x156ed78]` is set; untested, might stop the camera too).

## State / next
Training_QoL 1.0.1 released and confirmed in game (2026-10-20). 1.1.0
(swap on D-pad up, photo camera on D-pad down as above, pause guard =
0x156180 or the pause popup) confirmed in game (2026-10-20), including the
swap during ultimates; the HUD hide added after that is installed, not
seen in game (`Training_QoL\NEXT_STEPS.md`).

## XV2Patcher-only version (Epatch, 2026-10-21)
The same features without the mod loader, as one Eternity patch file
(`Training_QoL/epatch/`: `training_qol_cave.asm` + `make_epatch.py` ->
`XV2PATCHER/Epatches/Training_QoL.xml`; `build.bat epatch*`). Pure `write`
patches, no file replacement:
* **Code cave in .text slack at 0xF4B400** (~1.9 KB; .text VirtualSize
  ends 0xf4b05c, next section 0xf4c000, all zero in the image, no other
  installed Epatch writes there - Revamp's caves are in .bind slack
  0x1A1E5xx). The patch pattern is the last .text instruction
  (`48 C7 05 xx xx xx xx 00 00 00 00 C3` at 0xf4b050) + the zeros up to the
  cave + the cave's length of zeros, so it lands at 0xF4B400 or nowhere -
  the call patches jump to that fixed address. XV2Patcher's search reaches
  past VirtualSize (Revamp proves it) and search_start is an RVA (checked
  against Revamp's ChangeCueID1).
* **State in .data slack at 0x18A7F00** (0x80 bytes; .data ends
  0x18a76d0, the page is RW and zero). Needs no patch - the cave just uses
  it; layout in the asm (`S_*`).
* The three `e8` calls of 0x192ec0 rewritten to the cave's PerFrameApply /
  OtherApply (same logic as dllmain.cpp's), the HUD checks' `74 0d` ->
  `90 90` as the DLL does.
* The cave finds the exe base from its own address (`lea rbx, CaveStart;
  sub rbx, CAVE_RVA`), calls the game as `[rbx + RVA]`. The exe imports no
  XInput (xgcore.dll does): XInputGetState comes from the exe's IAT
  LoadLibraryW (0xf4c1b8) + GetProcAddress (0xf4c128) on first use -
  xinput1_3.dll is XV2Patcher itself, which forwards it.
* ml64 leaves rip-relative references to the cave's own labels as REL32
  relocations; make_epatch.py resolves those and refuses any other kind.
* Limits vs the DLL: buttons fixed (D-pad up / down, `SWITCH_BUTTON` /
  `PHOTO_BUTTON` in the asm), no keyboard key, no config, no log, HUD
  always hidden in photo mode. Checked offline: the patches applied to the
  decrypted 1.26.01 image land where expected, each call goes to its entry,
  the cave disassembles cleanly. In game: XV2PATCHER\xv2_log.txt lists
  each `TrainingQoL_*` patch as "located at" (Cave = 0xf4b050).

## 1.25.02 port of the Epatch (2026-10-22)
The Epatch is built per game version: `make_epatch.py <version> <out.xml>
[image]`, the addresses in `epatch/game_1.26.01.inc` /
`epatch/game_1.25.02.inc` (picked by `/DGAME_1_25_02`), everything else in
the asm is shared. `build.bat epatch125[_release]` builds the 1.25.02 one
(the version Revamp 5.1.1 targets; reference game in the modding folder's
`Refs\`). Built and checked offline only, never run in game.
* **Addresses** (1.26.01 -> 1.25.02): BATTLE_MANAGER 0x15681c0 ->
  0x1681c20, CAMERA_MANAGER 0x157f168 -> 0x1698bc8, training apply 0x192ec0
  -> 0x1961e0, set control mode 0xd9930 -> 0xd4210, camera target 0xb8ca0
  -> 0xb5240, local player mob 0x14d100 -> 0x50560, paused 0x156180 ->
  0x159a30, camera at 0xb9ec0 -> 0xb6230, switch camera 0xb8c70 -> 0xb5210,
  chara input 0xd9990 -> 0xd4270, camera input off 0x79f430 -> 0x7ed610,
  IAT GetProcAddress/LoadLibraryW 0xf4c128/0xf4c1b8 -> 0xfd9130/0xfd91b8.
  Apply call sites: 0x1961c3 (per frame), 0x195544 (setting changed),
  0x196a62 (menu setup); HUD checks 0x1679a7 / 0x167c84.
* **Fields**: only the HUD-hidden flag moved (`mgr+0xd528` -> `+0xd524`);
  the getters at mgr+0xb0/+0xc0 and the Training object's +0xbc are
  identical code in both builds. Camera vfunc 0xc8 checked by tallying the
  vcall after CameraAt(mgr, 0): 0xc8 x7, 0xc0 x1 in both images.
* **Method**: decrypted 1.25 image (dumped from the Refs game) +
  `ClaudeTools\exe\port_sites.py` (code / function / global addresses by
  codematch voting) and `port_offsets.py` (whether a struct offset moved).
* **Placement**: 1.25's .text slack is only 0x264 bytes (ends 0xfd8d9c), too
  small for the ~1.9 KB cave, so the cave is split into two sections:
  the main piece (`.text`, 0x578 bytes) goes into .bind slack at 0x1b77960
  (.bind is executable, ends 0x1b77448), right after Revamp 5.1.1's own
  caves 0x1b77500..0x1b77960 (RevampExtra.xml + RevampDynamicPartsets.xml);
  the `CAVE2` segment (SetCharacterInput, the photo code, the XInput
  names, 0x217 bytes) goes into .text slack at 0xfd8da0. make_epatch.py
  links REL32s across the two. The .bind anchor is the last .bind bytes
  `1D316133837F8F67` (0x1b77440) + XX wildcards over Revamp's caves (so it
  matches with or without Revamp) + our length of zeros. State block at
  0x19d7f00 (.data slack). New Transformations 3.4.5 has no Epatches.
* **Revamp check**: applying Revamp 5.1.1's Epatches and ours to the 1.25
  image in both orders: every patch still finds its site, none moved, no
  byte overlap. The 1.25 and 1.26 cave listings differ only in the base
  RVA, the 0xd524/0xd528 field and the two leas into CAVE2.
* 1.26.01 still uses .text slack 0xf4b400 for both pieces (output equal to
  the pre-split build except 8 bytes of alignment padding).

## 1.27 port of the Epatch (2026-10-29)
`epatch/game_1.27.inc` (`/DGAME_1_27`): addresses from the 1.27 address map
(the same ones the DLL uses), IAT GetProcAddress 0xf5a128 / LoadLibraryW
0xf5a1b8. One cave piece again (like 1.26.01): .text ends 0xf595ec, slack to
0xf5a000 (0xa14 bytes); main piece 0xf59600 (0x578), CAVE2 0xf59b80 (0x217).
State block 0x18bbf00 (.data ends 0x18bb5b0). Call sites 0x192ea5 / 0x191fc3
/ 0x192914, HUD checks 0x163817 / 0x163b14 (HUD hidden still +0xd528); field
offsets unchanged. Checked offline against `img127.bin` (all sites unpatched
there); never run in game.

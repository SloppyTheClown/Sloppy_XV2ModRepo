# BAC + BCM moveset behaviour

How moveset data acts in game: BCM input matching, hold loops, homing, BAC event rules. Game 1.27. Each fact says
whether it was seen in game or read from the files. Tools: `ClaudeTools\refinement\` (`homing_survey.py`,
`bcm_hold_survey.py`, `bac_function_uses.py`).

## BCM input matching and hold loops (found 2026-11-05 on Kid Buu's boost spin dash)
**The vanilla setup (read from MAP_PLAYER.bcm/.bac; CMN has the same roots):**
* Roots 4223 (Light -> BAC 493) and 4224 (Heavy -> 497), ActivatorState `Boosting`, DirectionalInput
  `Forward, Backwards, LeftRelative, RightRelative`, HoldDownConditions 0x60000,
  PrimaryActivatorConditions `Unk11, Idle, KiAboveZero`, ReceiverLinkID 0xa.
* Right after them: 4225 (Light -> 375) and 4454 (Heavy -> 377), the boost punch/kick. DirectionalInput 0 (any),
  HoldDownConditions 0x0. So boost + button with the stick pushed is the spin dash, and without it the punch/kick.
* 493/497 events:
  * Function 34 "BAC Loop (instant end)" over 4+30, with Function 4 Take Ki over the same frames.
  * Manual Movement 0x34 over 4+30, which steers with the stick.
  * Tracking 0.025 `TrackForwardAndBackwards` over 0+4.
  * BcmCallback `FlyingAndStepDash` only at 35+22, after the loop.

**Seen in game:**
* **Roots are checked in file order; the first match wins.** A Forward-only root placed after 4225/4454 never fired:
  the punch/kick took the press.
* **DirectionalInput is relative to the facing, and `Forward` is a quarter of the stick (about +-45 degrees).**
  `LeftRelative`/`RightRelative`/`Backwards` are the other quarters.
* **The dash turns him to face the stick**, so a second or so in, the stick reads `Forward` whatever it started as.
  The exception is a stick-back dash, which doesn't turn him.
* **A hold loop keeps going only while the first root matching the held button + current stick is the one that
  started it.** Inferred from two builds (fits everything seen, not proven):
  1. `Forward` taken off 4223/4224: sideways dashes stopped after a few frames, once he faced the stick.
  2. A held-type (0x60000) Forward-only copy placed just before each vanilla root:
     * Sideways dashes stopped after about a second, once the copy matched first.
     * Back dashes lasted until the stick moved out of the back quarter.
     * The copy's own dash stopped as soon as the stick left Forward or he flew past the target and his facing
       flipped.
  In both builds a stick-back dash held still never ended early.

  Nothing re-fires during the loop to cause this: the only BcmCallback window opens after it (frame 35).
* **Re-press quirk (not explained):** releasing the button ends the loop. Pressing it again during the tail (the
  BcmCallback window) and holding gives a dash that lasts in any stick direction. Not checked: which root fires
  there, and why its hold check differs.
* **3D chase:** a `HomingMovement` shaped like Dragon Homing's, in place of the Manual dash movement, chased the target
  well, vertical included. That shape is cl2 440: StraightLine, `EnableAutoTracking, UseFloatSpeedModifier, Unk3, Unk6`,
  Displacement Z -0.75, at the dash's speed, plus Tracking 0.5 and TargetingAssistance Z/Y. Tracking + TargetingAssistance
  Z with Auto movement and no HomingMovement turned only horizontally.
* **A Tracking event without `TrackForwardAndBackwards` only turns toward a target in front of the facing** (the top
  half of the stick).

**Read from the files:** no vanilla `*_PLAYER.bcm` splits one held button over sibling roots by DirectionalInput
(`bcm_hold_survey.py`, every character). The held-type values that exist, all characters together:
* 0x4: `Heavy, Guard` (charge releases), plus 1 on Light.
* 0x1: `Blast, Guard`.
* 0x20000: `Heavy, Guard`, Heavy, Light.
* 0x20001: Light.
* 0x60000: only the two spin dash roots.

So a direction split on a held move has no vanilla model.

**Being tried (Refinement 2.15 third build, not seen in game yet):**
* The Forward copy is press-type (0x0, like the punch/kick roots), assuming the hold check passes press-type roots by.
* Its BAC copy has no Function 34, with the 4..34 loop played 3 times instead.

The result goes in this section.

## BAC facts from the Refinement rework (2026-11-03)
Function types are printed in decimal by the serializer; names are in `Xv2CoreLib\ValuesDictionary\BAC.cs` `BacFunctionNames` (hex): 13 = Activate Transformation, 6 = Invisibility, 35/36 = Spawn/Knock Away Pebbles, 40 = Invisible Opponents. `AuraType` in the same file also has a 6 (HenshinEnd), which is unrelated. Vanilla 004_GOK_MAP pairs Function 13 with Function 6 and Common 612 at the end of its entry.

An effect Off placed past the animation end never runs, so LoopCancel parts keep looping after the transformation: keep Offs before Function 13 and the last frame.

A BCM link entry is only checked inside a BAC BcmCallback window, and `InTransformedState` is evaluated then: for a transformation whose intro chains into a second action through the BCM, Function 13 must come before the callback (entry 0: Function 13 at 274, callback 275). The reverse order silently skips the second action. Found from the data; check the result in NEXT_STEPS.

Skill files with their own id inside: an installer replaces 0xBACA (47818) with the installed id in BAC and BDM `Skill_ID` (LB `InstallSkillFolder` -> `ChangeNeutralSkillId`, also .bsa; xv2ins `ChangeReferencesToSkill`), renames `NNNN_CCC_SSSS` in file names, and copies the rest verbatim. So a skill edited in place needs its id back to 0xBACA before it ships, and the eepk's container names (`<prefix>.ptcl.emb`) need a fixed name, which works in a skill folder.

A skill's `_F.ean` can be regenerated from its male `.ean` with `Tools\EanRaceFit` (Battle Styles' fitter as a standalone tool, `skill` mode). Confirmed in game (female Majin, 2.0).

A BAC Camera event's Duration must cover the whole entry when the next action links at the end: entry 0's camera ran 250 of 276 frames and the gameplay camera came back for the last 26, which XenoKit does not show.

**The ki charge is not in an after BAC (2026-11-04, tested in game):** sounds + steam added to after BAC 68-73 (`BUS_BAS_STUN_*`, BAC.cs "Recover KO") changed nothing on a charge. Charging is a super skill with its own BAC, so a transformation's after BAC never runs for it. `ClaudeTools\refinement\bac_kiaicharge_scan.py` lists every BAC with a KiaiCharge aura event. The charge family:
* All share one pattern: Ultimate Charge (598 KTU, 2093 KTG), Burst Charge (599 KTB), Indomitable (2321 KFC), Energy Charge (591, 1105), Full Power Charge (592), Maximum Charge (593, 1123, 2343), Final Charge (1573), Pretty Charge (1564). 598/599 use the EAN + SE of 593 (`SPA/593_APL_KTH`, `CAR_BTL_S593_KTH_SE`); 2321 uses the SE only. That EAN has 2 animations, `APL_SPA_KTN_C` (0) and `APL_SPA_KTN_S_C` (1), 140 frames, played in 15-frame looping pieces. Which of 0/1 is ground or air wasn't checked.
* Events: `Character_VOX` cue 88 @0, `Skill_SE` cue 1 @16 (StopWhenParentEnds), StageBG effects, Functions 35 + 4 (+22 in KFC) every 15 frames from 25, `Aura KiaiCharge` @25. Ending: `Aura KiryokuMax`, `Aura KiaiCharge` + DisableAura; the recovery plays `Character` EAN 0.
* "Charge" 50 TGK / 54 GTGK is hitboxes and movement, an attack rush, not a ki charge.
* MAM/MAF (and other CaC player BACs) have charge-shaped entries 484/486 (Common EAN 249/250, same 15-frame loop, KiaiCharge @25) that MAP_PLAYER doesn't. They are the guard + heavy charge, below.

**Guard + heavy charge (2026-11-04, read from the files; a copy in Refinement 2.11 fires and charges in a transformed form without `InTransformedState`, seen in game):** Ultra Instinct's built-in charge is in the CaC files, not in its skill. Each CaC `*_PLAYER.bcm` (seen in MAM, MAF, HUM, HUF, SYM, NMC, FRI) has 4 roots with one child each, in MAM root positions 40-43 of 48:
* Root: `Heavy, Guard` + ActivatorState `Idle`, or `Heavy` + `Guarding`; HoldDownConditions 0x20000; PrimaryActivatorConditions `Standing|Floating, InTransformedState, Unk11, Idle, KiBelow100`; CUS_AURA 30 (gates it to UI's aura); BAC 484 (ground) / 486 (air); ReceiverLinkID 0x2.
* Child (the release): same buttons, HoldDownConditions 0x4, `InTransformedState, Unk11, Idle`, BAC 485 / 487, ReceiverLinkID 0x3.
* BAC 484/486: Common EAN 249/250 in 15-frame looping pieces from 25 to 430, end animation 430-507, then Character EAN 0/1. Function 4's ki ramp equals Ultimate Charge 598's (-1.0 at 25, up to -7.0 at 385-415), so it needs no skill EAN. Common_SE 179@16 is the loop, 180@470 the burst. Females use Common 247/248 (`GKS_SPA_KTM_F`, `_S_F`); that is the only MAM/MAF difference.
* The "Built-In Ki Charge" mod (Sportiax) installs the same entries for other forms with `InTransformedState` removed and CUS_AURA -1, which shows both gates can go.
* In an after BAC the ids must not collide: MAP_PLAYER.bac uses 484-486 itself. CaC BACs have empty slots up to DefaultEmptyEndIndex 900, so Refinement uses 640-643. Custom sounds there have to be Character_SE (Skill_SE has no bank in an after BAC entry).

**What can change a charge only while transformed:**
* The BAC event `Flags` are only Both/CaC/Roster (`ValuesDictionary/BAC.cs`), with no transformation condition. The EAN and SE come from the skill, and a skillset preset replaces slots without looking at what's in them. So no data route was found for animation or sound.
* The aura's KiaiCharge slot is the one conditional hook in the data: the skill fires the slot of whatever aura is active. Aur 14 holds effect 143, Revamp's aur 2500 holds 153. Editing the slot would affect anything else drawing that aur. Not tried.
* Otherwise a DLL: at the BAC Animation/Sound event, or entry start 0xf3da0, check `[char+0x22ec]` + the active skill `[char+0x364]`. Animation event handler not found yet.

Sounds in an after BAC must be Common SE or Character SE, not Skill SE; new Character SE cues need merged CSO rows, which an x2m can't do (`xenokit-audio-library.md`). Skill effects need type 12 + the neutral id, so only an after BAC inside the skill folder gets them.

# Porting a character's blast to a CaC

Goal: any blast works on a CaC the way a Battle Style does, with or without Battle Styles,
for roster and modded characters alike. Found by reverse engineering 1.27 and surveying the
game's files (`ClaudeTools\kiblast\blast_port_survey.py`, `blast_port_check.py`).

## What a blast takes from its owner
* Animation ids below 82 are the same moves on every race (`HUM_BAS_STAND_S` / `JRN_BAS_STAND_S`):
  they stay the CaC's own, so a Battle Style's poses still show.
* Ids 82 and up differ per character (on a Namekian they are the Giant form): Kid Buu's 21086,
  Golden Frieza's 21085. These must be carried.
* Effects with EepkType 2 come from the character's own EEPK: 21086, 21110.
* No blast in the game animates prop bones or uses a character camera, BDM or SE.

## The port
1. **BAC stage** on each blast needing it: `CHARACTER:n` with n >= 82 becomes `skill:<base + k>`;
   EepkType 2 effect records get the owner's chara id (dword rec+0xc, -1 in vanilla, passed as
   the id to SpawnEffect 0x965080, so the owner path and Battle Styles' owner hook aren't asked).
2. **EANs**: `<folder>_<code>.ean` per CaC race (HUM HUF SYM SYF NMC FRI MAM MAF; Saiyans on the
   human skeletons) and per real user having all the ids: the skill's own animations + the
   carried ones at base + k, fitted to that skeleton (Battle Styles' FitEanToRace + ComposeEan).
   Base = the most animations any skill EAN of the blast has, not counting ours. Source = the
   owner, else a user that isn't a created race, else a created race; the most `_BLT_` names wins
   (21086 Kid Buu, 21085 Frieza). A new file is written loose (`converted_eans.txt`); one that
   exists is served from memory (ReplaceFile). A user without the ids (BUS on 21086) gets no file
   and plays nothing, as in vanilla.
3. **Per-race lookup**: the skill EAN path (0x260ba0) skips `_<code>` for chara 100..109 at the
   call 0x260d83 (0x2900c0). KBO answers 0 there, and sets CUS flag 0x800 on every blast. A code
   without a file falls through to the vanilla `_F`, `_MAM` or plain lookup.
4. **Effects**: the character load calls RegisterEffects at 0x16c821. KBO also registers each
   ported blast's owner there. Registering is by id and looks up first; Battle Styles does the same.

## Not done
* Prop bones: no game blast needs them. A mod blast animating `a_x_*` is logged; carrying the
  bones in the fit works, but the model part needs the BCS path Battle Styles owns.
* Real users of a blast with character effects show the owner's (21110: all five use TE1's).

## Check in game
Log `per-race skill EANs: patched`, `port <folder>`, `effects of chara N registered`, and the
file pipeline's note on the BAC (`animation and effect records carried`).

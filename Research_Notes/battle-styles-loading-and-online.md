# Battle styles: battle loading, online sync and presets

Covers: BattleStyles - the battle setup list, the character select
setup record that carries the style online (phase 2), which battle
player the CMS getter is loading, the character select preview,
presets in memory (EditMyset, 0x653830) and the equipped_styles.sav
sidecar. Overview: battle-styles.md.

## Battle setup list (online transport)
Global **0x156b558**: 14 player slots x 0x1F0, chara id at +0x70, costume
at +0x74. Writer **0x656020** `fn(list rcx, playerIdx edx 0..13, chara r8d,
costume r9d, [rsp+28] -> +0x78, [rsp+30] -> +0x7c, [rsp+38] -> +0x84,
[rsp+40] -> +0x88)`, caller 0x66b422. **+0x78 = save character slot**: the
story setup 0x3a87f0 passes the loaded slot there (0x828c60 -> 0x433e0,
`mov eax,[rcx+0x44]` on [0x1568310]+8). Its caller 0x66b370 is called from
0x3a8ac4 (story) and 0x668d16 (packed bytes - CSS/online?) and uses the
list at [0x156eb60].

### Character select setup record (phase 2 transport, 1.26.01)
Each local player's pick is packed into a **0x100-byte record** and sent
to every member; every machine (sender too) applies the received records.
* Packer **0x66abb0** (from the Select events 0x3aa9b3, 0x3aba37, ...).
  Its 4th arg is the CaC's preset, passed to 0x653b90(character,
  rec+0x2c, preset) at **0x66addd** (save-character path only).
* Send **0x66a9c0**(state, record, 0x100, 1), call site **0x66b2b6**:
  online via network vtable +0x1f8 channel 2; offline -> 0x66c970 ->
  0x66ca00. Poll 0x6687f0 reads every member's data.
* Receive **0x66ca00**(state, record) -> 0x66b370 -> writer 0x656020 at
  **0x66b422**.
* Record: +0 type word (0 player), +2 sender, +4 seq, +8 setup index,
  +9 kind (1 save character, 3 NPC), +0xc chara id, +0x10 costume (forced
  0 for CaCs at 0x66b28b, IsCaC 0x2900c0 = ids 100..109), +0x12 save slot
  (-> setup +0x78), +0x2c loadout, **+0xc0..+0xff name** (strncpy 0x40,
  zero padded, read back with strlen).
* Mod design (own, not the reference mod's costume-field patches): the
  sender writes its style's CMS code (<= 4 chars) into the last 4 bytes
  +0xfc of the name field when the name ends before +0xfc (CaC names are
  short). A vanilla sender leaves zeros = own moveset. The receiver hook
  keeps the code per setup index; the battle loader uses it when the
  entry's chara id + save slot match the record (loader player index ==
  setup index, guarded by that check). No record (story 0x3a8ac4) ->
  local save slot as before.
* 0x656020 is a leaf with a rel8 `ja` early, not in .pdata (da.py shows
  the wrong function - use count mode): hooked by **call-site redirect**
  (E8 rel32 -> near cave -> AbsJmp), same for 0x66addd and 0x66b2b6.

### Which player the CMS getter is loading (1.26)
**0x16c060**`(loader rcx, playerIndex edx 0..13, setup r8 or null)`, void,
only caller 0x169f9e. Loads one battle player; its entry is
loader + 0xe0 + idx*0x1f0 (chara +0, costume +4, skills as words from
+0x34; +8 presumably = setup +0x78 save slot). With r8 non-null and
[r8] >= 0 it first copies 0x70 bytes from r8 into the entry - so read the
entry after the call / lazily. It calls 0x113e00 at 0x16c544 and via
0x16c5f0 (0x16c3f2, same edx) at 0x16c873; 0x113e00 asks the wrapper
0x2c55b0 for fields 1 (0x113e74), 3, 4 and the 6/7/8/9/12 loop (0x114590,
mask 0x13c0). All on one thread -> a thread_local "loading player" set in
a 0x16c060 hook tells the CMS hook whose file it is. Entry bytes
`48 8B C4 89 50 10 48 89 48 08` (6 bytes stolen; 16-aligned).
Other getter users, all outside battle loading: 0x2b34c0 (1/3/4), 0x3711ac
(1, chara [rbx+0x24]), 0x298300/0x2988b0 (field 0, CharaResourceBase /
LobbyCharaResourceBase vtables 0xf8fa28 / 0x107fff0), 0x19e994 (0),
0x2496d8 (chara 0xe4, field 6).
More getter callers: 0x65be10 (model loader, fields 1/3/7 for chara
[obj+0x10c]; 11 callers - SelectHero, SelectCharacter, EditSlot,
PartnerCustomize, RecordMain, EditCharacter event), 0x460c60 (field 0),
0x7b2550 (1/3/4).

### Character select preview (SelectCharacter)
**0x5e7210**`(menu, chara, costume, r9) -> int`, only caller 0x5e7bba
(update 0x5e7820): loads the model under the cursor; 0x65be10 at 0x5e74b4
asks the getter inside it. For a CaC it reads 0x828b40(save mgr
[0x1568310], **menu +0x450**); 0x828b40 = save character `slot` (0x120d0 +
slot*0xe880, -1 = loaded slot), so +0x450 is the previewed CaC's save slot.
Entry `48 8B C4 57 41 54 41 55` (6 bytes stolen). First per-character
build showed every CaC in the CSS with the loaded slot's idle (log
"loaded character" for 100..107 while browsing); battle itself was right.
Confirmed in game (2026-10-05): the Offline Battle CSS goes through here.
SelectHero (0x5e7d10 / 0x5e8490 -> 0x65be10, model index [+0x4e8]) not
handled - unclear which screen it is.

Dead end: the reference's `[0x1681bc8]+8 -> +0x18` (1.26 0x1554408) is
used with call 0x2c8b40 in 1.26, meaning unclear - not followed.

## Saves
Preset block: 8 presets x 96 bytes at 125184 + 50888*charaIdx, every byte
used (Xv2CoreLib SAV_File.cs) - no room for a style id, hence the sidecar
file.

### Presets in memory (1.26.01)
* 8 loadout records of **0x68 bytes at character + 0xf4** (character =
  0x828b40(mgr, slot) then 0x20b50). **Record 0 = the main (current)
  loadout**, 1..7 the saved presets.
* Preset menu = class **EditMyset** (vtable 0x10af228, ctor 0x506280, iggy
  CMN_PRESET_PLT, msg myset_text; created from 0x3ddb90 and 0x44e930). AS
  callback slot 17 = 0x506880 -> 0x5071f0(menu, arg0, arg1 @+0x48):
  sendType 2 = load: 0x506f80(menu, n, 0); 3 = save: 0x506f80(menu, 0, n);
  1 = rebuilds the menu (0x4e8530, 0x3a0 bytes), 0 = fills the 8 names.
* **0x506f80(menu, from, to)**: copies record `from` over `to` of the
  loaded character (both >= 0, no upper bound check), then 0x847f00. Its
  only callers are those two. `ClaudeTools\battle_styles\presetrefs.py` lists
  every function indexing the records (imul 0x68 + 0xf4): the rest only
  read them (battle setup 0x653830, item-in-use checks 0x84bbd0/0x7444d0,
  EditSlot 0x509d90 runs 0x84dbc0 over all 8 when a slot is cleared) - so
  hooking the EditMyset callback catches every preset save/load.
* **Character select presets don't copy anything**: the loadout for a
  battle or preview is built by **0x653830(character, out, preset)**
  (negative = record 0, cmovns 0x653908; first bytes
  `48 89 5C 24 08 48 89 6C 24 10`). Story/most modes pass -1; the battle
  CSS stores the picked preset per player in [0x828ce0(mgr)] + 0xec +
  player*4 (setter 0x850dc0, called from 0x3ab8e0; getter 0x8505e0, read at
  0x38db7e..0x38db38 in 0x38cae0); the CSS preview 0x5e7210 passes its r9
  (0x5e7294). Character pointer -> slot: (ptr - mgr - 0x120d0) / 0xe880.
  da.py's caller list stops at 12, so 0x5e7294 is missing there; raw scan
  for `[reg+reg*4+0xec]` found the setter (leaf, no .pdata entry).
  Mod: hooks 0x653830 to remember each slot's last-built preset (battle
  player loader uses it) and reads the preview's r9.
* Mod: equipped_styles.sav (JSON text; was .json, renamed so tools that sync the mod folder treat it as save data) = Steam account (SteamID64
  as decimal; since 2026-10-06 read from the loaded save, [0x15681a8]+0x68+8
  XOR "thebecon" - save-flags-and-unlocks.md; was steam_api64
  SteamAPI_ISteamUser_GetSteamID, same value) ->
  save slot -> preset -> code; the equip slot and battles use preset 0.
  Old {"slot": "code"} files are read as preset 0 of the current account.

# Ki blast type (charged ki blast) selection

Covers: how the game picks a CaC's charged ki blast (race, super soul,
transformation), every place it is computed, where it lives in the loadout
and the online character select record, the names the game has for the
types, and the plan for an equip-it-yourself mod (static RE on 1.27, not
yet confirmed in game).

## How the type is chosen
**0x618d30(talisman id ecx, race edx) -> blast skill ID1.**
* Looks up the talisman in IDB type 7 (0x86aa60([0x1568308], 7), then
  0x85f180(id)). If entry +0x30 (IDB "Model", I_32 at offset 48 in
  Xv2CoreLib IDB_File.cs) is non-zero: blast = 0x437 + Model.
* Otherwise blast = 0x438 + table **0x10d6fd0**[race] = [0,0,1,1,3,4,2,2]
  for races 0..7 (HUM HUF SYM SYF NMC FRI MAM MAF); a race outside 0..7
  gives 0x438. Only xref to the table: 0x618d81.
* Then + 0x2c5b70(4) = 20000, the blast ID1 base (CUS: blasts are
  20000..24999, awoken 25000+).
* Result: CaC blasts 21080.. (0x438 + 20000). Super souls only use Model
  1..5 = 21080..21084.
* Transformations swap the whole skillset in battle (awoken-skills.md);
  that path does not go through 0x618d30.

## Callers of 0x618d30 (all 7)
| Call | Function | Context | Writes |
|---|---|---|---|
| 0x653b18 | loadout builder 0x653830(character, out, preset) | rdi = character, r12 = preset*0x68 (negative -> 0), r15 = out, talisman = 0x17aae0(record) = [rec+0x18] | word out+0xa2 |
| 0x653d55 | 0x653b90 (only caller 0x66addd, CSS packer) | rbp = character, r15 = preset record (preset = (r15 - character - 0xf4)/0x68) | [rsi+0x76] = record +0x2c+0x76 = +0xa2 |
| 0x6542c9 | 0x653eb0 (callers 0x3a889c story, 0x5e6345, 0x66b224 NPC) | rsi = a 0x653830-format loadout ([rsi] chara id - 100, word [rsi+0x22] talisman) | [rbx+0x76] |
| 0x38ba8c | 0x38b910(loadout, out) (AreaChampion; callers 0x38bdf0 after 0x653830, 0x38bff8 NPC avatar from 0x654830) | rdi loadout, [rdi]-100, +0x22 | out+0x42 |
| 0x66aed7 | packer 0x66abb0, save-CaC path (rsi = character) | when [r14+0x5c] != 0 (souls off) forces talisman -1 = race default | [rbp-0x1e] |
| 0x66b25f | packer 0x66abb0, the other souls-off branch (0x66b205; rdi = object from 0x27a330, dword +0 = CMS id) | same, talisman -1; thought NPC only, unconfirmed, so Ki Blast Override applies its pick when rdi is a save CaC | [rbp-0x1e] |
| 0x4f609d | 0x4f6070, CSDetailWindow slot 9 | returns blast - 20000 - 0x438 (type index) for a talisman; 0x4f52b4 / 0x4f52db send it as user data 0x2f to AS | - |

## Loadout layout (0x653830 out)
* +0x18..+0x22: IDB part ids (0x61af70); +0x22 = talisman.
* +0x94..+0xa5: 9 skill words from preset record +0x44 + i*4, CUS
  skillset order (4 supers, 2 ults, evasive, blast, awoken); index 7 =
  **+0xa2 = blast**, written by 0x618d30 after the copy.
* Preset records: 0x68 bytes at character + 0xf4 (battle-styles-loading-
  and-online.md "Presets in memory").

## Online
The 0x100-byte CSS record (packer 0x66abb0, receiver 0x66ca00 -> 0x66b370
-> 0x656020) carries the loadout at +0x2c, so the blast word is at
record **+0xa2**. No 0x618d30 call is on the receive path, but the battle
setup re-derives the word from the soul (next section), so a vanilla receiver
does NOT use the sent blast: every player needs the mod, or peers see the soul
blast (possible desync). Risk: a receiver without that blast skill installed
(modded blast) - the mod's receiver should fall back to the race default for
an ID1 its CUS lacks. Name-tail bytes stay free: +0xfc..+0xff Battle Styles,
MultiAccessory uses the appearance block (multi-accessory.md).

## Battle setup overwrite (0x92d00, 0x92dc0)
Why a correct loadout word still gave the soul's blast in battle.
* **0x92d00(dst, src)**, 14 callers (Select event, BattleNormal, AreaChampion
  path...): copies 0x1b9c bytes to dst+0x10. If souls are on (`[dst+0x6c]==0`,
  same flag as the packer's `[r14+0x5c]`) it loops over 14 entries (stride
  0x1f0, first at dst+0x74; chara +0, blast word +0x42, talisman +0xb4). For a
  CaC with a talisman whose IDB type 7 Model is 1..0xb the word becomes
  `0x2c5b70(4) + Model + 0x437`. 0x92d87 is the `call 0x2c5b70`; rbx = &word,
  edi = Model.
* **0x92dc0(dst, idx, src)**: the same with no souls-off check, only called from
  `Event::TtlBattleInit` 0x8eb8a0. Its source 0x89f5b0 calls no pick, so the
  word there is raw and this overwrite is what installs the soul.
* 0x905fd0 calls 0x89f5b0 and then 0x92d00, so raw words reach 0x92d00 too:
  "keep any valid word" would change vanilla there. The mod keeps only words in
  a 16-entry ring (ours from the pick sites, others' from received CSS records,
  by redirecting 0x66ca00's callers 0x66c83a and 0x66c9e8).
* Scan for 0x437/0x438 blast arithmetic: only 0x92d33, 0x92ecd, 0x618d30, UI
  0x4f60b0.
* Limits: tournaments (0x92dc0) stay vanilla. A raw word in 0x905fd0's path that
  happens to match a ring word (a player's race default) would be kept.

## Roster blasts blanked before the setup (CUS race lock)
* Symptom: Hit's 21260 / 21350 on a CaC with a bomb soul. The log shows the online
  record with 21260, then `battle setup: blast 65535 replaced (soul gives 21082)`;
  GUNN's 21590 is `kept`. So the word becomes 0xffff between the record and 0x92d00,
  and 0x92d00 then installs the soul.
* Correlation: CUS entry byte +12 (race lock, Xv2CoreLib `I_12` / CusRaceLock) is 0xff
  on the blasts seen working (21080..21090, 21590) and 0 on Hit's and most roster ones.
  Hypothesis: the game blanks a blast whose lock excludes the CaC's race. Ki Blast
  Override sets it to 0xff on every blast through a loader stage on
  `system/custom_skill.cus`. Log `battle setup: blast <id> kept` decides it.
* Dead ends: the writer of the 0xffff wasn't found. Static scans for `byte ptr [r+0xc]`,
  `bt`, `mov word ptr ..., 0xffff`; read 0x6553e0, 0x66a430, 0x666610, 0x6fa960,
  0x8b4930, no match. Unchecked: 0x84ec39 (fn 0x84e690), 0x863683, 0x8661b4, and
  0x656020 / 0x66b370 on the receive path.

## Blast names
Where a list row's name can come from, best first (`BlastOwner`; check: `ClaudeTools\kiblast\blast_mod_names.py`):
* The owner: CMS entry ID2 / 10, `chara_<code>_...` in `proper_noun_character_name_en.msg`. Roster blasts
  have ids under the CaC range too (20340 Videl, 20400 Broly, 20520 Hercule). Mod skills share the nameless
  owner XZT (159).
* XV2 Mods Installer: `skill/BLT/<folder>/X2M_SKILL.ini` holds the x2m's GUID, `data\InstallData\<GUID>.x2d`
  is a zip of it with file stubs (x2m.xml deflated, `MOD_NAME`, `MOD_AUTHOR`, `SKILL_NAME_EN`). Old x2m.xml
  files are Latin-1, not UTF-8. Reader and offline check: `src\x2d_read.cpp`, `ClaudeTools\kiblast\x2d_read_test\`.
* An awoken skill: a CaC race's skillset (chara 100..107, preset at +26) with the blast, awoken CHANGE_SKILLSET
  1..3 (+76, i16) = that preset, race lock allows the race; name `met_skill_<ID2, 4 digits>` in
  `proper_noun_skill_met_name_en.msg`. Installer-package transformations get their blast this way.
* First skillset user: least reliable. A skillset can name an id the installer later gave a mod: the installed
  CUS has chara 149 (Jiren) costume 1 on 21590 and chara 148 costume 1 on 21591, neither from the base cpks.

## Which roster blasts differ
* Files flag +14 bit 0x4 (Xv2CoreLib `FilesLoadedFlags.Eepk`) and EEPK path offset
  +28. BACs differ byte-wise for every roster blast, so hashes don't separate them.
* Rule for "just a standard type": code POW/RSH/BOM/PAL/HOM and either no effect file
  or an EEPK path into a CMN skill (21140 -> `BLT/1082_CMN_BOM`). 15 of the 23 roster
  blasts: 20600 20630 20650 20750 20830 20860 21110 21140 21180 21260 21350 21360
  21510 21560 22210. Kept: 20340 NML, 20400 BOM (own effects), 20520 ISI, 21120 STP,
  21550 BOM (`BLT/400_BRL_BOM`), 21590 GUNN, 22060 GLO, 22070 GLT.
* The rule looks at effects only: a hidden blast could still differ in animation or
  damage data. `ClaudeTools\kiblast\blast_filter_check.py` applies it to the game's
  CUS, `blast_cus.py` dumps the fields.

* Race lock: confirmed by the log (`battle setup: blast 21120 / 22060 / 20400 kept`).
* In game STP (21120) is Homing with other face animations and longer knockback on
  some hits. Own files for a roster blast do not mean it plays differently.

## CaC animations (the endless firing, and the conversion)
* Symptom: 20400 BRL_BOM, 22060 GLO, 22070 GLT on a CaC: the charged shot never ends.
  (GLO and GLT then got a different fix, see the GOA / GTA bullets below.)
* Cause (static data, the fix is untested in game): the skill's BAC plays `skill:0/1`
  from an EAN the CaC can't load. With files flag 0x800 at CUS +14 and no EanPath the
  game picks `<folder>_<CMS code>.ean` for that character, else `_F` (female races),
  `_MAM` (male Majin), else plain. Evidence: 21080 `1080_CMN_POW` has flags 0x8a11 with
  `.ean`, `_f`, `_gok`, `_mam`. 20400 has only `_brl`; 22060 `_fio/_pr1/_pr2`; 22070
  `_fit/_pr1/_pr2`.
* Ordinary shots are not the problem: `CHARACTER:40..49` ids exist on every race
  (base ids 0..81 are shared, HUM_ATC_SHOT_* = FIO_ATC_SHOT_*), TM1_BOM does the same.
  Limit: a modded BAC using CHARACTER 82+ isn't handled (see MAS under Names).
* Fix (Ki Blast Override): when such a blast has no plain file, write fitted `_F` (HUF
  skeleton), `_MAM` (MAM), then plain (HUM) as loose files, plain last since it marks
  the blast as done. The owner keeps its suffix file. `FitEanToRace` with `styleOnly`
  keeps the ids (no race slots, no tail). Rejected: EanPath in the CUS (no single
  owner fits all races), hooks.
* Offline check: `ClaudeTools\kiblast\fitcheck` + `blast_ean_export.py` +
  `blast_ean_names.py`: BRL_BOM fitted to HUM has the same animation count, frames,
  nodes and root as `1082_CMN_BOM.ean`.
* 21089 GOA / 21090 GTA are the game's own CaC ports of 22060 GLO / 22070 GLT: own
  `CMN_BLT_GOA_*` EANs (11 animations), effects from GLO's and GLT's folder through the
  EEPK path (CUS +28 `BLT/2060_FIO_GLO`). The BSAs match except for one extra record per
  charged projectile (holding floats -0.75 and 0.5, probably a spawn offset; guessed from
  the bytes, not decoded) and the skill id.
* An EAN can't make GLO right on a CaC, whatever the fit (static data, seventh build's
  in-game look had no EAN at all): its ordinary shots play `CHARACTER:40..49`, the CaC's
  plain shot poses, with spawn bone 0x11 and muzzle effect 10 on `g_C_Head` (GOA: 0x16,
  effect 0 on `g_R_Hand`); only the charged shot is a skill animation. The gun is probably
  the projectile's EMO on bone `TRS` (effects 1, 2; assets not opened), which would put its
  place in the BSA.
* So Ki Blast Override hides a roster blast when a CaC blast's EEPK path names its folder
  (`ClaudeTools\kiblast\blast_ports.py`: exactly GOA and GTA) and redirects saved picks.
  Compare with `blast_bac.py`, `blast_eepk.py`.
* Dead end: the first conversion build wrote nothing, `GameDataDir` went up four folders
  instead of three. The log says `convert ...: cannot write`: check it before reading
  anything into an in-game result.

## Porting a blast (static reading, in-game test pending)
* Why: a blast's BAC can ask the user's own character EAN and EEPK. Ids below 82 are the same base
  moves on all races; from 82 on each has its own (a Namekian's are Giant), so Kid Buu's 21086 on a
  Namekian played Giant poses and lost its effects. Only 21085 GBM, 21086 MAS (animations),
  21086, 21110 RSH (character effects) do this in vanilla; none animate prop bones.
  `ClaudeTools\kiblast\blast_port_survey.py`, `blast_port_sources.py`.
* Skill EAN gate, fn 0x260ba0: `test [rax+0xc],0x4000` at 0x260c27 (CUS +13 flag 0x40, Xv2CoreLib
  `EAN`) off = no EAN at all, and a BAC asking a Skill animation leaves the character running in
  place (ninth build). Off in vanilla: NML, ISI, GBM, MAS only. Called from 0x16cdb2, 0x19f060.
* Skill EAN path, fn 0x260ba0: `test [r14+0xc],0x8000000` at 0x260d72 (CUS files flag 0x800, its
  only reader in the exe), then `call 0x2900c0` at 0x260d83 (`ecx-0x64 <= 9`: chara 100..109). On 0
  it tries `<base>_<code>.ean` (code from 0x28ff60, exists check 0x2dca90); missing or created
  character falls to `_F` (gender byte, CUS +13 flag 0x80), `_MAM` (race 4, files flag 0x1), plain.
  Other callers of 0x2900c0 (0x2625d1, 0x262971, 0x262ce1) are other paths, left alone.
* No vanilla blast lacking flag 0x800 has a per-character EAN (25 with the flag, 10 without),
  so setting it on all changes nothing until a file exists.
* Created characters are CMS 100..107: HUM HUF SYM SYF NMC FRI MAM MAF. MAP (Kid Buu) is 108: inside
  0x2900c0's range, outside Battle Styles' CaC range (its effect swap doesn't touch 108).
* Effect record (BAC type 8, 48 bytes): EepkType u16 +8, effect id i32 +16; dword +0xc reaches
  SpawnEffect 0x965080 as the chara id (`mov r9d,[rdi-0x10]` at 0xf9820). Vanilla -1 = ask the
  spawning character; a real id uses container (2, id) directly.
* RegisterEffects 0x9644b0(manager, type, chara, 0, 1, 1): looks up 0x965220 first, found = refresh
  via 0x9669c0, no new container. Same args repeat in a mirror match, so repeating is game-native.
  38 callers; a character's own load is 0x16cf26 (fn 0x16cee0, per battle slot, from 0x168acb) and
  0x16dadb (fn 0x16d900, caller 0x169fb9 in fn 0x169d10), both with the
  slot's CMS id at +0xe0. 0x16c821 (fn 0x16c5f0) registers an awoken skill's form: no plain battle
  reached it (ninth build's log). Battle Styles' load sites are 0x16c544, 0x16c873.
* A transformation's own blast is loaded in advance (static reading, not confirmed in game): fn
  0x16d900 loops r15 = 0..2 over an awoken's forms, `0x2c5870(cus, chara, costume, preset)` at
  0x16dd58, then 0x16cd30 for all 9 slots at 0x16dd88 (-> 0x260ba0 at 0x16cdb2, the same gate as a
  normal blast). So a preset's blast Skill EAN goes through the port path too.
* **Transform swap: fn 0xe7a00** (Mob vtable), `call 0x2c5870` at **0xe80c2**, r9 = the awoken's
  preset. rbx there = the fighter's entry (0xe7ab5: loader + 0xe0 + idx*0x1f0, the entries 0x92d00
  walks: chara +0, costume +4, skills +0x34, blast +0x42, talisman int +0xb4). Loop esi = 0xc..0x14:
  transformed reads the returned skillset +8+2k, revert reads the entry's words. A word of 0xfffe
  fails `ebx < base - 1` at 0xe8139 and skips 0xd86e0, so the slot stays. The returned pointer is
  used only in this loop. The mod's option returns a copy with blast = 0xfffe when the entry's word
  is neither the soul's nor the race's; only the entry decides, so all players agree.
* EAN layout: count u16 @18, skeleton i32 @20, animation table i32 @24, name table i32 @28; table
  offset 0 = empty slot; blocks are relocatable, keep offset mod 16 (movaps).
* CUS skillset blast slot holds the blast's ID1 (not ID1 - 20000); skillsets repeat per costume.
* Folders of 21080..21091 are `<id2>_CMN_<code>`; ID2 / 10 = 108 there is a coincidence (MAP).
* Skill slots of ported animations must not count the mod's own files. The tenth build counted its
  own `_MAM`/plain EAN on the next launch, so the BAC asked 5..9 (MAS) or 6..11 (GBM) while the race
  EANs still held 0..4 and 0..5: T-pose and a downward shift. The count now skips our files on
  blasts without the 0x40 flag; on flagged blasts `_F`/`_MAM` can be real, so they still count.
* Effects also come through a projectile, and that path is not a BAC effect record. BSA type 6
  reader 0x258770..0x258795 (again 0x25924e..0x259277): `movsx r9d, word [rdi+2]`, `movsx r8d, word
  [rdi]`, effect `movsx eax, word [rdi+4]`, flags `[rdi+8]`, so a record's u16 id goes to SpawnEffect
  as is, 65535 = -1 = the spawner. Vanilla also names characters there: CMN entries 180 `(2,150,20)`,
  321 `(2,211,1)`. `ClaudeTools\exe\spawn_args.py` lists SpawnEffect's callers and their sources.
* BAC projectile record (type 9, 48 bytes): skill u16 +8, BSA type u8 +10, entry u16 +12; type 255 +
  skill 65535 = `data/skill/CMN/CMN.bsa` (10022 slots, 220 used, free 473..9999, last used 10021).
* The only vanilla blast using a spawner effect that way: `1086_cmn_mas.bac`, direct entry 120
  `(2,65535,10)`, Kid Buu's leg on the enemy (no link chains; none of the 16 blast-owned BSAs use
  Character effects). The entry is shared, so a port copies it to a free slot (from 9999 down) naming
  the owner and points the record there. An x2m on a Majin gets this for free from the race ERS,
  which doesn't help a Namekian. `ClaudeTools\kiblast\bsa_character_effects.py`, `check_bsa_copy.py`.
* BSA layout (Xv2CoreLib BSA/Parser.cs): count u16 @18, table u32 @20 (0 = empty); entry: links
  Expires/Impact* u16 +26..+32, sub-type count u16 +34, types offset u16 +36; offsets relative to
  the entry, so an entry is relocatable as a block; sub-type header 16 bytes: type u16, records u16
  +6, header offset i32 +8, data offset i32 +12, both from that header; type 6 record 12 bytes:
  EepkType u16, id u16, effect u16.
* Dead ends: reading Xv2 XML through the serializer (gave none, raw parse is simpler);
  `data/shot/cmn/cmn.bsa` doesn't exist.
* RSH (21110) was missing from the list: its code is a standard type, so the copy filter hid it
  before anything looked at its BAC. A standard-coded blast taking things from its character now
  lists; `1110_te1_rsh.bac` is the only one in vanilla. Users E21, G21, J21, M21, TE1; effects from
  TE1 (`data/vfx/chara/te1/te1.eepk`), its animations are all below 82 and stay the user's.

## Names
* Modded blasts: X2M_SKILL.ini holds only a GUID and blasts rarely have a MSG name, so
  a row is `<character> - <type or code>`: the owner's roster name, else the first CUS
  skillset whose blast slot uses it (count +8, table +0xc, 32 bytes each, character
  +0, skill ID1s u16 from +8, slot 7), else the owner's code.
* menu_shop_text: ITM_STR_01_007 "Ki Blast Type", ITM_STR_01_008 "Your Ki
  Blast Type"; ITM_STR_02_015..019 "Power / Rush / Bomb / Paralyze / Homing
  Ki Blast" (order differs from Model: Model 4 = HOM 21083, 5 = PAL 21084 -
  match by short name, not index).
* Blast CUS entries have no msg names. Installed 1.27 + Revamp: 35 blasts.
  CaC 21080..21091: POW RSH BOM HOM PAL GBM MAS HRS ISA GOA GTA MGA; roster
  and mod ones e.g. NML 20340, BOM 20400, ISI 20520, POW 20600, PAL 20630,
  HOM 20650, STP 21120, GUNN 21590, GLO 22060, GLT 22070, PAL 22210. Owner =
  CMS id ID2/10; its name = first `chara_<code>_` key of
  `msg/proper_noun_character_name_en.msg` (mod characters without one: the code).
* The seven CaC specials, from who selects them (souls: Model in the talisman
  data; else CMS presets / awoken SkillsetChange):
  21085 GBM Golden Frieza (FRI preset 1, awoken GLD; files kidan_basfrz);
  21086 MAS Kid Buu (MAM/MAF preset 1 Purification, MAP preset 0);
  21087 HRS Ribrianne (soul 440, Model 8); 21088 ISA Hercule (soul 490, Model 9);
  21089 GOA Gamma 1 (souls 807/808/818, Model 10); 21090 GTA Gamma 2 (809/810/819,
  Model 11); 21091 MGA Ultra Instinct (all races preset 2; awoken MGT, MGQ).
  Models 6 and 7 have no souls.
* MAS on another race shows no foot or portals: `1086_cmn_mas.bac` plays Character
  animations 96..100 (MAP.ean) and Character EEPK effect 1 (MAP.eepk Leg_Warp), and
  only Majin CaCs load those (`awoken-skills.md`). Any blast using Character
  animations has this caveat. The fix would be a copy with own files, as
  Refinement's package does.
* Found with XV2DataEdit (`Game(find_game())`, TABLES['skill'/'item'],
  xv2data.msg.parse_msg_entries), `ClaudeTools\kiblast\blast_users.py` and
  `blast_files.py`.

## Menu rows (equipped blast in the Presets panel and the skill popup)
* Channel: the DLL appends `"\t<label>"` to the first skill-name user data string,
  only when `MenuEdited` says our script was served; the script splits it off.
  Battle Styles uses the last slot of the same menus the same way, so neither
  depends on the other.
* AVATAR_SET (EditMyset slot 17, sendType 0 fills preset n): skill string 9 =
  `ReceiveType_SkillStrStart`, drawn on the row whose `SkillItemArray[i][0]` is 0
  = `sys_sk_b_06` (not _01).
* CHARASELE: 0x5e62d0(menu) fills the popup; only site is 0x5e54b8, an **e9 tail
  jump**, not a call. Skill names start at user data 0x2120 (`lea edx,[rsi+0x2120]`
  at 0x5e6a79). `SetUserDataStr` 0x2c8340 indexes the vector at +0x160 directly.
* Sources: `ClaudeTools\kiblast\kbo_as3.py` rebuilds `as3\AvatarMyset.as` and
  `CharaSele*.as` (single-call edits, no initialised members, so they merge).
  The rows report where they went: sendType 9911 with a string, logged by the DLL.

## Mod design (KiBlastOverride; battle effect not yet confirmed in game)
* Two builds set 21590 (GUNN) in the loadout, its copies and the online
  record, yet the battle blast stayed the soul's: the overwrite above. 0x92d87
  is redirected now. Open: does a roster blast work on a CaC at all (next test,
  log `battle setup: blast`)?
* Two special picks: Default (-1) = vanilla, souls decide; Race Default (-2)
  = 0x618d30 with talisman -1, whatever the soul.
* Don't hook 0x653830 itself (Battle Styles hooks its entry, MultiAccessory
  redirects its 38 call sites). Redirect the 0x618d30 call sites (E8 rel32
  -> near cave -> AbsJmp) instead, passing the context registers.
* 0x653b18 / 0x653d55: equipped blast for (save slot, preset) else vanilla;
  remember out pointer -> blast. 0x6542c9 / 0x38ba8c: use it only when the
  loadout pointer is remembered and word +0xa2 still equals it (NPC
  loadouts and stack reuse stay vanilla). 0x66aed7, 0x66b25f: apply ours
  too. 0x4f609d (detail window) stays vanilla.
* Sidecar like equipped_styles.sav: SteamID64 -> slot -> preset -> ID1;
  EditMyset slot 17 chained for preset copy.
* UI: the Skills page (mode 4) uses all 9 edit_menu buttons (btn_1..btn_9;
  btn_9 = Battle Styles' row), the cloth tab's spare buttons are Multi
  Accessory's columns. AvatarMyset-style text copies (copyRow) work for
  labels but not for a selectable button.

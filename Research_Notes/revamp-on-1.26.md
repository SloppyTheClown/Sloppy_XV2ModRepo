# Revamp Xenoverse 2 5.1.1 on 1.26.01 (XV2_RevampBridge)

Covers: why Revamp's installer fails on 1.26, how the bridge rebuilds its
Xv2CoreLib fork, the Epatch (exe patch) port table 1.25.02 -> 1.26.01, the
content-level conflicts (rvp.cpk, msg, iggy textures, EMB), install history
on the PC and what is still untested. Read this before porting Epatches to a
new game version. Texture formats: ui-iggy-and-textures.md.

Revamp targets 1.25.02 / XV2Patcher 4.64. Reference: a 1.25.02 game copy
(exe + cpks; Steam console `download_depot 454650 454651
7238901667150908628`).
`Revamp Xenoverse 2 - 5.1.1\` in this workspace is the untouched download -
never edit it. `src\` and `tools\` paths below are in `XV2_RevampBridge\`.

## Why its installer fails
* LB Mod Installer fork (.NET 4.7, Costura-packed). Its embedded Xv2CoreLib
  is a Revamp fork that cannot parse 1.26: IDB entries 784 bytes (fork knows
  720/748/772), QXD character entries 132 bytes, CST entries 60 bytes,
  XV2P_SLOTS.x2s 11 fields (xv2ins/XV2Patcher 4.7 added `flag_kfk`). Current
  LB Mod Installer (3.32) has all four readers.
* Upstream Xv2CoreLib cannot replace the fork: the fork adds
  `HCI_File.Overwrite` / `InstallEntry(entry, bool)`, EMS stage-image install
  (`Time` is a string so it can hold `{AliasLink=...}`), IInstallable on
  EMS/FMP. The bridge rebuilds the fork from ILSpy output with the four
  upstream readers swapped in.
* ILSpy artefacts to fix: `Half.op_Implicit`, an `Unsafe.As` cast, an
  `ImageFormat` ambiguity, a DxgiFormat switch, a lost
  `[field: NonSerialized]`, and object-typed attribute arguments losing their
  type (`DefaultValue = (short)-1` became int; YAXLib then fails on EAN/CUS -
  broke an install). Fixed from attribute dumps (`src\attrdump.cs`); the build
  compares every attribute afterwards.
* Loading: Xv2CoreLib is 32BITREQUIRED -> load from bytes (like Costura), not
  LoadFrom. `Application.ResourceAssembly` refuses with an entry assembly ->
  set `Application._resourceAssembly` and
  `System.Windows.Navigation.BaseUriHelper._resourceAssembly`. The installer
  opens `<ProcessName>.installinfo` from the CWD.

## Epatches - ported
Revamp's code goes into the zero slack after the Steam stub's `.bind`
section (1.25.02 `0x1B77448..0x1B78000`, 1.26.01 `0x1A1E248..0x1A1F000`,
executable) with hard-coded rel32 jumps. On 1.26 most patterns are not found
and two match wrong places -> crash. Every port checked by simulating
XV2Patcher on the image, then by a live dump (`tools\epatch_check.py`,
`verify_epatches.py`, `port_epatches.py`).

| 1.25.02 | 1.26.01 | note |
|---|---|---|
| caves 0x1B775xx.. | 0x1A1E5xx.. | block moved by -0x159000 |
| global `[0x1681C08]` | `[0x1554448]` | 24/24 xrefs agree; offsets into it (+0x1E498 scratch, CaC records +0x1E8B4 x 0xC6C8) unchanged |
| "CABA_ALL" 0x11BD518 | 0x1101CE0 | |
| Part1 0x2B1500 (fn 0x2B1110) | 0x299DFC (fn 0x299A00) | index r12->r14, chara [rsp+0x68]->[rsp+0x70]; a 2nd copy (fn 0x29A060 = old 0x2B1900) was never hooked |
| Part2/6 host 0x2B49D0 | 0x29D290 | caller's index at [rsp+0x48] (was r12 at 0x50); chara 0xD8->0xE0 |
| Part3 0xC5BAB | 0xCA684 | rdi->rbx |
| Part4A 0x1703D3 | 0x16C16F | edi/r15d -> ebx/edi, 8 bytes |
| Part4B/5/CusAura1 0xFCA74.. | 0xFD411.. | movss interleaved, r14->r13; trampoline 12 bytes |
| body change 0xFD7A7 | 0xFE389 | r14->r13 |
| CusAura2 0xE3575 | 0xE7990 | movsx eax,[r12+3A] now before the flag test (1.25 Revamp bug: the call clobbered the flags of the next je - not reproduced) |
| CusAura3 0xC3E16 | 0xC8866 | identical |
| BAQ vox 0x853EDA | 0x7F2184 | identical |
| title BGM 0x6343F5 / 0x5ADD07 | 0x60721D / 0x58751B | `lea ecx,[rcx+1]` -> `lea ecx,[rdx-0x52]` (rcx not guaranteed 0) |
| colour fix 0x88FEFE | 0x83213E | int3 padding at the same +0x66 |
| partner code strings 0x113FE50 | zero block 0x10985D0 | unreferenced, same 64 KB as "RSV" 0x10981A4; 1.26 dropped VTB |
| partner rows 0x15D6018.. | 0x14A7518.. | row constant 0x45->0x44, terminator changed, still 42 RSV rows |

Battle character struct grew 0x20 above +0xC4 (0x22CC->0x22EC,
0x11B0->0x11D0); chara id stays at +0xB4.

## Content conflicts and the post-install fixes
* `data\rvp.cpk` (JUNGLE1) = 1.25.02 data2.cpk + movie/REVAMPINTRO.usm +
  data/demo/A00_01/A00_01.dem; AddRevampCPK loads it instead of data2.cpk.
  1.26 changed data2/config.dat (patchVersion 38->39, new consumerSecret) and
  the sysfont files. Rebuilt in place keeping Revamp's CpkMaker-1.36 tables
  (FileOffset relative to ContentOffset, TOC/ETOC after content, CRILAYLA with
  blanked magic).
* Character image: exe knows TU1..TU22 and reads the newest. TU22 = TU21 + 40
  inserted rows (state 32, chars 232-240) - 1. HCI EmbIndex is per character.
* ID collisions (`tools\idcollide.cs`, 2370 sections): BTL_CMN.eepk effect 54,
  CUS skillset 141_0_1 (VTB), GKS.bcs part set 1, MenuCostumeCustomList.oco
  partner 7 option 1 / partner 24 option 3. Voice cues are allocated after
  vanilla (no collision). 480 merged-into files changed in 1.26; merging into
  newer vanilla keeps the new content.
* data\msg: Revamp ships 963 whole 1.25-based files (224 lack entries vanilla
  1.26 has; pause_text lacks 19 that 1.25 had). Post-install step
  `Msg.AddMissing` appends every missing vanilla entry by name (keeps the
  vanilla id unless taken) - 2669 entries into 224 files. Fixed "Unknown
  Character" / "Missing MSG Entry" for 1.26 characters and the training
  settings text.
* msg line parameter count (string record's 4th u32; vanilla 0/1/2): Revamp's
  installer AND the bridge's old writer wrote 0 on every line, so the game
  showed format text raw - "You got TP Medals x %ls!", Mekuji's
  `SET_MSGTALK_PS` item request (reported 2026-10-04). 1642 lines in 53
  files (107 per lobby_text_* language). Not derivable from the text
  (vanilla "Body Type %d" has 0), so `Msg.RestoreParams` copies vanilla's by
  name + line index where the format specifiers match. Applied on the PC
  2026-10-04 (now 0 lines differ); user-confirmed in game 2026-10-05
  (TP medal pickup, Mekuji's item request). Found by
  diffing the raw records - the text itself was identical to vanilla, and
  the keys are not in the exe (they come from TSR scripts / iggy).
* Textures (post-install step `IggyTex`, data-driven by `src\iggymaps.txt`,
  both directions): TITLE_*, CMN_MENU_BATTLE_*, sysfont01 (_ab.iggyted,
  _cyex.iggytex, _kr/_tw.iggyted, _zh.iggytex), LOBYHUD.iggyted; CHARASELE
  textures renumbered to 1.26 (xv2ins's .iggy) keeping Revamp's art. Decides
  a loose .iggy's numbering from its id table (`tools\iggyids.py`). Writing
  old-numbered copies of ITEMLIST_DETAILS / STAGESELE was WRONG (they paired
  with xv2ins's 1.26 .iggy) - removed.
* Tools: `changedvanilla.py` (loose files whose vanilla changed 1.25->1.26:
  754), `iggytexdiff.py`, `iggypairs.py` (checks every texture file against
  the .iggy the game pairs it with), `msgdiff.py`, `embdiff.py`,
  `lobbyquests.py`, `ddspng.py`.
* Lobby skill emotes (Kamehameha, Special Beam Cannon, ...): Revamp 5.1.1's
  `chara\CMN\CMN.{HUM,HUF,MAM,NMC,SYF}.lby.bac` are built from an older game
  - entries 416-419 (serializer `Index` 410-413) are empty, though 1.25.02
  and 1.26 vanilla both have them (Revamp's MAF/SYM/UNQ files are fine; FRI
  is vanilla). Using one in an online lobby froze that player forever, even
  alone (reported 2026-10-04). Post-install step `Bac.AddMissingLobby` copies
  vanilla entries that are empty in the loose file (type table + item blocks
  appended; block size = distance to next offset in vanilla), self-checked,
  and verified with the XML serializer (new entries == vanilla, Revamp's
  kept). User-confirmed in game 2026-10-04 (PC + Deck online). A scan of
  all 83 loose BACs with a vanilla counterpart found only two other gaps,
  `cl2_player.bac` entry 476 and `167_VGT_GLX.bac` 6/11 - most likely
  deliberate Revamp rework, left alone.
* Lobby boost-flight stutter (reported 2026-10-14, offline, PC): holding L2
  in Conton City, the player's movement (not rendering) stops briefly every
  ~5 s; plain flight/walking fine. Absent in vanilla, present with the
  patcher + Revamp; the user first saw it right after installing Revamp
  alone. Not a DLL mod: pausing Combined Supers' scan thread live
  (`ClaudeTools\modthread.ps1`) changed nothing, the other mods have no flight
  code. `XV2_RevampBridge\tools\lbybacdiff.py` (lobby BAC vs cpk, per entry):
  every Revamp race file (HUM HUF MAF MAM NMC SYF SYM; UNQ only adds 700+)
  differs from vanilla in entries 10/11/12, type 2 (Movement) only -
  Direction Z and Drag Z all x1.4 (faster flight). Entry 11 = boost loop
  (anim 63 CMN_LBY_BOOST_F_01, 250 frames ~4.2 s): item 0 frames 0-20 Z
  -0.546 drag 0.0106 (Revamp -0.765/0.0148), item 1 frames 20-250 Z -0.351
  (Revamp -0.491); entry 10 Z -0.075/-0.3 (-0.105/-0.42), entry 12 Z -0.15
  (-0.21). Revamp's lobby EANs (HUM/HUF/MAF) keep anim 60-64 lengths
  identical to vanilla. Suspect: the stall comes once per entry-11 loop, at
  the restart burst at Revamp's speed. **Confirmed in game 2026-10-14:**
  `tools\lbyflight.py <game> vanilla` (vanilla type 2 in entries 10-12,
  backups `*.lby.bac.revamp.bak`; `undo` restores) - freeze gone. So
  Revamp's x1.4 lobby flight speed is the cause. Exact mechanism (which
  item, why speed stalls movement - maybe a map collision/streaming check
  outrun per loop) unknown. Backups deleted at the user's request.
  PostInstall now does the same (`Bac.VanillaLobbyFlight`, after
  `AddMissingLobby`); built, not run through an install yet. The user
  plans to add faster flight back themselves. Untried: keep only Revamp's
  cruise speed (entry 11 item 1) and revert the burst/take-off/stop.
  Likely mechanism (static only): the lobby physics step's teleport guard
  drops the velocity to x0.025 when one step's move delta is too long
  (lobby-physics-and-debug-menu.md "Flight speed"). General_QoL
  `lobby_flight_speed` scales after that guard instead.
* BAC layout: "#BAC", u32 count at 8, entry table at [0x10]; entry 16 bytes
  {u32 flags (0x80000000 = empty), u16 type count, u16, u64 type table};
  type header 16 bytes {u16 type, u16 items, u32, u64 items offset}.
* Same-name files 1.26 added that Revamp also ships (Revamp's wins): vfx/cmn
  XBG_Aura, XBF_Aura, AuraLoop_XBG; vfx/demo/DM_CMN BHole; skill/ULT
  2130_SVE_UE1_G18 and 2140_SVV_UV1_VDL animations.
* Vanilla 1.26 names XBG plain "Goku"; "- Ultra Supervillain (Quelled)" is
  chara_KFK_000, likely appended for slots with the KFK flag - not checked.

## Revamp content facts
* Revamp's button icons (sysfont01_ab*) and CHARASELE_en.iggytex hold BC7
  textures: fine on Windows, garbled/blank under Proton unless Microsoft's
  d3dx11_43 is used - see loader-and-steam-deck.md.
* Quest access (lobby_event_manage.tsd): after CTP_11_01 Bulma -> CTP_RVP_01,
  Caulifla -> BAQ_RVS_THC; then Android 21 -> CTP_RVP_02; then Supreme Kai ->
  Victory Road (OSQ_TMT_01..25). Partners 30-39: GGK GVG SHN S17 THC TE1 PR1
  PR2 EVG GKS. Roster slots compared by CST costume index looked missing -
  wrong, Revamp renumbers; none missing.
* Change summary + test checklist: `XV2_RevampBridge\Revamp 5.1.1 - What It
  Changes.pdf`.

## State (PC)
* Installed on the PC 2026-09-30 (defaults: no skin tones, no-glow off). A
  first attempt interrupted at 98% was moved to `<game>\data_revamp_partial`.
  On the Deck too since 2026-10-04. While testing, the other mods were disabled by renaming
  their folders to `_<name>` - check `XV2ModLoader\mods\` for the current state.
* User-confirmed: intro, title, title music, lobby map, equip screen, char
  select, stage select, training settings, 1.26 names, new BGM options and
  stages, TMQ "Training in the Arctic", Goku SS1-3 multi-stage transform with
  voices, 1.26 costumes for Goku/Trunks, Cooler/Bardock.
* Online (PC + Deck, both Revamp) user-confirmed 2026-10-04: lobby, skill
  emotes, Parallel Quest ready-up and start. Before the fix the PQ ready-up
  also failed (PC host: blank ready screen, client timer ran out). That went
  away after the Deck was synced to the PC and the emote fix went in - which
  of the two fixed it is not known (the emote freeze, or the two installs
  differing). Mismatch found: after an xv2ins uninstall on 10/03 the PC's
  CMS gained id 159 "XA8" (no files; xv2ins adds it), and CUS/CSO/BCS/IDB/
  stage def were rewritten - so after any xv2ins change, copy the data to
  every machine again before playing online together. Checked and fine:
  rvp.cpk == 1.26 data2.cpk + 2 files; Epatches all located, none in
  network code; CMN.ean / MCM.cam.ean / CMN_M_QEA.bac merged with 1.26
  entries; XV2PATCHER lobby.xml.bak ships disabled upstream.
* Open: partner slots 30-39, a BAQ quest voice, CUS aura on a non
  HUM/HUF/SYM/SYF race; Revamp's loose ui\texture\<CHAR>.emb (35 changed by
  1.26) lack 1.26's `*_CGK1` images and order textures differently, so the 40
  TU22 rows point at missing/wrong images (fix: append missing vanilla
  textures by name, remap EmbIndex by name); texture_cregit.emb lacks 50
  credits textures; interplay with General_QoL's replay NPC (Revamp replaces
  the lobby map and lobby_npc_list.tnl).

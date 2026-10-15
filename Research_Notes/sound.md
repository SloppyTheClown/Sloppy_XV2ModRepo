# Sound

Covers: identifying which cue an event plays, the sound system layout, ACB
editing (and the edit that crashes), lobby NPC talk voices, volume options
and categories. Game 1.26.01. Read before any sound question.

## Find which sound an event plays (cheap, works)
1. `python ClaudeTools\sndwatch.py 120` while the user does the thing in game: polls
   the live sound objects, prints bank slot, cue id and the bank's .awb path.
2. `python ClaudeTools\acbcues.py <data path of the .acb> <cue id>` -> cue name.
3. Have the user repeat it (plus a comparison, e.g. a vanilla NPC); the cue
   present every time is the one. Footsteps / text blips repeat every ~0.3 s.

## Layout
* Sound::System* = global `[0x1554490]` (snd). Live sound objects:
  std::vector<SoundObject*> at snd+0x18bd0 / +0x18bd8. SoundObject: +8 bank
  slot (getter 0x43140), +0x28 cue id, +0x20 bank handle whose +0 points to
  the .awb path.
* Bank table: snd+0x1e00 + slot*0x88 (+8 inline name for some slots only; use
  the handle path). Load 0x3630a0(snd, slot, path, flag), unload 0x3640d0;
  lobby banks at 0x442ab8.. (CAR_LOBBY_MENU/AMB/CHARA_SE, CAR_LBY_MSCT_SE,
  VOX/Quest/TMQ/CAQR_ALL_VOX).
* .acb loads go through the file request class, so the file hook can edit
  them (exe-and-tooling.md). ACB = CRI @UTF tables, big-endian; reader
  `ClaudeTools\gamefiles\acb_utf.py` (XV2DataEdit's cpk.py chokes on ACBs). Top table
  row 0: CueTable (CueId...), CueNameTable (CueName, CueIndex = CueTable row).
* Make cue A play cue B's sound: copy B's CueTable ReferenceIndex + Length
  into A's row, in place (ReferenceType is a constant column). Do NOT point A's
  CueNameTable CueIndex at B: two names on one cue CRASHED the game on play
  (access violation in ucrtbase).

## Lobby NPC talk voice
* One line from VOX/Quest/TMQ[/en]/CAQR_ALL_VOX.acb: CAQR_<code>_000 or
  BDQR_<code>_000 (avatar tables M01.., G01.. at 0x14ae950 / 0x14ae9d0).
  Dastar cue 254 CAQR_DST_000 (only Dastar uses DST); lobby Hercule cue 61
  BDQR_MST_000; robots HST BDQR_HST_000..008.
* The code (DST, MST) is NOT the model's CMS code and is in no data file (TNL,
  TSD, scripts, msg, cms, chara_sound.cso). Changing the NPC's model does not
  change its voice.
* Builder 0x7b3e40 (plays via 0x80d420, rcx=[[0x1554458]+0x948], edx=0x146),
  called from 0x7b4230, slot 14 of QuestNpcObject / QuestNpcCharaAnimObject /
  QuestShenronObject; code from [obj+0xc8]+0x18 (chara id) -> 0x28fea0
  (table 0x15544d8, code at entry+4). How the lobby's Dastar gets DST was not
  traced.
* Dead ends: heap scan (4.6 GB) for objects with those vtables found none;
  TNL fields / msg records hold no voice id; no robot NPC id table in the exe;
  hardware breakpoints (second debugger attach crashed the game).
* General_QoL `replay_npc.talk_voice` (default BDQR_MST_000): row 254 gets
  the configured cue's ReferenceIndex + Length, in both CAQR_ALL_VOX.acb.

## Volume options and categories
* Category volumes: floats at snd+0x1a404 (5 categories); SetCategoryVolume
  0x363d10(snd, cat, vol). ApplyOptionVolumes 0x8479e0: cat 0 <- option 3,
  cat 2 <- option 4, cat 1 <- option 5; level 0..5 via table 0x10d3178 (0.0,
  0.2 .. 1.0). Options = bytes at mgr+8+0x8c+4+id (0x8247f0 -> 0x829750),
  mgr = [0x15545b0].
* Cat 0 = BGM: title music (cue 29) and the intro movie's sound follow it.
  Confirmed live (lobby): options 0/1/2 -> cat 0 0.0, cat 1 0.4, cat 2 0.2.
* At the title the options are defaults (5) - the save's options load on
  Start, but the slot is already valid at the title (5 5 5 recorded 6.5 s
  after boot). Trust options only once the category volumes equal them.
  Ready: snd+0x10 != 0 && snd+0x18bc8 == 0.
* `ClaudeTools\qolprobe.py` prints slot, options, category volumes live.

## Character SE banks and merging two banks (Battle Styles, 1.26.01)
* `system/chara_sound.cso`: "#CSO", entry count i32 +8, table offset i32
  +12 (0x10); 32-byte entries: chara i32, costume u32, then SE, VOX, AMK,
  skill string offsets (absolute u32, 0 = none), 8 bytes padding
  (Xv2CoreLib CSO/CSO_File.cs). CaCs 100-107 get CAR_BTL_<RACE>_SE (SYM none).
* Bank load: 0x298e14 in 0x298c70 calls 0x3630a0(sound manager, slot,
  "SE/Battle/Chara/<bank>", 0); the game builds "data/sound/" + path +
  ".acb" (loose files first). Details: battle-styles-sound-and-effects.md "Build 7".
* **Merging two banks** (`BattleStyles\src\acb_merge.cpp`): ACB = top
  @UTF table whose data cells hold nested @UTF tables and the AFS2 audio.
  Writer rules that give the game's own bytes back (all 77 CAR_BTL_*_SE
  rewritten to the same tables; ZMS and NMC byte-identical -
  `ClaudeTools\battle_styles\acbroundtrip.py`): strings in encounter order, no
  dedup; top table: data starts 32-aligned, a gap before every non-first
  data cell, empty cells (0,0), file padded to 32; nested tables: no data
  padding, body padded to 4; a constant/zero column whose rows differ is
  written per row. AFS2: v1, 4-byte offsets, 2-byte ids, entries in id
  order, alignment kept.
* What moves with appended cues: waveforms (MemoryAwbId, ExtensionData),
  synths (ReferenceItems, CommandIndex, GlobalAisac start, ActionTrack
  start, ControlWorkArea1/2), tracks (EventIndex, CommandIndex), sequences
  (TrackIndex list, ControlWorkArea), commands (0x7d0 / 0x7d3 reference
  items, 0x4b global AISAC row, 0x6f / 0x55 string rows), cue names
  (CueIndex; "_STYLE<n>" when the name exists), ActionTrack targets,
  AcfReference (union). Refused: AISAC / graph / block / beat-sync rows,
  streamed audio, local AISACs. Cue AisacControlMap holds ACF control ids
  (02 in every character bank) and is copied as is.
* Check: `ClaudeTools\battle_styles\acbmergecheck.py MERGED RACE STYLE`;
  `acbwave.py ACB OUT CUE..` extracts a cue to .wav (ffmpeg).
// ---------------------------------------------------------------------------
//  Battle Styles - character sound bank merge
//
//  A CaC loads its race's character SE bank (system/chara_sound.cso:
//  104 NMC -> CAR_BTL_NMC_SE), but a style's BAC plays cues of the style
//  character's bank (142 ZMG -> CAR_BTL_ZMS_SE) under that bank's cue ids.
//  MergeAcb keeps the race bank as it is and appends the style bank's cues,
//  with everything they use (sequences, tracks, commands, synths,
//  waveforms, their audio), under new cue ids after the race's. The race's
//  own cues keep their ids, so its skills (Turn Giant's NMC cues) still play;
//  the style BAC is pointed at the new ids (ean_fit.h BacFit::characterSeCues).
//
//  Format: CRI ACB, a @UTF table whose cells hold nested @UTF tables and the
//  bank's AFS2 audio archive. Layout and command codes: XV2-Tools
//  Xv2CoreLib ACB/ACB_File.cs (by LazyBone) and UTF/UTF_File.cs; the
//  writer's padding rules were measured on the game's 77 CAR_BTL_*_SE banks
//  (rewriting a parsed bank gives the same tables on all 77, the same bytes
//  for ZMS and NMC - ClaudeTools\battle_styles\acbroundtrip.py).
// ---------------------------------------------------------------------------
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// `race` and `style` are whole .acb files. On success `out` is the merged
// bank, `cueIds` maps every style cue id to its id in it, `note` says what
// was appended. The style's cue limit work areas go after the race's (header
// counts summed, cue limit commands moved). Fails (note says why) on a style bank using what the merge
// doesn't remap: AISAC / graph tables, streamed audio, block sequences, beat
// sync. Same inputs give the same bytes.
bool MergeAcb(const uint8_t* race, size_t raceSize, const uint8_t* style, size_t styleSize,
              std::vector<uint8_t>& out, std::map<uint16_t, uint16_t>& cueIds, std::string& note);

// The character SE bank name (e.g. "CAR_BTL_NMC_SE") system/chara_sound.cso
// gives `charaId` (its costume 0 entry, else its first), or empty: none
// listed, or listed with no bank (SYM). CSO: "#CSO" header, entry count i32
// at +8, entry table offset i32 at +12, 32-byte entries (chara i32, costume
// u32, then SE, VOX, AMK and skill string offsets, absolute u32s, 0 = none,
// 8 bytes padding); layout Xv2CoreLib CSO/CSO_File.cs, checked on the
// 1.26.01 file.
std::string CharaSeBank(const uint8_t* cso, size_t size, int charaId);

// Mentor names on the lobby map: the file edit and the text encoding.
// The exe hook that sends the names lives in mod.cpp (HkMapEventAppend).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// LOBYHUD.iggy with General_QoL's map ActionScript (as3\LobbyMap.as, built
// into lobyhud_abc.h). Only a file whose AS3 block is the vanilla one is
// patched; `note` says what happened. Returns whether `out` holds the patched file.
bool PatchLobbyHud(const uint8_t* data, size_t size, std::vector<uint8_t>& out, std::string& note);

// Appends ",c0=<unit>,c1=<unit>,..." to a map "Event:" line: the name's
// UTF-16 code units in decimal (LobbyHUD.parseMapInfo parseInt()s every
// value). Reads at most maxChars units; appends nothing for an empty name
// or one with control characters (not a name). Returns whether it appended.
bool AppendNameFields(const wchar_t* name, size_t maxChars, std::string& line);

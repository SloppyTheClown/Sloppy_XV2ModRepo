#pragma once
#include "tables.h"

// Populate kRacesLive from the game's own parameter_spec_char.psc.
// Returns true if the file was read and parsed; on failure the baked fallback
// tables are left in place. gameRoot is the folder containing data/ and bin/.
bool Psc_Load(const wchar_t* gameRoot, char* whyNot, size_t whyNotLen);

// Race rows actually in use. Starts as a copy of the baked kRaces and is
// overwritten by Psc_Load when the game's file is available.
extern RaceRow kRacesLive[8];

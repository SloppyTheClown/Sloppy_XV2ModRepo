#pragma once
#include <cstdint>

// The game's image base (defined in dllmain.cpp).
extern uintptr_t g_exe;

// Implemented in dllmain.cpp.
int KiBlast_SlotOfCharacter(const char *character); // save slot 0..7, or -1 if not a save CaC
constexpr int kPickDefault = -1;     // nothing equipped: vanilla, super souls still decide
constexpr int kPickRaceDefault = -2; // "Race Default": the race's blast whatever the soul
int KiBlast_Equipped(int slot, int preset);          // blast ID1, kPickDefault or kPickRaceDefault
void KiBlast_Log(const char *format, ...);

// Redirects the 0x618d30 (ki blast pick) call sites. Call from the Start
// retry loop until it returns true (false while Steam DRM still has .text
// encrypted, or after a permanent failure, which it logs once).
bool BlastSites_Install();

// "override_transformation_blasts": a CaC with a chosen blast keeps it when a
// transformation's skillset would change it. Call before BlastSites_Install.
void BlastSites_KeepThroughTransformations(bool keep);

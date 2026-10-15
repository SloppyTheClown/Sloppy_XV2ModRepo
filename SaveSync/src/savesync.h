// The in-game sync - see savesync.cpp.
#pragma once

// Vtable patches (.rdata, usable at once): the save slot menu's AS callback,
// the game's save-now, and the file-request hook serving AVATAR_SLOT.iggy's
// script.
void SaveSync_InstallHooks();

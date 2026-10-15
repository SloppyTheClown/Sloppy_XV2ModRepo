#pragma once
// Lobby NPCs (lobby_npc_list.tnl IDs) whose triggers in lobby_event_manage.tsd
// the mod edits. Shared with tests\tsrtest.cpp; tests\check.py has a copy.
#include "tsr.h"

// Their story quest requirements become always true.
static const NpcRanges kStoryNpcs = {
    {3, 36},       // mentors
    {81, 81},      // Advancement Test Desk
    {94, 97},      // radio, Photo Mode, Crystal Raid Battles, Info Robot
    {148, 148},    // Kame House Reception
    {208, 273},    // expert mission rifts (lobby objects; later ones wait for earlier expert missions)
    {976, 976},    // Multiplayer Modes
    {982, 982},    // Voter Reception
    {1007, 1046},  // Festival of Universes lobby desks (not 1006, its entrance)
    {1309, 1309},  // Polisher (All-Star Battle)
};
// Not here: the tutorial and story NPCs (Trunks, Elder Kai, Supreme Kai...),
// the DLC story receptions (Spots 85, Culina 86, Waipa 87, 981), whose
// triggers pick the next story step, and the ones with DLC flags (Brashy 987,
// Dastar 1294).

// The parallel quest tutorial (TMQ_1400) requirement: always true.
static const char kPqTutorialDone[] = "QUEST(TMQ,STATE,TMQ_1400)==3";
static const char kAlwaysPqTutorial[] = "            AVATAR(LEVEL)>=1";
static_assert(sizeof(kPqTutorialDone) == sizeof(kAlwaysPqTutorial), "same length");
static const NpcRanges kPqTutorialNpcs = {{976, 976}, {1046, 1046}};  // Multiplayer Modes, Cross Versus Guide

// "Locked" triggers that fire while the prologue is not cleared: never true.
static const char kPrologueLocked[] = "QUEST(TPQ,STATE,CTP_00_01)!=3";
static const NpcRanges kTeleportLockedNpcs = {{65, 80}};  // transfer robots (unlock_teleports)
static const NpcRanges kStoryLockedNpcs = {{94, 94}};     // radio (clear_story_flags)

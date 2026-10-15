// ---------------------------------------------------------------------------
//  Tables derived from data/system/parameter_spec_char.psc (game build 1.26.01)
//
//  A CaC's PSC "spec" index is height*3 + weight, 4 heights x 3 weights.
//    * weight changes ONLY the four attack stats
//    * height changes ONLY health and speed
//
//  Every CaC stat is  G(level) * (1 + psc + points/500)  - see level_table.h.
//  Max HP is the same with 1000*G. Verified from Lv1 to Lv199 (Research_Notes\stats-and-combined-supers.md).
//
//  These are the RAW PSC values. psc.cpp replaces them with the ones read from
//  the game's own file at startup; these are only the fallback.
//  (An older version stored converted "bases" with fitted intercepts - those
//  intercepts turned out to contain a QQ Bang's +5 BLA and are gone.)
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

struct RaceRow {
    const char* name;
    float strike[3];   // psc strike_attack,  by weight tier 0..2
    float ki[3];       // psc ki_blast_super, by weight tier 0..2
    float health[4];   // psc health,         by height tier 0..3
};

// index by (race - 100), races 100..107
static const RaceRow kRaces[8] = {
  { "Human M",  {0.525f,0.575f,0.625f}, {0.450f,0.400f,0.350f}, {0.450f,0.475f,0.525f,0.550f} },
  { "Human F",  {0.350f,0.400f,0.450f}, {0.625f,0.575f,0.525f}, {0.450f,0.475f,0.525f,0.550f} },
  { "Saiyan M", {0.375f,0.425f,0.475f}, {0.475f,0.425f,0.375f}, {0.375f,0.400f,0.450f,0.475f} },
  { "Saiyan F", {0.450f,0.500f,0.550f}, {0.650f,0.600f,0.550f}, {0.350f,0.375f,0.425f,0.450f} },
  { "Namekian", {0.375f,0.425f,0.475f}, {0.475f,0.425f,0.375f}, {0.525f,0.550f,0.600f,0.625f} },
  { "Frieza",   {0.375f,0.425f,0.475f}, {0.550f,0.500f,0.450f}, {0.350f,0.375f,0.425f,0.450f} },
  { "Majin M",  {0.375f,0.425f,0.475f}, {0.475f,0.425f,0.375f}, {0.550f,0.575f,0.625f,0.650f} },
  { "Majin F",  {0.550f,0.600f,0.650f}, {0.450f,0.400f,0.350f}, {0.350f,0.375f,0.425f,0.450f} },
};

// offsets inside the battle character struct
enum : unsigned {
    OFF_RACE      = 0x04,   // 100..107 for a CaC
    OFF_SPEC      = 0x14,   // PSC spec index 0..11
    OFF_MARK      = 0x24,   // always 0x3D - struct fingerprint
    OFF_LEVEL     = 0x48,   // 1..199, indexes level_table.h
    OFF_HP_CUR    = 0x50,
    OFF_HP_MAX    = 0x54,
    OFF_BASIC     = 0x88,
    OFF_BASIC_KI  = 0x8C,
    OFF_STRIKE    = 0x90,   // <- hooked
    OFF_KIBLAST   = 0x94,   // <- hooked
};

// Investment is solved on a HALF-point lattice: an attribute point is G/500,
// a QQ Bang point is 10 points, but clothing pieces are 2.5 points each (the
// Time Patroller Battle Gear), so an odd number of pieces lands on a half.
// That is also the old "13.5-point character" that never solved.
static const float MGRID = 0.025f;   // every STR/BLA value in the PUP file is a multiple of this
static const int   EXLO  = -160;     // investment range in POINTS; gear can go below base
static const int   EXHI  = 240;

// The menu scripts Combined Supers serves through XV2 Mod Loader's file API
// (menu_scripts.cpp): the Supers box on Customize > Attributes and the chain
// link on the stat radar chart.
#pragma once
#include <string>

// Registers both scripts while the supers are combined (`mode` 'a' additive,
// 'h' highest) and takes them back out for '0' (disabled / "vanilla").
// Returns a line for the log; "" when nothing changed since the last call.
std::string MenuScripts_Apply(char mode);

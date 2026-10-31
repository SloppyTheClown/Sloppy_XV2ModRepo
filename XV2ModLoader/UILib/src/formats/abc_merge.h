// Merging AS3 script changes: several mods each ship a whole edited copy of a
// menu's ABC; MergeScript takes what one mod changed against its base (the
// script it was made from) and applies only that to the current script (the
// game's plus every earlier mod's changes), so the mods stack instead of the
// last one replacing the rest.
//
// A mod made with FFDec needs its own base: FFDec recompiles every method of
// a class it touches (`iftrue` -> `not` + `iffalse`, extra converts), so
// against the game's script every method would look changed. The mod base is
// the game's script with the same classes recompiled by FFDec unchanged
// (ClaudeTools ffdec_base.py); "did the mod change it" is asked against that,
// "did an earlier mod change it" against the game's script.
//
// Everything is matched by name, never by index (an edited ABC renumbers its
// pools freely):
//   classes            by QName;
//   traits             by kind + QName (all private namespaces are one:
//                      the game names them "", FFDec after the class);
//   scripts            by the names they define;
//   methods            compared semantically - operands resolved to their
//                      values, branches as instruction numbers, debug ops and
//                      stack/register sizes ignored.
// Applied:
//   a changed method   replaced in place (its code, signature, sizes), any
//                      functions it creates (newfunction) imported new;
//   a new trait        added (a slot keeps its id unless it is taken);
//   a new class        imported whole, a new script inserted before the last
//                      one (the last script is the movie's entry point);
//   removed things     kept (noted) - a merge only adds and changes;
//   both changed       a method: both edits merged instruction by
//                      instruction (diff3.h) when they don't overlap and
//                      the method has no try blocks / activation traits;
//                      otherwise (and for other traits) the later mod wins
//                      (noted as a conflict).
#pragma once
#include <string>
#include <vector>
#include "abc.h"

namespace xv2ui {
namespace abc {

struct MergeReport {
    size_t methodsChanged = 0, methodsMerged = 0, traitsAdded = 0, classesAdded = 0, scriptsAdded = 0, conflicts = 0;
    std::vector<std::string> notes;   // conflicts and skipped changes, one line each
    std::string Summary() const;
    bool Changed() const { return methodsChanged + traitsAdded + classesAdded + scriptsAdded > 0; }
};

// game: the game's own ABC; current: what to apply the mod to (may be game
// itself); mod: the mod's whole edited ABC; modBase: what the mod was made
// from when not the game's ABC (an FFDec rebuild of it, see above). Throws
// FormatError when an input doesn't parse or the change can't be applied.
Bytes MergeScript(const Bytes& game, const Bytes& current, const Bytes& mod, MergeReport* report = nullptr,
                  const Bytes* modBase = nullptr);

// Semantic differences between two ABCs (by the same matching), one line
// each; empty when they do the same thing. With game + ffdecBase, a class
// part that is FFDec's rebuild in `expected` and the game's own in `got`
// counts as the same (an FFDec-made expected result).
void CompareScripts(const Bytes& expected, const Bytes& got, std::string& differences, const Bytes* game = nullptr,
                    const Bytes* ffdecBase = nullptr);

}  // namespace abc
}  // namespace xv2ui

// Three-way merge of line lists (diff3, as in version control): given the
// base, one side's edit of it ("theirs") and another side's ("ours"), the
// result keeps both sides' changes where they don't overlap.
//
// Used by the ABC merge on method code, one line per instruction, so two mods
// editing different parts of the same method both get their edit.
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace xv2ui {

// One line of the merged result: which list it comes from and its index there.
struct Diff3Pick {
    bool fromTheirs = false;   // false: from ours
    size_t index = 0;
};

// Merges `theirs` (base -> theirs) into `ours` (base -> ours). Returns false
// when both sides changed the same lines differently (or the lists are too
// long for the line matcher); `picks` is then incomplete. Lines in a region
// only `theirs` changed come from theirs, everything else from ours.
// `theirsPosition` / `oursPosition` give each line's index in `picks`: where
// it was picked, where the same line of the other side was, or where the
// region replacing it starts (SIZE_MAX for a line of theirs that ours
// replaced - code branching there can't be placed). One past the end of a
// list maps to picks.size().
// Where both sides only inserted lines at the same place, both are kept
// (ours first, then theirs) and `bothInserted` is set.
bool Diff3(const std::vector<std::string>& base, const std::vector<std::string>& theirs,
           const std::vector<std::string>& ours, std::vector<Diff3Pick>& picks, std::vector<size_t>& theirsPosition,
           std::vector<size_t>& oursPosition, bool* bothInserted = nullptr);

}  // namespace xv2ui

// Display-list edits of an .iggy menu: add a sprite placement by cloning
// another one ("one more button row") and change a placement's matrix in one
// frame ("make the box taller"). Record layouts and what was found in
// AVATAR.iggy: Research_Notes\ui-iggy-display-list.md. Used by
// XV2UserInterfaceLib's display stage (ui_lib.cpp) and `xv2ui display`.
//
// Commands, one per line ('#' starts a comment, fields split on blanks):
//   clone  <sprite> <source> <name> <dx> <dy>
//       Gives <sprite> a new placement <name> that is a copy of every
//       placement (place and later moves) at <source>'s depth, shifted by
//       (dx, dy), on a new depth. Does nothing when <sprite> already has a
//       placement called <name>, so applying it twice is harmless.
//   matrix <sprite> <target> <frame> <a> <b> <c> <d> <tx> <ty>
//       Sets the matrix of <target>'s placement in <frame> of <sprite>;
//       '*' keeps a value, '+=<number>' adds to it (so several mods can each
//       grow the same thing).
//   <sprite>   the instance name a placement gives the sprite ("edit_menu"),
//              or "#<character id>"; every placement of that name must be
//              the same sprite
//   <source>/<target>  a placement name inside <sprite>, or "@<depth>"
//   <frame>    1-based, or a frame label of <sprite>
#pragma once
#include <string>
#include <vector>
#include "bytes.h"

namespace xv2ui {

struct DisplayCommand {
    bool clone = false;               // else matrix
    std::string sprite, source;       // source: clone's source, matrix's target
    std::string name;                 // clone: the new placement's name
    float dx = 0, dy = 0;             // clone
    std::string frame;                // matrix: number or label
    bool keep[6] = {};                // matrix: '*' (a b c d tx ty)
    bool add[6] = {};                 // matrix: '+=', values[i] is the amount
    float values[6] = {};
    int line = 0;                     // for messages
};

// Parses the commands; throws FormatError ("line 3: ...") on bad syntax.
std::vector<DisplayCommand> ParseDisplayCommands(const std::string& text);

struct DisplayReport {
    size_t cloned = 0, matrices = 0, unchanged = 0;   // unchanged: already there / same values
    std::vector<std::string> notes;                   // one line per command that changed nothing
    std::string Summary() const;
    bool Changed() const { return cloned + matrices > 0; }
};

// `current` (an .iggy) with the commands applied in order. Throws
// FormatError when a command can't be applied (nothing is returned then).
Bytes ApplyDisplay(const Bytes& current, const std::vector<DisplayCommand>& commands, DisplayReport* report = nullptr);

}  // namespace xv2ui

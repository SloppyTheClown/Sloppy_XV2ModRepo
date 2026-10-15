// The "Battle Style" equip slot on the Customize screen - see style_menu.cpp.
#pragma once
#include <map>
#include <string>
#include <vector>

// Which movesets the list offers (config "styles"). Every choice leaves out
// characters the character select hides for DLC.
enum class StyleChoice {
    All,       // every roster moveset (minus "exclude_styles")
    Valid,     // also without fake and giant movesets
    Roster,    // valid, and only characters unlocked in the character select (race movesets always)
    Unlocked,  // valid, and every character in the character select, locked or not (race movesets always)
    List,      // the config's CMS codes, in that order
};

// Builds the list of styles from the game's CMS and character names, sorted
// by name with the race movesets last ("Default" stays first).
// `codes`: CMS codes to offer for StyleChoice::List.
// `excluded`: movesets never offered - groups ("fake": no attack
// animations, "giant": a giant's, "race": a CaC race's own) and CMS codes.
// `names`: CMS code -> list name, over the automatic one (config "style_names").
// `search`: filter the list with the loader's search box (when the loader has one).
void StyleMenu_Build(StyleChoice choice, const std::vector<std::string>& codes, const std::vector<std::string>& excluded,
                     const std::map<std::string, std::wstring>& names, bool search);

// Serves AVATAR.iggy with the extra slot and hooks the menu's AS callback.
// Data-only patches: safe right away.
void StyleMenu_InstallDataHooks();

// Code patches (user-data slot count, list poll): call once Steam's DRM has
// decrypted the code.
void StyleMenu_InstallCodeHooks();

// The CMS code of the style on the loaded character slot's main loadout
// (preset 0) for the playing Steam account, or empty.
std::string StyleMenu_EquippedCode();
// The same for save character slot `slot` (0..7); empty for any other value.
std::string StyleMenu_EquippedCode(int slot);
// The style of loadout record `preset` (0 = main, 1..7 = saved presets) of
// save character slot `slot` (0..7, -1 = the loaded one), or empty.
std::string StyleMenu_PresetCode(int slot, int preset);
// The CMS code whose moveset CaC `charaId` (100..107) loads for the equipped
// style `code`: a custom style's line for that race
// (data\battle_styles\styles\*.xml), any other code unchanged.
std::string StyleMenu_MovesetFor(const std::string& code, int charaId);

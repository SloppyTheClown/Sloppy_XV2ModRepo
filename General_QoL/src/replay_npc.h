#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The replay NPC ("DLC Master"): Dastar's lobby slot in the Time Nest with a
// new model, name and lines, shown from the start; his quest menu is built by
// mod.cpp. Filled from the config's "replay_npc"; these defaults apply to
// settings it leaves out.
struct ReplayNpc {
    bool enabled = true;
    int  npcId = 1294;                     // Dastar's lobby NPC ID
    std::string npcNameKey = "name_4293";  // his name's key in lobby_text_*.msg
    std::string model = "STN";             // character code (CMS); STN = Hercule
    int  costume = 0;
    int  idleAction = 140;                 // lobby action looped while idle (Hercule's pose)
    std::string talkMotion = "Mot_Stand";
    // Lines, UTF-8. Empty = keep the game's own text.
    std::string name = "DLC Master";
    std::string greeting = "Hey there, champ! Want to relive one of the big adventures? "
                           "The World Champion remembers every single one!";
    std::string option = "Relive an adventure";  // the choice that opens the menu
    std::string menuLine = "Pick any one you like! I was there for all of them, of course!";
    std::string farewellGo = "Go get 'em! Make the World Champion proud!";
    std::string farewellStay = "No problem! The champ will be right here whenever you need him!";
    // Quest menu header text (game: "Future Saga"); non-empty also hides the
    // big "Future Saga" title art above it.
    std::string menuTitle = "DLC Adventures";
    // Voice line played as the player starts talking to him: a cue name in
    // CAQR_ALL_VOX.acb. The game plays Dastar's (CAQR_DST_000) for this NPC;
    // BDQR_MST_000 is the lobby Hercule's. Empty = keep Dastar's.
    std::string talkVoice = "BDQR_MST_000";
    std::string npcVoiceCue = "CAQR_DST_000";
    std::vector<int> hideNpcs = {987, 85, 86};  // Brashy, Spots, Culina
};

// The replay NPC's files, by (lower case, '/'-separated) game path; the path
// may carry a prefix. LobbyEvents is lobby_event_manage.tsd.
enum class NpcFile { None, LobbyEvents, LobbyNpcs, LobbyText, MenuTitles, MenuTitleArt, VoiceBank };
NpcFile ClassifyNpcFile(const std::string& path);

// Builds the edited file. Returns false when nothing needs changing or the
// file does not have the layout this expects (then the game gets the file as
// it was). `note` says what was done or why not. Only lobby_event_manage.tsd
// and the MSG files change size.
bool EditNpcFile(NpcFile kind, const ReplayNpc& npc, const uint8_t* src, size_t n, std::vector<uint8_t>& out,
                 std::string& note);

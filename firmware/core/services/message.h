#pragma once

#include <string>
#include <vector>

namespace cubey {

// A text message from the server (server/src/session.ts): its type; for "said" whether it was the goodbye; for
// "timer" how many minutes, and what for; for "letter" the letter (UTF-8; none: the game with letters is over), and
// in the word chain Cubey's word, for the letter to come from, and hers before it; for "talk_over" whether she may
// talk over Cubey; for "count" how many dots (none: the game is over); for "spell" the word Cubey spells (its
// letters, small), and for "spell.at" which of its letters (i) is said how far into Cubey's speech (ms; both -1 for
// none); for "show" a picture's rows or text; for
// "settings" the language Cubey speaks, all it speaks, whether it is a new Cubey, not set up
// (the grown-ups' menu), the screen's brightness and the speaker's volume in percent (0: not said), its theme
// (ui/dots.h; none: not said) and whether the child holds its screen to talk; for "update" the build offered and its
// version.
struct ServerMessage {
    std::string type; // "" for anything that isn't a message
    bool last = false;
    int minutes = 0;
    std::string kind;
    std::string letter;
    std::string word;
    std::string hers;
    bool on = false;
    int n = 0;
    std::vector<std::string> rows;
    std::string text;
    std::string language;
    std::vector<std::string> languages;
    bool fresh = false;
    int brightness = 0, volume = 0;
    std::string theme;
    int i = -1, ms = -1;
    bool hold = false;
    std::string build{}, version{};
};

ServerMessage parse_message(const std::string &json);

// The message saying which picture she tapped (her word game's).
std::string tapped_message(const std::string &id);

} // namespace cubey

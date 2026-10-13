#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cubey {

// The keys beside the power button, "+" and "–", as the platform reads them: held now, and how many times each was
// pressed since it was last asked (counted on the platform's own, so none is lost while the screen is busy drawing).
struct Keys {
    bool plus = false, minus = false;
    int plus_pressed = 0, minus_pressed = 0;
};

// The grown-ups' menu, hidden from the child: "–" held alone kHoldMs opens it, and again closes it ("+" held alone
// kHoldMs, the menu closed, is the Wi-Fi setup's: services/wifi_setup.h); so does a grown-up asking Cubey (open()). A
// new Cubey's opens by itself, once, as the server first says its languages (offer()). On it, three lines: the language
// Cubey speaks, the one its word game teaches (codes from the server's list) and its screen's theme (its own list); an
// arrow beside one, tapped, steps it back or on, round (a language past the other line's: never the same); under them
// its screen's brightness, slid. OK closes it (as do "–" held again, the power button, a minute left alone); what it
// shows then goes to the server if it changed, or the Cubey is new (to_send()).
// What it sets: the language Cubey speaks, the one it teaches, its screen's theme and brightness (10 to 100%).
struct Settings {
    std::string language, learns, theme;
    int brightness = 100;
    bool operator==(const Settings &o) const { return language == o.language && learns == o.learns && theme == o.theme && brightness == o.brightness; }
    bool operator!=(const Settings &o) const { return !(*this == o); }
};

class SettingsMenu {
public:
    static constexpr uint32_t kHoldMs = 5000, kIdleMs = 60000;
    enum class Event : uint8_t { None, Opened, Closed, Wifi };

    explicit SettingsMenu(std::vector<std::string> themes = {}) : themes_(std::move(themes)) {}

    // As the server has them; fresh: a Cubey not set up yet.
    void known(std::vector<std::string> languages, Settings settings, bool fresh);
    bool ready() const { return !languages_.empty(); } // the server has said them
    Event keys(const Keys &keys, uint32_t now);        // the keys as the platform reads them: call every step
    Event offer(uint32_t now);                         // a new Cubey's: opened, once
    Event open(uint32_t now);                          // asked for: a grown-up asked Cubey
    void step(int line, int by, uint32_t now);         // an arrow: line 0, 1 or 2
    void brighten(int percent, uint32_t now);          // the brightness slid to
    Event confirm() { return close(); }                // OK
    Event close();                                     // the power button

    bool is_open() const { return open_; }
    const std::string &language() const { return shown_.language; }
    const std::string &learns() const { return shown_.learns; }
    const std::string &theme() const { return shown_.theme; }
    int brightness() const { return shown_.brightness; }
    bool to_send() const { return fresh_ || shown_ != known_; }

private:
    std::vector<std::string> languages_, themes_;
    Settings known_, shown_;
    bool open_ = false, fresh_ = false, offered_ = false;
    bool plus_ = false, minus_ = false; // held at the last call
    uint32_t held_since_ = 0;           // the key held alone, since
    bool fired_ = false;                // this hold has done what it does
    uint32_t last_ = 0;                 // the last thing done on it
};

} // namespace cubey

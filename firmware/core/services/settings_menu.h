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
// new Cubey's opens by itself, once, as the server first says its languages (offer()). Two pages: on the first, the
// language Cubey speaks (a code from the server's list) and its screen's theme (its own list), an arrow beside each,
// tapped, stepping it back or on, round; on the second, its screen's brightness and its volume, slid. OK closes it (as do "–" held again, the power button,
// a minute left alone); what it shows then goes to the server if it changed, or the Cubey is new (to_send()). The
// language its word game teaches the child chooses by asking Cubey.
// What it sets: the language Cubey speaks, its screen's theme and brightness, and its volume (10 to 100%).
struct Settings {
    std::string language, theme;
    int brightness = 100, volume = 100;
    bool operator==(const Settings &o) const {
        return language == o.language && theme == o.theme && brightness == o.brightness && volume == o.volume;
    }
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
    void step(int line, int by, uint32_t now);         // an arrow: line 0 (its language) or 1 (its theme)
    void brighten(int percent, uint32_t now);          // the brightness slid to
    void set_volume(int percent, uint32_t now);        // the volume slid to
    void to_page(int page, uint32_t now);              // 0: its language and theme; 1: its brightness and volume
    Event confirm() { return close(); }                // OK
    Event close();                                     // the power button

    bool is_open() const { return open_; }
    bool first() const { return first_; } // a new Cubey's, opened by itself: only its language (ui/menu.h)
    const std::string &language() const { return shown_.language; }
    const std::string &theme() const { return shown_.theme; }
    int brightness() const { return shown_.brightness; }
    int volume() const { return shown_.volume; }
    int page() const { return page_; }
    bool to_send() const { return fresh_ || shown_ != known_; }

private:
    std::vector<std::string> languages_, themes_;
    Settings known_, shown_;
    Event opening(uint32_t now, bool first);
    bool open_ = false, fresh_ = false, offered_ = false, first_ = false;
    int page_ = 0;
    bool plus_ = false, minus_ = false; // held at the last call
    uint32_t held_since_ = 0;           // the key held alone, since
    bool fired_ = false;                // this hold has done what it does
    uint32_t last_ = 0;                 // the last thing done on it
};

} // namespace cubey

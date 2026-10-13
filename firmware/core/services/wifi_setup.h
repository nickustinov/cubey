#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "services/keyboard.h"

namespace cubey {

// How joining the network kept on the device goes (none kept: Unset; Refused: its password; NotFound: not there;
// Failed: anything else), and a network around, as the platform has them.
enum class Wifi : uint8_t { Unset, Joining, Joined, Refused, NotFound, Failed };
struct Network {
    std::string ssid;
    int rssi; // dBm
    bool open; // no password
};

// The Wi-Fi setup, for a grown-up, on the fine dot board (ui/wifi.h): a welcome when Cubey has no network (the first
// time, or the one it keeps isn't there), till it is tapped; the networks around, the chosen one's password typed on the keyboard
// (services/keyboard.h), joining it; joined, Cubey's eyes come back. "+" held alone opens it any time.
class WifiSetup {
public:
    enum class Screen : uint8_t { Off, Welcome, Networks, Password, Joining, Joined, Failed };
    static constexpr uint32_t kJoinMostMs = 25000, kDoneMs = 2500, kIdleMs = 180000;
    static constexpr uint32_t kBounceMs = 150; // a tap this soon after the last is the touch's bounce (a person's next is later)
    static constexpr int kShown = 5;     // networks on screen at once
    static constexpr size_t kMost = 63;  // a WPA password's most

    void open(bool welcome, uint32_t now); // asks for a scan
    void close() { screen_ = Screen::Off; }
    void back();                            // the power button: from the password to the networks, else out
    // Every step: the scan's networks once found, how joining goes, the screens' times. True when it changed.
    bool follow(uint32_t now);

    void tap(uint32_t now);                     // the welcome tapped: on to the networks
    void scroll(int by, uint32_t now);         // the networks, by rows ("+" and "–", or a drag)
    void pick(int shown, uint32_t now);         // the network on that row of the screen
    void press(int col, int row, uint32_t now); // a key of the keyboard

    bool is_open() const { return screen_ != Screen::Off; }
    Screen screen() const { return screen_; }
    const std::vector<Network> &networks() const { return networks_; }
    int top() const { return top_; } // the first network on screen
    const std::string &ssid() const { return ssid_; }
    const std::string &password() const { return password_; }
    keyboard::Page page() const { return page_; }
    Wifi why() const { return why_; } // the last join that failed: Refused, NotFound, Failed, or Joining (no answer in time)

private:
    void join(uint32_t now);
    void to(Screen screen, uint32_t now) { screen_ = screen, since_ = now; }

    Screen screen_ = Screen::Off;
    std::vector<Network> networks_;
    bool scanning_ = false;
    int top_ = 0;
    std::string ssid_, password_;
    keyboard::Page page_ = keyboard::Page::Small;
    Wifi why_ = Wifi::Failed;
    bool tapped(uint32_t now); // a tap, not a bounce: true

    uint32_t since_ = 0, touched_ = 0; // the screen's start, and her (or a grown-up's) last touch
    uint32_t tapped_ = 0;              // the last tap taken (0: none yet)
};

} // namespace cubey

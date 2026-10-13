#pragma once

#include <deque>
#include <string>
#include <vector>

#include "services/wifi_setup.h"

// A scriptable platform for tests: set what the "server" and "speaker" do, read what the core sent.
namespace fake {

struct Hal {
    bool connected = true;
    unsigned session = 1; // which connection: a new one after every reconnect
    bool mic_on = false;
    bool hears_over_speech = false; // the platform cancels its speaker's echo
    bool voice_over = false;        // ...and its detector heard her voice over the speech
    bool speaker_busy = false;
    std::vector<std::string> sent;    // messages the core sent to the server
    std::deque<std::string> incoming; // messages the server will deliver
    int speaker_stops = 0;
    cubey::Wifi wifi = cubey::Wifi::Joined;
    int scans = 0;                          // scans asked for
    bool scanned = false;                   // the last has finished, with these
    std::vector<cubey::Network> networks;
    std::vector<std::pair<std::string, std::string>> joined; // networks asked to join, with their passwords
};

extern Hal hal;
void reset();

} // namespace fake

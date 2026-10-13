#include "fake_hal.h"

#include "hal/hal.h"

namespace fake {

Hal hal;

void reset() { hal = Hal{}; }

} // namespace fake

namespace cubey::hal {

bool link_connected() { return fake::hal.connected; }
unsigned link_session() { return fake::hal.session; }
void link_send(const std::string &json) { fake::hal.sent.push_back(json); }

bool link_receive(std::string &json) {
    if (fake::hal.incoming.empty()) return false;
    json = fake::hal.incoming.front();
    fake::hal.incoming.pop_front();
    return true;
}

void mic_stream(bool on) { fake::hal.mic_on = on; }
bool hears_over_speech() { return fake::hal.hears_over_speech; }

bool voice_over_speech() {
    const bool heard = fake::hal.voice_over;
    fake::hal.voice_over = false;
    return heard;
}
int speaker_rate() { return 24000; }
bool speaker_busy() { return fake::hal.speaker_busy; }

void speaker_stop() {
    fake::hal.speaker_stops++;
    fake::hal.speaker_busy = false;
}

Battery battery() { return {}; }
PowerPress power_button() { return PowerPress::None; }
Gravity gravity() { return {}; }
Wifi wifi() { return fake::hal.wifi; }
void wifi_scan() { fake::hal.scans++, fake::hal.scanned = false; }
bool wifi_networks(std::vector<Network> &networks) {
    if (!fake::hal.scanned) return false;
    networks = fake::hal.networks;
    return true;
}
void wifi_join(const std::string &ssid, const std::string &password) { fake::hal.joined.emplace_back(ssid, password); }
Keys keys() { return {}; }
void screen(bool) {}
Firmware firmware() { return fake::hal.firmware; }
void firmware_good() {}
void update(const std::string &) {}
UpdateProgress update_progress() { return {}; }
void restart() {}

} // namespace cubey::hal

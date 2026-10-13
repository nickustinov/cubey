#pragma once

#include <string>
#include <vector>

#include "services/battery.h"
#include "services/power_button.h"
#include "services/settings_menu.h"
#include "services/update.h"
#include "services/wifi_setup.h"
#include "services/upright.h"

// What the core needs from a platform. The Mac simulator implements these (a board will too); the host tests use a
// fake. The core calls them from the UI thread only.
namespace cubey::hal {

// Server link: one WebSocket, connected and reconnected by the platform.
// Binary frames from the server are speech and go straight to the speaker.
bool link_connected();
unsigned link_session(); // which connection: it changes with every new one, however quickly it replaced the last
void link_send(const std::string &json);
bool link_receive(std::string &json); // next text message from the server; false when there is none

// Microphone: while on, 16 kHz mono PCM streams to the server link. A platform that cancels its speaker's echo
// streams her voice without Cubey's while it speaks (hears_over_speech), and its own detector hears when she talks
// over Cubey: a moment of her voice, since the last call (voice_over_speech).
void mic_stream(bool on);
bool hears_over_speech();
bool voice_over_speech();

// Speaker: plays the speech the server sends.
int speaker_rate();  // Hz, announced to the server
bool speaker_busy(); // speech queued or playing
void speaker_stop(); // drop queued speech
void volume(int percent); // the speaker's, 10 to 100: a grown-up's (kept while the codecs rest)

// Power: the battery's charge, the side power button's last press since the last call (None: none), the screen
// (asleep, the core also draws black) and its brightness in percent (kept while it sleeps).
Battery battery();
PowerPress power_button();
void screen(bool on);
void brightness(int percent);

// Which way is down, from the motion sensor (all 0 without one).
Gravity gravity();

// The keys beside the power button, "+" and "–": held now, and the presses since the last call (the grown-ups' menu).
Keys keys();

// Wi-Fi: how joining the network kept on the device goes (none kept: Unset); the networks around (a scan, asked for,
// read once it is done: strongest first, each name once); joining one, kept for next time.
Wifi wifi();
void wifi_scan();
bool wifi_networks(std::vector<Network> &networks);
void wifi_join(const std::string &ssid, const std::string &password);

// A grown-up's full reset (the server's admin page): all it keeps (its network, its key) forgotten, and it starts
// again, as flashed clean.
void wipe();

// Updates over the air: the firmware it runs (services/update.h); it talks with the server, so a new firmware on trial
// is kept (restarted before, it goes back to the one before); an update the server offers, fetched from it into the
// other slot on the platform's own, the link paused meanwhile, and how that goes; the restart into it.
Firmware firmware();
void firmware_good();
void update(const std::string &build);
UpdateProgress update_progress();
void restart();

} // namespace cubey::hal

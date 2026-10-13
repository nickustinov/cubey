#pragma once

#include <cstdint>

#include "services/battery.h"
#include "services/power_button.h"

// Starts the simulator's platform: the server link (reconnecting), the speaker, and the mic, its echo cancelled.
// A device token, when the server asks for one, comes from CUBEY_DEVICE_TOKEN.
void mac_hal_init(const char *server_url);

// For screenshots: the core believes the server is there (nothing is sent anywhere), and a message arrives
// from it as if it had sent it.
void mac_pretend_online();
void mac_receive(const char *json);
void mac_pretend_speech(uint32_t ms); // Cubey's speech plays this long from now (for a word it spells, --speech)

// The side power button (p in the window, --power for screenshots), and the pretend battery (b, --battery).
void mac_press(cubey::PowerPress press);
void mac_set_battery(cubey::Battery battery);

// The pretend cube turned so many quarters clockwise more (r, --turn), for the picture to turn back upright; shaken
// (s), for the die.
void mac_turn(int quarters);
void mac_shake();
void mac_set_keys(bool plus, bool minus); // the board's "+" and "–", as held
void mac_wifi_unset();                    // no network kept: the Wi-Fi setup welcomes

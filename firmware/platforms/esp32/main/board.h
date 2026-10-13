#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "sdkconfig.h"

namespace cubey::board {

// The board itself (board_amoled_2_16.cpp): the display with touch and LVGL on its own task; the core starts under
// the display lock.
void display_start();
void display_lock();
void display_unlock();
// Flash written (an update's, update.cpp): the panel's DMA reads PSRAM, which can't be read meanwhile, so the strip on
// its way is waited for, and none goes until it is done.
void flash_writing(bool writing);
void backlight(bool on);
void brightness(int percent); // 0..100, the AMOLED's own
i2c_master_bus_handle_t i2c(); // the bus the power chip shares; nullptr if it failed
// The codecs for the speaker and the mic, and the amplifier where the ES8311 doesn't switch it.
esp_codec_dev_handle_t speaker_codec();
esp_codec_dev_handle_t mic_codec();
void amplifier(bool on);
// The speaker's echo comes with the mic, its second channel when the mic opens with kMicChannels and kMicSlots,
// for audio.cpp to cancel.
constexpr bool kEchoReference = true;
constexpr int kMicChannels = 4;   // TDM: MIC1, MIC3 (the echo), MIC2, MIC4
constexpr uint16_t kMicSlots = 0b0011;
// The mic's gain, never changed while it runs (as Espressif's and xiaozhi's boards: a gain that jumps as the speaker
// starts and stops is an echo path the cancelling must learn again): in the codec as much as the speaker's echo
// takes without clipping at full volume (at 21 dB it clipped, and clipped echo can't be cancelled), the rest after
// the cancelling, digitally with a limiter (esp-sr's AGC), to Lapsa's 37.5 dB in all: small children speak softly.
// The echo is wired back quietly: raised for the cancelling.
constexpr float kMicGainDb = 12.0f;
constexpr int kMicDigitalDb = 25;
constexpr float kEchoGainDb = 12.0f;
constexpr int kSpeakerVolume = 100; // the codec's output, 0..100, until the server says the Cubey's

// Wi-Fi and the server link; they connect and reconnect on their own (network.cpp).
void network_start();
void link_send_audio(const int16_t *pcm, size_t samples); // mic PCM to the server, while connected
constexpr size_t kAudioPacketMost = 512;                    // samples in one: an echo-cancelling frame, 32 ms
void link_pause(bool pause);          // the link closed while an update is fetched, and back after
const std::string &server_headers(); // the server's token and who this Cubey is, as HTTP header lines

// Updates over the air (update.cpp): a new firmware on trial, kept once it talks with the server; else, a few minutes
// on, rolled back.
void update_start();

// The CPU's speed, and the AXP2101: the charger, the PWR button and switching off (power.cpp).
void power_start();
void keys_start(); // "+" and "–", read on a timer of their own (hal::keys)
void audio_performance(bool active); // the audio task: full speed while the codecs run

// The speaker and mic (audio.cpp); the speech the server sends arrives here.
void audio_start();
bool speech_received(const uint8_t *data, size_t len); // false if the bounded buffer overflows

} // namespace cubey::board

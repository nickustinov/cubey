#pragma once

#include <cstddef>
#include <cstdint>

// The audio mixer (from Lapsa, through Puce): the platform's audio thread calls mix() with the speech already in the
// buffer, and the music goes in under it, softer while the speech plays. The UI thread switches the music.
namespace cubey::audio {

constexpr float kMusicLevel = 0.25f; // of full scale, alone
constexpr float kDucked = 0.3f;      // of that, under speech

void music(bool on);
bool music_on();
bool sounding(); // music on: the speaker stays awake
void mix(int16_t *out, size_t n, int rate, bool speech);

} // namespace cubey::audio

#include "services/mixer.h"

#include <atomic>

#include "services/music.h"

namespace cubey::audio {

static std::atomic<bool> wanted{false}; // set by the UI thread
// Owned by the audio thread.
static bool playing = false;
static Beat beat;
static float gain = 0;

void music(bool on) { wanted = on; }

bool music_on() { return wanted; }

bool sounding() { return wanted; }

void mix(int16_t *out, size_t n, int rate, bool speech) {
    const bool on = wanted;
    if (on && !playing) beat = Beat(); // from its start each time
    playing = on;
    // The gain moves to its target across the buffer, so changes never click.
    const float target = on ? (speech ? kDucked : 1.0f) : 0;
    const float step = n ? (target - gain) / static_cast<float>(n) : 0;
    for (size_t i = 0; i < n; i++) {
        gain += step;
        float v = out[i];
        if (gain > 0) v += beat.next(rate) * kMusicLevel * gain * 32767.0f;
        out[i] = static_cast<int16_t>(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    gain = target;
}

} // namespace cubey::audio

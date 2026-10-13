#include "services/music.h"

#include <cmath>

namespace cubey {

float Beat::noise() {
    rng_ ^= rng_ << 13; // xorshift32
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.0f - 1;
}

float Beat::next(int rate) {
    static const float kTune[16] = {523.3f, 659.3f, 784.0f, 659.3f, 880.0f, 784.0f, 659.3f, 587.3f,
                                    523.3f, 659.3f, 784.0f, 880.0f, 784.0f, 659.3f, 587.3f, 523.3f};
    const uint32_t eighth = static_cast<uint32_t>(rate) * kBeatMs / 2000;
    const uint32_t step = t_ / eighth;
    const float in = static_cast<float>(t_ % eighth) / rate; // seconds into this eighth
    const float attack = std::fmin(1.0f, in * 400);           // no click at the start of a note
    float out = 0;
    if (step % 2 == 0) {
        kick_ += (90 + 130 * std::exp(-in * 25)) / rate; // falling from 220 to 90 Hz
        out += 0.8f * attack * std::sin(6.2831853f * kick_) * std::exp(-in * 14);
    } else {
        out += 0.15f * noise() * std::exp(-in * 60);
    }
    tune_ += kTune[step % 16] / rate;
    const float triangle = 4 * std::fabs(tune_ - std::floor(tune_ + 0.5f)) - 1;
    out += 0.3f * attack * triangle * std::exp(-in * 8);
    kick_ -= std::floor(kick_);
    tune_ -= std::floor(tune_);
    t_++;
    return std::fmax(-1.0f, std::fmin(1.0f, out));
}

} // namespace cubey

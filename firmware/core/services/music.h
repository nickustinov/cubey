#pragma once

#include <cstdint>

namespace cubey {

constexpr uint32_t kBeatMs = 500; // 120 to the minute

// A cheerful beat made on the device one sample at a time, in -1..1 (Lapsa's freeze dance beat, Puce's brushing
// one): a kick on every beat, a soft hi-hat between, and a little tune of sixteen eighth notes on a triangle wave,
// round and round.
class Beat {
public:
    float next(int rate);

private:
    float noise();

    uint32_t t_ = 0;            // samples since the start
    uint32_t rng_ = 0x5EED;     // the hi-hat's noise
    float kick_ = 0, tune_ = 0; // the oscillators' phases, 0..1
};

} // namespace cubey

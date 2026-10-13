#pragma once

#include <cstdint>

namespace cubey {

// A timer she or a grown-up asked Cubey for (cartoons, tidying up): so many minutes from when it starts, the clock on
// the screen, Cubey quiet but for a reminder 5 minutes and 1 minute before its end.
class TimerRun {
public:
    static constexpr int kOver = -1;

    TimerRun(uint32_t now, uint32_t minutes) : started_(now), ms_(minutes * 60000) {}

    uint32_t left_ms(uint32_t now) const;
    uint32_t total_ms() const { return ms_; }
    // What the server should hear now, each once: 5 or 1 minutes left (when it ran longer than that; a late step
    // says only the latest), or the end (kOver); 0 for nothing.
    int news(uint32_t now);

private:
    uint32_t started_, ms_;
    int told_ = 0; // the last reminder told: 0, 5, 1, or kOver
};

} // namespace cubey

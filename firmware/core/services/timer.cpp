#include "services/timer.h"

#include "services/ticks.h"

namespace cubey {

uint32_t TimerRun::left_ms(uint32_t now) const {
    const uint32_t gone = since(now, started_);
    return gone >= ms_ ? 0 : ms_ - gone;
}

int TimerRun::news(uint32_t now) {
    const uint32_t left = left_ms(now);
    int due = 0;
    if (left == 0) due = kOver;
    else if (left <= 60000 && ms_ > 60000) due = 1;
    else if (left <= 5 * 60000 && ms_ > 5 * 60000) due = 5;
    if (!due || due == told_ || told_ == kOver || (told_ == 1 && due == 5)) return 0;
    return told_ = due;
}

} // namespace cubey

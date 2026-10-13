#include "services/upright.h"

#include <cmath>

#include "services/ticks.h"

namespace cubey {

constexpr float kSlackDeg = 15; // past half-way to the next quarter before it turns

int Upright::turn(Gravity down, uint32_t now) {
    int next = turn_;
    const float plane = hypotf(down.x, down.y);
    if (plane > 0.5f * hypotf(plane, down.z)) { // more than 30 degrees from lying flat
        const float degrees = atan2f(-down.x, down.y) * 180 / 3.14159265f; // 0: upright, 90: its left edge down
        const float off = remainderf(degrees - 90.0f * turn_, 360);
        if (fabsf(off) > 45 + kSlackDeg) next = (static_cast<int>(lroundf(degrees / 90)) % 4 + 4) % 4;
    }
    if (next != next_) {
        next_ = next;
        next_since_ = now;
    }
    if (next_ != turn_ && since(now, next_since_) >= kTurnHoldMs) turn_ = next_;
    return turn_;
}

} // namespace cubey

#pragma once

#include <cstdint>

namespace cubey {

// Which way is down, from the motion sensor, in the screen's own axes as she sees it upright: x to its right,
// y to its bottom, z out of it toward her. Any units; all 0 without a sensor.
struct Gravity {
    float x = 0, y = 0, z = 0;
};

constexpr uint32_t kTurnHoldMs = 400; // a new way must hold this long: carried or shaken, it doesn't flip

// The quarter turns, clockwise, that keep the picture upright however Cubey is held: it turns once down is
// 15 degrees past half-way to the next quarter and has stayed there for kTurnHoldMs; lying flat (within 30 degrees),
// it keeps the way it was.
class Upright {
public:
    int turn(Gravity down, uint32_t now);

private:
    int turn_ = 0, next_ = 0;
    uint32_t next_since_ = 0;
};

} // namespace cubey

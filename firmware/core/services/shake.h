#pragma once

#include <cstdint>

#include "services/upright.h"

namespace cubey {

// She shakes the cube (to roll the die): the motion sensor's reading swings far from its usual size, gravity alone,
// several times within a moment. A knock, or setting it down, is one jolt; turning it slowly keeps the size. Any units:
// the usual size is learned while the cube is calm. One shake, then a moment before the next counts.
class Shake {
public:
    bool shaken(Gravity g, uint32_t now);

private:
    static constexpr int kJolts = 6; // readings that jolt within kWithinMs (about 0.2 s of shaking): a shake
    float usual_ = 0;
    uint32_t jolts_[kJolts] = {};
    int next_ = 0;
    uint32_t quiet_until_ = 0;
};

} // namespace cubey

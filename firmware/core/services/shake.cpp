#include "services/shake.h"

#include <cmath>

#include "services/ticks.h"

namespace cubey {

// ponytail: guessed, until felt on the board: higher if setting it down rolls the die, lower if a shake doesn't.
constexpr float kJolt = 0.6f;  // off its usual size by this much of it: a jolt (1.6 g, or 0.4 g, in a fall)
constexpr float kCalm = 0.15f; // within this, the cube is still, and its usual size is learned
constexpr uint32_t kWithinMs = 600;
constexpr uint32_t kRestMs = 1500; // after a shake, before the next one counts

bool Shake::shaken(Gravity g, uint32_t now) {
    const float size = std::sqrt(g.x * g.x + g.y * g.y + g.z * g.z);
    if (size == 0) return false; // no sensor
    if (usual_ == 0) usual_ = size;
    const float off = std::fabs(size - usual_) / usual_;
    if (off < kCalm) usual_ += (size - usual_) * 0.05f;
    if (off < kJolt || static_cast<int32_t>(now - quiet_until_) < 0) return false;
    jolts_[next_] = now;
    next_ = (next_ + 1) % kJolts;
    const uint32_t oldest = jolts_[next_]; // the kJolts-th jolt back
    if (oldest == 0 || since(now, oldest) > kWithinMs) return false;
    for (uint32_t &t : jolts_) t = 0;
    quiet_until_ = now + kRestMs;
    return true;
}

} // namespace cubey

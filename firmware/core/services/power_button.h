#pragma once

#include <cstdint>

namespace cubey {

// The side power button: awake, a press is a step back (from a routine, brushing or the battery, to Cubey's eyes;
// from its eyes to sleep) and a long press puts Cubey to sleep (the screen dark, the talk over); asleep, a press
// wakes it. Nothing else wakes it. With nothing going on for 45 seconds (no talk: Cubey offline), it sleeps by itself, and
// so it does when she says goodbye or goes quiet (sleep()). Off is the power chip's own, held longer.
enum class PowerPress : uint8_t { None, Short, Long };
enum class PowerAction : uint8_t { None, Back, Sleep, Wake };

class PowerButton {
public:
    static constexpr uint32_t kIdleSleepMs = 45 * 1000;

    PowerAction press(PowerPress press, uint32_t now);
    PowerAction sleep();
    PowerAction idle(uint32_t now, bool busy); // every so often: Sleep once idle long enough
    bool asleep() const { return asleep_; }

private:
    bool asleep_ = false;
    uint32_t active_at_ = 0; // the last wake, or moment something was going on
};

} // namespace cubey

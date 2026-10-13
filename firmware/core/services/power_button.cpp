#include "services/power_button.h"

#include "services/ticks.h"

namespace cubey {

PowerAction PowerButton::press(PowerPress press, uint32_t now) {
    if (press == PowerPress::None) return PowerAction::None;
    if (!asleep_) return press == PowerPress::Long ? sleep() : PowerAction::Back;
    asleep_ = false;
    active_at_ = now;
    return PowerAction::Wake;
}

PowerAction PowerButton::sleep() {
    if (asleep_) return PowerAction::None;
    asleep_ = true;
    return PowerAction::Sleep;
}

PowerAction PowerButton::idle(uint32_t now, bool busy) {
    if (busy) active_at_ = now;
    if (asleep_ || since(now, active_at_) < kIdleSleepMs) return PowerAction::None;
    return sleep();
}

} // namespace cubey

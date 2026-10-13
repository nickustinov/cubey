#include "services/screen_hold.h"

#include "services/ticks.h"

namespace cubey {

ScreenHold::Event ScreenHold::follow(bool down, uint32_t now) {
    if (down && !down_) since_ = now; // a finger comes
    down_ = down;
    if (down && !held_ && since(now, since_) >= kMs) {
        held_ = true;
        return Event::Held;
    }
    if (!down && held_) {
        held_ = false;
        return Event::Let;
    }
    return Event::None;
}

} // namespace cubey

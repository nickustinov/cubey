#pragma once

namespace cubey {

// The device's battery, as the platform reads it: 0..100, or -1 while it can't tell.
struct Battery {
    int percent = -1;
    bool charging = false;
};

} // namespace cubey

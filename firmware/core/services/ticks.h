#pragma once

#include <cstdint>

namespace cubey {

// Milliseconds from `then` to `now` on the 32-bit tick clock, across its wrap. A reading taken a moment before
// `then` (code that read the clock earlier in the same step) is no time gone, not a whole wrap of it.
inline uint32_t since(uint32_t now, uint32_t then) {
    const auto gone = static_cast<int32_t>(now - then);
    return gone > 0 ? static_cast<uint32_t>(gone) : 0;
}

} // namespace cubey

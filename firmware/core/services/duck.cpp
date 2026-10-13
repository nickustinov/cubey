#include "services/duck.h"

#include <algorithm>

namespace cubey {

constexpr int kRiseFrames = 16; // 320 ms

float duck_gain(float gain, bool ducked) {
    return ducked ? kDuckGain : std::min(1.0f, gain + (1 - kDuckGain) / kRiseFrames);
}

} // namespace cubey

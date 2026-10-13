#pragma once

namespace cubey {

// The character's speech goes quieter while she talks over it (the board's voice detector hears her through the
// cancelled echo): at full volume what the cancelling leaves of her voice is too little for the server to hear,
// 12 dB down it is enough. Down at once, back up over a third of a second.
constexpr float kDuckGain = 0.2f; // -14 dB

// The speech's gain for the next speaker frame (20 ms), from the last one's.
float duck_gain(float gain, bool ducked);

} // namespace cubey

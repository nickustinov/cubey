#pragma once

#include <cstdint>

namespace cubey {

// Her voice over Cubey's speech, from the cleaned mic (its echo cancelled), frame by frame: speech (as the platform's
// detector says) louder than both a floor and the room's background by kAboveDb. A TV or people across the room are
// speech too, but quieter at the cube than she is, talking to it: the background follows the room while Cubey speaks
// (the cleaned mic's level over a few seconds, in dB), so a TV raises the bar and a quiet room lowers it to the floor;
// her voice is in it too, but she has talked over Cubey long before it rises much. Heard kTalkOverMs, with gaps
// between her words, she talks over it.
class TalkOver {
public:
    static constexpr float kAboveDb = 10;           // ponytail: tune on the board from its log ("background")
    static constexpr uint32_t kBackgroundMs = 3000; // how slowly the background follows the room
    static constexpr uint32_t kTalkOverMs = 250;
    static constexpr uint32_t kGapMs = 160; // between her words: no longer, or it starts over

    explicit TalkOver(float floor_dbfs) : floor_(floor_dbfs) {}

    // A frame of the cleaned mic, ms long, this loud (dBFS); speaking: Cubey's speech plays and the server listens.
    // True while she is heard (the speech ducks).
    bool frame(bool speaking, bool speech, float dbfs, uint32_t ms);
    bool talking_over() const { return heard_ms_ >= kTalkOverMs; }
    float background() const { return background_; }

private:
    float floor_;
    float background_ = -70;
    uint32_t heard_ms_ = 0, gap_ms_ = 0; // her voice in this run, and the gap since it was last heard
};

} // namespace cubey

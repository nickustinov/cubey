#include "services/dice.h"

#include "services/ticks.h"

namespace cubey {

constexpr uint32_t kFirstFrameMs = 60;
constexpr float kSlower = 1.3f; // each face shows this much longer than the one before

DiceRoll::DiceRoll(uint32_t now, const std::function<uint32_t(uint32_t)> &pick) : started_(now) {
    const auto off = [&] { return static_cast<int>(pick(3)) - 1; };
    int face = static_cast<int>(pick(6)) + 1;
    float step = kFirstFrameMs;
    for (float at = 0; at < kRollMs; at += step, step *= kSlower) {
        frames_.push_back({static_cast<uint32_t>(at), face, off(), off()});
        face = (face + static_cast<int>(pick(5))) % 6 + 1; // another face each time
    }
    frames_.back().dx = frames_.back().dy = 0; // the last face: landed
}

const DiceRoll::Frame &DiceRoll::frame(uint32_t now) const {
    const uint32_t t = since(now, started_);
    size_t i = 0;
    while (i + 1 < frames_.size() && frames_[i + 1].at <= t) i++;
    return frames_[i];
}

bool DiceRoll::landed(uint32_t now) const { return since(now, started_) >= frames_.back().at; }

} // namespace cubey

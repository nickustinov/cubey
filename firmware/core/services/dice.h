#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace cubey {

// A roll of the die (the dice routine): it tumbles, showing one face after another, a little off its place, slower and
// slower, and lands on its number within kRollMs. pick(n): a random number below n.
class DiceRoll {
public:
    static constexpr uint32_t kRollMs = 1100;

    struct Frame {
        uint32_t at; // ms after the roll began
        int face;    // 1..6
        int dx, dy;  // dots off its place: -1, 0 or 1; landed, 0
    };

    DiceRoll(uint32_t now, const std::function<uint32_t(uint32_t)> &pick);

    const Frame &frame(uint32_t now) const; // what shows now
    int number() const { return frames_.back().face; }
    bool landed(uint32_t now) const;

private:
    uint32_t started_;
    std::vector<Frame> frames_;
};

} // namespace cubey

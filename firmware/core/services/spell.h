#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace cubey {

// A word Cubey spells (ui/spell.h): its letters show one by one as its voice says them. The server says when each is
// said, in ms into Cubey's speech (Soniox times what it speaks); the device knows when its speech began to play. A
// letter never timed holds back those after it until the speech is over, then they all show. All shown: still a
// moment, then the whole word passes twice (or stays still a while, if it fits), and it is over.
class SpellRun {
public:
    static constexpr uint32_t kHoldMs = 800;   // all said, before the whole word passes
    static constexpr int kPasses = 2;
    static constexpr uint32_t kStillMs = 3000; // a word that fits, still instead

    // pass_ms: one pass of the whole word across the board (0: it fits, still)
    SpellRun(int letters, uint32_t pass_ms) : said_(static_cast<size_t>(letters)), pass_ms_(pass_ms) {}

    void said(int i, uint32_t ms); // letter i is said ms into Cubey's speech
    void speaking(uint32_t now);   // its speech has begun playing (its first start counts)
    void spoken(uint32_t now);     // its speech is over, or Cubey's turn ended without it (its first end counts)

    int shown(uint32_t now) const;             // how many of its letters show, from the first
    uint32_t shown_since(uint32_t now) const;  // ms since the last of them showed
    std::optional<uint32_t> passing(uint32_t now) const; // ms into the whole word's passes, once they have begun
    bool over(uint32_t now) const;

private:
    std::optional<uint32_t> at(int i) const; // the moment letter i shows, once known

    std::vector<std::optional<uint32_t>> said_;
    std::optional<uint32_t> start_, end_;
    uint32_t pass_ms_;
};

} // namespace cubey

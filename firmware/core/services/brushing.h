#pragma once

#include <cstdint>

namespace cubey {

// Brushing with Cubey (from Lapsa's fox, through Puce): two minutes, the mouth in four parts of 30 s each, round from
// the top on one side. Cubey's mouth shows while she fetches her toothbrush; the two minutes start at her tap.
constexpr uint32_t kBrushingMs = 120000;
constexpr int kMouthParts = 4;

struct BrushingAt {
    bool done;
    int part;            // 0..3: the top on the left as she sees it, the top on the right, the bottom on the right, on the left
    float part_progress; // 0..1 of this part's 30 s
};
BrushingAt brushing_at(uint32_t elapsed_ms);

// One brushing, from Cubey's mouth showing.
class BrushingRun {
public:
    static constexpr uint32_t kReadyMostMs = 60000; // no tap by then: it's off
    static constexpr uint32_t kShineMs = 6000;      // done, its clean teeth shine while Cubey praises her
    static constexpr int kDone = kMouthParts + 1, kMissed = -1;

    explicit BrushingRun(uint32_t shown) : shown_(shown) {}

    bool start(uint32_t now); // her tap: the two minutes start; false once they have
    bool started() const { return started_; }
    uint32_t elapsed(uint32_t now) const; // of the two minutes: 0 until her tap
    // What the server should hear now, each once: a part has begun (1..4: her tap begins the first; a late step says
    // only the part it is in), the end (kDone), or no tap (kMissed); 0 for nothing.
    int news(uint32_t now);
    // Over: kShineMs after the end, or with no tap; finished at the end, or with no tap.
    bool over(uint32_t now) const;
    bool finished() const { return done_ || missed_; }

private:
    uint32_t shown_, started_at_ = 0, done_at_ = 0;
    bool started_ = false, done_ = false, missed_ = false;
    int told_ = 0; // the part last told
};

} // namespace cubey

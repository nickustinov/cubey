#include "services/brushing.h"

#include "services/ticks.h"

namespace cubey {

constexpr uint32_t kPartMs = kBrushingMs / kMouthParts;

BrushingAt brushing_at(uint32_t elapsed_ms) {
    if (elapsed_ms >= kBrushingMs) return {true, kMouthParts - 1, 1};
    return {false, static_cast<int>(elapsed_ms / kPartMs), static_cast<float>(elapsed_ms % kPartMs) / kPartMs};
}

bool BrushingRun::start(uint32_t now) {
    if (started_ || missed_) return false;
    started_ = true;
    started_at_ = now;
    return true;
}

uint32_t BrushingRun::elapsed(uint32_t now) const { return started_ ? since(now, started_at_) : 0; }

int BrushingRun::news(uint32_t now) {
    if (!started_) {
        if (missed_ || since(now, shown_) < kReadyMostMs) return 0;
        missed_ = true;
        return kMissed;
    }
    const BrushingAt at = brushing_at(elapsed(now));
    if (at.done) {
        if (done_) return 0;
        done_ = true;
        done_at_ = now;
        return kDone;
    }
    if (at.part + 1 == told_) return 0;
    return told_ = at.part + 1;
}

bool BrushingRun::over(uint32_t now) const { return missed_ || (done_ && since(now, done_at_) >= kShineMs); }

} // namespace cubey

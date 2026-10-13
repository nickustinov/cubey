#include "services/talk_over.h"

#include <algorithm>

namespace cubey {

bool TalkOver::frame(bool speaking, bool speech, float dbfs, uint32_t ms) {
    if (!speaking) {
        heard_ms_ = gap_ms_ = 0;
        return false;
    }
    const bool heard = speech && dbfs > std::max(floor_, background_ + kAboveDb);
    background_ += (dbfs - background_) * std::min(1.0f, static_cast<float>(ms) / kBackgroundMs);
    if (heard) heard_ms_ += ms, gap_ms_ = 0;
    else if (heard_ms_ && (gap_ms_ += ms) > kGapMs) heard_ms_ = gap_ms_ = 0; // longer than between her words
    return heard;
}

} // namespace cubey

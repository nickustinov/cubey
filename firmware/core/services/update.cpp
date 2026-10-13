#include "services/update.h"

#include "services/ticks.h"

namespace cubey {

void Updater::offer(const std::string &build) {
    if (!started_ && build != failed_) build_ = build;
}

Updater::Do Updater::step(bool free, const UpdateProgress &progress, uint32_t now) {
    if (!started_) {
        if (build_.empty() || !free) return Do::Nothing;
        started_ = true, restarted_ = false, ended_at_.reset();
        return Do::Start;
    }
    if (progress.state != UpdateState::Done && progress.state != UpdateState::Failed) return Do::Nothing;
    if (!ended_at_) ended_at_ = now;
    if (progress.state == UpdateState::Done) {
        if (restarted_ || since(now, *ended_at_) < kDoneMs) return Do::Nothing;
        restarted_ = true;
        return Do::Restart;
    }
    if (since(now, *ended_at_) < kFailedMs) return Do::Nothing;
    failed_ = build_;
    build_.clear();
    started_ = false;
    return Do::End;
}

} // namespace cubey

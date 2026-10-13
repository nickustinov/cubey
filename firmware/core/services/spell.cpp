#include "services/spell.h"

#include "services/ticks.h"

namespace cubey {

namespace {
// Whether `then` has come by `now` on the tick clock, across its wrap.
bool come(uint32_t now, uint32_t then) { return static_cast<int32_t>(now - then) >= 0; }
} // namespace

void SpellRun::said(int i, uint32_t ms) {
    if (i >= 0 && i < static_cast<int>(said_.size())) said_[static_cast<size_t>(i)] = ms;
}

void SpellRun::speaking(uint32_t now) {
    if (!start_) start_ = now;
}

void SpellRun::spoken(uint32_t now) {
    if (!end_) end_ = now;
}

std::optional<uint32_t> SpellRun::at(int i) const {
    std::optional<uint32_t> before;
    for (int j = 0; j <= i; j++) {
        const std::optional<uint32_t> &ms = said_[static_cast<size_t>(j)];
        std::optional<uint32_t> shows = start_ && ms ? std::optional<uint32_t>(*start_ + *ms) : std::nullopt;
        if (end_ && (!shows || !come(*end_, *shows))) shows = end_; // not timed, or timed past its speech's end
        if (!shows) return std::nullopt;
        if (before && !come(*shows, *before)) shows = before; // never before the letter before it
        before = shows;
    }
    return before;
}

int SpellRun::shown(uint32_t now) const {
    int n = 0;
    while (n < static_cast<int>(said_.size())) {
        const std::optional<uint32_t> shows = at(n);
        if (!shows || !come(now, *shows)) break;
        n++;
    }
    return n;
}

uint32_t SpellRun::shown_since(uint32_t now) const {
    const int n = shown(now);
    return n ? since(now, *at(n - 1)) : 0;
}

std::optional<uint32_t> SpellRun::passing(uint32_t now) const {
    const int n = static_cast<int>(said_.size());
    if (!n || shown(now) < n) return std::nullopt;
    const uint32_t begins = *at(n - 1) + kHoldMs;
    if (!come(now, begins)) return std::nullopt;
    return since(now, begins);
}

bool SpellRun::over(uint32_t now) const {
    const std::optional<uint32_t> ms = passing(now);
    return ms && *ms >= (pass_ms_ ? kPasses * pass_ms_ : kStillMs);
}

} // namespace cubey

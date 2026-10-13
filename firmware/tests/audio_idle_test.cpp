#include <doctest/doctest.h>

#include <cstdint>

#include "services/audio_idle.h"

using namespace cubey;

TEST_CASE("unused audio stays parked; the mic, speech or music wakes it, and a while without any parks it") {
    // Longer than Cubey takes to answer once the mic rests: at a second its first words waited for the codecs to
    // settle, on most turns (2026-10-11).
    CHECK(AudioIdle::kIdleMs >= 2500);
    for (int source = 0; source < 3; ++source) {
        AudioIdle idle;
        CHECK_FALSE(idle.update(0, false, false, false));
        CHECK(idle.update(100, source == 0, source == 1, source == 2));
        CHECK(idle.update(100 + AudioIdle::kIdleMs - 1, false, false, false));
        CHECK_FALSE(idle.update(100 + AudioIdle::kIdleMs, false, false, false));
    }
}

TEST_CASE("audio idle timeout works across clock wrap") {
    AudioIdle idle;
    const uint32_t start = UINT32_MAX - 500;
    CHECK(idle.update(start, false, true, false));
    CHECK(idle.update(start + AudioIdle::kIdleMs - 1, false, false, false));
    CHECK_FALSE(idle.update(start + AudioIdle::kIdleMs, false, false, false));
}

TEST_CASE("speech waits while the codec and amplifier settle after waking") {
    AudioIdle idle;
    CHECK_FALSE(idle.settled(0)); // parked: nothing to play into
    idle.woke(1000);
    CHECK_FALSE(idle.settled(1000));
    CHECK_FALSE(idle.settled(1000 + AudioIdle::kSettleMs - 1));
    CHECK(idle.settled(1000 + AudioIdle::kSettleMs));
    idle.parked();
    CHECK_FALSE(idle.settled(5000));
    idle.woke(UINT32_MAX - 10); // across clock wrap
    CHECK(idle.settled(AudioIdle::kSettleMs - 11));
}

#include <doctest/doctest.h>

#include "services/update.h"

using namespace cubey;
using Do = Updater::Do;

static const std::string kBuild = "0123456789abcdef", kOther = "fedcba9876543210";

TEST_CASE("an update offered waits until Cubey is free, then starts, once; another offered meanwhile is nothing") {
    Updater update;
    CHECK_FALSE(update.waiting());
    update.offer(kBuild);
    CHECK(update.waiting());
    CHECK(update.step(false, {}, 0) == Do::Nothing); // someone talks, or plays
    CHECK(update.waiting());
    CHECK(update.step(true, {}, 100) == Do::Start);
    CHECK(update.running());
    CHECK_FALSE(update.waiting());
    CHECK(update.build() == kBuild);
    CHECK(update.step(true, {UpdateState::Fetching, 40}, 200) == Do::Nothing);
    update.offer(kOther);
    CHECK(update.build() == kBuild);
    CHECK(update.step(true, {UpdateState::Fetching, 60}, 300) == Do::Nothing);
}

TEST_CASE("done: a moment to see it, then the restart into it, once") {
    Updater update;
    update.offer(kBuild);
    update.step(true, {}, 0);
    CHECK(update.step(true, {UpdateState::Done, 100}, 1000) == Do::Nothing);
    CHECK(update.step(true, {UpdateState::Done, 100}, 1000 + Updater::kDoneMs - 1) == Do::Nothing);
    CHECK(update.step(true, {UpdateState::Done, 100}, 1000 + Updater::kDoneMs) == Do::Restart);
    CHECK(update.step(true, {UpdateState::Done, 100}, 1000 + Updater::kDoneMs + 30) == Do::Nothing);
}

TEST_CASE("failed: a moment, then over, Cubey as it was; the same build offered again isn't tried until it restarts") {
    Updater update;
    update.offer(kBuild);
    update.step(true, {}, 0);
    CHECK(update.step(true, {UpdateState::Failed, 60}, 1000) == Do::Nothing);
    CHECK(update.running());
    CHECK(update.step(true, {UpdateState::Failed, 60}, 1000 + Updater::kFailedMs) == Do::End);
    CHECK_FALSE(update.running());
    CHECK_FALSE(update.waiting());
    CHECK(update.failed() == kBuild); // for the server to hear
    update.offer(kBuild); // the server's, again at its hello, before it heard
    CHECK_FALSE(update.waiting());
    update.offer(kOther);
    CHECK(update.waiting());
}

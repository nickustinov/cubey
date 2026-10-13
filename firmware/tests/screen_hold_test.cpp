#include <doctest/doctest.h>

#include "services/screen_hold.h"

using namespace cubey;
using Event = ScreenHold::Event;

TEST_CASE("a finger on the screen: held a moment, it is held until it lets go; a tap is nothing") {
    ScreenHold hold;
    CHECK(hold.follow(false, 0) == Event::None);
    CHECK(hold.follow(true, 100) == Event::None);
    CHECK(hold.follow(true, 100 + ScreenHold::kMs - 1) == Event::None);
    CHECK(hold.follow(true, 100 + ScreenHold::kMs) == Event::Held);
    CHECK(hold.follow(true, 5000) == Event::None); // once
    CHECK(hold.follow(false, 5030) == Event::Let);
    CHECK(hold.follow(false, 5060) == Event::None);

    CHECK(hold.follow(true, 6000) == Event::None); // a tap
    CHECK(hold.follow(false, 6000 + ScreenHold::kMs - 30) == Event::None);
    CHECK(hold.follow(true, 7000) == Event::None); // the next, timed anew
    CHECK(hold.follow(true, 7000 + ScreenHold::kMs - 1) == Event::None);
    CHECK(hold.follow(true, 7000 + ScreenHold::kMs) == Event::Held);
}

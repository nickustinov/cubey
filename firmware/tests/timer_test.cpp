#include <doctest/doctest.h>

#include "services/timer.h"

using namespace cubey;

TEST_CASE("a timer counts down its minutes from when it starts") {
    TimerRun timer(1000, 40);
    CHECK(timer.left_ms(1000) == 40 * 60000u);
    CHECK(timer.left_ms(61000) == 39 * 60000u);
    CHECK(timer.left_ms(1000 + 41 * 60000) == 0);
}

TEST_CASE("the server hears when 5 minutes are left and when 1 is, each once, then the end") {
    TimerRun timer(0, 40);
    CHECK(timer.news(34 * 60000) == 0);
    CHECK(timer.news(35 * 60000) == 5);
    CHECK(timer.news(35 * 60000 + 1000) == 0);
    CHECK(timer.news(39 * 60000) == 1);
    CHECK(timer.news(40 * 60000 - 1) == 0);
    CHECK(timer.news(40 * 60000) == TimerRun::kOver);
    CHECK(timer.news(40 * 60000 + 1000) == 0);
}

TEST_CASE("a short timer has only the reminders it outlasts; a late step says only the latest") {
    TimerRun three(0, 3);
    CHECK(three.news(0) == 0);
    CHECK(three.news(2 * 60000) == 1);
    TimerRun one(0, 1);
    CHECK(one.news(30000) == 0);
    CHECK(one.news(60000) == TimerRun::kOver);
    TimerRun late(0, 40);
    CHECK(late.news(39 * 60000 + 500) == 1); // the 5-minute one passed by unheard
}

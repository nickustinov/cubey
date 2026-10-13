#include <doctest/doctest.h>

#include "services/power_button.h"

using namespace cubey;

TEST_CASE("awake, a press is a step back (a routine to Cubey's eyes, the eyes to sleep) and a long one sleeps; asleep, a press wakes it") {
    PowerButton button;
    CHECK(button.press(PowerPress::None, 0) == PowerAction::None);
    CHECK(button.press(PowerPress::Short, 0) == PowerAction::Back);
    CHECK_FALSE(button.asleep()); // the core steps back: to the eyes, or sleep()
    CHECK(button.press(PowerPress::Long, 100) == PowerAction::Sleep);
    CHECK(button.asleep());
    CHECK(button.press(PowerPress::Short, 300) == PowerAction::Wake);
    CHECK_FALSE(button.asleep());
    button.press(PowerPress::Long, 400);
    CHECK(button.press(PowerPress::Long, 500) == PowerAction::Wake);
}

TEST_CASE("its goodbye puts it to sleep; already asleep, nothing more") {
    PowerButton button;
    CHECK(button.sleep() == PowerAction::Sleep);
    CHECK(button.asleep());
    CHECK(button.sleep() == PowerAction::None);
}

TEST_CASE("with no talk going on for 45 seconds, it sleeps by itself; a talk keeps it awake") {
    CHECK(PowerButton::kIdleSleepMs == 45000);
    PowerButton button;
    CHECK(button.idle(PowerButton::kIdleSleepMs - 1, false) == PowerAction::None);
    CHECK(button.idle(500000, true) == PowerAction::None); // she is talking to Cubey
    CHECK(button.idle(500000 + PowerButton::kIdleSleepMs - 1, false) == PowerAction::None);
    CHECK(button.idle(500000 + PowerButton::kIdleSleepMs, false) == PowerAction::Sleep);
    CHECK(button.idle(10 * PowerButton::kIdleSleepMs, false) == PowerAction::None); // asleep already
}

TEST_CASE("woken by the button, it waits another 45 seconds before sleeping again") {
    PowerButton button;
    button.idle(PowerButton::kIdleSleepMs, false);
    CHECK(button.press(PowerPress::Short, 900000) == PowerAction::Wake);
    CHECK(button.idle(900001, false) == PowerAction::None);
    CHECK(button.idle(900000 + PowerButton::kIdleSleepMs, false) == PowerAction::Sleep);
}

TEST_CASE("the 45 seconds count across the clock's wrap, and a reading from just before is no time at all") {
    PowerButton button;
    button.idle(UINT32_MAX - 1000, true);
    CHECK(button.idle(PowerButton::kIdleSleepMs - 1002, false) == PowerAction::None);
    CHECK(button.idle(PowerButton::kIdleSleepMs - 1001, false) == PowerAction::Sleep);
    PowerButton other;
    other.idle(500000, true);
    CHECK(other.idle(499990, false) == PowerAction::None);
}

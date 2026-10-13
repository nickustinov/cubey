#include <doctest/doctest.h>

#include <cmath>

#include "services/upright.h"

using namespace cubey;

// Gravity `degrees` round from the screen's bottom edge, clockwise as she sees it (90: its left edge is down).
static Gravity down_at(float degrees) {
    const float a = degrees * 3.14159265f / 180;
    return {-sinf(a), cosf(a), 0};
}

// Held still for long enough to count.
static int settled(Upright &upright, Gravity g, uint32_t &now) {
    upright.turn(g, now);
    now += kTurnHoldMs;
    return upright.turn(g, now);
}

TEST_CASE("the picture turns a quarter at a time so its bottom is down, however Cubey is held") {
    Upright upright;
    uint32_t now = 1000;
    CHECK(settled(upright, down_at(0), now) == 0);
    CHECK(settled(upright, down_at(90), now) == 1);  // its left edge down: the picture turned a quarter clockwise
    CHECK(settled(upright, down_at(180), now) == 2); // upside down
    CHECK(settled(upright, down_at(270), now) == 3);
    CHECK(settled(upright, down_at(0), now) == 0);
}

TEST_CASE("it stays put until held well past half-way, so it doesn't flip back and forth at the corner") {
    Upright upright;
    uint32_t now = 1000;
    CHECK(settled(upright, down_at(55), now) == 0);
    CHECK(settled(upright, down_at(-55), now) == 0);
    CHECK(settled(upright, down_at(65), now) == 1);
    CHECK(settled(upright, down_at(35), now) == 1); // back past half-way, not far enough
    CHECK(settled(upright, down_at(20), now) == 0);
}

TEST_CASE("a moment's tilt or a shake doesn't turn it: the new way must hold") {
    Upright upright;
    uint32_t now = 1000;
    CHECK(upright.turn(down_at(180), now) == 0);
    CHECK(upright.turn(down_at(180), now + kTurnHoldMs - 50) == 0);
    CHECK(upright.turn(down_at(0), now + kTurnHoldMs) == 0); // back before it held
    CHECK(upright.turn(down_at(180), now + kTurnHoldMs + 10) == 0); // a new start
    CHECK(upright.turn(down_at(180), now + 2 * kTurnHoldMs + 10) == 2);
}

TEST_CASE("lying flat, or with no sensor, it keeps the way it was") {
    Upright upright;
    uint32_t now = 1000;
    CHECK(settled(upright, down_at(180), now) == 2);
    CHECK(settled(upright, {0.2f, 0.1f, 1}, now) == 2); // on its back on the table, a little tilted
    CHECK(settled(upright, {0, 0, -1}, now) == 2);      // face down
    CHECK(settled(upright, {0, 0, 0}, now) == 2);       // no sensor
    CHECK(settled(upright, {0, 4, 0}, now) == 0);       // in whatever units: only the way matters
}

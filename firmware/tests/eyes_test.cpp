#include <doctest/doctest.h>

#include <bitset>
#include <cstdlib>
#include <vector>

#include "ui/eyes.h"

using namespace cubey::eyes;

static int lit(const Frame &frame) {
    int n = 0;
    for (uint8_t row : frame) n += static_cast<int>(std::bitset<8>(row).count());
    return n;
}

TEST_CASE("an open eye is a solid upright rectangle of dots, its corners cut, no pupil") {
    const Frame &eye = open();
    CHECK(kWide < kTall);
    const uint8_t full = (1 << kWide) - 1;
    CHECK(eye.front() == (full & ~1 & ~(1 << (kWide - 1)))); // the corners cut
    CHECK(eye.back() == eye.front());
    for (int row = 1; row < kTall - 1; row++) CHECK(eye[row] == full); // solid
}

TEST_CASE("shut, an eye is one line, low down") {
    const Frame &eye = shut();
    CHECK(lit(eye) == kWide);
    CHECK(eye[kTall - 2] == (1 << kWide) - 1);
}

TEST_CASE("a blink: its lid comes down to shut and goes up again, a frame every kFrameMs, then it's over") {
    std::vector<const Frame *> frames;
    for (uint32_t ms = 0; blink(ms); ms += kFrameMs) frames.push_back(blink(ms));
    REQUIRE(frames.size() >= 3);
    CHECK(frames.size() * kFrameMs < 500); // still a blink
    size_t closed = 0;
    while (frames[closed] != &shut()) closed++;
    for (size_t i = 1; i <= closed; i++) CHECK(lit(*frames[i]) < lit(*frames[i - 1])); // down
    for (size_t i = closed + 1; i < frames.size(); i++) CHECK(lit(*frames[i]) > lit(*frames[i - 1])); // up
    CHECK(lit(*frames.front()) < lit(open()));
    CHECK(lit(*frames.back()) < lit(open()));
    CHECK(blink(kFrameMs - 1) == frames[0]); // a frame holds for its whole time
    CHECK(blink(static_cast<uint32_t>(frames.size()) * kFrameMs) == nullptr);
}

TEST_CASE("the eyes sit in the middle of the dot board, side by side, a little above its middle") {
    const Place place = place_on(24);
    CHECK(place.left + kWide + kGap + kWide + place.left == 24); // as far from either side
    CHECK(place.top + kTall <= 24);
    CHECK(place.top + kTall / 2.0f <= 12); // not below the middle
    CHECK(place.top + kTall / 2.0f >= 10);
}

// A random source that gives these numbers in turn (each below what's asked for), then 0.
static std::vector<uint32_t> script;
static uint32_t scripted(uint32_t below) {
    if (script.empty()) return 0;
    const uint32_t n = script.front();
    script.erase(script.begin());
    return n % below;
}

TEST_CASE("idle, the eyes look ahead at first, then now and then glance aside a dot a frame, hold it, and come back") {
    script = {3000, 0, 1, 1, 700}; // after 2.5 s + 3000 ms: a glance, 2 dots right, level, held 0.8 s + 700 ms
    Wander wander(scripted, 0);
    CHECK(wander.at(0).dx == 0);
    const uint32_t start = 2500 + 3000;
    CHECK(wander.at(start - 1).dx == 0);
    CHECK(wander.at(start).dx == 1); // one dot a frame
    CHECK(wander.at(start + kFrameMs - 1).dx == 1);
    CHECK(wander.at(start + kFrameMs).dx == 2);
    CHECK(wander.at(start + 1000).dx == 2); // held
    const uint32_t back = start + 800 + 700;
    CHECK(wander.at(back).dx == 1);
    CHECK(wander.at(back + kFrameMs).dx == 0);
    CHECK(wander.at(back + 2000).dx == 0);
}

TEST_CASE("a glance is a dot or two either way, now and then a dot up too, never off the board") {
    for (uint32_t way = 0; way < 6; way++)
        for (uint32_t up = 0; up < 4; up++) {
            script = {0, 0, way, up, 0};
            Wander wander(scripted, 0);
            const Look look = wander.at(2500 + 10 * kFrameMs);
            CHECK(look.dx != 0);
            CHECK(std::abs(look.dx) <= 2);
            CHECK(look.dy == (up == 0 ? -1 : 0));
        }
}

TEST_CASE("now and then a squint instead, for a moment; or nothing at all") {
    script = {0, 60, 500}; // a squint, 1.5 s
    Wander squinting(scripted, 0);
    CHECK(squinting.at(2500).squint);
    CHECK(squinting.at(2500 + 1499).squint);
    CHECK_FALSE(squinting.at(2500 + 1500).squint);
    script = {0, 90};
    Wander resting(scripted, 0);
    const Look look = resting.at(2500 + 1000);
    CHECK(look.dx == 0);
    CHECK_FALSE(look.squint);
}

TEST_CASE("a squint lowers the lids a little; looking far aside, the far eye is a little smaller") {
    CHECK(lit(squinted()) < lit(open()));
    CHECK(lit(squinted()) > lit(shut()));
    CHECK(lit(farther()) < lit(open()));
    CHECK(farther()[0] == 0);
    CHECK(farther()[kTall - 1] == 0);
}

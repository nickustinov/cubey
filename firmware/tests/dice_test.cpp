#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#include "services/dice.h"
#include "services/shake.h"

using namespace cubey;

// Picks that go round, so every face and offset comes up.
static std::function<uint32_t(uint32_t)> picks(uint32_t from = 0) {
    return [n = from](uint32_t below) mutable { return n++ % below; };
}

TEST_CASE("a roll: faces one after another, each another, slower and slower, landing on its number within kRollMs") {
    const DiceRoll roll(1000, picks());
    std::vector<uint32_t> changes;
    int last = roll.frame(1000).face;
    for (uint32_t t = 1000; t <= 1000 + DiceRoll::kRollMs; t++) {
        const DiceRoll::Frame &f = roll.frame(t);
        CHECK(f.face >= 1);
        CHECK(f.face <= 6);
        CHECK(std::abs(f.dx) <= 1);
        CHECK(std::abs(f.dy) <= 1);
        if (f.face != last) changes.push_back(t), last = f.face;
    }
    CHECK(changes.size() >= 5);
    for (size_t i = 2; i < changes.size(); i++) CHECK(changes[i] - changes[i - 1] + 1 >= changes[i - 1] - changes[i - 2]); // slower
    CHECK_FALSE(roll.landed(1000 + DiceRoll::kRollMs / 2));
    CHECK(roll.landed(1000 + DiceRoll::kRollMs));
    CHECK(roll.landed(changes.back()));
    CHECK_FALSE(roll.landed(changes.back() - 1));
    const DiceRoll::Frame &landed = roll.frame(1000 + DiceRoll::kRollMs + 5000);
    CHECK(landed.face == roll.number());
    CHECK(landed.dx == 0);
    CHECK(landed.dy == 0);
}

TEST_CASE("rolls land on every number") {
    bool seen[7] = {};
    for (uint32_t from = 0; from < 60; from++) seen[DiceRoll(0, picks(from)).number()] = true;
    for (int n = 1; n <= 6; n++) CHECK(seen[n]);
}

TEST_CASE("a shake: the reading swings several times within a moment; a knock, turning it or no sensor isn't one") {
    Shake shake;
    uint32_t now = 0;
    const auto at_rest = [&](float size, int samples) {
        bool any = false;
        for (int i = 0; i < samples; i++) any |= shake.shaken({0, size, 0}, now += 30);
        return any;
    };
    CHECK_FALSE(at_rest(8192, 50)); // still, in the board's units: 1 g
    CHECK_FALSE(shake.shaken({0, 8192 * 2.5f, 0}, now += 30)); // a knock
    CHECK_FALSE(at_rest(8192, 30));
    bool turned = false;
    for (int i = 0; i < 40; i++, now += 30) turned |= shake.shaken({8192 * std::sin(i / 10.0f), 8192 * std::cos(i / 10.0f), 0}, now);
    CHECK_FALSE(turned); // turning it keeps the size

    bool shook = false;
    for (int i = 0; i < 10 && !shook; i++) shook = shake.shaken({0, i % 2 ? 8192 * 2.5f : 8192 * 0.2f, 0}, now += 30);
    CHECK(shook);
    bool again = false;
    for (int i = 0; i < 10; i++) again |= shake.shaken({0, i % 2 ? 8192 * 2.5f : 8192 * 0.2f, 0}, now += 30);
    CHECK_FALSE(again); // one shake, one roll
    CHECK_FALSE(at_rest(8192, 60));
    for (int i = 0; i < 10 && !again; i++) again = shake.shaken({0, i % 2 ? 8192 * 2.5f : 8192 * 0.2f, 0}, now += 30);
    CHECK(again); // later, the next

    Shake none;
    CHECK_FALSE(none.shaken({}, 100));
}

#include "ui/die.h"

// The die's dots as lit, dim or off.
static int count(const DiceRoll::Frame &frame, dots::Dot which) {
    int n = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) n += die::dot(frame, col, row) == which;
    return n;
}

TEST_CASE("the die: a square of dim dots, its corners round; each face its pips, four dots across; nothing before a roll") {
    for (int face = 1; face <= 6; face++) {
        CHECK(count({0, face, 0, 0}, dots::Dot::On) == face * 16);
        CHECK(count({0, face, 0, 0}, dots::Dot::Dim) == 4 * 18);
    }
    CHECK(die::dot({0, 1, 0, 0}, 11, 11) == dots::Dot::On); // one: in the middle
    CHECK(die::dot({0, 1, 0, 0}, 2, 2) == dots::Dot::Off);  // a round corner
    CHECK(count({0, 0, 0, 0}, dots::Dot::On) == 0);
    // Tumbling, a dot off its place, still on the board.
    CHECK(die::dot({0, 1, 1, -1}, 12, 10) == dots::Dot::On);
    CHECK(count({0, 6, 1, 1}, dots::Dot::On) == 6 * 16);
}

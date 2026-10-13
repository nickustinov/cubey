#include <doctest/doctest.h>

#include <bitset>

#include "ui/count.h"
#include "ui/font.h"

using namespace cubey;

// Picks that go round from `from`.
static std::function<uint32_t(uint32_t)> picks(uint32_t from = 0) {
    return [n = from](uint32_t below) mutable { return n++ % below; };
}

static int lit(const count::Shown &shown, count::Phase phase, dots::Dot which = dots::Dot::On) {
    int n = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) n += count::dot(shown, phase, col, row) == which;
    return n;
}

TEST_CASE("how many dots: n places of the grid's, each another; a number past kMost is kMost, none is one") {
    for (int n = 1; n <= count::kMost; n++) CHECK(std::bitset<16>(count::place(n, 0, picks()).places).count() == static_cast<size_t>(n));
    CHECK(std::bitset<16>(count::place(25, 0, picks()).places).count() == count::kMost);
    CHECK(count::place(0, 0, picks()).n == 1);
    CHECK(count::place(4, 0, picks(0)).places != count::place(4, 0, picks(5)).places); // at random
}

TEST_CASE("the dots: round, four across, coming dim, then lit") {
    const count::Shown three = count::place(3, 0, picks());
    CHECK(count::phase(three, 0) == count::Phase::Coming);
    CHECK(count::phase(three, count::kDimMs) == count::Phase::Lit);
    CHECK(lit(three, count::Phase::Coming, dots::Dot::Dim) == 3 * 12);
    CHECK(lit(three, count::Phase::Coming) == 0);
    CHECK(lit(three, count::Phase::Lit) == 3 * 12); // 4 x 4, its corners off
    const count::Shown one{1, 1, 0}; // the first place: top left
    CHECK(count::dot(one, count::Phase::Lit, 1, 1) == dots::Dot::Off); // a round corner
    CHECK(count::dot(one, count::Phase::Lit, 2, 1) == dots::Dot::On);
    CHECK(count::dot(one, count::Phase::Lit, 1, 2) == dots::Dot::On);
}

TEST_CASE("a new number: the one before first, its digits big in the middle, then the new dots") {
    const count::Shown next = count::place(2, 7, picks());
    CHECK(count::phase(next, 0) == count::Phase::Number);
    CHECK(count::phase(next, count::kNumberMs - 1) == count::Phase::Number);
    CHECK(count::phase(next, count::kNumberMs) == count::Phase::Coming);
    CHECK(count::phase(next, count::kNumberMs + count::kDimMs) == count::Phase::Lit);
    int seven = 0;
    for (const char *row : font::digit('7').rows)
        for (const char *c = row; *c; c++) seven += *c == '#';
    CHECK(lit(next, count::Phase::Number) == seven * 4); // each of its dots two by two
    CHECK(count::dot(next, count::Phase::Number, 7, 6) == dots::Dot::On); // 7's top bar, from its first column
    CHECK(count::dot(next, count::Phase::Number, 6, 6) == dots::Dot::Off);
    CHECK(lit(count::place(2, 10, picks()), count::Phase::Number) > 0); // "10" fits
}

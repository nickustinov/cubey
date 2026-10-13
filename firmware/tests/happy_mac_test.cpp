#include <doctest/doctest.h>

#include "ui/happy_mac.h"

using namespace cubey;
using dots::Dot;

namespace {
Dot at(const fine::Frame &f, int col, int row) { return f[static_cast<size_t>(row * fine::kBoard + col)]; }
int lit(const fine::Frame &f, int row1, int row2, int col1, int col2, Dot as) {
    int n = 0;
    for (int row = row1; row < row2; row++)
        for (int col = col1; col < col2; col++) n += at(f, col, row) == as;
    return n;
}
} // namespace

TEST_CASE("the Happy Mac as it fetches: its eyes open, now and then a blink; the bar as far as it has come") {
    const fine::Frame f = happy_mac::paint(UpdateState::Fetching, 50, 400);
    for (int row = 10; row <= 12; row++) CHECK(at(f, 24, row) == Dot::On); // its eyes
    for (int row = 10; row <= 12; row++) CHECK(at(f, 35, row) == Dot::On);
    CHECK(at(f, 29, 10) == Dot::On); // its nose
    CHECK(at(f, 25, 18) == Dot::On); // its smile, up at its ends
    CHECK(at(f, 26, 19) == Dot::On);
    CHECK(lit(f, 48, 51, 9, 51, Dot::On) == 3 * 21); // half the bar
    CHECK(lit(happy_mac::paint(UpdateState::Fetching, 100, 400), 48, 51, 9, 51, Dot::On) == 3 * 42);

    const fine::Frame blink = happy_mac::paint(UpdateState::Fetching, 50, happy_mac::kBlinkEveryMs - 1);
    CHECK(at(blink, 24, 10) != Dot::On);
    CHECK(at(blink, 23, 12) == Dot::On);
    CHECK(at(blink, 25, 12) == Dot::On);
}

TEST_CASE("a floppy goes into its slot again and again: under it, then in, hidden above it; then the next") {
    const fine::Frame none = happy_mac::paint(UpdateState::Fetching, 50, happy_mac::kDiskMs - 100);
    const fine::Frame under = happy_mac::paint(UpdateState::Fetching, 50, 0);
    const fine::Frame going = happy_mac::paint(UpdateState::Fetching, 50, 900);
    CHECK(lit(under, 32, 41, 30, 41, Dot::On) > lit(none, 32, 41, 30, 41, Dot::On));
    CHECK(lit(going, 32, 41, 30, 41, Dot::On) < lit(under, 32, 41, 30, 41, Dot::On));
    for (uint32_t ms = 0; ms < happy_mac::kDiskMs; ms += 50) // never above its slot
        CHECK(lit(happy_mac::paint(UpdateState::Fetching, 50, ms), 24, 30, 30, 41, Dot::Off) == 6 * 11);
    const fine::Frame again = happy_mac::paint(UpdateState::Fetching, 50, happy_mac::kDiskMs + 900); // the next floppy, the same
    for (int row = 30; row < 42; row++)
        for (int col = 30; col < 41; col++) CHECK(at(again, col, row) == at(going, col, row));
}

TEST_CASE("done, it winks; failed, the Sad Mac: its eyes crossed out, a frown, the bar dim") {
    const fine::Frame done = happy_mac::paint(UpdateState::Done, 100, 0);
    for (int row = 10; row <= 12; row++) CHECK(at(done, 24, row) == Dot::On);
    CHECK(at(done, 35, 10) != Dot::On);
    for (int col = 34; col <= 36; col++) CHECK(at(done, col, 12) == Dot::On);
    CHECK(lit(done, 32, 41, 30, 41, Dot::On) == lit(happy_mac::paint(UpdateState::Fetching, 100, happy_mac::kDiskMs - 100), 32, 41, 30, 41, Dot::On)); // no floppy

    const fine::Frame failed = happy_mac::paint(UpdateState::Failed, 60, 0);
    CHECK(at(failed, 23, 10) == Dot::On);
    CHECK(at(failed, 24, 11) == Dot::On);
    CHECK(at(failed, 25, 12) == Dot::On);
    CHECK(at(failed, 24, 10) != Dot::On);
    CHECK(at(failed, 26, 18) == Dot::On); // its frown, down at its ends
    CHECK(at(failed, 25, 19) == Dot::On);
    CHECK(lit(failed, 46, 53, 7, 53, Dot::On) == 0);
    CHECK(lit(failed, 46, 53, 7, 53, Dot::Dim) > 0);
}

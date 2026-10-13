#include <doctest/doctest.h>

#include "ui/sketch.h"

using namespace cubey;

namespace {
int shown(const sketch::Frame &f) {
    int n = 0;
    for (dots::Dot d : f) n += d != dots::Dot::Off;
    return n;
}
dots::Dot at(const sketch::Frame &f, sketch::Spot s) { return f[static_cast<size_t>(s.row * dots::kBoard + s.col)]; }
bool middle(int col, int row) {
    const double dc = col - (dots::kBoard - 1) / 2.0, dr = row - (dots::kBoard - 1) / 2.0;
    return dc * dc + dr * dr <= (sketch::kRadius + 0.5) * (sketch::kRadius + 0.5);
}
} // namespace

TEST_CASE("a dot of the sketch rises and fades: dim, half, dim, then gone") {
    CHECK(sketch::shade(0) == dots::Dot::Dim);
    CHECK(sketch::shade(299) == dots::Dot::Dim);
    CHECK(sketch::shade(300) == dots::Dot::Half);
    CHECK(sketch::shade(799) == dots::Dot::Half);
    CHECK(sketch::shade(800) == dots::Dot::Dim);
    CHECK(sketch::shade(sketch::kLifeMs - 1) == dots::Dot::Dim);
    CHECK(sketch::shade(sketch::kLifeMs) == dots::Dot::Off);
}

TEST_CASE("a sketch forming: faint dots come one after another at places here and there in the board's middle") {
    const uint32_t seed = 7;
    const sketch::Frame first = sketch::frame(seed, 0);
    CHECK(shown(first) == 1);
    CHECK(at(first, sketch::spot(seed, 0)) == dots::Dot::Dim);
    CHECK(at(sketch::frame(seed, 500), sketch::spot(seed, 0)) == dots::Dot::Half);
    const int most = static_cast<int>(sketch::kLifeMs / sketch::kEveryMs) + 1;
    const int later = shown(sketch::frame(seed, 5000));
    CHECK(later > most / 2);
    CHECK(later <= most); // as many come as go
    for (uint32_t ms = 0; ms < 10000; ms += 37) {
        const sketch::Frame f = sketch::frame(seed, ms);
        for (int row = 0; row < dots::kBoard; row++)
            for (int col = 0; col < dots::kBoard; col++) {
                const dots::Dot d = f[static_cast<size_t>(row * dots::kBoard + col)];
                CHECK(d != dots::Dot::On); // faint: a sketch, not the picture
                if (d != dots::Dot::Off) CHECK(middle(col, row));
            }
    }
}

TEST_CASE("each sketch its own: another seed, other places; the same seed, the same sketch") {
    int same = 0;
    for (uint32_t k = 0; k < 20; k++) {
        const sketch::Spot a = sketch::spot(1, k), b = sketch::spot(2, k);
        same += a.col == b.col && a.row == b.row;
        CHECK(middle(a.col, a.row));
    }
    CHECK(same < 5);
    CHECK(sketch::frame(9, 2345) == sketch::frame(9, 2345));
}

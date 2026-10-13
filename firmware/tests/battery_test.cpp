#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>

#include "ui/battery.h"
#include "ui/dots.h"

using namespace cubey;

static int count(int filled) {
    int n = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) n += battery::lit(filled, col, row);
    return n;
}

TEST_CASE("its charge in columns: none when empty, at least one for any charge, all of them full") {
    CHECK(battery::fill(0) == 0);
    CHECK(battery::fill(1) == 1);
    CHECK(battery::fill(50) == (battery::kFillMost + 1) / 2);
    CHECK(battery::fill(100) == battery::kFillMost);
    CHECK(battery::fill(140) == battery::kFillMost);
    CHECK(battery::fill(-1) == 0);
}

TEST_CASE("a battery of dots: its outline and nub, and its charge inside a dot clear of them, column by column") {
    const int empty = count(0);
    CHECK(empty > 0);
    for (int filled = 1; filled <= battery::kFillMost; filled++) CHECK(count(filled) - count(filled - 1) > 1); // a column
    // On the board, in its middle: as many dots lit either side, and above and below.
    int left = dots::kBoard, right = -1, top = dots::kBoard, bottom = -1;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++)
            if (battery::lit(battery::kFillMost, col, row)) {
                left = std::min(left, col), right = std::max(right, col);
                top = std::min(top, row), bottom = std::max(bottom, row);
            }
    CHECK(left >= 1);
    CHECK(right <= dots::kBoard - 2);
    CHECK(std::abs(left - (dots::kBoard - 1 - right)) <= 1);
    CHECK(std::abs(top - (dots::kBoard - 1 - bottom)) <= 1);
    // The charge never touches the outline.
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++)
            if (battery::lit(battery::kFillMost, col, row) && !battery::lit(0, col, row)) {
                CHECK_FALSE(battery::lit(0, col - 1, row));
                CHECK_FALSE(battery::lit(0, col + 1, row));
                CHECK_FALSE(battery::lit(0, col, row - 1));
                CHECK_FALSE(battery::lit(0, col, row + 1));
            }
}

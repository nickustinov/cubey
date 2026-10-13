#include <doctest/doctest.h>

#include "ui/dots.h"

using namespace cubey;

TEST_CASE("turned a quarter clockwise, the picture's top-left dot shows at the screen's top-right, and so on round") {
    constexpr int last = dots::kBoard - 1;
    const auto shows = [](int quarters, int col, int row, int want_col, int want_row) {
        const dots::Cell c = dots::turned(quarters, col, row);
        CHECK(c.col == want_col);
        CHECK(c.row == want_row);
    };
    shows(0, 3, 5, 3, 5);
    shows(1, last, 0, 0, 0);    // the screen's top-right: the picture's top-left
    shows(1, last, last, last, 0);
    shows(2, last, last, 0, 0); // upside down
    shows(3, 0, last, 0, 0);    // a quarter back: its top-left at the screen's bottom-left
    shows(4, 3, 5, 3, 5);       // all the way round
}

TEST_CASE("the board's colours, by theme: lit, half, dim and faintly there, over its ground; amber unless the server says another") {
    using dots::Dot;
    CHECK(dots::colour(Dot::On) == 0xFFB020); // amber, as old LED panels
    CHECK(dots::ground() == 0x000000);
    CHECK(dots::colour(Dot::Half) == 0xAA7515); // the amber at 170 of 255 over black: a colour of its own, drawn faster
    CHECK(dots::set_theme("green"));
    CHECK_FALSE(dots::set_theme("green")); // as it was
    CHECK(dots::colour(Dot::On) == 0x33FF66);
    CHECK(dots::set_theme("white")); // ink on paper: the faint dots a shade darker than the paper
    CHECK(dots::colour(Dot::On) == 0x1C1C1C);
    CHECK(dots::ground() == 0xE6E1D3);
    CHECK(dots::colour(Dot::Off) < dots::ground());
    CHECK(dots::colour(Dot::Off) > dots::colour(Dot::Dim));
    CHECK(dots::set_theme("tartan")); // none such: amber
    CHECK(dots::colour(Dot::On) == 0xFFB020);
    CHECK_FALSE(dots::set_theme(""));
    CHECK(dots::themes() == std::vector<std::string>{"amber", "green", "red", "blue", "vfd", "white"}); // for the menu
}

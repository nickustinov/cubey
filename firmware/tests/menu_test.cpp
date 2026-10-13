#include <doctest/doctest.h>

#include "ui/menu.h"

using namespace cubey;
using dots::Dot;

static int lit(const fine::Frame &f, int row1, int row2, int col1, int col2, Dot as) {
    int n = 0;
    for (int row = row1; row < row2; row++)
        for (int col = col1; col < col2; col++) n += f[row * fine::kBoard + col] == as;
    return n;
}

TEST_CASE("the menu: three lines, each what it is in small capitals over its setting, an arrow either side; its brightness; OK") {
    SettingsMenu menu({"amber", "green"});
    menu.known({"en", "lv"}, {"lv", "en", "green", 50}, true);
    REQUIRE(menu.offer(0) == SettingsMenu::Event::Opened);
    const fine::Frame f = menu::paint(menu);
    CHECK(lit(f, 2, 6, 10, 50, Dot::Half) > 0);   // SPEAKS, four rows
    CHECK(lit(f, 0, 7, 0, 60, Dot::On) == 0);
    CHECK(lit(f, 7, 13, 20, 40, Dot::On) > 0);    // LV
    CHECK(lit(f, 7, 13, 0, 10, Dot::Half) > 0);   // ◀
    CHECK(lit(f, 7, 13, 50, 60, Dot::Half) > 0);  // ▶
    CHECK(lit(f, 15, 19, 10, 50, Dot::Half) > 0); // TEACHES
    CHECK(lit(f, 20, 26, 20, 40, Dot::On) > 0);   // EN
    CHECK(lit(f, 28, 32, 10, 50, Dot::Half) > 0); // THEME
    CHECK(lit(f, 33, 39, 12, 48, Dot::On) > 0);   // GREEN
    CHECK(lit(f, 39, 42, 0, 60, Dot::On) + lit(f, 39, 42, 0, 60, Dot::Half) == 0); // room above the brightness
    CHECK(lit(f, 42, 47, 0, 7, Dot::Half) > 0);   // a sun, for its brightness
    CHECK(lit(f, 42, 47, 9, 60, Dot::On) == lit(f, 42, 47, 9, 60, Dot::Dim)); // half of it lit
    CHECK(lit(f, 47, 50, 0, 60, Dot::On) + lit(f, 47, 50, 0, 60, Dot::Dim) == 0); // and below it
    CHECK(lit(f, 50, 60, 18, 42, Dot::Dim) > 0);  // OK's button
    CHECK(lit(f, 50, 60, 18, 42, Dot::On) > 0);   // OK
}

TEST_CASE("a tap: on an arrow's side of a line, a step back or on; on the brightness, as far along; on the bottom, OK") {
    using Does = menu::Tap::Does;
    const auto step = [](int col, int row, int line, int by) {
        const menu::Tap t = menu::tap_at(col, row);
        CHECK(t.does == Does::Step);
        CHECK(t.line == line);
        CHECK(t.by == by);
    };
    step(5, 10, 0, -1);
    step(55, 10, 0, 1);
    step(5, 22, 1, -1);
    step(55, 22, 1, 1);
    step(5, 35, 2, -1);
    step(55, 35, 2, 1);
    CHECK(menu::tap_at(30, 10).does == Does::Nothing);
    CHECK(menu::tap_at(9, 44).percent == 10);
    CHECK(menu::tap_at(31, 44).percent == 50);
    CHECK(menu::tap_at(59, 44).percent == 100);
    CHECK(menu::tap_at(2, 44).percent == 10); // the sun's side: the dimmest
    CHECK(menu::tap_at(30, 55).does == Does::Ok);
}

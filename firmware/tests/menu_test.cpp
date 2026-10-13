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
    REQUIRE(menu.open(0) == SettingsMenu::Event::Opened);
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
        const menu::Tap t = menu::tap_at(col, row, false);
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
    CHECK(menu::tap_at(30, 10, false).does == Does::Nothing);
    CHECK(menu::tap_at(9, 44, false).percent == 10);
    CHECK(menu::tap_at(31, 44, false).percent == 50);
    CHECK(menu::tap_at(59, 44, false).percent == 100);
    CHECK(menu::tap_at(2, 44, false).percent == 10); // the sun's side: the dimmest
    CHECK(menu::tap_at(30, 55, false).does == Does::Ok);
}

TEST_CASE("a new Cubey's menu, opened by itself after its Wi-Fi: only its two languages, their codes big, filling the screen; OK") {
    SettingsMenu menu({"amber", "green"});
    menu.known({"en", "lv"}, {"lv", "en", "green", 50}, true);
    REQUIRE(menu.offer(0) == SettingsMenu::Event::Opened);
    const fine::Frame f = menu::paint(menu);
    CHECK(lit(f, 1, 5, 10, 50, Dot::Half) > 0);    // SPEAKS, small
    CHECK(lit(f, 7, 25, 12, 48, Dot::On) > 150);   // LV, three times the font's size
    CHECK(lit(f, 7, 25, 0, 12, Dot::Half) > 0);    // ◀
    CHECK(lit(f, 7, 25, 48, 60, Dot::Half) > 0);   // ▶
    CHECK(lit(f, 27, 31, 10, 50, Dot::Half) > 0);  // TEACHES
    CHECK(lit(f, 33, 51, 12, 48, Dot::On) > 150);  // EN
    CHECK(lit(f, 52, 60, 18, 42, Dot::Dim) > 0);   // OK's button
    CHECK(lit(f, 52, 60, 18, 42, Dot::On) > 0);    // OK
    CHECK(lit(f, 0, 60, 0, 60, Dot::On) == lit(f, 7, 25, 12, 48, Dot::On) + lit(f, 33, 51, 12, 48, Dot::On) + lit(f, 52, 60, 18, 42, Dot::On)); // no theme, no brightness
}

TEST_CASE("a new Cubey's menu, tapped: an arrow's side of the top half steps what it speaks, of the bottom half what it teaches; OK") {
    using Does = menu::Tap::Does;
    CHECK(menu::tap_at(3, 15, true).does == Does::Step);
    CHECK(menu::tap_at(3, 15, true).line == 0);
    CHECK(menu::tap_at(3, 15, true).by == -1);
    CHECK(menu::tap_at(56, 40, true).line == 1);
    CHECK(menu::tap_at(56, 40, true).by == 1);
    CHECK(menu::tap_at(30, 15, true).does == Does::Nothing); // on the code itself
    CHECK(menu::tap_at(9, 44, true).does != Does::Brightness);  // none here
    CHECK(menu::tap_at(30, 56, true).does == Does::Ok);
}

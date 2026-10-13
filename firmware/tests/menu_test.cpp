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

TEST_CASE("the menu's first page: two lines, each what it is in small capitals over its setting, an arrow either side; its pages; OK") {
    SettingsMenu menu({"amber", "green"});
    menu.known({"en", "lv"}, {"lv", "green", 50, 30}, true);
    REQUIRE(menu.open(0) == SettingsMenu::Event::Opened);
    const fine::Frame f = menu::paint(menu);
    CHECK(lit(f, 5, 9, 10, 50, Dot::Half) > 0);   // SPEAKS, four rows
    CHECK(lit(f, 0, 10, 0, 60, Dot::On) == 0);
    CHECK(lit(f, 10, 16, 20, 40, Dot::On) > 0);   // LV
    CHECK(lit(f, 10, 16, 0, 10, Dot::Half) > 0);  // ◀
    CHECK(lit(f, 10, 16, 50, 60, Dot::Half) > 0); // ▶
    CHECK(lit(f, 22, 26, 10, 50, Dot::Half) > 0); // THEME
    CHECK(lit(f, 27, 33, 12, 48, Dot::On) > 0);   // GREEN
    CHECK(lit(f, 33, 37, 0, 60, Dot::On) + lit(f, 33, 37, 0, 60, Dot::Half) == 0); // room above its pages
    CHECK(lit(f, 39, 42, 25, 28, Dot::On) == 9);  // this page's dot
    CHECK(lit(f, 39, 42, 32, 35, Dot::Dim) == 9); // the other's
    CHECK(lit(f, 34, 44, 38, 43, Dot::Half) > 0); // > towards it
    CHECK(lit(f, 44, 50, 0, 60, Dot::On) + lit(f, 44, 50, 0, 60, Dot::Dim) == 0); // room above OK
    CHECK(lit(f, 50, 60, 18, 42, Dot::Dim) > 0);  // OK's button
    CHECK(lit(f, 50, 60, 18, 42, Dot::On) > 0);   // OK
}

TEST_CASE("its second page: its brightness and its volume, each under its word, its icon beside ten segments lit as far as it goes") {
    SettingsMenu menu({"amber", "green"});
    menu.known({"en", "lv"}, {"lv", "green", 50, 30}, false);
    REQUIRE(menu.open(0) == SettingsMenu::Event::Opened);
    menu.to_page(1, 0);
    const fine::Frame f = menu::paint(menu);
    CHECK(lit(f, 5, 9, 5, 55, Dot::Half) > 0);    // BRIGHTNESS
    CHECK(lit(f, 11, 16, 0, 7, Dot::Half) > 0);   // a sun
    CHECK(lit(f, 12, 15, 9, 60, Dot::On) == lit(f, 12, 15, 9, 60, Dot::Dim)); // half of it lit
    CHECK(lit(f, 22, 26, 10, 50, Dot::Half) > 0); // VOLUME
    CHECK(lit(f, 28, 33, 0, 7, Dot::Half) > 0);   // a speaker
    CHECK(lit(f, 29, 32, 9, 60, Dot::On) * 7 == lit(f, 29, 32, 9, 60, Dot::Dim) * 3); // 30%
    CHECK(lit(f, 39, 42, 32, 35, Dot::On) == 9);  // this page's dot
    CHECK(lit(f, 34, 44, 17, 23, Dot::Half) > 0); // < towards the first
}

TEST_CASE("a tap: on an arrow's side of a line, a step back or on; on a slider, as far along; about the dots, the other page; at the bottom, OK") {
    using Does = menu::Tap::Does;
    const auto step = [](int col, int row, int line, int by) {
        const menu::Tap t = menu::tap_at(col, row, false, 0);
        CHECK(t.does == Does::Step);
        CHECK(t.line == line);
        CHECK(t.by == by);
    };
    step(5, 12, 0, -1);
    step(55, 12, 0, 1);
    step(5, 28, 1, -1);
    step(55, 28, 1, 1);
    CHECK(menu::tap_at(30, 12, false, 0).does == Does::Nothing);
    CHECK(menu::tap_at(30, 40, false, 0).does == Does::Page);
    CHECK(menu::tap_at(30, 40, false, 0).page == 1);
    CHECK(menu::tap_at(30, 40, false, 1).page == 0);
    CHECK(menu::tap_at(9, 13, false, 1).does == Does::Brightness);
    CHECK(menu::tap_at(9, 13, false, 1).percent == 10);
    CHECK(menu::tap_at(31, 13, false, 1).percent == 50);
    CHECK(menu::tap_at(2, 13, false, 1).percent == 10); // the sun's side: the dimmest
    CHECK(menu::tap_at(59, 30, false, 1).does == Does::Volume);
    CHECK(menu::tap_at(59, 30, false, 1).percent == 100);
    CHECK(menu::tap_at(30, 55, false, 0).does == Does::Ok);
    CHECK(menu::tap_at(30, 55, false, 1).does == Does::Ok);
}

TEST_CASE("a new Cubey's menu, opened by itself after its Wi-Fi: only the language it speaks, its code big, filling the screen; OK") {
    SettingsMenu menu({"amber", "green"});
    menu.known({"en", "lv"}, {"lv", "green", 50}, true);
    REQUIRE(menu.offer(0) == SettingsMenu::Event::Opened);
    const fine::Frame f = menu::paint(menu);
    CHECK(lit(f, 6, 10, 10, 50, Dot::Half) > 0);   // SPEAKS, small
    CHECK(lit(f, 14, 38, 8, 52, Dot::On) > 300);   // LV, four times the font's size
    CHECK(lit(f, 14, 38, 0, 8, Dot::Half) > 0);    // ◀
    CHECK(lit(f, 14, 38, 52, 60, Dot::Half) > 0);  // ▶
    CHECK(lit(f, 50, 58, 18, 42, Dot::Dim) > 0);   // OK's button
    CHECK(lit(f, 50, 58, 18, 42, Dot::On) > 0);    // OK
    CHECK(lit(f, 0, 60, 0, 60, Dot::On) == lit(f, 14, 38, 8, 52, Dot::On) + lit(f, 50, 58, 18, 42, Dot::On)); // no theme, no brightness
}

TEST_CASE("a new Cubey's menu, tapped: an arrow's side steps the language it speaks, back or on; OK") {
    using Does = menu::Tap::Does;
    CHECK(menu::tap_at(3, 25, true, 0).does == Does::Step);
    CHECK(menu::tap_at(3, 25, true, 0).line == 0);
    CHECK(menu::tap_at(3, 25, true, 0).by == -1);
    CHECK(menu::tap_at(56, 25, true, 0).by == 1);
    CHECK(menu::tap_at(30, 25, true, 0).does == Does::Nothing); // on the code itself
    CHECK(menu::tap_at(9, 40, true, 0).does != Does::Brightness); // none here
    CHECK(menu::tap_at(30, 54, true, 0).does == Does::Ok);
}

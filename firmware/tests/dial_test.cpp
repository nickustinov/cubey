#include <doctest/doctest.h>

#include <cstring>
#include <set>
#include <string>

#include "ui/dial.h"
#include "ui/dots.h"
#include "ui/font.h"

using namespace cubey;

TEST_CASE("the dot font's digits: six rows each, from its row 1, as its 1 and 3; the 1 narrow, the rest 5 wide") {
    for (char d = '0'; d <= '9'; d++) {
        const font::Glyph &g = font::digit(d);
        CHECK(g.width == (d == '1' ? 3 : 5));
        for (int row = 0; row < font::kDigitRows; row++) CHECK(strlen(g.rows[row]) == static_cast<size_t>(g.width));
    }
    CHECK(strcmp(font::digit('3').rows[0], "####.") == 0); // as the font's own
    CHECK(font::width("40") == 11);
    CHECK(font::width("1") == 3);
}

TEST_CASE("its digits say the minutes left, rounded up, and in the last minute the seconds") {
    CHECK(dial::shown(40 * 60) == "40");
    CHECK(dial::shown(39 * 60 + 1) == "40");
    CHECK(dial::shown(39 * 60) == "39");
    CHECK(dial::shown(61) == "2");
    CHECK(dial::shown(60) == "60");
    CHECK(dial::shown(7) == "7");
}

TEST_CASE("a square bar round the board goes out over the whole timer, not each minute") {
    const uint32_t total = 40 * 60;
    CHECK(dial::kBar == 84);
    CHECK(dial::bar_lit(total, total) == dial::kBar);
    CHECK(dial::bar_lit(total - 1, total) == dial::kBar); // a dot lasts about half a minute of 40
    CHECK(dial::bar_lit(total - 29, total) == dial::kBar - 1);
    CHECK(dial::bar_lit(total / 2, total) == dial::kBar / 2);
    CHECK(dial::bar_lit(1, total) == 1);
    CHECK(dial::bar_lit(0, total) == 0);
    // From the top's middle, clockwise, every dot on the square's edge, one dot in from the board's, each its own.
    CHECK(dial::bar_dot(0).col == 12);
    CHECK(dial::bar_dot(0).row == 1);
    CHECK(dial::bar_dot(1).col == 13);
    CHECK(dial::bar_dot(dial::kBar - 1).col == 11);
    std::set<std::pair<int, int>> seen;
    for (int i = 0; i < dial::kBar; i++) {
        const auto [col, row] = dial::bar_dot(i);
        CHECK((col == 1 || col == 22 || row == 1 || row == 22));
        CHECK(col >= 1);
        CHECK(col <= 22);
        CHECK(row >= 1);
        CHECK(row <= 22);
        seen.insert({col, row});
    }
    CHECK(seen.size() == static_cast<size_t>(dial::kBar));
}

TEST_CASE("the bar's last lit dot blinks each second, the ones gone are dim") {
    const uint32_t total = 40 * 60;
    const auto at = [&](uint32_t left, int i) { return dial::dot({left, total, dial::For::Other}, dial::bar_dot(i).col, dial::bar_dot(i).row); };
    CHECK(at(total / 2, dial::kBar / 2) == dots::Dot::On);
    CHECK(at(total / 2 - 1, dial::kBar / 2) == dots::Dot::Dim);
    CHECK(at(total / 2, dial::kBar / 2 - 1) == dots::Dot::Dim);
    CHECK(at(total / 2, dial::kBar - 1) == dots::Dot::On);
}

TEST_CASE("what it's for, as a little picture above the digits, each its own") {
    std::set<std::string> pictures;
    for (int kind = 0; kind < static_cast<int>(dial::For::Count); kind++) {
        const dial::View view{40 * 60, 40 * 60, static_cast<dial::For>(kind)};
        std::string picture;
        int lit = 0;
        for (int row = 2; row < 12; row++) // inside the bar, above the digits
            for (int col = 2; col < dots::kBoard - 2; col++) {
                lit += dial::dot(view, col, row) == dots::Dot::On;
                picture += dial::dot(view, col, row) == dots::Dot::On ? '#' : '.';
            }
        CHECK(lit > 10);
        pictures.insert(picture);
    }
    CHECK(pictures.size() == static_cast<size_t>(dial::For::Count));
    CHECK(dial::for_named("cartoons") == dial::For::Cartoons);
    CHECK(dial::for_named("swimming") == dial::For::Other);
}

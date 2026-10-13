#include <doctest/doctest.h>

#include "services/brushing.h"
#include <vector>

#include "ui/dots.h"
#include "ui/eyes.h"
#include "ui/mouth.h"

using namespace cubey;

// How many dots are lit, and how many shine, in this view.
static std::pair<int, int> count(const mouth::View &view) {
    int lit = 0, on = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) {
            const dots::Dot d = mouth::dot(view, col, row);
            lit += d != dots::Dot::Off;
            on += d == dots::Dot::On;
        }
    return {lit, on};
}

// Whether any shining dot is in this quarter of the board (as she sees it).
static bool shines_in(const mouth::View &view, bool left, bool top) {
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++)
            if ((col < dots::kBoard / 2) == left && (row < dots::kBoard / 2) == top && mouth::dot(view, col, row) == dots::Dot::On &&
                mouth::dot({}, col, row) != dots::Dot::On)
                return true;
    return false;
}

TEST_CASE("waiting for her tap: lips, and teeth not yet clean, dimmer than the lips") {
    const auto [lit, on] = count({});
    CHECK(lit > on);
    CHECK(on > 0);
}

TEST_CASE("brushing, the toothbrush scrubs the part to brush now, round the mouth from the top on the left as she sees it") {
    mouth::View view{true, brushing_at(0), 0, false};
    CHECK(shines_in(view, true, true));
    CHECK_FALSE(shines_in(view, false, false));
    view.at = brushing_at(31000);
    CHECK(shines_in(view, false, true));
    view.at = brushing_at(61000);
    CHECK(shines_in(view, false, false));
    view.at = brushing_at(91000);
    CHECK(shines_in(view, true, false));
}

TEST_CASE("the scrub moves the brush; the teeth shine as they are brushed, and all of them once done") {
    const mouth::View still{true, brushing_at(1000), 0, false}, moved{true, brushing_at(1000), 3, false};
    int moves = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) moves += mouth::dot(still, col, row) != mouth::dot(moved, col, row);
    CHECK(moves > 0);
    CHECK(count({true, brushing_at(29000), 0, false}).second > count({true, brushing_at(1000), 0, false}).second);
    const auto [lit, on] = count({true, brushing_at(kBrushingMs), 0, false});
    CHECK(lit == on); // clean and shiny
    CHECK(count({true, brushing_at(kBrushingMs), 0, true}).second != on); // sparkles
}

// The lit rows of Cubey's eyes above its mouth, the left one's.
static std::vector<int> eye_rows(const mouth::View &view) {
    std::vector<int> rows;
    const int col = eyes::place_on(dots::kBoard).left + eyes::kWide / 2;
    for (int row = 0; row < dots::kBoard / 3; row++)
        if (mouth::dot(view, col, row) == dots::Dot::On) rows.push_back(row);
    return rows;
}

TEST_CASE("Cubey's eyes above its mouth, flattened, its head back; shut as it blinks, happy arches once done") {
    const auto open = eye_rows({});
    REQUIRE(open.size() >= 2);
    CHECK(open.size() < static_cast<size_t>(eyes::kTall)); // flatter than its eyes ahead
    mouth::View blinking;
    blinking.blink = true;
    CHECK(eye_rows(blinking).size() == 1);
    mouth::View done{true, brushing_at(kBrushingMs), 0, false};
    CHECK(eye_rows(done).size() == 1); // the arch's top: its middle
    CHECK(eye_rows(done)[0] == open[0]);
}

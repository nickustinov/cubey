#include <doctest/doctest.h>

#include <cstdlib>

#include "ui/cards.h"

using namespace cubey;

TEST_CASE("one card over all of the screen; two side by side; three, two above one; all on the screen, apart") {
    const auto at = [](int count, int index, int quarters = 0) { return cards::place(count, index, quarters); };
    CHECK(at(1, 0).x == 240);
    CHECK(at(1, 0).y == 240);
    CHECK(at(1, 0).side == 352);
    CHECK(at(2, 0).x < at(2, 1).x);
    CHECK(at(2, 0).y == at(2, 1).y);
    CHECK(at(3, 0).y == at(3, 1).y);
    CHECK(at(3, 2).y > at(3, 0).y);
    CHECK(at(3, 2).x == 240);
    for (int count = 1; count <= 3; count++)
        for (int i = 0; i < count; i++) {
            const cards::Place p = at(count, i);
            CHECK(p.x - p.side / 2 >= 0);
            CHECK(p.x + p.side / 2 <= 480);
            CHECK(p.y - p.side / 2 >= 0);
            CHECK(p.y + p.side / 2 <= 480);
            for (int j = 0; j < i; j++) {
                const cards::Place q = at(count, j);
                CHECK((std::abs(p.x - q.x) >= p.side + 8 || std::abs(p.y - q.y) >= p.side + 8));
            }
        }
}

TEST_CASE("the cube turned a quarter clockwise: the cards go round with it, the top-left one to the top-right") {
    const cards::Place upright = cards::place(3, 0, 0), turned = cards::place(3, 0, 1);
    CHECK(upright.x < 240);
    CHECK(upright.y < 240);
    CHECK(turned.x > 240);
    CHECK(turned.y < 240);
    CHECK(cards::place(3, 2, 2).y < 240); // upside down: the one below is above
    CHECK(cards::place(3, 0, 4).x == upright.x);
}

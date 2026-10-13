#include <doctest/doctest.h>

#include "ui/picture.h"

using namespace cubey;

TEST_CASE("a picture Cubey drew: '#' lit, '*' half, '+' dim, anything else and past its rows off") {
    const std::vector<std::string> rows = {"#+.*", "x"};
    CHECK(picture::dot(rows, 0, 0) == dots::Dot::On);
    CHECK(picture::dot(rows, 3, 0) == dots::Dot::Half);
    CHECK(picture::dot(rows, 1, 0) == dots::Dot::Dim);
    CHECK(picture::dot(rows, 2, 0) == dots::Dot::Off);
    CHECK(picture::dot(rows, 0, 1) == dots::Dot::Off);
    CHECK(picture::dot(rows, 5, 0) == dots::Dot::Off);
    CHECK(picture::dot(rows, 0, 7) == dots::Dot::Off);
}

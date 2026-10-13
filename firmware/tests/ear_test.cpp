#include <doctest/doctest.h>

#include "ui/ear.h"

using namespace cubey;

namespace {
struct Box {
    int left = dots::kBoard, right = -1, top = dots::kBoard, bottom = -1, lit = 0;
};
Box box(const ear::Frame &f) {
    Box b;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++)
            if (f[static_cast<size_t>(row * dots::kBoard + col)] != dots::Dot::Off) {
                b.left = std::min(b.left, col), b.right = std::max(b.right, col);
                b.top = std::min(b.top, row), b.bottom = std::max(b.bottom, row);
                b.lit++;
            }
    return b;
}
} // namespace

TEST_CASE("the ear sways gently as it listens: a dot right, back, a dot left, back, a while at each") {
    CHECK(ear::sway(0) == 0);
    CHECK(ear::sway(ear::kSwayMs - 1) == 0);
    CHECK(ear::sway(ear::kSwayMs) == 1);
    CHECK(ear::sway(2 * ear::kSwayMs) == 0);
    CHECK(ear::sway(3 * ear::kSwayMs) == -1);
    CHECK(ear::sway(4 * ear::kSwayMs) == 0);
    CHECK(ear::sway(41 * ear::kSwayMs) == 1); // on and on
}

TEST_CASE("the ear: in the board's middle, where the eyes are; swayed, the same a dot aside") {
    const ear::Frame f = ear::frame(0);
    const Box b = box(f);
    CHECK(b.lit > 30);
    CHECK(std::abs(b.left + b.right - (dots::kBoard - 1)) <= 1);
    CHECK(std::abs(b.top + b.bottom - (dots::kBoard - 1)) <= 1);
    const ear::Frame right = ear::frame(1);
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col + 1 < dots::kBoard; col++)
            CHECK(right[static_cast<size_t>(row * dots::kBoard + col + 1)] == f[static_cast<size_t>(row * dots::kBoard + col)]);
}

#include <doctest/doctest.h>

#include <set>

#include "services/keyboard.h"
#include "ui/font.h"

using namespace cubey;
using keyboard::Does;
using keyboard::Page;

TEST_CASE("the keyboard: ten keys in four rows; digits over small letters, shifted to capitals; symbols on a page of their own") {
    CHECK(keyboard::key(Page::Small, 0, 0).text == "1");
    CHECK(keyboard::key(Page::Small, 9, 0).text == "0");
    CHECK(keyboard::key(Page::Small, 0, 1).text == "q");
    CHECK(keyboard::key(Page::Capitals, 0, 1).text == "Q");
    CHECK(keyboard::key(Page::Small, 9, 2).does == Does::Back);
    CHECK(keyboard::key(Page::Small, 0, 3).does == Does::Shift);
    CHECK(keyboard::key(Page::Small, 8, 3).does == Does::Symbols);
    CHECK(keyboard::key(Page::Small, 9, 3).does == Does::Join);
    CHECK(keyboard::key(Page::Symbols, 1, 0).text == "@");
    CHECK(keyboard::key(Page::Symbols, 0, 3).does == Does::Symbols); // back to letters
    CHECK(keyboard::key(Page::Symbols, 5, 3).text == " ");           // a space bar, five keys wide
    CHECK(keyboard::key(Page::Small, 10, 0).does == Does::Nothing);   // off the keyboard
}

TEST_CASE("every key has its glyph in the dot font, each its own, on its line") {
    std::set<std::string> typed;
    for (Page page : {Page::Small, Page::Capitals, Page::Symbols})
        for (int row = 0; row < keyboard::kRows; row++)
            for (int col = 0; col < keyboard::kCols; col++) {
                const keyboard::Key k = keyboard::key(page, col, row);
                const font::Letter *glyph = font::letter(keyboard::glyph(k, page));
                REQUIRE_MESSAGE(glyph, keyboard::glyph(k, page));
                CHECK(glyph->top + static_cast<int>(glyph->rows.size()) <= font::kLineRows);
                CHECK(glyph->rows[0].size() <= 5);
                if (k.does == Does::Type) typed.insert(k.text);
            }
    CHECK(typed.size() == 26 * 2 + 10 + 32 + 1); // letters both ways, digits, symbols, the space
}

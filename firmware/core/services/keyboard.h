#pragma once

#include <string>

// The keyboard of the Wi-Fi setup (services/wifi_setup.h): ten keys in four rows. Digits over small letters, shift
// for capitals; the symbols on a page of their own, a space bar five keys wide; and the keys that do: shift, the
// symbols' page and back, back a letter, join.
namespace cubey::keyboard {

constexpr int kCols = 10, kRows = 4;
enum class Page { Small, Capitals, Symbols };
enum class Does { Nothing, Type, Shift, Symbols, Back, Join };
struct Key {
    Does does;
    std::string text; // what it types (UTF-8)
};

Key key(Page page, int col, int row);
std::string glyph(const Key &key, Page page); // its picture in the dot font (ui/font.h)

} // namespace cubey::keyboard

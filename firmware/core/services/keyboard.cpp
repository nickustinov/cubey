#include "services/keyboard.h"

#include <cctype>

namespace cubey::keyboard {

namespace {

// Each row's keys, one character each: what it types, or "<" back, "^" shift, "%" the symbols' page, "=" join, "_"
// the space. The symbols' page writes those characters as themselves (\x01 back, \x02 shift... below).
const char *const kLetters[kRows] = {"1234567890", "qwertyuiop", "asdfghjkl<", "^zxcvbnm%="};
const char *const kSymbols[kRows] = {"!@#$%&*()-", "_=+.,:;?/'", "\"<>[]{}~^\x01", "\x03\\|`     \x04"};

} // namespace

Key key(Page page, int col, int row) {
    if (col < 0 || col >= kCols || row < 0 || row >= kRows) return {Does::Nothing, ""};
    const char c = (page == Page::Symbols ? kSymbols : kLetters)[row][col];
    if (page == Page::Symbols) {
        if (c == '\x01') return {Does::Back, ""};
        if (c == '\x03') return {Does::Symbols, ""};
        if (c == '\x04') return {Does::Join, ""};
        return {Does::Type, std::string(1, c)};
    }
    if (c == '<') return {Does::Back, ""};
    if (c == '^') return {Does::Shift, ""};
    if (c == '%') return {Does::Symbols, ""};
    if (c == '=') return {Does::Join, ""};
    return {Does::Type, std::string(1, page == Page::Capitals ? static_cast<char>(std::toupper(c)) : c)};
}

std::string glyph(const Key &key, Page page) {
    switch (key.does) {
    case Does::Type: return key.text == " " ? "␣" : key.text;
    case Does::Shift: return "⇧";
    case Does::Symbols: return page == Page::Symbols ? "a" : "#";
    case Does::Back: return "⌫";
    case Does::Join: return "✓";
    case Does::Nothing: return "";
    }
    return "";
}

} // namespace cubey::keyboard

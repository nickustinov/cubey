#pragma once

#include <optional>
#include <string>
#include <vector>

// Cubey's dot font, for what it writes on the dot board (ui/dots.h): the dot font of ../allflix-app
// (src/lib/dotfont.ts), one empty column between letters. Its digits: its own 1 and 3, the rest drawn in their
// proportions, 6 rows. Its letters on a line of 11 rows: the font's small ones as it has them, and q and Latvian's
// drawn in their style, from row 2 (a dot over i or j there, ascenders from row 3, the x-height rows 4–8, descenders
// to row 10; a macron or a caron on rows 2–3, ģ's comma there too, ķ ļ ņ's below); its capitals, drawn in its style
// (it has none), 6 rows on the same line as the ascenders (rows 3–8), a macron on row 1 or a caron on rows 0–1 above
// them with a row between, Ģ Ķ Ļ Ņ's comma below on rows 9–10.
namespace cubey::font {

constexpr int kDigitRows = 6;

struct Glyph {
    int width;
    const char *rows[kDigitRows]; // '#' a dot, '.' none
};

const Glyph &digit(char d); // '0'..'9'
int width(const std::string &digits); // across, with a column between each

constexpr int kLineRows = 11;
constexpr int kCapitalTop = 3, kCapitalRows = 6; // where the capitals are on the line

struct Letter {
    int top;                       // its first row of the line's 9
    std::vector<std::string> rows; // '#' a dot, '.' none; all as wide
};

// A letter as UTF-8 ("a", "š", "A", "Š"), the font's own (no copy: it is looked up for every dot drawn); none for one
// the font hasn't.
const Letter *letter(const std::string &utf8);
std::string upper(const std::string &utf8); // a small letter's capital ("ā": "Ā"); anything else as it is
// Small capitals, A to Z, for the grown-ups' screens' words (ui/menu.h): four rows from the line's top, three across (M
// and W five, N four); a small letter's, its capital's; none for anything else. Not the allflix font's: it has one size.
const Letter *tiny(const std::string &utf8);
std::vector<std::string> split(const std::string &utf8); // its letters, each as UTF-8

} // namespace cubey::font

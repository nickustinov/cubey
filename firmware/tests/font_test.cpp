#include <doctest/doctest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "letter_board.h"
#include "ui/dots.h"
#include "ui/font.h"
#include "ui/letter.h"

using namespace cubey;

TEST_CASE("the dot font's letters: the font's lowercase, q, and Latvian's; each row as wide, within the line") {
    const char *alphabet[] = {"a", "ā", "b", "c", "č", "d", "e", "ē", "f", "g", "ģ", "h", "i", "ī", "j", "k", "ķ", "l", "ļ",
                              "m", "n", "ņ", "o", "p", "q", "r", "s", "š", "t", "u", "ū", "v", "w", "x", "y", "z", "ž"};
    std::set<std::vector<std::string>> drawn;
    for (const char *name : alphabet) {
        const auto l = font::letter(name);
        REQUIRE_MESSAGE(l, name);
        CHECK(l->top >= 0);
        CHECK(l->top + static_cast<int>(l->rows.size()) <= font::kLineRows);
        for (const std::string &row : l->rows) CHECK(row.size() == l->rows[0].size());
        drawn.insert(l->rows);
    }
    CHECK(drawn.size() == sizeof alphabet / sizeof alphabet[0]); // each its own
    CHECK(font::letter("a")->rows[0] == ".###."); // the font's
    CHECK(font::letter("š")->rows.back() == font::letter("s")->rows.back()); // s with a caron
    CHECK_FALSE(font::letter("Ы"));
    CHECK_FALSE(font::letter("ы"));
    CHECK_FALSE(font::letter(""));
}

TEST_CASE("its capitals, drawn in its style: 6 rows as its digits, Latvian's marks above with a row between or a comma below") {
    const char *alphabet[] = {"A", "Ā", "B", "C", "Č", "D", "E", "Ē", "F", "G", "Ģ", "H", "I", "Ī", "J", "K", "Ķ", "L", "Ļ",
                              "M", "N", "Ņ", "O", "P", "Q", "R", "S", "Š", "T", "U", "Ū", "V", "W", "X", "Y", "Z", "Ž"};
    std::set<std::vector<std::string>> drawn;
    for (const char *name : alphabet) {
        const auto l = font::letter(name);
        REQUIRE_MESSAGE(l, name);
        CHECK(l->top + static_cast<int>(l->rows.size()) <= font::kLineRows);
        for (const std::string &row : l->rows) CHECK(row.size() == l->rows[0].size());
        drawn.insert(l->rows);
    }
    CHECK(drawn.size() == sizeof alphabet / sizeof alphabet[0]); // each its own
    CHECK(font::letter("S")->top == font::kCapitalTop);
    CHECK(font::letter("S")->rows.size() == font::kCapitalRows);
    CHECK(font::letter("Š")->rows.back() == font::letter("S")->rows.back()); // S with a caron
    CHECK(font::letter("Š")->rows[2] == ".....");                             // a row between
    CHECK(font::letter("Ņ")->top == font::kCapitalTop);                       // its comma below
    const auto x = font::letter("x"), X = font::letter("X"), h = font::letter("h");
    CHECK(x->top + x->rows.size() == X->top + X->rows.size()); // the small letters on the capitals' line
    CHECK(h->top == X->top);                                   // ascenders as tall
    CHECK(font::upper("ā") == "Ā");
    CHECK(font::upper("s") == "S");
    CHECK(font::upper("ы") == "ы");
}

TEST_CASE("its digits and a few signs on the capitals' line too, for a sum or a question; a space") {
    const auto seven = font::letter("7");
    REQUIRE(seven);
    CHECK(seven->top == font::kCapitalTop);
    CHECK(seven->rows == std::vector<std::string>(font::digit('7').rows, font::digit('7').rows + font::kDigitRows));
    for (const char *name : {"0", "9", "+", "-", "=", "?", "!", ".", ",", ":", " "}) {
        const auto l = font::letter(name);
        REQUIRE_MESSAGE(l, name);
        CHECK(l->top + static_cast<int>(l->rows.size()) <= font::kLineRows);
        for (const std::string &row : l->rows) CHECK(row.size() == l->rows[0].size());
    }
    CHECK(font::letter(" ")->rows[0].find('#') == std::string::npos);
}

using letter_board::count;
using letter_board::same_lit;

TEST_CASE("a letter big on the board, a capital: its dots two by two, in the board's middle, across and down") {
    const letter::Shown s = letter::start("", "", "s");
    REQUIRE(s.words.size() == 1);
    const letter::Frame shown = letter::frame(s, 0); // a letter alone shows at once
    int ink = 0, left = dots::kBoard, right = 0, top = dots::kBoard, bottom = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++)
            if (letter::dot(s, shown, col, row) == dots::Dot::On)
                left = std::min(left, col), right = std::max(right, col), top = std::min(top, row), bottom = std::max(bottom, row);
    const font::Letter capital = *font::letter("S");
    for (const std::string &row : capital.rows) ink += static_cast<int>(std::count(row.begin(), row.end(), '#'));
    CHECK(count(s, shown, dots::Dot::On) == 4 * ink); // the capital's
    CHECK(left + right == dots::kBoard - 1);          // across the middle
    CHECK(top + bottom == dots::kBoard - 1);          // and down
    const letter::Shown i = letter::start("", "", "ī");
    CHECK(letter::dot(i, letter::frame(i, 0), 11, 3) == dots::Dot::On); // Ī's macron, on the line's first row
    CHECK(letter::start("", "", "ы").words.empty());
}

TEST_CASE("Cubey's first word scrolls in from the right as big, stops with the letter she needs in the middle, the rest goes") {
    const letter::Shown ezis = letter::start("", "ezis", "s");
    REQUIRE(ezis.words.size() == 1);
    REQUIRE(ezis.words[0].letters.size() == 4);
    CHECK(ezis.words[0].at == 3);
    CHECK(count(ezis, letter::frame(ezis, 0), dots::Dot::On) == 0); // from just off the board's right
    CHECK(letter::frame(ezis, 5 * letter::kDotMs).left == dots::kBoard - 5);

    const letter::Shown s = letter::start("", "", "s");
    const letter::Frame end = letter::frame(ezis, 60000), alone = letter::frame(s, 0);
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) CHECK(letter::dot(ezis, end, col, row) == letter::dot(s, alone, col, row));

    const uint32_t stops = static_cast<uint32_t>(dots::kBoard - end.left) * letter::kDotMs;
    const letter::Frame stopped = letter::frame(ezis, stops);
    CHECK(stopped.left == end.left);
    CHECK(count(ezis, stopped, dots::Dot::On) > count(s, alone, dots::Dot::On)); // the I still beside it
    CHECK(count(ezis, letter::frame(ezis, stops + letter::kHoldMs), dots::Dot::Dim) > 0);
    CHECK(count(ezis, letter::frame(ezis, stops + letter::kHoldMs + letter::kDimMs), dots::Dot::Dim) == 0);
    CHECK(letter::length(ezis.words[0]) == stops + letter::kHoldMs + letter::kDimMs);

    CHECK(letter::start("", "aka", "a").words[0].at == 2);              // its last of the letter
    CHECK(letter::start("", "ыs", "s").words[0].letters.size() == 1);   // a letter the font lacks: hers alone
    CHECK(letter::start("", "ezis", "a").words[0].letters.size() == 1); // hers not in it: hers alone
}

TEST_CASE("as it is said: her word at once from its first letter, quicker; Cubey's going on from where hers stops") {
    letter::Shown shown = letter::start("suka", "", "a");
    REQUIRE(shown.words.size() == 1);
    CHECK(shown.words[0].from == 0);
    CHECK(shown.words[0].at == 3); // her A, where Cubey's word begins
    CHECK(shown.words[0].dot_ms == letter::kHersDotMs);
    CHECK(letter::frame(shown, 0).arriving); // her S in the middle, the rest of her word fading in

    REQUIRE(letter::go_on(shown, 100, "acs", "s")); // written while hers scrolls: it follows
    const uint32_t at = shown.starts[1];
    CHECK(at == letter::length(shown.words[0]));
    CHECK(letter::frame(shown, at - 1).word == 0);
    CHECK(letter::frame(shown, at).word == 1);
    CHECK(letter::frame(shown, at).arriving);
    CHECK(same_lit(shown, letter::frame(shown, at - 1), shown, letter::frame(shown, at))); // her A stays as Cubey's
    CHECK(count(shown, letter::frame(shown, at), dots::Dot::Dim) > 0);

    const letter::Shown s = letter::start("", "", "s");
    CHECK(same_lit(shown, letter::frame(shown, 60000), s, letter::frame(s, 0))); // her letter now, alone

    letter::Shown late = letter::start("suka", "", "a");
    REQUIRE(letter::go_on(late, 60000, "acs", "s")); // long after hers stopped: from then
    CHECK(late.starts[1] == 60000);
    CHECK(letter::frame(late, 60000).arriving);

    letter::Shown other = letter::start("zivis", "", "s");
    CHECK_FALSE(letter::go_on(other, 100, "acs", "s")); // Cubey's doesn't begin with her letter: shown anew
}

TEST_CASE("text Cubey writes: in capitals, still in the middle if it fits, else scrolling past from the right, again and again") {
    const letter::Shown twelve = letter::text("12");
    REQUIRE(twelve.words.size() == 1);
    CHECK(letter::length(twelve.words[0]) == 0); // still
    const letter::Frame now = letter::frame(twelve, 0), later = letter::frame(twelve, 5000);
    CHECK(now.left == (dots::kBoard - (3 + 1 + 5) * 2) / 2); // 1 is 3 dots wide, 2 is 5, a column between
    CHECK(same_lit(twelve, now, twelve, later));
    CHECK(count(twelve, now, dots::Dot::Dim) == 0);

    const letter::Shown sum = letter::text("12+34=46");
    REQUIRE(sum.words[0].letters.size() == 8);
    const uint32_t pass = letter::length(sum.words[0]);
    CHECK(pass > 0);
    CHECK(letter::frame(sum, 0).left == dots::kBoard);
    CHECK(letter::frame(sum, 5 * letter::kDotMs).left == dots::kBoard - 5);
    CHECK(letter::frame(sum, pass).left == dots::kBoard); // again
    CHECK(count(sum, letter::frame(sum, pass / 2), dots::Dot::On) > 0);

    CHECK(same_lit(letter::text("kaķis"), letter::frame(letter::text("kaķis"), 700), letter::text("KAĶIS"), letter::frame(letter::text("KAĶIS"), 700)));
    CHECK(letter::text("ы7").words[0].letters.size() == 1); // what the font hasn't is left out
    CHECK(letter::text("ыы").words.empty());
}

TEST_CASE("small capitals, for the grown-ups' screens' words: four rows, three across (M and W five), whatever the case") {
    for (char c = 'A'; c <= 'Z'; c++) {
        const font::Letter *l = font::tiny(std::string(1, c));
        REQUIRE(l != nullptr);
        CHECK(l->top == 0);
        CHECK(l->rows.size() == 4);
        CHECK(l->rows[0].size() == (c == 'M' || c == 'W' ? 5u : c == 'N' ? 4u : 3u));
    }
    CHECK(font::tiny("s") == font::tiny("S"));
    CHECK(font::tiny("ы") == nullptr);
}

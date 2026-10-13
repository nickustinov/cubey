#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lvgl.h"
#include "ui/dots.h"
#include "ui/font.h"

// A letter big on the dot board (ui/dots.h), in place of Cubey's eyes while she plays with letters: the one she needs
// now, a capital of the dot font (ui/font.h), each of its dots two by two, in the board's middle. In the word chain the
// words come first, as big, a column at a time, as they are said: hers as her turn ends, from its first letter, quicker
// (while Cubey thinks), stopping on the letter Cubey's goes on from; still a moment, the rest of it dims and goes;
// then Cubey's, once written, grows from that letter, the rest of it fading in, and goes on to the letter she needs
// now; still, the rest goes, and her letter stays. Cubey's first word comes in from the right. A word Cubey spells
// (ui/spell.h) arrives a letter at a time, as big: each new one lit beside the one before, the word moving on to it,
// those before dim; then all of it passes, lit.
namespace cubey::letter {

constexpr uint32_t kDotMs = 50;     // Cubey's word: a column of dots moved
constexpr uint32_t kHersDotMs = 30; // hers, done before Cubey says its own
constexpr uint32_t kHoldMs = 400;   // a word still, whole, before the rest goes
constexpr uint32_t kDimMs = 250;    // the rest dim, going or coming
constexpr uint32_t kSpellDotMs = 25; // a word spelled, moving on to the letter just said

struct Word {
    std::vector<const font::Letter *> letters; // the font's own
    int at;        // the letter it stops on, in the middle; none: text, all of it
    int from = -1; // the letter it starts on in the middle; none: in from the right
    uint32_t dot_ms = kDotMs;
};

// Words shown one after another, each from its start (ms after the first began), and the letter the last stops on.
struct Shown {
    std::vector<Word> words;
    std::vector<uint32_t> starts;
    std::string ends_on;
};

// What a letter message shows anew: her word (UTF-8, small letters) stopping on `stop_on`, its last such; else Cubey's
// word, from the right, stopping on `stop_on`; else, or if the word lacks it or has a letter the font hasn't,
// `stop_on` alone. Nothing if the font hasn't it.
Shown start(const std::string &hers, const std::string &cubeys, const std::string &stop_on);
// Cubey's word going on from what shows (ms after it began), from the letter that stops on, once the word before is
// still (or then, if later); false if its first letter isn't that one, or it can't be drawn.
bool go_on(Shown &shown, uint32_t ms, const std::string &cubeys, const std::string &stop_on);
// Text Cubey writes (a word, a sum), as big, all lit: still in the middle if it fits, else scrolling past from the
// right, again and again; what the font hasn't is left out, nothing if none of it.
Shown text(const std::string &utf8);
uint32_t length(const Word &word); // ms until it is its letter alone; text: one pass, 0 still

struct Frame {
    int word;       // which shows
    int left;       // its first column on the board
    dots::Dot rest; // how its letters but the one it stops on show
    bool arriving;  // from a letter in the middle: only that one lit, the rest fading in
};
Frame frame(const Shown &shown, uint32_t ms);
dots::Dot dot(const Shown &shown, const Frame &frame, int col, int row);
// A word Cubey spells: its first letters so far, each so many stopping on its last, then all of it as text; nothing
// if the font hasn't one of its letters.
Shown spelling(const std::string &utf8);
// Its moment `ms` after letter `said` (1 to all) showed: the word moving on to that letter from the one before, it lit
// and those before it dim; the first, at once in the middle.
Frame spelled(const Shown &spelling, int said, uint32_t ms);
Frame passing(const Shown &spelling, uint32_t ms); // all of it passing, lit, `ms` into its passes

lv_obj_t *create(lv_obj_t *parent, Shown shown, uint32_t now);
bool go_on(lv_obj_t *letter, const std::string &cubeys, const std::string &stop_on, uint32_t now);
void show(lv_obj_t *letter, uint32_t now); // redrawn only when it looks different

} // namespace cubey::letter

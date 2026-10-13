#include <doctest/doctest.h>

#include "letter_board.h"
#include "services/spell.h"
#include "ui/letter.h"
#include "ui/spell.h"

using namespace cubey;

using letter_board::count;
using letter_board::same_lit;

TEST_CASE("spelling: each letter shows as its voice says it, from when its speech began, in the word's order") {
    SpellRun run(3, 1000);
    run.said(0, 200);
    run.said(1, 600);
    run.said(2, 1000);
    CHECK(run.shown(5000) == 0); // its speech hasn't begun
    run.speaking(1000);
    run.speaking(1500); // only its first start counts
    CHECK(run.shown(1199) == 0);
    CHECK(run.shown(1200) == 1);
    CHECK(run.shown_since(1300) == 100);
    CHECK(run.shown(1600) == 2);
    CHECK(run.shown(2000) == 3);
    CHECK(run.shown_since(2050) == 50);
}

TEST_CASE("a letter not timed holds back those after it; once its speech is over, they all show then") {
    SpellRun run(3, 1000);
    run.said(0, 100);
    run.said(2, 300);
    run.said(7, 400); // a letter it hasn't: nothing
    run.speaking(0);
    CHECK(run.shown(1000) == 1);
    run.spoken(2000);
    run.spoken(2500); // only its first end counts
    CHECK(run.shown(1999) == 1);
    CHECK(run.shown(2000) == 3);

    SpellRun never(2, 1000); // its speech never came (the turn failed): all of it, once the turn is over
    never.said(0, 100);
    CHECK(never.shown(1000) == 0);
    never.spoken(3000);
    CHECK(never.shown(3000) == 2);
}

TEST_CASE("a letter timed before the one before it shows with it; one timed after its speech's end shows at the end") {
    SpellRun run(3, 1000);
    run.said(0, 500);
    run.said(1, 300);
    run.said(2, 9000);
    run.speaking(0);
    CHECK(run.shown(499) == 0);
    CHECK(run.shown(500) == 2);
    run.spoken(2000);
    CHECK(run.shown(2000) == 3);
}

TEST_CASE("all said: still a moment, then the whole word passes once and it is over; a word that fits stays still a while") {
    SpellRun run(2, 1000);
    run.said(0, 0);
    run.said(1, 100);
    run.speaking(0);
    CHECK_FALSE(run.passing(100 + SpellRun::kHoldMs - 1));
    REQUIRE(run.passing(100 + SpellRun::kHoldMs));
    CHECK(*run.passing(100 + SpellRun::kHoldMs + 10) == 10);
    CHECK_FALSE(run.over(100 + SpellRun::kHoldMs + 1000 - 1));
    CHECK(run.over(100 + SpellRun::kHoldMs + 1000));

    SpellRun still(1, 0);
    still.said(0, 0);
    still.speaking(0);
    CHECK_FALSE(still.over(SpellRun::kHoldMs + SpellRun::kStillMs - 1));
    CHECK(still.over(SpellRun::kHoldMs + SpellRun::kStillMs));
}

TEST_CASE("its speech began just before the tick clock wrapped") {
    SpellRun run(1, 1000);
    run.said(0, 0x200);
    run.speaking(0xFFFFFF00u);
    CHECK(run.shown(0x0FF) == 0);
    CHECK(run.shown(0x100) == 1);
}

TEST_CASE("the word spelled big: its first letter alone in the middle; each next arrives beside it and the word moves on to it, those before dim") {
    const letter::Shown word = letter::spelling("kaķis");
    REQUIRE(word.words.size() == 6); // each so far, then all of it
    const letter::Shown k = letter::start("", "", "k");
    const letter::Frame first = letter::spelled(word, 1, 0);
    CHECK(same_lit(word, first, k, letter::frame(k, 0))); // as the letter games show a letter
    CHECK(count(word, first, dots::Dot::Dim) == 0);

    const letter::Frame arriving = letter::spelled(word, 2, 0);
    CHECK(arriving.left == first.left); // the K where it was, the A lit beside it
    CHECK(count(word, arriving, dots::Dot::On) > 0);
    CHECK(count(word, arriving, dots::Dot::Dim) > 0);
    CHECK(letter::spelled(word, 2, letter::kSpellDotMs).left == first.left - 1);
    const letter::Shown a = letter::start("", "", "a");
    const letter::Frame there = letter::spelled(word, 2, 60000);
    CHECK(same_lit(word, there, a, letter::frame(a, 0))); // the A in the middle, lit
    CHECK(count(word, there, dots::Dot::Dim) > 0);         // the K before it, dim

    const letter::Shown marked = letter::start("", "", "ķ");
    CHECK(same_lit(word, letter::spelled(word, 3, 60000), marked, letter::frame(marked, 0))); // a letter with a mark, its own
}

TEST_CASE("then all of it passes, lit, as text Cubey writes; a word with a letter the font lacks is spelled on no screen") {
    const letter::Shown word = letter::spelling("kaķis");
    const letter::Shown text = letter::text("kaķis");
    const uint32_t pass = letter::length(word.words.back());
    CHECK(pass == letter::length(text.words[0]));
    CHECK(same_lit(word, letter::passing(word, pass / 3), text, letter::frame(text, pass / 3)));
    CHECK(count(word, letter::passing(word, pass / 3), dots::Dot::Dim) == 0);
    CHECK(letter::spelling("kы").words.empty());
    CHECK(letter::spelling("").words.empty());
}

TEST_CASE("the screen follows the run: the letters said so far, then all of it passing") {
    const letter::Shown word = letter::spelling("kaķis");
    SpellRun run(5, letter::length(word.words.back()));
    for (int i = 0; i < 5; i++) run.said(i, 100 + 500 * static_cast<uint32_t>(i));
    run.speaking(0);
    const letter::Frame two = spell::frame(word, run, 700);
    CHECK(two.word == letter::spelled(word, 2, 100).word);
    CHECK(two.left == letter::spelled(word, 2, 100).left);
    const uint32_t passes = 2100 + SpellRun::kHoldMs;
    CHECK(spell::frame(word, run, passes + 300).left == letter::passing(word, 300).left);
    CHECK(spell::frame(word, run, passes + 300).word == 5);
}

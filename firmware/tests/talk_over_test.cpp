#include <doctest/doctest.h>

#include "services/talk_over.h"

using namespace cubey;

constexpr uint32_t kFrameMs = 32; // the board's audio front end gives 512 samples at 16 kHz
constexpr float kFloor = -55;

// Feeds `ms` of frames at `dbfs`, speech or not, while Cubey speaks; whether any was heard.
static bool feed(TalkOver &t, uint32_t ms, float dbfs, bool speech = true) {
    bool heard = false;
    for (uint32_t at = 0; at < ms; at += kFrameMs) heard |= t.frame(true, speech, dbfs, kFrameMs);
    return heard;
}

TEST_CASE("in a quiet room her voice talks over Cubey: heard at once (it ducks), talking over after kTalkOverMs") {
    TalkOver t(kFloor);
    feed(t, 4000, -66, false); // what the cancelling leaves of Cubey
    CHECK(t.background() == doctest::Approx(-66).epsilon(0.02));
    CHECK(t.frame(true, true, -45, kFrameMs));
    CHECK_FALSE(t.talking_over());
    feed(t, TalkOver::kTalkOverMs, -45);
    CHECK(t.talking_over());
}

TEST_CASE("a TV across the room is speech too, but the background follows it: only her voice, louder at the cube, talks over") {
    TalkOver t(kFloor);
    const auto tv = [&](int cycles) {
        bool heard = false, over = false;
        for (int i = 0; i < cycles; i++) heard |= feed(t, 300, -48), heard |= feed(t, 200, -62, false), over |= t.talking_over();
        return heard || over;
    };
    tv(10); // its words and the gaps between, for 5 s: on just now, it may be taken for her
    CHECK(t.background() > -58); // its words and gaps, -54 on average, over a few seconds
    CHECK_FALSE(tv(20)); // learned
    feed(t, 400, -36); // hers
    CHECK(t.talking_over());
}

TEST_CASE("never below the floor, however quiet the room (what the cancelling leaves of Cubey's loudest)") {
    TalkOver t(kFloor);
    feed(t, 4000, -90, false);
    CHECK_FALSE(feed(t, 1000, -58));
    CHECK_FALSE(t.talking_over());
}

TEST_CASE("her words have gaps between them: short ones go on counting, a long one starts over") {
    TalkOver t(kFloor);
    feed(t, 4000, -66, false);
    feed(t, 160, -45);
    feed(t, 96, -64, false); // between two words
    CHECK_FALSE(t.talking_over());
    feed(t, 128, -45);
    CHECK(t.talking_over());
    TalkOver u(kFloor);
    feed(u, 4000, -66, false);
    feed(u, 160, -45);
    feed(u, 400, -66, false);
    feed(u, 160, -45);
    CHECK_FALSE(u.talking_over());
}

TEST_CASE("her voice raises the background only a little before Cubey stops; nothing counts while it is quiet") {
    TalkOver t(kFloor);
    feed(t, 4000, -66, false);
    CHECK(feed(t, 1500, -42)); // the server waits for her first word meanwhile
    CHECK(t.frame(true, true, -42, kFrameMs));
    const float background = t.background();
    CHECK_FALSE(t.frame(false, true, -40, kFrameMs)); // Cubey has stopped
    CHECK_FALSE(t.talking_over());
    CHECK(t.background() == background); // only while it speaks
}

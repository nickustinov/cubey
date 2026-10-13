#include <doctest/doctest.h>
#include <algorithm>
#include <random>
#include "services/link_inbox.h"

using cubey::LinkInbox;

TEST_CASE("text waits for both the last transport chunk and websocket FIN") {
    LinkInbox inbox(32, 48, 2);
    std::string out;
    CHECK(inbox.receive(1, false, 0, 4, "ab", 2) == LinkInbox::Result::Accepted);
    CHECK(inbox.receive(1, false, 2, 4, "cd", 2) == LinkInbox::Result::Accepted);
    CHECK_FALSE(inbox.pop(out));
    CHECK(inbox.receive(9, true, 0, 1, "!", 1) == LinkInbox::Result::Accepted);
    CHECK(inbox.receive(0, true, 0, 2, "ef", 2) == LinkInbox::Result::Accepted);
    REQUIRE(inbox.pop(out));
    CHECK(out == "abcdef");
}

TEST_CASE("binary continuation survives control frames and resets with the connection") {
    LinkInbox inbox(32, 48, 2);
    CHECK(inbox.receive(2, false, 0, 1, "a", 1) == LinkInbox::Result::Speech);
    inbox.receive(9, true, 0, 0, "", 0);
    CHECK(inbox.receive(0, true, 0, 1, "b", 1) == LinkInbox::Result::Speech);
    inbox.receive(1, false, 0, 3, "old", 3);
    inbox.clear();
    inbox.receive(1, true, 0, 3, "new", 3);
    std::string out;
    REQUIRE(inbox.pop(out));
    CHECK(out == "new");
}

TEST_CASE("inbox bounds message size, queue bytes, and count") {
    LinkInbox inbox(8, 12, 2);
    CHECK(inbox.receive(1, true, 0, 9, "x", 1) == LinkInbox::Result::Overflow);
    inbox.clear();
    inbox.receive(1, false, 0, 6, "123456", 6);
    CHECK(inbox.receive(0, true, 0, 3, "789", 3) == LinkInbox::Result::Overflow);
    inbox.clear();
    inbox.receive(1, true, 0, 8, "12345678", 8);
    CHECK(inbox.receive(1, true, 0, 5, "12345", 5) == LinkInbox::Result::Overflow);
    inbox.clear();
    inbox.receive(1, true, 0, 1, "a", 1);
    inbox.receive(1, true, 0, 1, "b", 1);
    CHECK(inbox.receive(1, true, 0, 1, "c", 1) == LinkInbox::Result::Overflow);
    std::string out;
    REQUIRE(inbox.pop(out));
    CHECK(out == "a");
}

TEST_CASE("repeated fragmented messages release their budget when consumed") {
    LinkInbox inbox(512, 512, 2);
    std::mt19937 rng(23);
    for (int turn = 0; turn < 10000; ++turn) {
        const std::string expected(1 + rng() % 512, static_cast<char>('a' + turn % 26));
        size_t at = 0;
        while (at < expected.size()) {
            const size_t frame = std::min<size_t>(1 + rng() % 100, expected.size() - at);
            for (size_t offset = 0; offset < frame;) {
                const size_t chunk = std::min<size_t>(1 + rng() % 31, frame - offset);
                REQUIRE(inbox.receive(at ? 0 : 1, at + frame == expected.size(), offset, frame,
                                      expected.data() + at + offset, chunk) == LinkInbox::Result::Accepted);
                offset += chunk;
            }
            at += frame;
        }
        std::string actual;
        REQUIRE(inbox.pop(actual));
        REQUIRE(actual == expected);
        REQUIRE_FALSE(inbox.pop(actual));
    }
}

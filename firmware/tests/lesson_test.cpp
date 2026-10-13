#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "services/lesson.h"

using namespace cubey;

// A 2 x 3 PNG, in base64.
static const char *kPng = "iVBORw0KGgoAAAANSUhEUgAAAAIAAAADCAYAAAC56t6BAAAAEUlEQVR4nGP4z8DwH4QZMBgAoXkL9U3EmgcAAAAASUVORK5CYII=";

TEST_CASE("base64: padding, none, and anything invalid is nothing") {
    CHECK(base64_decode("TWFu") == std::vector<uint8_t>{'M', 'a', 'n'});
    CHECK(base64_decode("TWE=") == std::vector<uint8_t>{'M', 'a'});
    CHECK(base64_decode("TQ==") == std::vector<uint8_t>{'M'});
    CHECK(base64_decode("").empty());
    CHECK(base64_decode("TWF").empty());  // not a multiple of four
    CHECK(base64_decode("TW!u").empty()); // not base64
    CHECK(base64_decode("T=Fu").empty()); // padding in the middle
    CHECK(base64_decode("+/+/") == std::vector<uint8_t>{0xFB, 0xFF, 0xBF});
}

TEST_CASE("a PNG's size, from its header; anything else has none") {
    uint32_t w = 0, h = 0;
    CHECK(png_size(base64_decode(kPng), w, h));
    CHECK(w == 2);
    CHECK(h == 3);
    CHECK_FALSE(png_size(base64_decode("TWFu"), w, h));
}

TEST_CASE("the word game's picture over all of the screen, or none for Cubey's eyes") {
    auto show = lesson_show(std::string(R"({"type":"lesson.show","picture":")") + kPng + R"("})");
    REQUIRE(show);
    CHECK(*show == base64_decode(kPng));
    CHECK(lesson_show(R"({"type":"lesson.show"})")->empty());
    CHECK(lesson_show(R"({"type":"lesson.show","picture":"TWFu"})")->empty()); // not a PNG
    CHECK_FALSE(lesson_show(R"({"type":"timer","minutes":3})"));
    CHECK_FALSE(lesson_show("not json"));
}

TEST_CASE("pictures to tap: each with its id; bad ones skipped, three at most") {
    const std::string png = kPng;
    auto choose = lesson_choose(R"({"type":"lesson.choose","pictures":[{"id":"cat","picture":")" + png +
                                R"("},{"id":"dog","picture":"TW!u"},{"picture":")" + png + R"("},{"id":"cow","picture":")" +
                                png + R"("}]})");
    REQUIRE(choose);
    REQUIRE(choose->size() == 2);
    CHECK((*choose)[0].id == "cat");
    CHECK((*choose)[0].png == base64_decode(kPng));
    CHECK((*choose)[1].id == "cow");
    std::string four = R"({"type":"lesson.choose","pictures":[)";
    for (const char *id : {"a", "b", "c", "d"}) four += std::string(R"({"id":")") + id + R"(","picture":")" + png + R"("},)";
    four.back() = ']';
    CHECK(lesson_choose(four + "}")->size() == 3);
    CHECK(lesson_choose(R"({"type":"lesson.choose"})")->empty());
    CHECK_FALSE(lesson_choose(R"({"type":"lesson.show"})"));
}

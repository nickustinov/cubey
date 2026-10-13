#include <doctest/doctest.h>

#include "services/message.h"

using namespace cubey;

TEST_CASE("a server message: its type, and whether a said was the goodbye") {
    CHECK(parse_message(R"({"type":"heard"})").type == "heard");
    CHECK_FALSE(parse_message(R"({"type":"said","heard":"Kāpēc?","said":"Tāpēc.","last":false})").last);
    CHECK(parse_message(R"({"type":"said","heard":"Atā","said":"Atā!","last":true})").last);
    CHECK(parse_message("not json").type.empty());
    CHECK(parse_message(R"({"type":7})").type.empty());
}

TEST_CASE("a timer from the server: how many minutes, and what for") {
    const ServerMessage timer = parse_message(R"({"type":"timer","minutes":40,"for":"cartoons"})");
    CHECK(timer.minutes == 40);
    CHECK(timer.kind == "cartoons");
    CHECK(parse_message(R"({"type":"timer","minutes":-3})").minutes == 0);
}

TEST_CASE("the picture she tapped, its id escaped") {
    CHECK(tapped_message("cat") == R"({"type":"tapped","id":"cat"})");
    CHECK(tapped_message(R"(a"b)") == R"({"type":"tapped","id":"a\"b"})");
}

TEST_CASE("a letter to show big (the games with letters), or none: the game is over") {
    CHECK(parse_message(R"({"type":"letter","letter":"š"})").letter == "š");
    CHECK(parse_message(R"({"type":"letter","letter":"s","word":"ezis"})").word == "ezis"); // Cubey's, in the chain
    CHECK(parse_message(R"({"type":"letter","letter":"s","word":"ezis","hers":"egle"})").hers == "egle"); // hers before it
    CHECK(parse_message(R"({"type":"talk_over","on":true})").on);
    CHECK_FALSE(parse_message(R"({"type":"talk_over","on":false})").on);
    CHECK(parse_message(R"({"type":"letter"})").letter.empty());
}

TEST_CASE("how many dots to show, or none: the game is over") {
    CHECK(parse_message(R"({"type":"count","n":4})").n == 4);
    CHECK(parse_message(R"({"type":"count"})").n == 0);
    CHECK(parse_message(R"({"type":"count","n":-2})").n == 0);
}

TEST_CASE("a word Cubey spells (its letters), and when each of them is said in its speech") {
    CHECK(parse_message(R"({"type":"spell","word":"kaķis"})").word == "kaķis");
    const ServerMessage at = parse_message(R"({"type":"spell.at","i":0,"ms":700})");
    CHECK(at.i == 0);
    CHECK(at.ms == 700);
    const ServerMessage none = parse_message(R"({"type":"spell.at","i":-1})");
    CHECK(none.i < 0); // none: nothing to time
    CHECK(none.ms < 0);
}

TEST_CASE("something to show on the screen: a picture's rows, or text") {
    const ServerMessage picture = parse_message(R"({"type":"show","rows":["#+.","..#"]})");
    CHECK(picture.rows == std::vector<std::string>{"#+.", "..#"});
    CHECK(picture.text.empty());
    CHECK(parse_message(R"({"type":"show","text":"12+34=46"})").text == "12+34=46");
    CHECK(parse_message(R"({"type":"show","rows":[1,"#"]})").rows == std::vector<std::string>{"#"}); // strings only
}

TEST_CASE("the languages for the grown-ups' menu: Cubey's, and all it speaks") {
    const ServerMessage s = parse_message(R"({"type":"settings","language":"lv","languages":["en","lv","de"]})");
    CHECK(s.language == "lv");
    CHECK(s.languages == std::vector<std::string>{"en", "lv", "de"});
    CHECK_FALSE(s.fresh);
    CHECK(parse_message(R"({"type":"settings","language":"lv","languages":["en"],"new":true})").fresh); // not set up yet
    CHECK(s.brightness == 0); // not said
    CHECK(s.theme.empty()); // not said
    CHECK(parse_message(R"({"type":"settings","language":"lv","languages":["en"],"theme":"green"})").theme == "green");
    CHECK(parse_message(R"({"type":"settings","language":"lv","languages":["en"],"brightness":40})").brightness == 40);
    CHECK(parse_message(R"({"type":"settings","language":"lv","languages":["en"],"volume":60})").volume == 60);
    CHECK(s.volume == 0); // not said
}

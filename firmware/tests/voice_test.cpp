#include <doctest/doctest.h>

#include "fake_hal.h"
#include "services/voice.h"

using namespace cubey;

static const std::string kHello = R"({"type":"hello","speaker_rate":24000})";
static const std::string kOpen = R"({"type":"open"})";
static const std::string kListen = R"({"type":"listen"})";
static const std::string kStop = R"({"type":"stop"})";
static const std::string kClose = R"({"type":"close"})";
static const std::string kOverhear = R"({"type":"overhear"})";
static const std::string kHeard = R"({"type":"heard"})";
static const std::string kSaid = R"({"type":"said","heard":"Kāpēc?","said":"Tāpēc.","last":false})";
static const std::string kLast = R"({"type":"said","heard":"Atā","said":"Atā!","last":true})";
static const std::string kFailed = R"({"type":"failed","reason":"no network"})";

// The server has the hello, the talk is open and Cubey has said hello: the device listens.
static void listening(Voice &voice, uint32_t now = 0) {
    voice.tick(now);
    REQUIRE(voice.open(now));
    fake::hal.incoming.push_back(kSaid);
    voice.tick(now);
    voice.tick(now);
    REQUIRE(voice.state() == VoiceState::Listening);
    fake::hal.sent.clear();
}

// She asks, Cubey answers (`answer`), and the speech finishes playing at `now`.
static void one_turn(Voice &voice, uint32_t now, const std::string &answer = kSaid) {
    fake::hal.incoming.push_back(kHeard);
    voice.tick(now - 900);
    fake::hal.speaker_busy = true;
    voice.tick(now - 600);
    fake::hal.incoming.push_back(answer);
    voice.tick(now - 300);
    fake::hal.speaker_busy = false;
    voice.tick(now);
}

TEST_CASE("says hello with the speaker rate once connected, and again on every new connection") {
    fake::reset();
    Voice voice;
    voice.tick(0);
    voice.tick(40);
    CHECK(fake::hal.sent == std::vector<std::string>{kHello});
    fake::hal.session++;
    voice.tick(80);
    CHECK(fake::hal.sent == std::vector<std::string>{kHello, kHello});
}

TEST_CASE("its hello says its firmware, for updates: its version, its build, and an update it rolled back from") {
    fake::reset();
    fake::hal.firmware = {"a1b92ab", "0123456789abcdef", "1111111111111111"};
    Voice voice;
    voice.tick(0);
    CHECK(fake::hal.sent == std::vector<std::string>{
        R"({"type":"hello","speaker_rate":24000,"version":"a1b92ab","build":"0123456789abcdef","failed":"1111111111111111"})"});
    fake::reset();
    fake::hal.firmware = {"a1b92ab", "0123456789abcdef", ""};
    Voice kept;
    kept.tick(0);
    CHECK(fake::hal.sent == std::vector<std::string>{R"({"type":"hello","speaker_rate":24000,"version":"a1b92ab","build":"0123456789abcdef"})"});
}

TEST_CASE("opening: the server is told, Cubey's hello plays, then the device listens") {
    fake::reset();
    Voice voice;
    voice.tick(0);
    fake::hal.sent.clear();
    CHECK(voice.open(100));
    CHECK(fake::hal.sent == std::vector<std::string>{kOpen});
    CHECK(voice.state() == VoiceState::Thinking);
    CHECK_FALSE(fake::hal.mic_on);

    fake::hal.speaker_busy = true;
    voice.tick(400);
    CHECK(voice.state() == VoiceState::Speaking);
    fake::hal.incoming.push_back(R"({"type":"said","heard":"","said":"Čau!","last":false})");
    voice.tick(1000);
    CHECK(voice.state() == VoiceState::Speaking); // still playing
    fake::hal.speaker_busy = false;
    voice.tick(1500);
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kOpen, kListen});
}

TEST_CASE("no server: no talk opens") {
    fake::reset();
    fake::hal.connected = false;
    Voice voice;
    CHECK_FALSE(voice.open(0));
    CHECK_FALSE(voice.is_open());
    CHECK(fake::hal.sent.empty());
}

TEST_CASE("opening with what happened instead of a hello: the timer's end") {
    fake::reset();
    Voice voice;
    voice.tick(0);
    fake::hal.sent.clear();
    REQUIRE(voice.open(100, false, R"("timer":"done","for":"cartoons")"));
    CHECK(fake::hal.sent == std::vector<std::string>{R"({"type":"open","timer":"done","for":"cartoons"})"});
}

TEST_CASE("opening again after the link was lost: the server goes on without a hello") {
    fake::reset();
    Voice voice;
    voice.tick(0);
    fake::hal.sent.clear();
    REQUIRE(voice.open(100, true));
    CHECK(fake::hal.sent == std::vector<std::string>{R"({"type":"open","again":true})"});
}

TEST_CASE("a question: listen, the server hears the pause, think, speak, listen again") {
    fake::reset();
    Voice voice;
    listening(voice);

    fake::hal.incoming.push_back(kHeard);
    voice.tick(2000);
    CHECK(voice.state() == VoiceState::Thinking);
    CHECK_FALSE(fake::hal.mic_on); // no echo cancelling: the mic rests while Cubey answers

    fake::hal.speaker_busy = true;
    voice.tick(3500);
    CHECK(voice.state() == VoiceState::Speaking);
    fake::hal.incoming.push_back(kSaid);
    fake::hal.speaker_busy = false;
    voice.tick(6000);
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kListen});
}

TEST_CASE("the last answer is the goodbye: once it has played, the talk is over") {
    fake::reset();
    fake::hal.hears_over_speech = true;
    Voice voice;
    listening(voice);
    one_turn(voice, 5000, kLast);
    CHECK_FALSE(voice.is_open());
    CHECK(voice.state() == VoiceState::Idle);
    CHECK(voice.ended() == TalkEnd::Goodbye);
    CHECK_FALSE(fake::hal.mic_on); // it listened over the goodbye; no more
    CHECK(fake::hal.sent == std::vector<std::string>{kOverhear}); // nothing to close: the server ended it
}

TEST_CASE("closing stops whatever Cubey is doing, and tells the server the talk is over") {
    fake::reset();
    Voice voice;
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(2000);
    fake::hal.speaker_busy = true;
    voice.tick(3000);

    voice.close();
    CHECK_FALSE(voice.is_open());
    CHECK(voice.state() == VoiceState::Idle);
    CHECK(voice.ended() == TalkEnd::None); // closed here: nothing to follow
    CHECK(fake::hal.speaker_stops == 1);
    CHECK(fake::hal.sent == std::vector<std::string>{kClose});
    voice.tick(3500);
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.sent.size() == 1);
    voice.close(); // closed already: nothing more
    CHECK(fake::hal.sent.size() == 1);
}

TEST_CASE("closing while listening turns the mic off") {
    fake::reset();
    Voice voice;
    listening(voice);
    voice.close();
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kClose});
}

TEST_CASE("closes by itself after half a minute with nobody speaking, counted from the last speech") {
    fake::reset();
    Voice voice;
    listening(voice, 1000);
    voice.tick(31000);
    CHECK(voice.is_open());
    voice.tick(31001);
    CHECK_FALSE(voice.is_open());
    CHECK(voice.ended() == TalkEnd::Quiet);
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kClose});

    fake::reset();
    Voice again;
    listening(again);
    one_turn(again, 50000);
    again.tick(80000);
    CHECK(again.is_open());
    again.tick(80001);
    CHECK_FALSE(again.is_open());
}

TEST_CASE("while something goes on (she brushes), nobody speaking is no reason to close") {
    fake::reset();
    Voice voice;
    listening(voice, 1000);
    for (uint32_t t = 1000; t < 120000; t += 1000) {
        voice.stay(t);
        voice.tick(t);
    }
    CHECK(voice.is_open());
    voice.tick(120000 + 30001);
    CHECK_FALSE(voice.is_open()); // and half a minute after it
}

TEST_CASE("after a failed turn the device waits a moment before listening again") {
    fake::reset();
    Voice voice;
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    fake::hal.incoming.push_back(kFailed);
    fake::hal.sent.clear();
    voice.tick(1100);
    CHECK(voice.state() == VoiceState::Idle);
    voice.tick(6099);
    CHECK_FALSE(fake::hal.mic_on);
    voice.tick(6100);
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.sent == std::vector<std::string>{kListen});
}

TEST_CASE("gives up on an answer that never comes and listens again") {
    fake::reset();
    Voice voice;
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    voice.tick(16000);
    CHECK(voice.state() == VoiceState::Thinking);
    voice.tick(16001);
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.sent == std::vector<std::string>{kStop, kListen});
}

TEST_CASE("losing the server ends the talk: the mic goes off and the speech stops") {
    fake::reset();
    Voice voice;
    listening(voice);
    fake::hal.connected = false;
    voice.tick(100);
    CHECK_FALSE(voice.is_open());
    CHECK(voice.ended() == TalkEnd::Lost);
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.speaker_stops == 1);
}

TEST_CASE("a new connection between two ticks ends the talk too: the server has forgotten it") {
    fake::reset();
    Voice voice;
    listening(voice);
    fake::hal.session++;
    voice.tick(100);
    CHECK_FALSE(voice.is_open());
    CHECK(voice.ended() == TalkEnd::Lost);
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kHello});
}

TEST_CASE("messages from an old talk don't reach a closed one") {
    fake::reset();
    Voice voice;
    listening(voice);
    voice.close();
    fake::hal.incoming.push_back(kHeard);
    fake::hal.incoming.push_back(kSaid);
    voice.tick(100);
    voice.tick(200);
    CHECK(voice.state() == VoiceState::Idle);
    CHECK_FALSE(fake::hal.mic_on);
}

TEST_CASE("messages that aren't the talk's go on, talk or no talk: show the battery") {
    fake::reset();
    Voice voice;
    std::vector<std::string> other;
    const auto pass = [&](const ServerMessage &msg, const std::string &json) {
        other.push_back(msg.type);
        CHECK(json.find(msg.type) != std::string::npos); // as it came
    };
    listening(voice);
    fake::hal.incoming = {kHeard, R"({"type":"battery"})"};
    voice.tick(100, pass);
    CHECK(voice.state() == VoiceState::Thinking);
    CHECK(other == std::vector<std::string>{"battery"});
    voice.close();
    fake::hal.incoming = {kSaid, R"({"type":"battery"})"}; // the talk's, but there is none now
    voice.tick(200, pass);
    CHECK(other == std::vector<std::string>{"battery", "battery"});
}

TEST_CASE("where the platform cancels its echo, the mic streams while Cubey speaks, and the server is told") {
    fake::reset();
    fake::hal.hears_over_speech = true;
    Voice voice;
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    CHECK_FALSE(fake::hal.mic_on); // thinking: it rests
    fake::hal.speaker_busy = true;
    voice.tick(1100);
    CHECK(voice.state() == VoiceState::Speaking);
    CHECK(fake::hal.mic_on);
    voice.tick(1200);
    CHECK(fake::hal.sent == std::vector<std::string>{kOverhear}); // once
    fake::hal.incoming.push_back(kSaid);
    fake::hal.speaker_busy = false;
    voice.tick(2000);
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kOverhear, kListen});
}

TEST_CASE("the platform's own detector hears her voice over Cubey's speech: the server is told, once an answer") {
    fake::reset();
    fake::hal.hears_over_speech = true;
    Voice voice;
    listening(voice);
    fake::hal.voice_over = true; // from before it spoke: stale
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    fake::hal.speaker_busy = true;
    voice.tick(1100);
    voice.tick(1200);
    CHECK(fake::hal.sent == std::vector<std::string>{kOverhear});
    fake::hal.voice_over = true;
    voice.tick(1300);
    fake::hal.voice_over = true;
    voice.tick(1400);
    CHECK(fake::hal.sent == std::vector<std::string>{kOverhear, R"({"type":"voice"})"});
}

TEST_CASE("the server may turn talking over Cubey off (CUBEY_TALK_OVER): the mic rests while it speaks, nothing sent") {
    fake::reset();
    fake::hal.hears_over_speech = true;
    Voice voice;
    fake::hal.incoming.push_back(R"({"type":"talk_over","on":false})");
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    fake::hal.speaker_busy = true;
    fake::hal.voice_over = true;
    voice.tick(1100);
    voice.tick(1200);
    CHECK(voice.state() == VoiceState::Speaking);
    CHECK_FALSE(fake::hal.mic_on); // so the platform neither streams nor ducks Cubey for the room's noise
    CHECK(fake::hal.sent.empty());
    fake::hal.incoming.push_back(R"({"type":"talk_over","on":true})"); // on again (a new connection's)
    fake::hal.incoming.push_back(kSaid);
    fake::hal.speaker_busy = false;
    voice.tick(2000);
    fake::hal.sent.clear();
    one_turn(voice, 4000);
    CHECK(fake::hal.sent.front() == kOverhear);
}

TEST_CASE("the server heard her over Cubey: the speech stops and it listens to her; its goodbye is no more") {
    fake::reset();
    fake::hal.hears_over_speech = true;
    Voice voice;
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    fake::hal.speaker_busy = true;
    voice.tick(1100);
    fake::hal.incoming.push_back(kLast);
    fake::hal.incoming.push_back(R"({"type":"interrupted"})");
    fake::hal.sent.clear();
    voice.tick(1200);
    CHECK(voice.state() == VoiceState::Listening);
    CHECK_FALSE(fake::hal.speaker_busy);
    CHECK(fake::hal.mic_on);
    CHECK(fake::hal.sent.empty()); // the server listens already
    one_turn(voice, 3000);
    CHECK(voice.is_open());
    CHECK(voice.state() == VoiceState::Listening);
}

TEST_CASE("a tapped picture: sent while listening, kept while the question is still being asked, dropped while thinking") {
    static const std::string kTapped = R"({"type":"tapped","id":"cat"})";
    fake::reset();
    Voice voice;
    listening(voice);
    fake::hal.sent.clear();
    CHECK(voice.tap("cat"));
    CHECK(fake::hal.sent == std::vector<std::string>{kTapped});

    fake::hal.sent.clear();
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    CHECK_FALSE(voice.tap("dog")); // thinking: the pictures on show belong to the last question
    fake::hal.speaker_busy = true;
    voice.tick(1500);
    CHECK(voice.tap("cat")); // the next question is being asked: its answer
    CHECK(fake::hal.sent.empty());
    fake::hal.incoming.push_back(kSaid);
    fake::hal.speaker_busy = false;
    voice.tick(2000);
    CHECK(fake::hal.sent == std::vector<std::string>{kListen, kTapped});
}

TEST_CASE("a kept tap goes with the talk when it ends; no talk, no tap") {
    fake::reset();
    Voice voice;
    CHECK_FALSE(voice.tap("cat"));
    listening(voice);
    fake::hal.incoming.push_back(kHeard);
    voice.tick(1000);
    fake::hal.speaker_busy = true;
    voice.tick(1500);
    voice.tap("cat");
    voice.close();
    fake::hal.speaker_busy = false;
    fake::hal.sent.clear();
    REQUIRE(voice.open(2000));
    fake::hal.incoming.push_back(kSaid);
    voice.tick(2100);
    CHECK(fake::hal.sent == std::vector<std::string>{kOpen, kListen});
}

// Held to talk: she holds the screen as she talks.
static const std::string kListenHeld = R"({"type":"listen","hold":true})";
static const std::string kRelease = R"({"type":"release"})";

// Held to talk: the talk is open and Cubey has said hello: the device waits for her to hold its screen.
static void waiting(Voice &voice, uint32_t now = 0) {
    voice.set_hold(true);
    voice.tick(now);
    REQUIRE(voice.open(now));
    fake::hal.incoming.push_back(kSaid);
    voice.tick(now);
    voice.tick(now);
    REQUIRE(voice.state() == VoiceState::Idle);
    fake::hal.sent.clear();
}

TEST_CASE("held to talk: once Cubey has spoken it waits for her; held, it listens; let go, her turn is over") {
    fake::reset();
    Voice voice;
    waiting(voice);
    voice.tick(10000);
    CHECK(voice.state() == VoiceState::Idle);
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.sent.empty());

    CHECK(voice.press(10000));
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kListenHeld});
    CHECK_FALSE(voice.press(10100)); // held already
    voice.release(11000);
    CHECK(voice.state() == VoiceState::Thinking);
    CHECK_FALSE(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kListenHeld, kRelease});

    fake::hal.incoming.push_back(kHeard); // the server's, as her turn ended there
    fake::hal.speaker_busy = true;
    voice.tick(11300);
    CHECK(voice.state() == VoiceState::Speaking);
    fake::hal.incoming.push_back(kSaid);
    fake::hal.speaker_busy = false;
    voice.tick(12000);
    CHECK(voice.state() == VoiceState::Idle); // waits for her again
    voice.tick(20000);
    CHECK(voice.state() == VoiceState::Idle);
    CHECK(fake::hal.sent == std::vector<std::string>{kListenHeld, kRelease});
}

TEST_CASE("held to talk: no talk, or hands-free, holding it is nothing") {
    fake::reset();
    Voice closed;
    closed.set_hold(true);
    closed.tick(0);
    CHECK_FALSE(closed.press(0));

    fake::reset();
    Voice free;
    listening(free);
    CHECK_FALSE(free.press(100));
    free.release(200);
    CHECK(free.state() == VoiceState::Listening);
    CHECK(fake::hal.sent.empty());
}

TEST_CASE("held to talk: held as Cubey speaks, it stops, and so does the speech on its way until the server listens") {
    fake::reset();
    Voice voice;
    waiting(voice);
    voice.press(100);
    voice.release(500);
    fake::hal.speaker_busy = true;
    voice.tick(600);
    REQUIRE(voice.state() == VoiceState::Speaking);
    fake::hal.sent.clear();
    const int stops = fake::hal.speaker_stops;

    CHECK(voice.press(700));
    CHECK(voice.state() == VoiceState::Listening);
    CHECK(fake::hal.mic_on);
    CHECK(fake::hal.sent == std::vector<std::string>{kListenHeld});
    CHECK(fake::hal.speaker_stops == stops + 1);
    fake::hal.speaker_busy = true; // the answer's speech, sent before the server heard her
    voice.tick(750);
    CHECK_FALSE(fake::hal.speaker_busy);
    fake::hal.incoming.push_back(R"({"type":"listening"})"); // all that came before it was the answer's
    voice.tick(800);
    const int heard = fake::hal.speaker_stops;
    voice.tick(850);
    CHECK(fake::hal.speaker_stops == heard);
    CHECK(voice.state() == VoiceState::Listening);
}

TEST_CASE("held to talk: held through Cubey's goodbye, the talk goes on") {
    fake::reset();
    Voice voice;
    waiting(voice);
    voice.press(100);
    voice.release(500);
    fake::hal.speaker_busy = true;
    fake::hal.incoming.push_back(kLast);
    voice.tick(600);
    CHECK(voice.press(700));
    voice.tick(800);
    CHECK(voice.is_open());
    CHECK(voice.state() == VoiceState::Listening);
}

TEST_CASE("held to talk: a minute with nobody holding it ends the talk; holding it is no quiet") {
    fake::reset();
    Voice voice;
    waiting(voice);
    voice.tick(59000);
    CHECK(voice.is_open()); // half a minute is hands-free's
    voice.press(59000);
    voice.tick(150000); // held all along
    CHECK(voice.is_open());
    voice.release(150000);
    fake::hal.incoming.push_back(kSaid);
    voice.tick(150100);
    REQUIRE(voice.state() == VoiceState::Idle);
    voice.tick(210100);
    CHECK(voice.is_open());
    voice.tick(210101);
    CHECK_FALSE(voice.is_open());
    CHECK(voice.ended() == TalkEnd::Quiet);
}

TEST_CASE("held to talk: no talking over Cubey; the mic is on only while she holds it") {
    fake::reset();
    fake::hal.hears_over_speech = true;
    Voice voice;
    waiting(voice);
    voice.press(100);
    voice.release(500);
    fake::hal.speaker_busy = true;
    voice.tick(600);
    CHECK(voice.state() == VoiceState::Speaking);
    CHECK_FALSE(fake::hal.mic_on);
    fake::hal.voice_over = true;
    voice.tick(700);
    CHECK(fake::hal.sent == std::vector<std::string>{kListenHeld, kRelease});
}

TEST_CASE("held to talk: a tapped picture is her turn while it waits for her, and kept while the question is asked") {
    static const std::string kCat = R"({"type":"tapped","id":"cat"})", kDog = R"({"type":"tapped","id":"dog"})";
    fake::reset();
    Voice voice;
    waiting(voice);
    CHECK(voice.tap("cat"));
    voice.tick(100);
    CHECK(fake::hal.sent == std::vector<std::string>{kCat});
    CHECK(voice.state() == VoiceState::Thinking);
    CHECK_FALSE(voice.tap("dog")); // the pictures on show belong to the last question

    fake::hal.speaker_busy = true;
    voice.tick(200);
    CHECK(voice.tap("dog")); // the next question is being asked: its answer
    fake::hal.incoming.push_back(kSaid);
    fake::hal.speaker_busy = false;
    voice.tick(300);
    CHECK(fake::hal.sent == std::vector<std::string>{kCat, kDog});
    CHECK(voice.state() == VoiceState::Thinking);
}

TEST_CASE("her turn: hands-free, while it listens; held to talk, while it waits for her or she holds its screen") {
    fake::reset();
    Voice free;
    CHECK_FALSE(free.her_turn());
    listening(free);
    CHECK(free.her_turn());
    fake::hal.incoming.push_back(kHeard);
    free.tick(1000);
    CHECK_FALSE(free.her_turn()); // thinking

    fake::reset();
    Voice held;
    waiting(held);
    CHECK(held.her_turn());
    held.press(100);
    CHECK(held.her_turn());
    held.release(200);
    CHECK_FALSE(held.her_turn());
}

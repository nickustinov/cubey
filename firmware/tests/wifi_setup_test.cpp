#include <doctest/doctest.h>

#include "fake_hal.h"
#include "services/wifi_setup.h"

using namespace cubey;
using Screen = WifiSetup::Screen;

static WifiSetup scanned(std::vector<Network> networks, bool welcome = false) {
    fake::reset();
    WifiSetup setup;
    setup.open(welcome, 0);
    fake::hal.networks = std::move(networks);
    fake::hal.scanned = true;
    if (welcome) setup.tap(10);
    setup.follow(10);
    return setup;
}

TEST_CASE("Wi-Fi setup: a welcome first when there is no network, till it is tapped; then the networks around, from a scan") {
    fake::reset();
    WifiSetup setup;
    setup.open(true, 0);
    CHECK(setup.screen() == Screen::Welcome);
    CHECK(fake::hal.scans == 1); // looking meanwhile
    setup.follow(60000);
    CHECK(setup.screen() == Screen::Welcome); // waits for its tap
    setup.tap(60000);
    CHECK(setup.screen() == Screen::Networks);
    CHECK(setup.networks().empty()); // still looking
    fake::hal.networks = {{"Home", -45, false}, {"Kaimiņi", -80, false}};
    fake::hal.scanned = true;
    CHECK(setup.follow(60100));
    CHECK(setup.networks().size() == 2);
    WifiSetup asked;
    asked.open(false, 0); // "+" held: straight to the networks
    CHECK(asked.screen() == Screen::Networks);
}

TEST_CASE("the networks scroll a row at a time, as many as there are; a tap on one asks its password, or joins an open one") {
    std::vector<Network> many;
    for (int i = 0; i < 8; i++) many.push_back({"Tīkls " + std::to_string(i), -50 - i, i == 6});
    WifiSetup setup = scanned(many);
    setup.scroll(1, 100);
    CHECK(setup.top() == 1);
    setup.scroll(10, 200);
    CHECK(setup.top() == 8 - WifiSetup::kShown);
    setup.scroll(-10, 300);
    CHECK(setup.top() == 0);
    setup.pick(2, 400);
    CHECK(setup.screen() == Screen::Password);
    CHECK(setup.ssid() == "Tīkls 2");
    setup.back();
    CHECK(setup.screen() == Screen::Networks);
    setup.scroll(3, 500);
    setup.pick(3, 600); // Tīkls 6, open
    CHECK(setup.screen() == Screen::Joining);
    CHECK(fake::hal.joined == std::vector<std::pair<std::string, std::string>>{{"Tīkls 6", ""}});
    setup.pick(7, 700); // off the list: nothing
}

TEST_CASE("the password typed on the keyboard: shift for one capital, symbols, back a letter; join with it") {
    WifiSetup setup = scanned({{"Home", -45, false}});
    setup.pick(0, 100);
    setup.press(0, 3, 400); // shift
    CHECK(setup.page() == keyboard::Page::Capitals);
    setup.press(0, 1, 700); // Q
    CHECK(setup.password() == "Q");
    CHECK(setup.page() == keyboard::Page::Small); // one capital
    setup.press(1, 1, 1000);                      // w
    setup.press(8, 3, 1300);                      // the symbols' page
    setup.press(1, 0, 1600);                      // @
    setup.press(0, 3, 1900);                      // back to letters
    setup.press(0, 0, 2200);                      // 1
    CHECK(setup.password() == "Qw@1");
    setup.press(9, 2, 2500); // back a letter
    CHECK(setup.password() == "Qw@");
    setup.press(9, 3, 2800); // join
    CHECK(setup.screen() == Screen::Joining);
    CHECK(fake::hal.joined.back() == std::make_pair(std::string("Home"), std::string("Qw@")));
}

TEST_CASE("joining: joined, a tick, then done; failed or too long, a cross and back to the password, as typed") {
    WifiSetup setup = scanned({{"Home", -45, false}});
    setup.pick(0, 100);
    setup.press(1, 1, 400);
    setup.press(9, 3, 700);
    fake::hal.wifi = Wifi::Joining;
    CHECK_FALSE(setup.follow(1000));
    fake::hal.wifi = Wifi::Refused;
    CHECK(setup.follow(1100));
    CHECK(setup.screen() == Screen::Failed);
    CHECK(setup.why() == Wifi::Refused); // its password
    setup.follow(1100 + WifiSetup::kDoneMs);
    CHECK(setup.screen() == Screen::Password);
    CHECK(setup.password() == "w");
    setup.press(9, 3, 4000);
    fake::hal.wifi = Wifi::Joining;
    setup.follow(4000 + WifiSetup::kJoinMostMs);
    CHECK(setup.screen() == Screen::Failed); // too long
    CHECK(setup.why() == Wifi::Joining);
    setup.follow(4000 + WifiSetup::kJoinMostMs + WifiSetup::kDoneMs);
    setup.press(9, 3, 30000);
    fake::hal.wifi = Wifi::Joined;
    setup.follow(30100);
    CHECK(setup.screen() == Screen::Joined);
    setup.follow(30100 + WifiSetup::kDoneMs);
    CHECK_FALSE(setup.is_open());
}

TEST_CASE("left alone a few minutes it closes; the power button steps back, from the password to the networks, then out") {
    WifiSetup setup = scanned({{"Home", -45, false}});
    setup.follow(WifiSetup::kIdleMs + 5);
    CHECK_FALSE(setup.is_open());
    WifiSetup other = scanned({{"Home", -45, false}});
    other.pick(0, 100);
    other.back();
    CHECK(other.screen() == Screen::Networks);
    other.back();
    CHECK_FALSE(other.is_open());
}

// On the board (2026-10-12): its touch reported one tap as two, and a letter came twice.
TEST_CASE("a tap again within kBounceMs is the touch's bounce: not a second letter, nor a key under a network just picked") {
    WifiSetup setup = scanned({{"Home", -45, false}});
    setup.pick(0, 1000);
    setup.press(1, 1, 1000 + WifiSetup::kBounceMs - 1); // the pick's bounce, on the keyboard now
    CHECK(setup.password().empty());
    setup.press(1, 1, 2000);
    setup.press(1, 1, 2000 + WifiSetup::kBounceMs - 1);
    CHECK(setup.password() == "w");
    setup.press(1, 1, 2000 + WifiSetup::kBounceMs);
    CHECK(setup.password() == "ww");
}

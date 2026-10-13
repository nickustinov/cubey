#include <doctest/doctest.h>

#include "services/settings_menu.h"

using namespace cubey;

using Event = SettingsMenu::Event;

static SettingsMenu known(bool fresh = false) {
    SettingsMenu menu({"amber", "green", "white"});
    menu.known({"en", "lv", "de", "ru"}, {"lv", "amber", 100}, fresh);
    return menu;
}

// A key held alone from `at` for `ms`, then let go; what came of it.
static Event hold(SettingsMenu &menu, bool plus, uint32_t at, uint32_t ms) {
    Event seen = Event::None;
    for (uint32_t t = at; t <= at + ms; t += 30)
        if (const Event e = menu.keys({plus, !plus, t == at && plus, t == at && !plus}, t); e != Event::None) seen = e;
    menu.keys({}, at + ms + 30);
    return seen;
}

TEST_CASE("the grown-ups' menu: – held alone 5 s opens it, once a hold; not before the server has said its languages") {
    SettingsMenu none;
    CHECK_FALSE(none.ready());
    CHECK(hold(none, false, 0, 6000) == Event::None);
    SettingsMenu menu = known();
    CHECK(menu.ready());
    CHECK(hold(menu, false, 0, SettingsMenu::kHoldMs - 100) == Event::None);
    CHECK(hold(menu, false, 10000, 9000) == Event::Opened); // held long: opened, not closed again
    CHECK(menu.is_open());
}

TEST_CASE("asked for (a grown-up asked Cubey), it opens; not before the server has said its languages, nor twice") {
    SettingsMenu none;
    CHECK(none.open(0) == Event::None);
    SettingsMenu menu = known();
    CHECK(menu.open(0) == Event::Opened);
    CHECK(menu.open(100) == Event::None);
    CHECK(menu.is_open());
}

TEST_CASE("an arrow tapped steps the language it speaks, round; the keys alone don't (2026-10-12)") {
    SettingsMenu menu = known();
    hold(menu, false, 0, 5100);
    menu.step(0, 1, 6000);
    CHECK(menu.language() == "de");
    menu.step(0, 1, 6100);
    menu.step(0, 1, 6200);
    CHECK(menu.language() == "en"); // round
    menu.keys({true, false, 1, 0}, 7000);
    CHECK(menu.language() == "en"); // a press of "+" isn't a step any more
}

TEST_CASE("the second line, its theme: an arrow steps it round its themes; changed, it is sent") {
    SettingsMenu menu = known();
    CHECK(menu.theme() == "amber");
    hold(menu, false, 0, 5100);
    menu.step(1, -1, 6000);
    CHECK(menu.theme() == "white"); // round
    menu.step(1, 1, 6100);
    menu.step(1, 1, 6200);
    CHECK(menu.theme() == "green");
    menu.confirm();
    CHECK(menu.to_send());
    menu.known({"en", "lv", "de", "ru"}, {"lv", "green", 100}, false); // the server, as it was set
    CHECK_FALSE(menu.to_send());
}

TEST_CASE("its brightness, slid: 10 to 100%, in tens; changed, it is sent") {
    SettingsMenu menu = known();
    CHECK(menu.brightness() == 100);
    hold(menu, false, 0, 5100);
    menu.brighten(40, 6000);
    CHECK(menu.brightness() == 40);
    menu.brighten(0, 6100);
    CHECK(menu.brightness() == 10); // as dim as it goes
    menu.brighten(57, 6200);
    CHECK(menu.brightness() == 60);
    menu.confirm();
    CHECK(menu.to_send());
    menu.known({"en", "lv", "de", "ru"}, {"lv", "amber", 60}, false);
    CHECK_FALSE(menu.to_send());
}

TEST_CASE("its volume, slid: 10 to 100%, in tens; changed, it is sent") {
    SettingsMenu menu = known();
    CHECK(menu.volume() == 100);
    hold(menu, false, 0, 5100);
    menu.set_volume(30, 6000);
    CHECK(menu.volume() == 30);
    menu.set_volume(0, 6100);
    CHECK(menu.volume() == 10); // as quiet as it goes: heard all the same
    menu.confirm();
    CHECK(menu.to_send());
    menu.known({"en", "lv", "de", "ru"}, {"lv", "amber", 100, 10}, false);
    CHECK_FALSE(menu.to_send());
}

TEST_CASE("two pages: its language and theme, then its brightness and volume; it opens on the first") {
    SettingsMenu menu = known();
    hold(menu, false, 0, 5100);
    CHECK(menu.page() == 0);
    menu.to_page(1, 6000);
    CHECK(menu.page() == 1);
    menu.to_page(5, 6100); // none such
    CHECK(menu.page() == 1);
    menu.confirm();
    hold(menu, false, 10000, 5100);
    CHECK(menu.page() == 0); // anew
}

TEST_CASE("OK closes it; so do – held again, the power button and a minute alone; changed, or a new Cubey's, it is sent") {
    SettingsMenu menu = known();
    hold(menu, false, 0, 5100);
    CHECK(menu.confirm() == Event::Closed);
    CHECK_FALSE(menu.is_open());
    CHECK_FALSE(menu.to_send()); // nothing changed
    hold(menu, false, 10000, 5100);
    menu.step(0, 1, 16000);
    CHECK(hold(menu, false, 17000, 5100) == Event::Closed);
    CHECK(menu.to_send());
    menu.known({"en", "lv", "de", "ru"}, {"de", "amber", 100}, false); // the server, as it was set
    CHECK_FALSE(menu.to_send());
    hold(menu, false, 30000, 5100);
    CHECK(menu.keys({}, 35100 + SettingsMenu::kIdleMs - 100) == Event::None);
    CHECK(menu.keys({}, 35100 + SettingsMenu::kIdleMs + 100) == Event::Closed);
    hold(menu, false, 200000, 5100);
    CHECK(menu.close() == Event::Closed);
    CHECK(menu.close() == Event::None);
}

TEST_CASE("a new Cubey's menu opens by itself, once, when the server first says its languages; OK sends them as they are") {
    SettingsMenu menu = known(true);
    CHECK(menu.offer(100) == Event::Opened);
    CHECK(menu.offer(200) == Event::None);
    CHECK(menu.confirm() == Event::Closed);
    CHECK(menu.to_send()); // the server hears it is set up
    CHECK(known().offer(100) == Event::None); // one set up before
}

TEST_CASE("only a new Cubey's menu, opened by itself, is its language alone; opened by hand or asked for, the whole of it") {
    SettingsMenu menu;
    menu.known({"en", "lv"}, {"lv", "amber", 100}, true);
    REQUIRE(menu.offer(0) == Event::Opened);
    CHECK(menu.first());
    menu.confirm();
    REQUIRE(menu.open(100) == Event::Opened);
    CHECK_FALSE(menu.first());
}

TEST_CASE("+ held alone 5 s, the menu closed: the Wi-Fi setup; on the menu, nothing") {
    SettingsMenu menu = known();
    CHECK(hold(menu, true, 0, SettingsMenu::kHoldMs + 30) == Event::Wifi);
    hold(menu, false, 10000, 5100);
    CHECK(hold(menu, true, 20000, 6000) == Event::None);
    CHECK(menu.is_open());
}

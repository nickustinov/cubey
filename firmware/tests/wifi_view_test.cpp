#include <doctest/doctest.h>

#include "fake_hal.h"
#include "ui/wifi.h"

using namespace cubey;
using dots::Dot;
using wifi_view::kBoard;
using wifi_view::kRow;

static int count(const wifi_view::Frame &f, Dot as, int row1 = 0, int row2 = kBoard, int col1 = 0, int col2 = kBoard) {
    int n = 0;
    for (int row = row1; row < row2; row++)
        for (int col = col1; col < col2; col++) n += f[row * kBoard + col] == as;
    return n;
}

static WifiSetup networks() {
    fake::reset();
    WifiSetup setup;
    setup.open(false, 0);
    fake::hal.networks = {{"Home", -45, false}, {"Kaimini", -65, false}, {"Kafejnica ar garu vardu", -85, true}};
    fake::hal.scanned = true;
    setup.follow(10);
    return setup;
}

TEST_CASE("the networks, a row each in the dot font: lit as strong, half or dim as their signal; a long name cut in its middle with …") {
    const wifi_view::Frame f = wifi_view::paint(networks(), 100);
    CHECK(count(f, Dot::On, 0, kRow) > 0);
    CHECK(count(f, Dot::Half, 0, kRow) == 0);
    CHECK(count(f, Dot::Half, kRow, 2 * kRow) > 0);
    CHECK(count(f, Dot::Dim, 2 * kRow, 3 * kRow) > 0);
    CHECK(count(f, Dot::Dim, 2 * kRow + 8, 2 * kRow + 9, 20, 40) > 0); // its …, low in the middle of the row
    CHECK(count(f, Dot::On, 3 * kRow) == 0);                        // no more
    // Clear of the screen's rounded corners: in from either side (2026-10-12)
    CHECK(count(f, Dot::Off, 0, 3 * kRow, 0, wifi_view::kPad) == 3 * kRow * wifi_view::kPad);
    CHECK(count(f, Dot::Off, 0, 3 * kRow, kBoard - wifi_view::kPad) == 3 * kRow * wifi_view::kPad);
}

TEST_CASE("the password on top, the keyboard under it: dim caps with lit glyphs; shift shows lit, its glyph dark") {
    WifiSetup setup = networks();
    setup.pick(0, 100);
    setup.press(1, 1, 400); // w
    wifi_view::Frame f = wifi_view::paint(setup, 500);
    CHECK(count(f, Dot::On, 0, kRow) > 0);                                 // "w" and the cursor
    CHECK(count(f, Dot::Dim, kRow + kRow, kRow + 2 * kRow, 0, 5) > 0);     // q's cap
    CHECK(count(f, Dot::On, kRow + kRow, kRow + 2 * kRow, 0, 5) > 0);      // q
    const int shift_dim = count(f, Dot::Dim, kRow + 3 * kRow, kBoard, 0, 5);
    setup.press(0, 3, 700);
    f = wifi_view::paint(setup, 800);
    CHECK(count(f, Dot::On, kRow + 3 * kRow, kBoard, 0, 5) > count(f, Dot::Off, kRow + 3 * kRow, kBoard, 0, 5)); // lit
    CHECK(count(f, Dot::Dim, kRow + 3 * kRow, kBoard, 0, 5) < shift_dim);
}

TEST_CASE("a big hi to welcome; joining, the Wi-Fi sign lighting arc by arc over the network's name; a tick, or a cross and why") {
    fake::reset();
    WifiSetup setup;
    setup.open(true, 0);
    const wifi_view::Frame hi = wifi_view::paint(setup, 100);
    CHECK(count(hi, Dot::On, 0, 24) > 100);  // HI!
    CHECK(count(hi, Dot::Half, 24) > 50);    // "Tap to connect Wi-Fi", small, under it
    setup = networks();
    setup.pick(2, 100); // open: joins
    fake::hal.wifi = Wifi::Joining;
    const wifi_view::Frame early = wifi_view::paint(setup, 0), later = wifi_view::paint(setup, 3 * wifi_view::kArcMs);
    CHECK(count(later, Dot::On, 0, 40) > count(early, Dot::On, 0, 40)); // more arcs lit
    CHECK(count(early, Dot::Dim, 0, 40) > 0);                            // the rest dim
    CHECK(count(early, Dot::Half, 44) > 0);                              // its name, under it, still
    CHECK(wifi_view::paint(setup, 0) == wifi_view::paint(setup, wifi_view::kArcMs / 2)); // no scrolling
    fake::hal.wifi = Wifi::Joined;
    setup.follow(2100);
    CHECK(count(wifi_view::paint(setup, 2200), Dot::On) > 50);
    WifiSetup wrong = networks();
    wrong.pick(2, 100);
    fake::hal.wifi = Wifi::NotFound;
    wrong.follow(200);
    const wifi_view::Frame failed = wifi_view::paint(wrong, 300);
    CHECK(count(failed, Dot::On, 0, 34) > 30); // the cross
    CHECK(count(failed, Dot::Half, 36) > 0);   // why: "not found"
}

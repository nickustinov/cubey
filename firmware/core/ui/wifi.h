#pragma once

#include <cstdint>

#include "lvgl.h"
#include "services/wifi_setup.h"
#include "ui/fine.h"

// The Wi-Fi setup (services/wifi_setup.h) on the fine dot board (ui/fine.h), over everything. The networks, five on
// screen, a row each (12 dots high, a tap's height), in from either side, clear of the screen's rounded corners: their
// names lit, half lit or dim as their signal is strong or weak, cut in the middle with "…" when long; dragged, they
// scroll. The password typed on top, and under it the keyboard, ten keys by four: dim caps with lit glyphs, shift lit
// while it is on. A big "HI!" to welcome; joining, the Wi-Fi sign lighting arc by arc over the network's name; a tick,
// or a cross and why. The welcome: "HI!" and, small, "Tap to connect Wi-Fi".
namespace cubey::wifi_view {

constexpr int kBoard = fine::kBoard, kRow = 12, kKey = kBoard / keyboard::kCols; // a row: the font's 11-row line, one between
constexpr int kPad = 3;           // the networks in from either side
constexpr uint32_t kArcMs = 400;  // joining: an arc more lit each
using Frame = fine::Frame;

Frame paint(const WifiSetup &setup, uint32_t now);
lv_obj_t *create(lv_obj_t *parent, WifiSetup *setup, uint32_t now); // taps and drags go to the setup
void show(lv_obj_t *view, const WifiSetup &setup, uint32_t now);    // only what changed drawn again

} // namespace cubey::wifi_view

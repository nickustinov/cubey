#pragma once

#include <cstdint>
#include <string>

#include "lvgl.h"
#include "services/timer.h"
#include "ui/dots.h"

// A timer's dial on the dot board (ui/dots.h), in place of Cubey's eyes while it runs: a little picture of what it is
// for at the top, the minutes left in the middle in the dot font (ui/font.h; in the last minute, the seconds), and a
// square bar of dots round the board going out over the whole timer, clockwise from the top's middle, its last dot
// blinking each second: a clock ticking.
namespace cubey::dial {

enum class For : uint8_t { Cartoons, Tidying, Bath, Food, Sleep, Reading, Play, Other, Count };
For for_named(const std::string &name); // the server's name for it; anything else: Other

struct View {
    uint32_t left_s, total_s; // seconds left, of so many
    For kind;
};

std::string shown(uint32_t left_s); // the digits: the minutes left, rounded up; in the last minute, the seconds

constexpr int kBarEdge = 1;                         // the bar's dots in from the board's edge
constexpr int kBarSide = dots::kBoard - 1 - 2 * kBarEdge; // a side's dots, its corner counted once
constexpr int kBar = 4 * kBarSide;                  // all of the bar's dots
int bar_lit(uint32_t left_s, uint32_t total_s); // how many of the bar's dots are still lit, the last ones
struct BarDot {
    int col, row;
};
BarDot bar_dot(int dot); // the bar's dot (0..kBar-1, clockwise from the top's middle)

dots::Dot dot(const View &view, int col, int row);

// The dial over all of `parent`, following `timer` (which lives longer).
lv_obj_t *create(lv_obj_t *parent, const TimerRun *timer, For kind);

} // namespace cubey::dial

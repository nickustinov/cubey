#pragma once

#include "lvgl.h"
#include "services/battery.h"

// Cubey's battery on the dot board (ui/dots.h), shown as it says how much is left: an outline of dots with a nub on
// its right, the charge inside it a dot clear all round, filling column by column.
namespace cubey::battery {

constexpr int kFillMost = 13; // the columns of charge inside it

// How many columns of charge show `percent`: none when empty, at least one for any charge, all of them full.
int fill(int percent);
// Whether the board's dot at (col, row) is lit: the outline, the nub, and `filled` columns of charge.
bool lit(int filled, int col, int row);

// The battery over all of `parent`: its charge sweeps in a column a frame up to `battery`'s; charging, the column
// after it blinks.
lv_obj_t *create(lv_obj_t *parent, Battery battery);

} // namespace cubey::battery

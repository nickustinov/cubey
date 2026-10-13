#pragma once

#include "lvgl.h"
#include "services/dice.h"
#include "ui/dots.h"

// The die on the dot board (ui/dots.h), in place of Cubey's eyes while she plays with it: a square of dim dots, its
// corners round, its pips lit, four dots across, as a die's; tumbling a dot off its place as it rolls (services/dice.h).
namespace cubey::die {

dots::Dot dot(const DiceRoll::Frame &frame, int col, int row);

// The die over all of `parent`, showing nothing until show(); a tap on it calls on_tap (to roll it again).
lv_obj_t *create(lv_obj_t *parent, void (*on_tap)());
void show(lv_obj_t *die, const DiceRoll::Frame &frame); // redrawn only when it looks different

} // namespace cubey::die

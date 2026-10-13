#pragma once

#include <array>
#include <cstdint>

#include "lvgl.h"
#include "ui/dots.h"

// Cubey's ear on the dot board (ui/dots.h), in place of its eyes while it listens to her holding its screen (held to
// talk: services/voice.h): its rim lit, its inner curl half, swaying gently, a dot aside and back, a while at each.
namespace cubey::ear {

constexpr uint32_t kSwayMs = 700; // at each place of its sway: the middle, a dot right, the middle, a dot left

int sway(uint32_t ms); // where it is `ms` after it showed, in dots from the middle (right is +)

using Frame = std::array<dots::Dot, dots::kBoard * dots::kBoard>;
Frame frame(int dx); // the ear, dx dots from the board's middle

lv_obj_t *create(lv_obj_t *parent, uint32_t now);
void show(lv_obj_t *ear, uint32_t now); // redrawn only as it sways

} // namespace cubey::ear

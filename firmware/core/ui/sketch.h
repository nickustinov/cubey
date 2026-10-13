#pragma once

#include <array>
#include <cstdint>

#include "lvgl.h"
#include "ui/dots.h"

// A sketch forming on the dot board (ui/dots.h) in place of Cubey's eyes while a drawing is on its way (it takes the
// drawing model 5 to 14 s): faint dots come one after another at places here and there in the board's middle, each
// rising and fading, as many coming as going, until the picture replaces them. Each sketch has its own places (seed).
namespace cubey::sketch {

constexpr uint32_t kEveryMs = 90;  // a new dot
constexpr uint32_t kLifeMs = 1200; // a dot, from coming to gone
constexpr int kRadius = 8;         // within this many dots of the board's middle

struct Spot {
    int col, row;
};
Spot spot(uint32_t seed, uint32_t k); // where its k-th dot comes
dots::Dot shade(uint32_t age);        // a dot `age` ms after it came: dim, half, dim, then off

using Frame = std::array<dots::Dot, dots::kBoard * dots::kBoard>;
Frame frame(uint32_t seed, uint32_t ms); // the sketch `ms` after it began

lv_obj_t *create(lv_obj_t *parent, uint32_t seed, uint32_t now);
void show(lv_obj_t *sketch, uint32_t now); // redrawn only when it looks different

} // namespace cubey::sketch

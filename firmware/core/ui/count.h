#pragma once

#include <cstdint>
#include <functional>

#include "lvgl.h"
#include "ui/dots.h"

// How many dots: big round dots on the dot board (ui/dots.h), in place of Cubey's eyes, for her to say how many; four
// dots across, at random places of a grid of four by four. A new number: the one before shows first, its digits
// (ui/font.h) big in the middle, each of its dots two by two, while Cubey says it; then the new dots come, dim, then lit.
namespace cubey::count {

constexpr int kMost = 10;
constexpr uint32_t kNumberMs = 1800; // the number before, big
constexpr uint32_t kDimMs = 250;     // the dots coming

struct Shown {
    int n;           // how many dots
    uint16_t places; // where: a bit for each of the grid's 16, row by row from the top left
    int was;         // the number before, to show first; 0 none
};

// n dots (1..kMost) at random places; rand(below) picks 0..below-1.
Shown place(int n, int was, const std::function<uint32_t(uint32_t)> &rand);

enum class Phase { Number, Coming, Lit };
Phase phase(const Shown &shown, uint32_t ms); // ms after it began
dots::Dot dot(const Shown &shown, Phase phase, int col, int row);

lv_obj_t *create(lv_obj_t *parent, Shown shown, uint32_t now);
int number(lv_obj_t *count); // how many dots it shows
void show(lv_obj_t *count, uint32_t now); // redrawn only when it looks different

} // namespace cubey::count

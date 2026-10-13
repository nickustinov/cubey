#pragma once

#include <string>
#include <vector>

#include "lvgl.h"
#include "ui/dots.h"

// A picture Cubey drew, on the dot board (ui/dots.h) in place of its eyes for a while: its rows as the server sends
// them (24 of 24, server/src/cubey.ts), '#' lit, '*' half, '+' dim, anything else off.
namespace cubey::picture {

dots::Dot dot(const std::vector<std::string> &rows, int col, int row);
lv_obj_t *create(lv_obj_t *parent, std::vector<std::string> rows);

} // namespace cubey::picture

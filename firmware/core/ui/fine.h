#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "lvgl.h"
#include "ui/dots.h"
#include "ui/font.h"

// A finer dot board, 60 by 60, amber dots as Cubey's own, for the grown-ups' screens (the language menu, the Wi-Fi
// setup) over everything: text in the dot font a dot to a dot, or bigger. A view paints its whole frame; shown, only
// the dots that changed are drawn again.
namespace cubey::fine {

constexpr int kBoard = 60;
using Frame = std::array<dots::Dot, kBoard * kBoard>;

// Text in the dot font: each letter's glyph (the font's own; what it hasn't left out), and how wide it is drawn.
struct Text {
    std::vector<const font::Letter *> letters;
    int wide(int scale) const;
};
Text text(const std::string &utf8);
Text small(const std::string &utf8); // in small capitals (font::tiny)

void set(Frame &f, int col, int row, dots::Dot as);
// Writes text on its line (11 rows a scale) with the line's top left at (x, y); `as` for its dots.
void write(Frame &f, const Text &t, int x, int y, int scale, dots::Dot as);
// Across the middle, its line's top at y.
void centred(Frame &f, const std::string &utf8, int y, int scale, dots::Dot as);
void centred(Frame &f, const Text &t, int y, int scale, dots::Dot as);

// The board over all of `parent`, showing `frame`; a touch's cell, as the picture has it (the screen however turned).
lv_obj_t *create(lv_obj_t *parent, const Frame &frame);
void show(lv_obj_t *board, const Frame &frame); // only the dots that changed drawn again
dots::Cell cell_at(lv_obj_t *board, lv_point_t point);

} // namespace cubey::fine

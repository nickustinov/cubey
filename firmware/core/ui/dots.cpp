#include "ui/dots.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

// LVGL's image cache API (lv_image_cache_drop), which 9.6 leaves out of include/ (as ui/cards.cpp)
#include "src/misc/cache/instance/lv_image_cache.h"

namespace cubey::dots {

struct Theme {
    const char *name;
    uint32_t lit, ground;
};
constexpr Theme kThemes[] = {
    {"amber", 0xFFB020, 0x000000}, {"green", 0x33FF66, 0x000000}, {"red", 0xFF3B1F, 0x000000},
    {"blue", 0x3D9BFF, 0x000000},  {"vfd", 0x5BF0D8, 0x000000},   {"white", 0x1C1C1C, 0xE6E1D3},
};
constexpr lv_opa_t kOpa[] = {22, 90, 170, 255}; // Off, faintly there; Dim; Half; On
constexpr float kDot = 0.8f; // of the space from small dot to small dot: square dots, the rest the ground

// Each Dot's colour: the lit one at its opacity over the ground, a colour of its own (a solid fill draws faster than a
// blend).
constexpr std::array<uint32_t, 4> colours_of(const Theme &t) {
    std::array<uint32_t, 4> out{};
    for (int d = 0; d < 4; d++)
        for (int shift = 0; shift <= 16; shift += 8) {
            const int g = static_cast<int>(t.ground >> shift & 0xFF), l = static_cast<int>(t.lit >> shift & 0xFF);
            out[d] |= static_cast<uint32_t>(g + (l - g) * kOpa[d] / 255) << shift;
        }
    return out;
}

static const Theme *theme_ = &kThemes[0]; // amber, until the server says
static std::array<uint32_t, 4> colours_ = colours_of(kThemes[0]);

// A dot of each kind as a picture, a dot's pixels across and down: its small square dots on the ground. A run of dots
// alike is drawn as its picture tiled: one draw task, copied as memory (a square a dot, and the gaps over them as
// lines, took a frame up to a third longer on the board).
struct Tile {
    Dot dot;
    int pitch, split;
    std::vector<uint16_t> px;
    lv_image_dsc_t img{};
};
static std::vector<std::unique_ptr<Tile>> tiles_; // made as first drawn, in the theme's colours

static const lv_image_dsc_t *tile(Dot d, int pitch, int split) {
    for (const auto &t : tiles_)
        if (t->dot == d && t->pitch == pitch && t->split == split) return &t->img;
    auto t = std::make_unique<Tile>(Tile{d, pitch, split, std::vector<uint16_t>(pitch * pitch), {}});
    const int small = pitch / split, gap = static_cast<int>(std::lround(small * (1 - kDot)));
    const uint16_t lit = lv_color_to_u16(lv_color_hex(colours_[static_cast<int>(d)])), dark = lv_color_to_u16(lv_color_hex(theme_->ground));
    const auto inside = [&](int at) { return at % small >= gap / 2 && at % small < small - (gap - gap / 2); };
    for (int y = 0; y < pitch; y++)
        for (int x = 0; x < pitch; x++) t->px[y * pitch + x] = inside(x) && inside(y) ? lit : dark;
    t->img.header.magic = LV_IMAGE_HEADER_MAGIC;
    t->img.header.cf = LV_COLOR_FORMAT_RGB565;
    t->img.header.w = t->img.header.h = static_cast<uint32_t>(pitch);
    t->img.header.stride = static_cast<uint32_t>(pitch * 2);
    t->img.data = reinterpret_cast<const uint8_t *>(t->px.data());
    t->img.data_size = static_cast<uint32_t>(pitch * pitch * 2);
    tiles_.push_back(std::move(t));
    return &tiles_.back()->img;
}

bool set_theme(const std::string &name) {
    const Theme *to = &kThemes[0];
    for (const Theme &t : kThemes)
        if (name == t.name) to = &t;
    if (to == theme_) return false;
    theme_ = to, colours_ = colours_of(*to);
    for (const auto &t : tiles_) lv_image_cache_drop(&t->img);
    tiles_.clear();
    return true;
}

std::vector<std::string> themes() {
    std::vector<std::string> names;
    for (const Theme &t : kThemes) names.emplace_back(t.name);
    return names;
}

uint32_t colour(Dot d) { return colours_[static_cast<int>(d)]; }
uint32_t ground() { return theme_->ground; }

static int turn_ = 0;

void set_turn(int quarters) { turn_ = quarters & 3; }
int turn() { return turn_; }

Cell turned(int quarters, int col, int row, int board) {
    for (int k = 0; k < (quarters & 3); k++) col = board - 1 - col, std::swap(col, row); // a quarter back
    return {col, row};
}

void invalidate(lv_obj_t *obj, int col1, int row1, int col2, int row2, int board) {
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    const float pitch = static_cast<float>(lv_area_get_width(&box)) / board;
    const Cell a = turned(4 - turn_, col1, row1, board), b = turned(4 - turn_, col2, row2, board); // where on the screen
    lv_area_t area{box.x1 + static_cast<int32_t>(std::min(a.col, b.col) * pitch), box.y1 + static_cast<int32_t>(std::min(a.row, b.row) * pitch),
                   box.x1 + static_cast<int32_t>((std::max(a.col, b.col) + 1) * pitch), box.y1 + static_cast<int32_t>((std::max(a.row, b.row) + 1) * pitch)};
    lv_obj_invalidate_area(obj, &area);
}

void draw(lv_event_t *e, const std::function<Dot(int col, int row)> &dot, int board, int split) {
    auto *obj = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    const int pitch = lv_area_get_width(&box) / board; // ponytail: 480 across, a dot 20 or 8 whole pixels, for its picture to tile
    lv_layer_t *layer = lv_event_get_layer(e);
    // Only the dots in the strip being drawn (a frame is drawn in strips): each asks its view how it shows.
    const lv_area_t &strip = layer->_clip_area;
    const auto first = [&](int32_t from) { return std::max(0, static_cast<int>((from - 1) / pitch)); };
    const auto last = [&](int32_t to) { return std::min(board - 1, static_cast<int>((to + 1) / pitch)); };
    const int col1 = first(strip.x1 - box.x1), col2 = last(strip.x2 - box.x1), row1 = first(strip.y1 - box.y1), row2 = last(strip.y2 - box.y1);
    const auto at = [&](int col, int row) {
        const Cell c = turned(turn_, col, row, board);
        return dot(c.col, c.row);
    };
    // Each row as runs of dots alike (mostly dark: a few runs a row), each its dot's picture tiled.
    for (int row = row1; row <= row2; row++) {
        int from = col1;
        Dot run = at(col1, row);
        for (int col = col1 + 1; col <= col2 + 1; col++) {
            const Dot d = col <= col2 ? at(col, row) : Dot::Off;
            if (col <= col2 && d == run) continue;
            lv_draw_image_dsc_t image;
            lv_draw_image_dsc_init(&image);
            image.src = tile(run, pitch, split);
            image.tile = 1;
            const lv_area_t area{box.x1 + from * pitch, box.y1 + row * pitch, box.x1 + col * pitch - 1, box.y1 + (row + 1) * pitch - 1};
            image.image_area = {area.x1, area.y1, area.x1 + pitch - 1, area.y2};
            lv_draw_image(layer, &image, &area);
            from = col, run = d;
        }
    }
}

} // namespace cubey::dots

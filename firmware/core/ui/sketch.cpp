#include "ui/sketch.h"

#include <algorithm>
#include <cmath>

namespace cubey::sketch {

namespace {

constexpr double kMiddle = (dots::kBoard - 1) / 2.0;
const int kFrom = static_cast<int>(std::ceil(kMiddle - kRadius - 0.5)); // the middle's first column and row
const int kSpan = dots::kBoard - 2 * kFrom;

// A number that looks random, the same for the same one given.
uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

bool inside(int col, int row) {
    const double dc = col - kMiddle, dr = row - kMiddle;
    return dc * dc + dr * dr <= (kRadius + 0.5) * (kRadius + 0.5);
}

struct Board {
    uint32_t seed, since;
    Frame frame;
};

Board *board_of(lv_obj_t *obj) { return static_cast<Board *>(lv_obj_get_user_data(obj)); }
Board *board_of(lv_event_t *e) { return board_of(static_cast<lv_obj_t *>(lv_event_get_current_target(e))); }

} // namespace

Spot spot(uint32_t seed, uint32_t k) {
    for (uint32_t h = mix(seed ^ mix(k + 1));; h = mix(h)) { // a place in the square around the middle, until one in it
        const Spot s{kFrom + static_cast<int>(h % static_cast<uint32_t>(kSpan)), kFrom + static_cast<int>((h >> 16) % static_cast<uint32_t>(kSpan))};
        if (inside(s.col, s.row)) return s;
    }
}

dots::Dot shade(uint32_t age) {
    if (age < 300) return dots::Dot::Dim;
    if (age < 800) return dots::Dot::Half;
    if (age < kLifeMs) return dots::Dot::Dim;
    return dots::Dot::Off;
}

Frame frame(uint32_t seed, uint32_t ms) {
    Frame f{};
    const uint32_t newest = ms / kEveryMs, oldest = ms >= kLifeMs ? (ms - kLifeMs) / kEveryMs + 1 : 0;
    for (uint32_t k = oldest; k <= newest; k++) {
        const Spot s = spot(seed, k);
        dots::Dot &d = f[static_cast<size_t>(s.row * dots::kBoard + s.col)];
        d = std::max(d, shade(ms - k * kEveryMs));
    }
    return f;
}

lv_obj_t *create(lv_obj_t *parent, uint32_t seed, uint32_t now) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    lv_obj_set_user_data(obj, new Board{seed, now, frame(seed, 0)});
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const Board *board = board_of(e);
            dots::draw(e, [&](int col, int row) { return board->frame[static_cast<size_t>(row * dots::kBoard + col)]; });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, [](lv_event_t *e) { delete board_of(e); }, LV_EVENT_DELETE, nullptr);
    return obj;
}

void show(lv_obj_t *sketch, uint32_t now) {
    Board *board = board_of(sketch);
    const Frame next = frame(board->seed, now - board->since);
    if (next == board->frame) return;
    board->frame = next;
    lv_obj_invalidate(sketch);
}

} // namespace cubey::sketch

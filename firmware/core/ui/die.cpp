#include "ui/die.h"

namespace cubey::die {

namespace {

constexpr int kNear = 2, kFar = dots::kBoard - 1 - kNear; // the square's sides, a dot in from the bar's place
constexpr int kPip = 4;                                   // a pip's dots across and down
constexpr int kPips[] = {kNear + 2, kNear + 8, kNear + 14}; // where the pips start: left, middle, right (or top...)

// The pips of each face, on a grid of three by three (column, row).
struct At {
    int x, y;
};
const std::vector<At> kFaces[] = {
    {},
    {{1, 1}},
    {{0, 0}, {2, 2}},
    {{0, 0}, {1, 1}, {2, 2}},
    {{0, 0}, {2, 0}, {0, 2}, {2, 2}},
    {{0, 0}, {2, 0}, {1, 1}, {0, 2}, {2, 2}},
    {{0, 0}, {0, 1}, {0, 2}, {2, 0}, {2, 1}, {2, 2}},
};

struct Shown {
    DiceRoll::Frame frame{0, 0, 0, 0}; // face 0: nothing yet
    void (*on_tap)();
};

} // namespace

dots::Dot dot(const DiceRoll::Frame &frame, int col, int row) {
    using dots::Dot;
    if (frame.face < 1 || frame.face > 6) return Dot::Off;
    col -= frame.dx;
    row -= frame.dy;
    const bool across = col == kNear || col == kFar, down = row == kNear || row == kFar;
    if (col < kNear || col > kFar || row < kNear || row > kFar || (across && down)) return Dot::Off; // round corners
    if (across || down) return Dot::Dim;
    for (const At &p : kFaces[frame.face])
        if (col >= kPips[p.x] && col < kPips[p.x] + kPip && row >= kPips[p.y] && row < kPips[p.y] + kPip) return Dot::On;
    return Dot::Off;
}

lv_obj_t *create(lv_obj_t *parent, void (*on_tap)()) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_user_data(obj, new Shown{{0, 0, 0, 0}, on_tap});
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const auto *shown = static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
            dots::draw(e, [&](int col, int row) { return dot(shown->frame, col, row); });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(
        obj, [](lv_event_t *e) { static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))))->on_tap(); },
        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(
        obj, [](lv_event_t *e) { delete static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)))); },
        LV_EVENT_DELETE, nullptr);
    return obj;
}

void show(lv_obj_t *die, const DiceRoll::Frame &frame) {
    auto *shown = static_cast<Shown *>(lv_obj_get_user_data(die));
    const DiceRoll::Frame &was = shown->frame;
    if (was.face == frame.face && was.dx == frame.dx && was.dy == frame.dy) return;
    shown->frame = frame;
    lv_obj_invalidate(die);
}

} // namespace cubey::die

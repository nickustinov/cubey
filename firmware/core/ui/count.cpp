#include "ui/count.h"

#include <algorithm>
#include <string>

#include "ui/font.h"

namespace cubey::count {

namespace {

constexpr int kGrid = 4, kPlaces = kGrid * kGrid;
constexpr int kDot = 4, kPitch = 6;                          // a dot's dots across, and from one place to the next
constexpr int kEdge = (dots::kBoard - (kGrid - 1) * kPitch - kDot) / 2; // the grid in the middle
constexpr int kScale = 2;
constexpr int kTop = (dots::kBoard - font::kDigitRows * kScale) / 2;

bool in_dot(int x, int y) {
    if (x < 0 || y < 0 || x >= kDot || y >= kDot) return false;
    return !((x == 0 || x == kDot - 1) && (y == 0 || y == kDot - 1)); // round: its corners off
}

dots::Dot number_dot(int number, int col, int row) {
    const std::string digits = std::to_string(number);
    int x = col - (dots::kBoard - font::width(digits) * kScale) / 2;
    const int y = (row - kTop) / kScale;
    if (x < 0 || row < kTop || y >= font::kDigitRows) return dots::Dot::Off;
    for (char d : digits) {
        const font::Glyph &g = font::digit(d);
        if (x < g.width * kScale) return g.rows[y][x / kScale] == '#' ? dots::Dot::On : dots::Dot::Off;
        x -= (g.width + 1) * kScale;
        if (x < 0) return dots::Dot::Off; // between digits
    }
    return dots::Dot::Off;
}

struct Board {
    Shown shown;
    uint32_t since;
    Phase phase;
};

Board *board_of(lv_obj_t *obj) { return static_cast<Board *>(lv_obj_get_user_data(obj)); }
Board *board_of(lv_event_t *e) { return board_of(static_cast<lv_obj_t *>(lv_event_get_current_target(e))); }

} // namespace

Shown place(int n, int was, const std::function<uint32_t(uint32_t)> &rand) {
    n = std::clamp(n, 1, kMost);
    int order[kPlaces];
    for (int i = 0; i < kPlaces; i++) order[i] = i;
    uint16_t places = 0;
    for (int i = 0; i < n; i++) { // the first n of a shuffle
        std::swap(order[i], order[i + static_cast<int>(rand(kPlaces - i))]);
        places |= 1u << order[i];
    }
    return {n, places, was};
}

Phase phase(const Shown &shown, uint32_t ms) {
    if (shown.was > 0) {
        if (ms < kNumberMs) return Phase::Number;
        ms -= kNumberMs;
    }
    return ms < kDimMs ? Phase::Coming : Phase::Lit;
}

dots::Dot dot(const Shown &shown, Phase phase, int col, int row) {
    if (phase == Phase::Number) return number_dot(shown.was, col, row);
    for (int i = 0; i < kPlaces; i++)
        if (shown.places & (1u << i) && in_dot(col - kEdge - i % kGrid * kPitch, row - kEdge - i / kGrid * kPitch))
            return phase == Phase::Coming ? dots::Dot::Dim : dots::Dot::On;
    return dots::Dot::Off;
}

lv_obj_t *create(lv_obj_t *parent, Shown shown, uint32_t now) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    lv_obj_set_user_data(obj, new Board{shown, now, phase(shown, 0)});
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const Board *board = board_of(e);
            dots::draw(e, [&](int col, int row) { return dot(board->shown, board->phase, col, row); });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, [](lv_event_t *e) { delete board_of(e); }, LV_EVENT_DELETE, nullptr);
    return obj;
}

int number(lv_obj_t *count) { return board_of(count)->shown.n; }

void show(lv_obj_t *count, uint32_t now) {
    Board *board = board_of(count);
    const Phase next = phase(board->shown, now - board->since);
    if (next == board->phase) return;
    board->phase = next;
    lv_obj_invalidate(count);
}

} // namespace cubey::count

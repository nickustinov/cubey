#include "ui/ear.h"


#include "services/ticks.h"

namespace cubey::ear {

namespace {

// A right ear, from the side: '#' its rim, lit; '+' its inner curl, half.
constexpr int kWide = 12, kTall = 15;
const char *const kEar[kTall] = {
    "...######...",
    "..#......#..",
    ".#........#.",
    "#...++++...#",
    "#..+....+..#",
    "#..+..+..+.#",
    "...+.+...+.#",
    "....+....+.#",
    "........+..#",
    ".......+..#.",
    "......+..#..",
    "..#..+..#...",
    "..#.+..#....",
    "...#..#.....",
    "....##......",
};

struct Board {
    uint32_t since;
    int dx;
    Frame frame;
};

Board *board_of(lv_obj_t *obj) { return static_cast<Board *>(lv_obj_get_user_data(obj)); }
Board *board_of(lv_event_t *e) { return board_of(static_cast<lv_obj_t *>(lv_event_get_current_target(e))); }

} // namespace

int sway(uint32_t ms) {
    static constexpr int kPlaces[4] = {0, 1, 0, -1};
    return kPlaces[(ms / kSwayMs) % 4];
}

Frame frame(int dx) {
    Frame f{};
    const int left = (dots::kBoard - kWide) / 2 + dx, top = (dots::kBoard - kTall) / 2;
    for (int row = 0; row < kTall; row++)
        for (int x = 0; x < kWide; x++) {
            const int col = left + x;
            if (col < 0 || col >= dots::kBoard || kEar[row][x] == '.') continue;
            f[static_cast<size_t>((top + row) * dots::kBoard + col)] = kEar[row][x] == '#' ? dots::Dot::On : dots::Dot::Half;
        }
    return f;
}

lv_obj_t *create(lv_obj_t *parent, uint32_t now) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    lv_obj_set_user_data(obj, new Board{now, 0, frame(0)});
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

void show(lv_obj_t *obj, uint32_t now) {
    Board *board = board_of(obj);
    const int dx = sway(since(now, board->since));
    if (dx == board->dx) return;
    board->dx = dx;
    board->frame = frame(dx);
    lv_obj_invalidate(obj);
}

} // namespace cubey::ear

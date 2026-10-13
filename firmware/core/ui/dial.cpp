#include "ui/dial.h"

#include "ui/font.h"

namespace cubey::dial {

namespace {

constexpr int kIconTop = 5, kDigitsTop = 12;
constexpr int kIconWide = 9, kIconTall = 6;

// What it is for, 9 dots across and 6 down: a TV with its aerial, a box with a ball on it, a bath with bubbles, a bowl
// with a spoon, the moon, an open book, a ball, an hourglass.
const char *const kIcons[][kIconTall] = {
    {"..#...#..", "...#.#...", "#########", "#.......#", "#.......#", "#########"},
    {"...##....", "..####...", "#########", "#.......#", "#.#####.#", "#########"},
    {".....#.#.", "......#..", "#########", "#.......#", ".#.....#.", "..#####.."},
    {".......#.", "......#..", "#########", ".#.....#.", "..#...#..", "...###..."},
    {"...###...", "..##.....", ".##......", ".##......", "..##.....", "...###..."},
    {"###...###", "#..#.#..#", "#...#...#", "#...#...#", "#...#...#", "#########"},
    {"...###...", "..#.#.#..", ".#..#..#.", ".#######.", "..#.#.#..", "...###..."},
    {".#######.", "..#...#..", "...#.#...", "...#.#...", "..#...#..", ".#######."},
};
static_assert(sizeof kIcons / sizeof kIcons[0] == static_cast<size_t>(For::Count), "a picture for each");

struct Shown {
    lv_timer_t *timer = nullptr;
    const TimerRun *run;
    View view;
};

// Once a second it redraws: the bar's last dot blinks, and goes out in its time.
void tick(lv_timer_t *t) {
    auto *obj = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
    auto *shown = static_cast<Shown *>(lv_obj_get_user_data(obj));
    const uint32_t left_s = (shown->run->left_ms(lv_tick_get()) + 999) / 1000;
    if (left_s == shown->view.left_s) return;
    shown->view.left_s = left_s;
    lv_obj_invalidate(obj);
}

} // namespace

For for_named(const std::string &name) {
    static const char *const names[] = {"cartoons", "tidying", "bath", "food", "sleep", "reading", "play"};
    for (int i = 0; i < static_cast<int>(sizeof names / sizeof names[0]); i++)
        if (name == names[i]) return static_cast<For>(i);
    return For::Other;
}

std::string shown(uint32_t left_s) { return std::to_string(left_s > 60 ? (left_s + 59) / 60 : left_s); }

int bar_lit(uint32_t left_s, uint32_t total_s) {
    if (total_s == 0) return 0;
    return static_cast<int>((static_cast<uint64_t>(kBar) * left_s + total_s - 1) / total_s);
}

BarDot bar_dot(int dot) {
    const int k = (dot + dots::kBoard / 2 - kBarEdge) % kBar, along = k % kBarSide, far = dots::kBoard - 1 - kBarEdge;
    switch (k / kBarSide) {
    case 0: return {kBarEdge + along, kBarEdge};
    case 1: return {far, kBarEdge + along};
    case 2: return {far - along, far};
    default: return {kBarEdge, far - along};
    }
}

dots::Dot dot(const View &view, int col, int row) {
    using dots::Dot;
    const int last = kBar - bar_lit(view.left_s, view.total_s); // the first still lit: the next to go
    for (int i = 0; i < kBar; i++) {
        const BarDot b = bar_dot(i);
        if (b.col != col || b.row != row) continue;
        if (i == last && view.left_s % 2) return Dot::Dim; // ticking
        return i >= last ? Dot::On : Dot::Dim;
    }
    const int icon_left = (dots::kBoard - kIconWide) / 2;
    if (row >= kIconTop && row < kIconTop + kIconTall && col >= icon_left && col < icon_left + kIconWide)
        return kIcons[static_cast<int>(view.kind)][row - kIconTop][col - icon_left] == '#' ? Dot::On : Dot::Off;
    const std::string digits = shown(view.left_s);
    int x = (dots::kBoard - font::width(digits)) / 2;
    if (row < kDigitsTop || row >= kDigitsTop + font::kDigitRows) return Dot::Off;
    for (char d : digits) {
        const font::Glyph &g = font::digit(d);
        if (col >= x && col < x + g.width) return g.rows[row - kDigitsTop][col - x] == '#' ? Dot::On : Dot::Off;
        x += g.width + 1;
    }
    return Dot::Off;
}

lv_obj_t *create(lv_obj_t *parent, const TimerRun *run, For kind) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    auto *shown = new Shown{nullptr, run, {(run->left_ms(lv_tick_get()) + 999) / 1000, run->total_ms() / 1000, kind}};
    shown->timer = lv_timer_create(tick, 100, obj);
    lv_obj_set_user_data(obj, shown);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const auto *shown = static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
            dots::draw(e, [&](int col, int row) { return dot(shown->view, col, row); });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            auto *shown = static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
            lv_timer_delete(shown->timer);
            delete shown;
        },
        LV_EVENT_DELETE, nullptr);
    return obj;
}

} // namespace cubey::dial

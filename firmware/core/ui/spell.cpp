#include "ui/spell.h"

#include <algorithm>

namespace cubey::spell {

namespace {

struct Board {
    letter::Shown spelling;
    const SpellRun *run;
    letter::Frame frame;
};

Board *board_of(lv_obj_t *obj) { return static_cast<Board *>(lv_obj_get_user_data(obj)); }
Board *board_of(lv_event_t *e) { return board_of(static_cast<lv_obj_t *>(lv_event_get_current_target(e))); }

} // namespace

letter::Frame frame(const letter::Shown &spelling, const SpellRun &run, uint32_t now) {
    if (const auto ms = run.passing(now)) return letter::passing(spelling, *ms);
    return letter::spelled(spelling, std::max(run.shown(now), 1), run.shown_since(now));
}

lv_obj_t *create(lv_obj_t *parent, letter::Shown spelling, const SpellRun *run, uint32_t now) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    const letter::Frame first = frame(spelling, *run, now);
    lv_obj_set_user_data(obj, new Board{std::move(spelling), run, first});
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const Board *board = board_of(e);
            dots::draw(e, [&](int col, int row) { return letter::dot(board->spelling, board->frame, col, row); });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, [](lv_event_t *e) { delete board_of(e); }, LV_EVENT_DELETE, nullptr);
    return obj;
}

void show(lv_obj_t *spell, uint32_t now) {
    Board *board = board_of(spell);
    const letter::Frame next = frame(board->spelling, *board->run, now);
    const letter::Frame &was = board->frame;
    if (next.word == was.word && next.left == was.left && next.rest == was.rest) return;
    board->frame = next;
    lv_obj_invalidate(spell);
}

} // namespace cubey::spell

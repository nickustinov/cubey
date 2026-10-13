#include "ui/picture.h"

namespace cubey::picture {

namespace {

using Rows = std::vector<std::string>;
Rows *rows_of(lv_event_t *e) { return static_cast<Rows *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)))); }

} // namespace

dots::Dot dot(const Rows &rows, int col, int row) {
    if (row < 0 || row >= static_cast<int>(rows.size()) || col < 0 || col >= static_cast<int>(rows[row].size())) return dots::Dot::Off;
    const char c = rows[row][col];
    return c == '#' ? dots::Dot::On : c == '*' ? dots::Dot::Half : c == '+' ? dots::Dot::Dim : dots::Dot::Off;
}

lv_obj_t *create(lv_obj_t *parent, Rows rows) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    lv_obj_set_user_data(obj, new Rows(std::move(rows)));
    lv_obj_add_event_cb(
        obj, [](lv_event_t *e) { dots::draw(e, [rows = rows_of(e)](int col, int row) { return dot(*rows, col, row); }); },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, [](lv_event_t *e) { delete rows_of(e); }, LV_EVENT_DELETE, nullptr);
    return obj;
}

} // namespace cubey::picture

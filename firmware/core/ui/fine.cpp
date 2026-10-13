#include "ui/fine.h"

#include <algorithm>

namespace cubey::fine {

int Text::wide(int scale) const {
    int w = -1;
    for (const font::Letter *l : letters) w += static_cast<int>(l->rows[0].size()) + 1;
    return std::max(0, w) * scale;
}

Text text(const std::string &utf8) {
    Text t;
    for (const std::string &name : font::split(utf8))
        if (const font::Letter *l = font::letter(name)) t.letters.push_back(l);
    return t;
}

Text small(const std::string &utf8) {
    Text t;
    for (const std::string &name : font::split(utf8))
        if (const font::Letter *l = font::tiny(name)) t.letters.push_back(l);
    return t;
}

void set(Frame &f, int col, int row, dots::Dot as) {
    if (col >= 0 && col < kBoard && row >= 0 && row < kBoard) f[row * kBoard + col] = as;
}

void write(Frame &f, const Text &t, int x, int y, int scale, dots::Dot as) {
    for (const font::Letter *l : t.letters) {
        const int wide = static_cast<int>(l->rows[0].size());
        for (int gy = 0; gy < static_cast<int>(l->rows.size()); gy++)
            for (int gx = 0; gx < wide; gx++)
                if (l->rows[gy][gx] == '#')
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++) set(f, x + gx * scale + sx, y + (l->top + gy) * scale + sy, as);
        x += (wide + 1) * scale;
    }
}

void centred(Frame &f, const std::string &utf8, int y, int scale, dots::Dot as) { centred(f, text(utf8), y, scale, as); }

void centred(Frame &f, const Text &t, int y, int scale, dots::Dot as) { write(f, t, (kBoard - t.wide(scale)) / 2, y, scale, as); }

lv_obj_t *create(lv_obj_t *parent, const Frame &frame) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(obj, lv_color_hex(dots::ground()), 0); // over everything: nothing beneath shows through
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    auto *shown = new Frame(frame);
    lv_obj_set_user_data(obj, shown);
    lv_obj_add_event_cb(
        obj, [](lv_event_t *e) { dots::draw(e, [f = static_cast<Frame *>(lv_event_get_user_data(e))](int col, int row) { return (*f)[row * kBoard + col]; }, kBoard, 1); },
        LV_EVENT_DRAW_MAIN, shown);
    lv_obj_add_event_cb(obj, [](lv_event_t *e) { delete static_cast<Frame *>(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, shown);
    return obj;
}

void show(lv_obj_t *board, const Frame &frame) {
    auto *shown = static_cast<Frame *>(lv_obj_get_user_data(board));
    int col1 = kBoard, row1 = kBoard, col2 = -1, row2 = -1; // what changed
    for (int row = 0; row < kBoard; row++)
        for (int col = 0; col < kBoard; col++)
            if (frame[row * kBoard + col] != (*shown)[row * kBoard + col])
                col1 = std::min(col1, col), row1 = std::min(row1, row), col2 = std::max(col2, col), row2 = std::max(row2, row);
    if (col2 < 0) return;
    *shown = frame;
    dots::invalidate(board, col1, row1, col2, row2, kBoard);
}

dots::Cell cell_at(lv_obj_t *board, lv_point_t p) {
    lv_area_t box;
    lv_obj_get_coords(board, &box);
    const float pitch = static_cast<float>(lv_area_get_width(&box)) / kBoard;
    return dots::turned(dots::turn(), static_cast<int>((p.x - box.x1) / pitch), static_cast<int>((p.y - box.y1) / pitch), kBoard);
}

} // namespace cubey::fine

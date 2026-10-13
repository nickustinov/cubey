#include "ui/menu.h"

#include <algorithm>

namespace cubey::menu {

namespace {

using dots::Dot;

constexpr int kLines[3] = {2, 15, 28};  // each line's word's first row, in small capitals; its setting's a row under it
constexpr int kSlider = 42;             // the brightness, from this row down
constexpr int kButton = 50;             // OK's button, from this row down
constexpr int kSide = 20;               // an arrow's side of a line: this many columns in from either edge
constexpr int kTrack = 9, kSegment = 5; // the brightness's ten segments: the first's column, from one to the next
// A new Cubey's: its two languages only, each code kBig times the font's size under its word.
constexpr int kBig = 3;
constexpr int kFirstLines[2] = {1, 27}; // each line's word's first row; its code two rows under it
constexpr int kFirstButton = 52;

std::string capitals(const std::string &code) {
    std::string out;
    for (const std::string &letter : font::split(code)) out += font::upper(letter);
    return out;
}

} // namespace

Tap tap_at(int col, int row, bool first) {
    if (first) {
        if (row >= kFirstButton - 1) return {Tap::Does::Ok};
        const int line = row >= kFirstLines[1] - 2 ? 1 : 0;
        if (col < kSide) return {Tap::Does::Step, line, -1};
        if (col >= fine::kBoard - kSide) return {Tap::Does::Step, line, 1};
        return {};
    }
    if (row >= kButton - 2) return {Tap::Does::Ok};
    if (row >= kSlider - 2) return {Tap::Does::Brightness, 0, 0, std::clamp((col - kTrack) / kSegment + 1, 1, 10) * 10};
    const int line = row >= kLines[2] - 1 ? 2 : row >= kLines[1] - 1 ? 1 : 0;
    if (col < kSide) return {Tap::Does::Step, line, -1};
    if (col >= fine::kBoard - kSide) return {Tap::Does::Step, line, 1};
    return {};
}

// OK, on a dim button from `top` down.
static void ok(fine::Frame &f, int top, int rows) {
    for (int row = top; row < top + rows; row++)
        for (int col = 18; col < 42; col++) fine::set(f, col, row, Dot::Dim);
    fine::centred(f, "OK", top + (rows - font::kCapitalRows) / 2 - font::kCapitalTop, 1, Dot::On);
}

// A new Cubey's: what each line is, small, over its language's code, big, an arrow either side as big; OK.
static fine::Frame paint_first(const SettingsMenu &menu) {
    fine::Frame f;
    f.fill(Dot::Off);
    const char *words[2] = {"speaks", "teaches"};
    const std::string *codes[2] = {&menu.language(), &menu.learns()};
    for (int line = 0; line < 2; line++) {
        fine::centred(f, fine::small(words[line]), kFirstLines[line], 1, Dot::Half);
        const int top = kFirstLines[line] + 6;                  // its code's capitals' first row
        const int y = top - font::kCapitalTop * kBig;           // the line they stand on
        fine::centred(f, capitals(*codes[line]), y, kBig, Dot::On);
        const int arrow = 2, ay = top + (font::kCapitalRows * kBig - font::kCapitalRows * arrow) / 2 - font::kCapitalTop * arrow;
        fine::write(f, fine::text("◀"), 1, ay, arrow, Dot::Half);
        fine::write(f, fine::text("▶"), fine::kBoard - 1 - fine::text("▶").wide(arrow), ay, arrow, Dot::Half);
    }
    ok(f, kFirstButton, 8);
    return f;
}

fine::Frame paint(const SettingsMenu &menu) {
    if (menu.first()) return paint_first(menu);
    fine::Frame f;
    f.fill(Dot::Off);
    const char *words[3] = {"speaks", "teaches", "theme"};
    const std::string *settings[3] = {&menu.language(), &menu.learns(), &menu.theme()};
    for (int line = 0; line < 3; line++) {
        fine::centred(f, fine::small(words[line]), kLines[line], 1, Dot::Half);
        const int y = kLines[line] + 5 - font::kCapitalTop; // its capitals (the font's rows 3 to 8), a row under the word's four
        fine::centred(f, capitals(*settings[line]), y, 1, Dot::On);
        fine::write(f, fine::text("◀"), 3, y, 1, Dot::Half);
        fine::write(f, fine::text("▶"), fine::kBoard - 3 - fine::text("▶").wide(1), y, 1, Dot::Half);
    }
    // The brightness: a small sun, then ten segments, lit as far as it goes.
    const char *sun[5] = {"#.#.#", ".###.", "#####", ".###.", "#.#.#"};
    for (int row = 0; row < 5; row++)
        for (int x = 0; x < 5; x++)
            if (sun[row][x] == '#') fine::set(f, 1 + x, kSlider + row, Dot::Half);
    for (int segment = 0; segment < 10; segment++)
        for (int row = kSlider + 1; row < kSlider + 4; row++)
            for (int x = 0; x < kSegment - 1; x++)
                fine::set(f, kTrack + segment * kSegment + x, row, segment * 10 < menu.brightness() ? Dot::On : Dot::Dim);
    ok(f, kButton, 9);
    return f;
}

// A finger on it: as it goes down, a step of a line, the brightness or OK; as it moves, the brightness slides with it.
static void touched(lv_event_t *e) {
    auto *view = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    auto *menu = static_cast<SettingsMenu *>(lv_event_get_user_data(e));
    lv_point_t p{};
    lv_indev_get_point(lv_indev_active(), &p);
    const dots::Cell c = fine::cell_at(view, p);
    const uint32_t now = lv_tick_get();
    const Tap tap = tap_at(c.col, c.row, menu->first());
    if (lv_event_get_code(e) == LV_EVENT_PRESSING && tap.does != Tap::Does::Brightness) return;
    switch (tap.does) {
    case Tap::Does::Step: menu->step(tap.line, tap.by, now); break;
    case Tap::Does::Brightness: menu->brighten(tap.percent, now); break;
    case Tap::Does::Ok: menu->confirm(); break;
    case Tap::Does::Nothing: break;
    }
}

lv_obj_t *create(lv_obj_t *parent, SettingsMenu *menu) {
    lv_obj_t *view = fine::create(parent, paint(*menu));
    lv_obj_add_event_cb(view, touched, LV_EVENT_PRESSED, menu);
    lv_obj_add_event_cb(view, touched, LV_EVENT_PRESSING, menu);
    return view;
}

void show(lv_obj_t *view, const SettingsMenu &menu) { fine::show(view, paint(menu)); }

} // namespace cubey::menu

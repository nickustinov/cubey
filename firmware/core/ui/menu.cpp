#include "ui/menu.h"

#include <algorithm>

namespace cubey::menu {

namespace {

using dots::Dot;

constexpr int kLines[2] = {5, 22};      // each line's word's first row, in small capitals; its setting a row under it
constexpr int kSecond[3] = {2, 16, 30}; // the second page's: its brightness, its volume, how the child talks
constexpr int kPages = 45;              // the pages' dots, from this row down; a tap about them turns to the other page
constexpr int kButton = 51;             // OK's button, from this row down
constexpr int kSide = 20;               // an arrow's side of a line: this many columns in from either edge
constexpr int kTrack = 9, kSegment = 5; // a slider's ten segments: the first's column, from one to the next
const char *const kSun[5] = {"#.#.#", ".###.", "#####", ".###.", "#.#.#"};     // the brightness's icon
const char *const kSpeaker[5] = {"..#..", ".##.#", "###.#", ".##.#", "..#.."}; // the volume's
// A new Cubey's: the language it speaks only, its code kBig times the font's size under its word, an arrow either side.
constexpr int kBig = 4, kArrow = 2;
constexpr int kFirstWord = 6, kFirstCode = 14; // the word's first row; the code's capitals' first row
constexpr int kFirstButton = 50;

std::string capitals(const std::string &code) {
    std::string out;
    for (const std::string &letter : font::split(code)) out += font::upper(letter);
    return out;
}

} // namespace

Tap tap_at(int col, int row, bool first, int page) {
    if (first) {
        if (row >= kFirstButton - 1) return {Tap::Does::Ok};
        if (col < kSide) return {Tap::Does::Step, 0, -1};
        if (col >= fine::kBoard - kSide) return {Tap::Does::Step, 0, 1};
        return {};
    }
    if (row >= kButton - 2) return {Tap::Does::Ok};
    if (row >= kPages - 4) return {Tap::Does::Page, 0, 0, 0, 1 - page};
    if (page == 1) {
        if (row >= kSecond[2] - 2) return {Tap::Does::Hold}; // either way: there are two
        const int percent = std::clamp((col - kTrack) / kSegment + 1, 1, 10) * 10;
        return {row >= kSecond[1] - 2 ? Tap::Does::Volume : Tap::Does::Brightness, 0, 0, percent};
    }
    const int line = row >= kLines[1] - 2 ? 1 : 0;
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

// A new Cubey's: what it is, small, over the code of the language it speaks, big, an arrow either side; OK.
static fine::Frame paint_first(const SettingsMenu &menu) {
    fine::Frame f;
    f.fill(Dot::Off);
    fine::centred(f, fine::small("speaks"), kFirstWord, 1, Dot::Half);
    fine::centred(f, capitals(menu.language()), kFirstCode - font::kCapitalTop * kBig, kBig, Dot::On);
    const int ay = kFirstCode + font::kCapitalRows * (kBig - kArrow) / 2 - font::kCapitalTop * kArrow; // the arrows' line
    fine::write(f, fine::text("◀"), 1, ay, kArrow, Dot::Half);
    fine::write(f, fine::text("▶"), fine::kBoard - 1 - fine::text("▶").wide(kArrow), ay, kArrow, Dot::Half);
    ok(f, kFirstButton, 8);
    return f;
}

// A line at `top`: what it is, small, over its setting in capitals, an arrow either side.
static void choice(fine::Frame &f, const char *word, const std::string &setting, int top) {
    fine::centred(f, fine::small(word), top, 1, Dot::Half);
    const int y = top + 5 - font::kCapitalTop; // its capitals (the font's rows 3 to 8), a row under the word's four
    fine::centred(f, setting, y, 1, Dot::On);
    fine::write(f, fine::text("◀"), 3, y, 1, Dot::Half);
    fine::write(f, fine::text("▶"), fine::kBoard - 3 - fine::text("▶").wide(1), y, 1, Dot::Half);
}

// A slider at `top`, under its word: its icon, then ten segments, lit as far as it goes.
static void slider(fine::Frame &f, const char *word, const char *const icon[5], int top, int percent) {
    fine::centred(f, fine::small(word), top, 1, Dot::Half);
    for (int row = 0; row < 5; row++)
        for (int x = 0; x < 5; x++)
            if (icon[row][x] == '#') fine::set(f, 1 + x, top + 6 + row, Dot::Half);
    for (int segment = 0; segment < 10; segment++)
        for (int row = top + 7; row < top + 10; row++)
            for (int x = 0; x < kSegment - 1; x++) fine::set(f, kTrack + segment * kSegment + x, row, segment * 10 < percent ? Dot::On : Dot::Dim);
}

fine::Frame paint(const SettingsMenu &menu) {
    if (menu.first()) return paint_first(menu);
    fine::Frame f;
    f.fill(Dot::Off);
    if (menu.page() == 0) {
        choice(f, "speaks", capitals(menu.language()), kLines[0]);
        choice(f, "theme", capitals(menu.theme()), kLines[1]);
    } else {
        slider(f, "brightness", kSun, kSecond[0], menu.brightness());
        slider(f, "volume", kSpeaker, kSecond[1], menu.volume());
        choice(f, "talk", menu.hold() ? "HOLD" : "FREE", kSecond[2]);
    }
    // Its pages: a dot each, this one lit, an arrow towards the other.
    for (int page = 0; page < 2; page++)
        for (int row = kPages; row < kPages + 3; row++)
            for (int x = 0; x < 3; x++) fine::set(f, 25 + 7 * page + x, row, page == menu.page() ? Dot::On : Dot::Dim);
    if (menu.page() == 0) fine::write(f, fine::text(">"), 38, kPages - 5, 1, Dot::Half);
    else fine::write(f, fine::text("<"), 18, kPages - 5, 1, Dot::Half);
    ok(f, kButton, 8);
    return f;
}

// A finger on it: as it goes down, a step of a line, a slider, the other page or OK; as it moves, a slider slides with
// it.
static void touched(lv_event_t *e) {
    auto *view = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    auto *menu = static_cast<SettingsMenu *>(lv_event_get_user_data(e));
    lv_point_t p{};
    lv_indev_get_point(lv_indev_active(), &p);
    const dots::Cell c = fine::cell_at(view, p);
    const uint32_t now = lv_tick_get();
    const Tap tap = tap_at(c.col, c.row, menu->first(), menu->page());
    if (lv_event_get_code(e) == LV_EVENT_PRESSING && tap.does != Tap::Does::Brightness && tap.does != Tap::Does::Volume) return;
    switch (tap.does) {
    case Tap::Does::Step: menu->step(tap.line, tap.by, now); break;
    case Tap::Does::Brightness: menu->brighten(tap.percent, now); break;
    case Tap::Does::Volume: menu->set_volume(tap.percent, now); break;
    case Tap::Does::Hold: menu->set_hold(!menu->hold(), now); break;
    case Tap::Does::Page: menu->to_page(tap.page, now); break;
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

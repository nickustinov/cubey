#include "ui/wifi.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cubey::wifi_view {

namespace {

using dots::Dot;
using fine::set;
using fine::Text;
using fine::text;
using fine::write;
using Screen = WifiSetup::Screen;

constexpr uint32_t kBlinkMs = 500; // the cursor, and the dots while it looks for networks

// A name on its row, cut in its middle with "…" when it would run past `room`: names differ at their ends ("Apartment"
// and "Apartment-5G": cut at the end, they were the same).
Text fitted(const std::string &utf8, int room) {
    Text t = text(utf8);
    if (t.wide(1) <= room) return t;
    const font::Letter *more = font::letter("…");
    t.letters.insert(t.letters.begin() + static_cast<long>(t.letters.size() / 2), more);
    while (t.letters.size() > 1 && t.wide(1) > room) {
        const auto at = std::find(t.letters.begin(), t.letters.end(), more);
        t.letters.erase(at - t.letters.begin() > t.letters.end() - at - 1 ? at - 1 : at + 1); // from the longer side
    }
    return t;
}

void networks(Frame &f, const WifiSetup &setup, uint32_t now) {
    const std::vector<Network> &all = setup.networks();
    if (all.empty()) { // still looking: three dots, one more each moment
        const int lit = static_cast<int>(now / kBlinkMs % 4);
        for (int i = 0; i < lit; i++)
            for (int y = 0; y < 2; y++)
                for (int x = 0; x < 2; x++) set(f, kBoard / 2 - 7 + i * 6 + x, kBoard / 2 + y, Dot::On);
        return;
    }
    for (int i = 0; i < WifiSetup::kShown && setup.top() + i < static_cast<int>(all.size()); i++) {
        const Network &n = all[setup.top() + i];
        write(f, fitted(n.ssid, kBoard - 2 * kPad), kPad, i * kRow, 1, n.rssi >= -60 ? Dot::On : n.rssi >= -72 ? Dot::Half : Dot::Dim);
    }
}

void password(Frame &f, const WifiSetup &setup, uint32_t now) {
    // What is typed: its end, if it runs past the board, and a cursor blinking after it.
    Text typed = text(setup.password());
    const int cursor = static_cast<int>(font::letter("_")->rows[0].size()) + 1;
    while (!typed.letters.empty() && typed.wide(1) + 1 + cursor > kBoard) typed.letters.erase(typed.letters.begin());
    write(f, typed, 0, 0, 1, Dot::On);
    if (now / kBlinkMs % 2 == 0) write(f, text("_"), typed.letters.empty() ? 0 : typed.wide(1) + 2, 0, 1, Dot::On);
    using keyboard::Does;
    for (int row = 0; row < keyboard::kRows; row++)
        for (int col = 0; col < keyboard::kCols; col++) {
            const keyboard::Key key = keyboard::key(setup.page(), col, row);
            const bool on = key.does == Does::Shift && setup.page() == keyboard::Page::Capitals;
            const bool bar = key.text == " " && keyboard::key(setup.page(), col + 1, row).text == " "; // the space bar
            const int x = col * kKey, y = kRow + row * kRow;
            for (int cy = 0; cy < font::kLineRows; cy++)
                for (int cx = 0; cx < kKey - (bar ? 0 : 1); cx++) set(f, x + cx, y + cy, on ? Dot::On : Dot::Dim);
            if (key.text == " " && col != 6) continue; // the bar's glyph once, in its middle
            const Text glyph = text(keyboard::glyph(key, setup.page()));
            write(f, glyph, x + (kKey - 1 - glyph.wide(1)) / 2, y, 1, on ? Dot::Off : Dot::On);
        }
}

// Joining: the Wi-Fi sign, its arcs lit one more each moment (the rest dim), over the network's name.
void joining(Frame &f, const WifiSetup &setup, uint32_t now) {
    constexpr int kX = kBoard / 2, kY = 32, kArc = 8; // the sign's dot, and from one arc to the next
    const int lit = static_cast<int>(now / kArcMs % 4);
    for (int row = 0; row < kY; row++)
        for (int col = 0; col < kBoard; col++) {
            const float dx = col + 0.5f - kX, dy = row + 0.5f - kY, d = std::sqrt(dx * dx + dy * dy);
            if (std::fabs(dx) > -dy) continue; // a quarter of a circle, upwards
            const int arc = static_cast<int>(std::lround(d / kArc));
            if (arc >= 1 && arc <= 3 && std::fabs(d - arc * kArc) < 1.5f) set(f, col, row, arc <= lit ? Dot::On : Dot::Dim);
        }
    for (int y = kY - 2; y < kY + 1; y++)
        for (int x = kX - 1; x < kX + 2; x++) set(f, x, y, Dot::On);
    const Text name = fitted(setup.ssid(), kBoard - 2 * kPad);
    write(f, name, (kBoard - name.wide(1)) / 2, 42, 1, Dot::Half);
}

// Not joined: a cross, and why, small.
void failed(Frame &f, const WifiSetup &setup) {
    fine::centred(f, "✗", -6, 4, Dot::On);
    if (setup.why() == Wifi::Refused) fine::centred(f, "wrong", 34, 1, Dot::Half), fine::centred(f, "password?", 46, 1, Dot::Half);
    else fine::centred(f, setup.why() == Wifi::NotFound ? "not found" : "not joined", 40, 1, Dot::Half);
}

struct View {
    WifiSetup *setup;
    lv_point_t from; // where the finger went down
    int dragged = 0; // rows scrolled by this touch
};

View *view_of(lv_event_t *e) { return static_cast<View *>(lv_event_get_user_data(e)); }
lv_obj_t *board_of(lv_event_t *e) { return static_cast<lv_obj_t *>(lv_event_get_current_target(e)); }

lv_point_t point() {
    lv_point_t p{};
    lv_indev_get_point(lv_indev_active(), &p);
    return p;
}

} // namespace

Frame paint(const WifiSetup &setup, uint32_t now) {
    Frame f;
    f.fill(Dot::Off);
    switch (setup.screen()) {
    case Screen::Welcome: // a big hi, and what to do, small (too long for one line)
        fine::centred(f, "HI!", 3 - font::kCapitalTop * 3, 3, Dot::On);
        fine::centred(f, "Tap to", 24, 1, Dot::Half), fine::centred(f, "connect", 35, 1, Dot::Half), fine::centred(f, "Wi-Fi", 46, 1, Dot::Half);
        break;
    case Screen::Networks: networks(f, setup, now); break;
    case Screen::Password: password(f, setup, now); break;
    case Screen::Joining: joining(f, setup, now); break;
    case Screen::Joined: fine::centred(f, "✓", 30 - (4 + 2) * 6, 6, Dot::On); break;
    case Screen::Failed: failed(f, setup); break;
    case Screen::Off: break;
    }
    return f;
}

lv_obj_t *create(lv_obj_t *parent, WifiSetup *setup, uint32_t now) {
    lv_obj_t *board = fine::create(parent, paint(*setup, now));
    auto *view = new View{setup, {}, 0};
    lv_obj_add_event_cb(board, [](lv_event_t *e) { view_of(e)->from = point(), view_of(e)->dragged = 0; }, LV_EVENT_PRESSED, view);
    // The networks follow the finger, a row for each row's height it moves.
    lv_obj_add_event_cb(
        board,
        [](lv_event_t *e) {
            View *v = view_of(e);
            if (v->setup->screen() != Screen::Networks) return;
            const int rows = (fine::cell_at(board_of(e), v->from).row - fine::cell_at(board_of(e), point()).row) / kRow;
            if (rows != v->dragged) v->setup->scroll(rows - v->dragged, lv_tick_get()), v->dragged = rows;
        },
        LV_EVENT_PRESSING, view);
    lv_obj_add_event_cb(
        board,
        [](lv_event_t *e) {
            View *v = view_of(e);
            if (v->dragged) return;
            const dots::Cell c = fine::cell_at(board_of(e), v->from);
            const uint32_t now = lv_tick_get();
            if (v->setup->screen() == Screen::Welcome) v->setup->tap(now);
            else if (v->setup->screen() == Screen::Networks) v->setup->pick(c.row / kRow, now);
            else if (v->setup->screen() == Screen::Password && c.row >= kRow) v->setup->press(c.col / kKey, (c.row - kRow) / kRow, now);
        },
        LV_EVENT_RELEASED, view);
    lv_obj_add_event_cb(board, [](lv_event_t *e) { delete view_of(e); }, LV_EVENT_DELETE, view);
    return board;
}

void show(lv_obj_t *board, const WifiSetup &setup, uint32_t now) { fine::show(board, paint(setup, now)); }

} // namespace cubey::wifi_view

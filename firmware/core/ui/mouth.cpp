#include "ui/mouth.h"

#include <cstdlib>

#include "services/music.h"
#include "ui/eyes.h"

namespace cubey::mouth {

namespace {

// On the 24-dot board: the eyes' top row, the lips' outline, the teeth (each 2 dots wide, their left columns outer to
// inner on each side, rows at the top and the bottom), the brush's head and handle rows, and the sparkles' middles.
constexpr int kEyesTop = 3;
constexpr int kLipLeft = 2, kLipRight = 21, kLipTop = 8, kLipBottom = 19;
constexpr int kTeethLeft[] = {3, 6, 9}, kTeethRight[] = {19, 16, 13};
constexpr int kTopTeeth = 9, kBottomTeeth = 17; // the first of each 2 rows
constexpr int kTopHead = 11, kBottomHead = 15;  // the brush's head, 3 wide and 2 tall, below or above the teeth
constexpr int kSparkles[][2] = {{6, 13}, {17, 14}};
constexpr uint32_t kScrubMs = kBeatMs / 4, kSparkleMs = 250; // the brush scrubs on the beat
constexpr uint32_t kBlinkEveryMs = 4000, kBlinkMs = 140;

// Its eyes, its head back: 3 rows, a byte a row (the leftmost dot the bit eyes::kWide - 1); shut; happy arches.
constexpr uint8_t kEyeBack[] = {0b01110, 0b11111, 0b01110}, kEyeShut[] = {0, 0b11111, 0}, kEyeHappy[] = {0b01110, 0b10001, 0};

bool eye(const View &view, int col, int row) {
    const int r = row - kEyesTop, left = eyes::place_on(dots::kBoard).left, c = col - left;
    const int x = c >= eyes::kWide + eyes::kGap ? c - eyes::kWide - eyes::kGap : c;
    if (r < 0 || r >= 3 || x < 0 || x >= eyes::kWide) return false;
    const uint8_t *shape = view.at.done ? kEyeHappy : view.blink ? kEyeShut : kEyeBack;
    return shape[r] & (1 << (eyes::kWide - 1 - x));
}

bool left_part(int part) { return part == 0 || part == 3; }
bool top_part(int part) { return part < 2; }

// Which part of the mouth a tooth's dot is in, and the tooth's place in it, outer first; -1 where none is.
int tooth(int col, int row, int &k) {
    const bool top = row == kTopTeeth || row == kTopTeeth + 1, bottom = row == kBottomTeeth || row == kBottomTeeth + 1;
    if (!top && !bottom) return -1;
    for (k = 0; k < 3; k++) {
        if (col == kTeethLeft[k] || col == kTeethLeft[k] + 1) return top ? 0 : 3;
        if (col == kTeethRight[k] || col == kTeethRight[k] + 1) return top ? 1 : 2;
    }
    return -1;
}

bool brush(const View &view, int col, int row) {
    const int part = view.at.part, head_row = top_part(part) ? kTopHead : kBottomHead;
    const int head = (left_part(part) ? kTeethLeft[0] : kTeethRight[2]) + view.scrub;
    if (col >= head && col < head + 3 && (row == head_row || row == head_row + 1)) return true;
    const int handle_row = top_part(part) ? kTopHead + 1 : kBottomHead; // along its row nearer the middle, out of the mouth
    return row == handle_row && (left_part(part) ? col < head : col >= head + 3);
}

bool sparkle(const View &view, int col, int row) {
    for (const auto &s : kSparkles) {
        const int dx = std::abs(col - s[0]), dy = std::abs(row - s[1]);
        if (dx + dy == 0 || (view.sparkle && dx + dy == 1)) return true;
    }
    return false;
}

struct Shown {
    lv_timer_t *timer = nullptr;
    const BrushingRun *run;
    void (*tapped)();
    View view;
};

// A frame at a time, from the run: it redraws only as the picture changes.
void tick(lv_timer_t *t) {
    auto *obj = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
    auto *shown = static_cast<Shown *>(lv_obj_get_user_data(obj));
    const uint32_t now = lv_tick_get();
    View view{shown->run->started(), brushing_at(shown->run->elapsed(now)), 0, (now / kSparkleMs) % 2 == 1,
              now % kBlinkEveryMs < kBlinkMs};
    const int step = static_cast<int>((now / kScrubMs) % (2 * kScrubMost));
    view.scrub = step <= kScrubMost ? step : 2 * kScrubMost - step; // there and back
    const View &was = shown->view;
    const bool same = view.started == was.started && view.at.done == was.at.done && view.at.part == was.at.part &&
                      static_cast<int>(view.at.part_progress * 3) == static_cast<int>(was.at.part_progress * 3) &&
                      view.scrub == was.scrub && view.sparkle == was.sparkle && view.blink == was.blink;
    shown->view = view;
    if (!same) lv_obj_invalidate(obj);
}

} // namespace

dots::Dot dot(const View &view, int col, int row) {
    using dots::Dot;
    const bool lips = ((row == kLipTop || row == kLipBottom) && col > kLipLeft && col < kLipRight) ||
                      ((col == kLipLeft || col == kLipRight) && row > kLipTop && row < kLipBottom);
    if (lips || eye(view, col, row)) return Dot::On;
    const bool brushing = view.started && !view.at.done;
    if (brushing && brush(view, col, row)) return Dot::On;
    if (view.at.done && sparkle(view, col, row)) return Dot::On;
    int k = 0;
    const int part = tooth(col, row, k);
    if (part < 0) return Dot::Off;
    const bool clean = view.at.done || (view.started && (part < view.at.part ||
                                                         (part == view.at.part && view.at.part_progress >= (k + 1) / 3.0f)));
    return clean ? Dot::On : Dot::Dim;
}

lv_obj_t *create(lv_obj_t *parent, const BrushingRun *run, void (*tapped)()) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    auto *shown = new Shown{nullptr, run, tapped, {}};
    shown->timer = lv_timer_create(tick, eyes::kFrameMs, obj);
    lv_obj_set_user_data(obj, shown);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const auto *shown = static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
            dots::draw(e, [&](int col, int row) { return dot(shown->view, col, row); });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(
        obj, [](lv_event_t *e) { static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))))->tapped(); },
        LV_EVENT_PRESSED, nullptr);
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

} // namespace cubey::mouth

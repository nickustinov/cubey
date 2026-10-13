#include "ui/eyes.h"

#include <iterator>

#include "services/ticks.h"
#include "ui/dots.h"

namespace cubey::eyes {

namespace {

constexpr Frame kOpen = {0b01110, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b01110};
constexpr Frame kShut = {0, 0, 0, 0, 0, 0b11111, 0};
// The lid down two and four rows.
constexpr Frame kLid2 = {0, 0, 0b11111, 0b11111, 0b11111, 0b11111, 0b01110};
constexpr Frame kLid4 = {0, 0, 0, 0, 0b11111, 0b11111, 0b01110};
const Frame *const kBlink[] = {&kLid2, &kLid4, &kShut, &kLid4, &kLid2};
constexpr Frame kSquint = {0, 0, 0b01110, 0b11111, 0b11111, 0b11111, 0b01110};
constexpr Frame kFar = {0, 0b01110, 0b11111, 0b11111, 0b11111, 0b01110, 0};

constexpr uint32_t kBlinkLeastMs = 2500, kBlinkMostMs = 7000;
// Between the eyes' moves: a rest, then of every hundred, this many glances and squints (the rest nothing).
constexpr uint32_t kRestLeastMs = 2500, kRestSpreadMs = 3500;
constexpr uint32_t kGlances = 50, kSquints = 15;
constexpr uint32_t kGlanceLeastMs = 800, kGlanceSpreadMs = 1700, kSquintLeastMs = 1000, kSquintSpreadMs = 1000;

bool reached(uint32_t now, uint32_t at) { return static_cast<int32_t>(now - at) >= 0; }
int toward(int from, int to) { return from + (to > from) - (to < from); }
uint32_t random_below(uint32_t below) { return lv_rand(0, below - 1); }

struct Eyes {
    explicit Eyes(uint32_t now) : wander(random_below, now) {}
    lv_timer_t *timer = nullptr;
    Wander wander;
    const Frame *shown[2] = {&kOpen, &kOpen};
    Look look;
    bool shut = false;
    uint32_t blink_at = 0, next_blink = 0;
};

void draw(lv_event_t *e) {
    const auto *eyes = static_cast<Eyes *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
    const Place place = place_on(dots::kBoard);
    const int top = place.top + eyes->look.dy, left = place.left + eyes->look.dx;
    dots::draw(e, [&](int col, int row) {
        const int r = row - top, c = col - left, eye = c >= kWide + kGap ? 1 : 0, x = c - eye * (kWide + kGap);
        const bool on = r >= 0 && r < kTall && x >= 0 && x < kWide && ((*eyes->shown[eye])[r] & (1 << (kWide - 1 - x)));
        return on ? dots::Dot::On : dots::Dot::Off;
    });
}

// A frame at a time: where they look and how (Wander), a blink's frames now and then (sometimes twice running), or
// shut. Looking well aside, the far eye is the smaller. It redraws only when something changes.
void tick(lv_timer_t *t) {
    auto *obj = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
    auto *eyes = static_cast<Eyes *>(lv_obj_get_user_data(obj));
    const uint32_t now = lv_tick_get();
    if (!eyes->shut && now >= eyes->next_blink) {
        eyes->blink_at = now;
        const uint32_t length = static_cast<uint32_t>(std::size(kBlink)) * kFrameMs;
        eyes->next_blink = now + (lv_rand(0, 4) == 0 ? length + 150 : lv_rand(kBlinkLeastMs, kBlinkMostMs));
    }
    const Frame *blinking = blink(since(now, eyes->blink_at));
    const Look look = eyes->shut ? Look{} : eyes->wander.at(now);
    bool changed = look.dx != eyes->look.dx || look.dy != eyes->look.dy;
    for (int eye = 0; eye < 2; eye++) {
        const bool far = eye == 0 ? look.dx >= 2 : look.dx <= -2;
        const Frame *shown = eyes->shut ? &kShut : blinking ? blinking : look.squint ? &kSquint : far ? &kFar : &kOpen;
        changed = changed || shown != eyes->shown[eye];
        eyes->shown[eye] = shown;
    }
    eyes->look = look;
    if (changed) lv_obj_invalidate(obj);
}

} // namespace

const Frame &open() { return kOpen; }
const Frame &shut() { return kShut; }
const Frame &squinted() { return kSquint; }
const Frame &farther() { return kFar; }

Wander::Wander(Pick pick, uint32_t now) : pick_(pick), next_at_(now + kRestLeastMs + pick(kRestSpreadMs)) {}

Look Wander::at(uint32_t now) {
    if (holding_ && reached(now, back_at_)) { // back to looking ahead, open, and a rest
        holding_ = false;
        target_ = {};
        stepped_at_ = now - kFrameMs;
        next_at_ = now + kRestLeastMs + pick_(kRestSpreadMs);
    }
    if (!holding_ && reached(now, next_at_)) {
        const uint32_t what = pick_(100);
        if (what < kGlances) {
            static const int ways[] = {-2, 2, -1, 1, -1, 1}; // mostly a dot
            target_.dx = ways[pick_(6)];
            target_.dy = pick_(4) == 0 ? -1 : 0;
            back_at_ = now + kGlanceLeastMs + pick_(kGlanceSpreadMs);
            holding_ = true;
            stepped_at_ = now - kFrameMs; // the first dot at once
        } else if (what < kGlances + kSquints) {
            target_.squint = true;
            back_at_ = now + kSquintLeastMs + pick_(kSquintSpreadMs);
            holding_ = true;
        } else {
            next_at_ = now + kRestLeastMs + pick_(kRestSpreadMs);
        }
    }
    shown_.squint = target_.squint;
    if ((shown_.dx != target_.dx || shown_.dy != target_.dy) && since(now, stepped_at_) >= kFrameMs) {
        shown_.dx = toward(shown_.dx, target_.dx);
        shown_.dy = toward(shown_.dy, target_.dy);
        stepped_at_ = now;
    }
    return shown_;
}

Place place_on(int dots) { return {(dots - 2 * kWide - kGap) / 2, (dots - kTall) / 2}; }

const Frame *blink(uint32_t ms) {
    const uint32_t frame = ms / kFrameMs;
    return frame < std::size(kBlink) ? kBlink[frame] : nullptr;
}

lv_obj_t *create(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    auto *eyes = new Eyes(lv_tick_get());
    eyes->blink_at = lv_tick_get() - 1000; // no blink as they open
    eyes->next_blink = lv_tick_get() + lv_rand(kBlinkLeastMs, kBlinkMostMs);
    eyes->timer = lv_timer_create(tick, kFrameMs, obj);
    lv_obj_set_user_data(obj, eyes);
    lv_obj_add_event_cb(obj, draw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            auto *eyes = static_cast<Eyes *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
            lv_timer_delete(eyes->timer);
            delete eyes;
        },
        LV_EVENT_DELETE, nullptr);
    return obj;
}

void set_shut(lv_obj_t *obj, bool shut) { static_cast<Eyes *>(lv_obj_get_user_data(obj))->shut = shut; }

} // namespace cubey::eyes

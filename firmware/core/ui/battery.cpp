#include "ui/battery.h"

#include <algorithm>
#include <cmath>

#include "ui/dots.h"
#include "ui/eyes.h"

namespace cubey::battery {

namespace {

// On the 24-dot board: the outline's columns and rows, the nub's right of it, the charge's first column and rows.
constexpr int kLeft = 3, kRight = 19, kTop = 8, kBottom = 15;
constexpr int kNubRight = 21, kNubTop = 10, kNubBottom = 13;
constexpr int kFillLeft = kLeft + 2, kFillTop = kTop + 2, kFillBottom = kBottom - 2;
static_assert(kFillLeft + kFillMost - 1 == kRight - 2, "the charge, full, a dot clear of the outline");
constexpr uint32_t kSparkMs = 500; // charging: the column after the charge, on and off

struct Shown {
    lv_timer_t *timer = nullptr;
    int target = 0, filled = 0;
    bool charging = false, spark = false;
    uint32_t spark_at = 0;
};

// A column a frame up to its charge; then, charging, the next column blinks. It redraws only as it changes.
void tick(lv_timer_t *t) {
    auto *obj = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
    auto *shown = static_cast<Shown *>(lv_obj_get_user_data(obj));
    const uint32_t now = lv_tick_get();
    if (shown->filled < shown->target) {
        shown->filled++;
        shown->spark_at = now;
    } else if (shown->charging && shown->target < kFillMost && now - shown->spark_at >= kSparkMs) {
        shown->spark = !shown->spark;
        shown->spark_at = now;
    } else {
        return;
    }
    lv_obj_invalidate(obj);
}

} // namespace

int fill(int percent) {
    if (percent <= 0) return 0;
    return std::max(1, static_cast<int>(lroundf(static_cast<float>(std::min(percent, 100)) * kFillMost / 100)));
}

bool lit(int filled, int col, int row) {
    const bool outline = ((row == kTop || row == kBottom) && col >= kLeft && col <= kRight) ||
                         ((col == kLeft || col == kRight) && row >= kTop && row <= kBottom);
    const bool nub = col > kRight && col <= kNubRight && row >= kNubTop && row <= kNubBottom;
    const bool charge = col >= kFillLeft && col < kFillLeft + filled && row >= kFillTop && row <= kFillBottom;
    return outline || nub || charge;
}

lv_obj_t *create(lv_obj_t *parent, Battery battery) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    auto *shown = new Shown;
    shown->target = fill(battery.percent);
    shown->charging = battery.charging;
    shown->timer = lv_timer_create(tick, eyes::kFrameMs, obj);
    lv_obj_set_user_data(obj, shown);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const auto *shown = static_cast<Shown *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e))));
            dots::draw(e, [&](int col, int row) { return lit(shown->filled + (shown->spark ? 1 : 0), col, row) ? dots::Dot::On : dots::Dot::Off; });
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

} // namespace cubey::battery

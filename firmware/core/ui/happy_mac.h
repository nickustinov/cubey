#pragma once

#include <cstdint>

#include "lvgl.h"
#include "services/update.h"
#include "ui/fine.h"

// The update's screen (services/update.h) on the fine dot board (ui/fine.h), over everything: the Happy Mac, Susan
// Kare's icon from the first Macintosh's start-up, a dot to its pixel, and under it a progress bar of the old Mac OS,
// filling as the update is fetched, a barber pole turning in the rest. Meanwhile a floppy goes into its slot, again
// and again, and now and then it blinks; done, it winks; failed, it is the Sad Mac, its eyes crossed out.
namespace cubey::happy_mac {

constexpr uint32_t kDiskMs = 2000;       // a floppy under the slot, going in, gone; then the next
constexpr uint32_t kBlinkEveryMs = 3000; // a blink, at the end of each
constexpr uint32_t kBlinkMs = 150;

// The screen `ms` after the update began, as far as it has come.
fine::Frame paint(UpdateState state, int percent, uint32_t ms);
lv_obj_t *create(lv_obj_t *parent);
void show(lv_obj_t *view, UpdateState state, int percent, uint32_t ms); // only what changed drawn again

} // namespace cubey::happy_mac

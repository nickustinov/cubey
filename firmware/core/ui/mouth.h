#pragma once

#include "lvgl.h"
#include "services/brushing.h"
#include "ui/dots.h"

// Cubey's face on the dot board (ui/dots.h), brushing with her, its head back: its eyes above, flattened, blinking
// now and then, and its mouth wide open below them, its lips a rounded outline, three teeth on each side at the top
// and the bottom, dim until brushed. Once she taps it, a toothbrush scrubs the part to brush now (round the mouth from
// the top on the left as she sees it), and its teeth shine one by one as the part goes; done, they all shine,
// sparkles twinkle in the mouth, and its eyes are happy arches.
namespace cubey::mouth {

constexpr int kScrubMost = 5; // the brush's steps along its part, there and back

struct View {
    bool started = false; // her tap: the brush scrubs
    BrushingAt at = brushing_at(0);
    int scrub = 0;        // 0..kScrubMost along its part
    bool sparkle = false; // done: the sparkles at their biggest
    bool blink = false;   // its eyes shut for a moment
};

dots::Dot dot(const View &view, int col, int row);

// The mouth over all of `parent`, following `run` (which lives longer); a tap on it calls `tapped`.
lv_obj_t *create(lv_obj_t *parent, const BrushingRun *run, void (*tapped)());

} // namespace cubey::mouth

#pragma once

#include <array>
#include <cstdint>

#include "lvgl.h"

// Cubey's eyes: two lit rectangles of dots on the dot board (ui/dots.h, as an old LED panel, and the peg board of
// ../allflix-app, whose dot font Cubey will write with). Now and
// then they blink, glance aside and back, or squint, a dot a frame at a low rate; offline they stay shut.
namespace cubey::eyes {

constexpr int kWide = 5, kTall = 7; // an eye's dots
constexpr int kGap = 4;             // dots between the eyes
constexpr uint32_t kFrameMs = 70;   // a frame of their blinks and moves

// One eye's picture: a byte a row, its leftmost dot the bit kWide - 1.
using Frame = std::array<uint8_t, kTall>;

const Frame &open();
const Frame &shut();
const Frame &squinted(); // the lids down a little: content
const Frame &farther();  // a row smaller top and bottom: the far eye, looking well aside
// The blink's frame `ms` after it began, from open to shut and back; nullptr once it is over.
const Frame *blink(uint32_t ms);

// Where the eyes look, in dots from the middle (right and down are +), and whether they squint.
struct Look {
    int dx = 0, dy = 0;
    bool squint = false;
};

// Their idle life between blinks: after a rest of a few seconds, now and then a glance a dot or two aside (now and
// then a dot up too), held a moment and back, a dot a frame; or a squint for a moment; or nothing. pick(n): a random
// number below n.
class Wander {
public:
    using Pick = uint32_t (*)(uint32_t);
    Wander(Pick pick, uint32_t now);
    Look at(uint32_t now); // call every frame or so

private:
    Pick pick_;
    Look shown_, target_;
    uint32_t next_at_, back_at_ = 0, stepped_at_ = 0;
    bool holding_ = false;
};

// Where the left eye's top left goes on a board `dots` across and down: the two in the middle, a little above it.
struct Place {
    int left, top;
};
Place place_on(int dots);

// The board and the eyes over all of `parent`, open, blinking and glancing now and then.
lv_obj_t *create(lv_obj_t *parent);
// Shut (no server to talk to), or open again.
void set_shut(lv_obj_t *eyes, bool shut);

} // namespace cubey::eyes

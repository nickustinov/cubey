#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lvgl.h"
#include "services/lesson.h"

// Her word game's pictures (Puce's parrot's cards, Fluent Emoji 3D, from the server): cream cards over all of the black
// screen in place of Cubey's eyes, upright however the cube stands, as the dot board is (ui/dots.h). One big card, the
// step's picture; or two or three to tap, a tapped one popping and the others fading.
namespace cubey::cards {

// Where a card goes, upright as she sees it with the screen turned `quarters` clockwise: its middle, in the screen's
// pixels, and its side; of `count` cards (1 to 3: one big; two side by side; three, two above one).
struct Place {
    int32_t x, y, side;
};
Place place(int count, int index, int quarters);

// Over all of `parent`, hidden until it shows something. on_tap: she tapped a picture, its id; whether it counted.
lv_obj_t *create(lv_obj_t *parent, std::function<bool(const std::string &id)> on_tap);

void show(std::vector<uint8_t> png);             // one picture; none: hidden
void choose(std::vector<LessonPicture> pictures); // two or three to tap; fewer: hidden
void clear();                                     // hidden, its pictures freed
bool showing();
void set_turn(int quarters); // the cube turned: the cards go round with it

} // namespace cubey::cards

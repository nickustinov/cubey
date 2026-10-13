#pragma once

#include "lvgl.h"
#include "services/settings_menu.h"
#include "ui/fine.h"

// The grown-ups' menu (services/settings_menu.h) on the fine dot board (ui/fine.h), over everything, all tapped: three
// lines, each a word in small capitals saying what it is ("speaks": the language Cubey speaks; "teaches": the one its
// word game teaches; "theme": its screen's colours) over its setting in capitals, an arrow either side to step it back
// or on; under them its brightness, ten segments lit as far as it goes, a sun at their dim end, tapped or slid; OK at
// the bottom, on a dim button. (Its first, flipped with the side keys and saved with the power button, a parent found
// unnatural; its codes twice the size, and its words as tall, left no room, 2026-10-12.)
namespace cubey::menu {

// What a tap on the board's cell does: a step of a line, back (-1) or on (1); the brightness, to so many percent; OK.
struct Tap {
    enum class Does : uint8_t { Nothing, Step, Brightness, Ok } does = Does::Nothing;
    int line = 0, by = 0;
    int percent = 0;
};
Tap tap_at(int col, int row);

fine::Frame paint(const SettingsMenu &menu);
lv_obj_t *create(lv_obj_t *parent, SettingsMenu *menu);
void show(lv_obj_t *view, const SettingsMenu &menu); // only what changed drawn again

} // namespace cubey::menu

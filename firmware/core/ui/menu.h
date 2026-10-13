#pragma once

#include "lvgl.h"
#include "services/settings_menu.h"
#include "ui/fine.h"

// The grown-ups' menu (services/settings_menu.h) on the fine dot board (ui/fine.h), over everything, all tapped, in two
// pages with room between what is tapped: on the first, two lines, each a word in small capitals saying what it is
// ("speaks": the language Cubey speaks; "theme": its screen's colours) over its setting in capitals, an arrow either
// side to step it back or on; on the second, its brightness and its volume, each ten segments lit as far as it goes, a
// sun or a speaker at their quiet end, tapped or slid, and how the child talks to it ("talk": FREE, hands-free, or
// HOLD, holding its screen), a line like the first page's, tapped anywhere. Under them a dot for each page, an arrow towards the other: a
// tap there turns to it; OK at the bottom, on a dim button. (Its first, flipped with the side keys and saved with the
// power button, a parent found unnatural, 2026-10-12.) A new
// Cubey's, opened by itself after its Wi-Fi, has only the language it speaks, its code big under its word, filling the
// screen, and OK. The language its word game teaches, the child chooses by asking Cubey.
namespace cubey::menu {

// What a tap on the board's cell does: a step of a line, back (-1) or on (1); the brightness or the volume, to so many
// percent; how the child talks, the other way; the other page; OK.
struct Tap {
    enum class Does : uint8_t { Nothing, Step, Brightness, Volume, Hold, Page, Ok } does = Does::Nothing;
    int line = 0, by = 0;
    int percent = 0;
    int page = 0;
};
Tap tap_at(int col, int row, bool first, int page); // first: a new Cubey's; page: the one showing

fine::Frame paint(const SettingsMenu &menu);
lv_obj_t *create(lv_obj_t *parent, SettingsMenu *menu);
void show(lv_obj_t *view, const SettingsMenu &menu); // only what changed drawn again

} // namespace cubey::menu

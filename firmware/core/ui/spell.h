#pragma once

#include <cstdint>

#include "lvgl.h"
#include "services/spell.h"
#include "ui/letter.h"

// A word Cubey spells, on the dot board in place of its eyes (ui/letter.h, letter::spelling): its letters big, one by
// one as its voice says them (services/spell.h), then all of it passing.
namespace cubey::spell {

// Its moment: the letters shown so far, the last arriving (at least the first: shown only once it shows), or all of
// it passing.
letter::Frame frame(const letter::Shown &spelling, const SpellRun &run, uint32_t now);

lv_obj_t *create(lv_obj_t *parent, letter::Shown spelling, const SpellRun *run, uint32_t now);
void show(lv_obj_t *spell, uint32_t now); // redrawn only when it looks different

} // namespace cubey::spell

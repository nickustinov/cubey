#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "services/base64.h"

namespace cubey {

// Her word game's pictures come from the server with its messages (PNG bytes; the device carries none):
// server/src/lesson-game.ts.

// lesson.show: the step's picture, over all of the screen; none (or not a PNG): Cubey's eyes.
std::optional<std::vector<uint8_t>> lesson_show(const std::string &json);

// lesson.choose: two or three pictures to tap, each with the id that goes back when she taps it. Pictures without an
// id or a PNG are skipped; three at most.
struct LessonPicture {
    std::string id;
    std::vector<uint8_t> png;
};
std::optional<std::vector<LessonPicture>> lesson_choose(const std::string &json);

// A PNG's width and height, from its header; false for anything that is not a PNG.
bool png_size(const std::vector<uint8_t> &png, uint32_t &w, uint32_t &h);

} // namespace cubey

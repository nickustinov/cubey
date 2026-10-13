#pragma once

#include <cstdint>

namespace cubey {

// A finger on the screen, held to talk (services/voice.h): held kMs, it is held until it lets go; let go sooner, it
// was a tap, and means what a tap does (a picture in her word game: LVGL's long press is set to the same, so a tap is
// never a hold too).
class ScreenHold {
public:
    static constexpr uint32_t kMs = 300;
    enum class Event : uint8_t { None, Held, Let };

    Event follow(bool down, uint32_t now); // whether a finger is on the screen: call every step

private:
    bool down_ = false, held_ = false;
    uint32_t since_ = 0;
};

} // namespace cubey

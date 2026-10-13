#pragma once

#include <cstdint>

#include "services/ticks.h"

namespace cubey {

// The board's audio standby (from Lapsa): keep the codecs and the amplifier running through short gaps, then
// park them a while after the mic, the speech and the music have all stopped: longer than Cubey takes to answer once
// the mic rests (at a second its first words waited for them to settle, on most turns, 2026-10-11). After waking, the DAC's reference and
// the amplifier need a moment before they pass sound: speech written earlier is lost, so it waits in its
// buffer until they settle.
class AudioIdle {
public:
    static constexpr uint32_t kSettleMs = 500;
    static constexpr uint32_t kIdleMs = 3000;

    // Whether the audio should be awake now.
    bool update(uint32_t now, bool mic, bool speech, bool music) {
        if (mic || speech || music) {
            active_ = true;
            last_needed_ = now;
        } else if (active_ && since(now, last_needed_) >= kIdleMs) {
            active_ = false;
        }
        return active_;
    }
    void woke(uint32_t now) {
        awake_ = true;
        woke_at_ = now;
    }
    void parked() { awake_ = false; }
    bool awake() const { return awake_; }
    bool settled(uint32_t now) const { return awake_ && since(now, woke_at_) >= kSettleMs; }

private:
    bool active_ = false, awake_ = false;
    uint32_t last_needed_ = 0, woke_at_ = 0;
};

} // namespace cubey

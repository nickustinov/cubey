#pragma once

#include <cstddef>
#include <cstdint>

namespace cubey {

// PCM16 little-endian on the board and Mac. Storage belongs to the caller (PSRAM on the board).
// The caller serializes access. Copies use at most two spans, even when the ring wraps.
class SpeechBuffer {
public:
    SpeechBuffer(uint8_t *storage, size_t capacity, size_t prebuffer_bytes, uint32_t wait_ms)
        : storage_(storage), capacity_(capacity), prebuffer_(prebuffer_bytes), wait_ms_(wait_ms) {}
    size_t write(const uint8_t *data, size_t bytes, uint32_t now);
    size_t read(int16_t *out, size_t samples, uint32_t now);
    size_t samples() const { return count_ / 2; }
    void clear();

private:
    uint8_t *storage_;
    size_t capacity_, prebuffer_, head_ = 0, count_ = 0;
    uint32_t wait_ms_, queued_at_ = 0;
    bool primed_ = false;
};

} // namespace cubey

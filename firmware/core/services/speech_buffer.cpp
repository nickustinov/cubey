#include "services/speech_buffer.h"

#include <algorithm>
#include <cstring>

#include "services/ticks.h"

namespace cubey {

size_t SpeechBuffer::write(const uint8_t *data, size_t bytes, uint32_t now) {
    if (!bytes || !storage_ || bytes > capacity_ - count_) return 0;
    if (!count_) queued_at_ = now;
    const size_t tail = (head_ + count_) % capacity_;
    const size_t first = std::min(bytes, capacity_ - tail);
    std::memcpy(storage_ + tail, data, first);
    std::memcpy(storage_, data + first, bytes - first);
    count_ += bytes;
    return bytes;
}

size_t SpeechBuffer::read(int16_t *out, size_t samples, uint32_t now) {
    if (!samples || count_ < 2) return 0;
    if (!primed_ && count_ < prebuffer_ && since(now, queued_at_) < wait_ms_) return 0;
    primed_ = true;
    const size_t bytes = std::min(samples * 2, count_ & ~size_t(1));
    const size_t first = std::min(bytes, capacity_ - head_);
    std::memcpy(out, storage_ + head_, first);
    std::memcpy(reinterpret_cast<uint8_t *>(out) + first, storage_, bytes - first);
    head_ = (head_ + bytes) % capacity_;
    count_ -= bytes;
    if (count_ < 2) {
        primed_ = false;
        queued_at_ = now;
    }
    return bytes / 2;
}

void SpeechBuffer::clear() {
    head_ = count_ = 0;
    primed_ = false;
}

} // namespace cubey

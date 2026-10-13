#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

namespace cubey {

// WebSocket messages can span both transport chunks and continuation frames. Bounded so a stalled UI
// cannot consume the audio/display heap. On Overflow, the caller reconnects and clears the inbox.
class LinkInbox {
public:
    enum class Result { Accepted, Speech, Overflow };
    LinkInbox(size_t message_limit, size_t byte_limit, size_t count_limit)
        : message_limit_(message_limit), byte_limit_(byte_limit), count_limit_(count_limit) {}
    Result receive(uint8_t opcode, bool fin, size_t offset, size_t payload,
                   const char *data, size_t bytes);
    bool pop(std::string &out);
    void clear();

private:
    size_t message_limit_, byte_limit_, count_limit_, queued_bytes_ = 0;
    uint8_t kind_ = 0;
    std::deque<std::string> queue_;
    std::string partial_;
};

} // namespace cubey

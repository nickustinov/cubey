#include "services/link_inbox.h"

#include <utility>

namespace cubey {

LinkInbox::Result LinkInbox::receive(uint8_t opcode, bool fin, size_t offset, size_t payload,
                                     const char *data, size_t bytes) {
    if (opcode > 2) return Result::Accepted; // ping/pong/close don't change a fragmented message
    if (offset > payload || bytes > payload - offset) return Result::Overflow;
    if (offset == 0 && opcode != 0) {
        kind_ = opcode;
        partial_.clear();
    }
    const bool complete = fin && offset + bytes == payload;
    if (kind_ == 2) {
        if (complete) kind_ = 0;
        return Result::Speech;
    }
    if (kind_ != 1) return Result::Accepted;
    if (payload - offset > message_limit_ - partial_.size() ||
        payload - offset > byte_limit_ - queued_bytes_ - partial_.size() || queue_.size() >= count_limit_)
        return Result::Overflow;
    if (offset == 0) partial_.reserve(partial_.size() + payload);
    if (bytes) partial_.append(data, bytes);
    if (complete) {
        queued_bytes_ += partial_.size();
        queue_.push_back(std::move(partial_));
        partial_.clear();
        kind_ = 0;
    }
    return Result::Accepted;
}

bool LinkInbox::pop(std::string &out) {
    if (queue_.empty()) return false;
    queued_bytes_ -= queue_.front().size();
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void LinkInbox::clear() {
    queue_.clear();
    std::string{}.swap(partial_);
    queued_bytes_ = 0;
    kind_ = 0;
}

} // namespace cubey

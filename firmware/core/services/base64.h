#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace cubey {

// Standard base64 (with padding, no whitespace) to bytes; empty when the text is not base64.
std::vector<uint8_t> base64_decode(std::string_view text);

} // namespace cubey

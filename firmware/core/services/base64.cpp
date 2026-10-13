#include "services/base64.h"

namespace cubey {

static int value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> base64_decode(std::string_view text) {
    std::vector<uint8_t> out;
    if (text.size() % 4) return out;
    out.reserve(text.size() / 4 * 3);
    for (size_t i = 0; i < text.size(); i += 4) {
        const bool last = i + 4 == text.size();
        const int pad = last ? (text[i + 3] == '=') + (text[i + 2] == '=') : 0;
        if (pad == 1 && text[i + 2] == '=') return {}; // "x=y=" is not padding
        int v[4];
        for (int k = 0; k < 4; k++) {
            v[k] = k >= 4 - pad ? 0 : value(text[i + k]);
            if (v[k] < 0) return {};
        }
        const uint32_t n = (v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3];
        out.push_back(n >> 16);
        if (pad < 2) out.push_back((n >> 8) & 0xFF);
        if (pad < 1) out.push_back(n & 0xFF);
    }
    return out;
}

} // namespace cubey

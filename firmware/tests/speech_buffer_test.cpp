#include <doctest/doctest.h>

#include <array>
#include <deque>
#include <random>
#include "services/speech_buffer.h"

using cubey::SpeechBuffer;

TEST_CASE("speech waits for a cushion, but short phrases have bounded latency") {
    uint8_t storage[64]{};
    SpeechBuffer ring(storage, sizeof storage, 8, 120);
    const uint8_t pcm[] = {1, 0, 2, 0, 3, 0, 4, 0};
    int16_t out[4]{};
    CHECK(ring.write(pcm, 4, 100) == 4);
    CHECK(ring.samples() == 2);
    CHECK(ring.read(out, 4, 219) == 0);
    CHECK(ring.read(out, 4, 220) == 2);
    CHECK(out[0] == 1);
    CHECK(out[1] == 2);
    CHECK(ring.write(pcm, sizeof pcm, 300) == sizeof pcm);
    CHECK(ring.read(out, 4, 300) == 4);
    CHECK(out[3] == 4);
}

TEST_CASE("speech refill batches packets after a gap, including across clock wrap") {
    uint8_t storage[64]{};
    SpeechBuffer ring(storage, sizeof storage, 8, 120);
    const uint8_t pcm[] = {0, 128, 255, 127, 255, 255, 0, 0};
    int16_t out[4]{};
    ring.write(pcm, 8, 0);
    CHECK(ring.read(out, 4, 0) == 4);
    CHECK(out[0] == -32768);
    CHECK(out[1] == 32767);
    CHECK(out[2] == -1);
    ring.write(pcm, 2, UINT32_MAX - 50);
    CHECK(ring.read(out, 4, 20) == 0);
    CHECK(ring.read(out, 4, 70) == 1);
    ring.clear();
    CHECK(ring.samples() == 0);
    ring.write(pcm, 8, 80);
    CHECK(ring.read(out, 4, 80) == 4);
}

TEST_CASE("speech ring preserves PCM under repeated odd packets and wrapping") {
    uint8_t storage[62]{};
    SpeechBuffer ring(storage, sizeof storage, 2, 0);
    std::deque<uint8_t> expected;
    std::mt19937 rng(17);
    for (int turn = 0; turn < 10000; ++turn) {
        std::array<uint8_t, 13> bytes{};
        for (auto &b : bytes) b = static_cast<uint8_t>(rng());
        const size_t n = 1 + rng() % bytes.size();
        // Overflow rejects the complete packet, preserving sample alignment.
        const size_t accepted = ring.write(bytes.data(), n, turn);
        if (expected.size() + n > sizeof storage) CHECK(accepted == 0);
        else CHECK(accepted == n);
        expected.insert(expected.end(), bytes.begin(), bytes.begin() + accepted);
        int16_t out[7];
        const size_t got = ring.read(out, rng() % 8, turn);
        for (size_t i = 0; i < got; ++i) {
            REQUIRE(expected.size() >= 2);
            const uint8_t lo = expected.front(); expected.pop_front();
            const uint8_t hi = expected.front(); expected.pop_front();
            CHECK(out[i] == static_cast<int16_t>(lo | (uint16_t(hi) << 8)));
        }
        CHECK(ring.samples() == expected.size() / 2);
    }
}

TEST_CASE("cancelling speech also discards a partial sample") {
    uint8_t storage[16]{};
    SpeechBuffer ring(storage, sizeof storage, 2, 0);
    const uint8_t pcm[] = {42, 0};
    ring.write(pcm, 1, 0);
    ring.clear();
    ring.write(pcm, 2, 0);
    int16_t out = 0;
    CHECK(ring.read(&out, 1, 0) == 1);
    CHECK(out == 42);
}

TEST_CASE("speech absorbs packet jitter across 33 minutes of simulated playback") {
    std::array<uint8_t, 6400> storage{}; // deliberately small, so the ring wraps constantly
    SpeechBuffer ring(storage.data(), storage.size(), 3840, 120);
    std::array<int16_t, 320> in{}, out{};
    uint32_t sent = 0, played = 0;
    // 100,000 frames = 33 minutes. Every fifth packet is delayed by one frame (40 ms bursts).
    for (uint32_t frame = 0; frame < 100000; ++frame) {
        const int packets = frame % 5 == 3 ? 0 : frame % 5 == 4 ? 2 : 1;
        for (int p = 0; p < packets; ++p) {
            for (auto &sample : in) sample = static_cast<int16_t>(sent++);
            REQUIRE(ring.write(reinterpret_cast<const uint8_t *>(in.data()), sizeof in, frame * 20) == sizeof in);
        }
        const size_t got = ring.read(out.data(), out.size(), frame * 20);
        if (frame >= 5) REQUIRE(got == out.size());
        for (size_t i = 0; i < got; ++i) {
            if (out[i] != static_cast<int16_t>(played++)) FAIL("speech reordered or dropped a sample");
        }
    }
    CHECK(sent - played == ring.samples());
}

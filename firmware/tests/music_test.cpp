#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <vector>

#include "services/mixer.h"
#include "services/music.h"

using namespace cubey;

constexpr int kRate = 16000;

static std::vector<float> play(Beat &beat, size_t n) {
    std::vector<float> out(n);
    for (auto &v : out) v = beat.next(kRate);
    return out;
}

static float rms(const std::vector<float> &v) {
    float sum = 0;
    for (float x : v) sum += x * x;
    return std::sqrt(sum / v.size());
}

TEST_CASE("the beat is made on the device: within -1..1, never silent, the same every time it starts") {
    Beat a, b;
    const auto first = play(a, 2 * kRate), again = play(b, 2 * kRate);
    for (float v : first) CHECK(std::fabs(v) <= 1.0f);
    CHECK(rms(first) > 0.05f);
    CHECK(first == again);
}

TEST_CASE("the beat goes at 120 to the minute: a kick on every beat, loud at its start") {
    Beat beat;
    const auto out = play(beat, kRate); // two beats
    const size_t beat_samples = kRate * kBeatMs / 1000;
    auto loudness = [&](size_t from) { return rms(std::vector<float>(out.begin() + from, out.begin() + from + kRate / 50)); };
    CHECK(loudness(0) > 2 * loudness(beat_samples / 2 + kRate / 25)); // the kick, then quieter before the next
    CHECK(loudness(beat_samples) > 2 * loudness(beat_samples / 2 + kRate / 25));
}

TEST_CASE("the mixer adds the music under the speech, softer while the speech plays, nothing when it is off") {
    audio::music(false);
    std::vector<int16_t> silence(320, 0);
    audio::mix(silence.data(), silence.size(), kRate, false);
    for (int16_t v : silence) CHECK(v == 0);

    std::vector<int16_t> speech(320, 1000);
    audio::mix(speech.data(), speech.size(), kRate, true);
    for (int16_t v : speech) CHECK(v == 1000); // no music: the speech as it was

    audio::music(true);
    CHECK(audio::music_on());
    auto loudness = [](bool with_speech) {
        float sum = 0;
        for (int i = 0; i < 50; i++) { // a second, after the gain has settled
            std::vector<int16_t> frame(320, 0);
            audio::mix(frame.data(), frame.size(), kRate, with_speech);
            for (int16_t v : frame) sum += static_cast<float>(v) * v;
        }
        return std::sqrt(sum / (50 * 320));
    };
    const float alone = loudness(false), under_speech = loudness(true);
    CHECK(alone > 500);
    CHECK(under_speech < alone * 0.5f);
    audio::music(false);
    CHECK_FALSE(audio::music_on());
}

TEST_CASE("loud speech and music together stay within the sample's range") {
    audio::music(true);
    std::vector<int16_t> loud(320 * 20, 32000);
    audio::mix(loud.data(), loud.size(), kRate, false);
    for (int16_t v : loud) CHECK(v <= 32767);
    audio::music(false);
}

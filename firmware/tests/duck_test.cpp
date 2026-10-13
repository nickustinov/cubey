#include <doctest/doctest.h>

#include "services/duck.h"

using namespace cubey;

TEST_CASE("the speech ducks at once while she talks over it, and comes back up over a third of a second") {
    CHECK(duck_gain(1, true) == doctest::Approx(kDuckGain));
    float gain = kDuckGain;
    int frames = 0;
    while (gain < 1 && frames < 100) gain = duck_gain(gain, false), frames++;
    CHECK(gain == 1);
    CHECK(frames >= 15);
    CHECK(frames <= 17); // 20 ms speaker frames
    CHECK(duck_gain(1, false) == 1);
    CHECK(duck_gain(0.5f, true) == doctest::Approx(kDuckGain));
}

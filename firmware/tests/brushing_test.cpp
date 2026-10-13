#include <doctest/doctest.h>

#include "services/brushing.h"

using namespace cubey;

TEST_CASE("two minutes in four parts of 30 s, then done") {
    CHECK(brushing_at(0).part == 0);
    CHECK(brushing_at(29999).part == 0);
    CHECK(brushing_at(30000).part == 1);
    CHECK(brushing_at(45000).part_progress == doctest::Approx(0.5f));
    CHECK(brushing_at(119999).part == 3);
    CHECK_FALSE(brushing_at(119999).done);
    CHECK(brushing_at(kBrushingMs).done);
}

TEST_CASE("the two minutes start at her tap, and once") {
    BrushingRun run(1000);
    CHECK_FALSE(run.started());
    CHECK(run.elapsed(5000) == 0);
    CHECK(run.start(5000));
    CHECK_FALSE(run.start(6000));
    CHECK(run.elapsed(35000) == 30000);
}

TEST_CASE("the server hears of each part as it begins, her tap the first, and of the end, each once") {
    BrushingRun run(0);
    CHECK(run.news(500) == 0); // waiting for her tap
    run.start(1000);
    CHECK(run.news(1000) == 1);
    CHECK(run.news(1100) == 0);
    CHECK(run.news(31000) == 2);
    CHECK(run.news(31100) == 0);
    CHECK(run.news(91000) == 4); // a late step: only the part it is in
    CHECK(run.news(121000) == BrushingRun::kDone);
    CHECK(run.news(122000) == 0);
}

TEST_CASE("with no tap for a minute it is off, and the server hears so; once done, it is over a while after") {
    BrushingRun waiting(0);
    CHECK(waiting.news(BrushingRun::kReadyMostMs - 1) == 0);
    CHECK_FALSE(waiting.over(BrushingRun::kReadyMostMs - 1));
    CHECK(waiting.news(BrushingRun::kReadyMostMs) == BrushingRun::kMissed);
    CHECK(waiting.over(BrushingRun::kReadyMostMs));

    BrushingRun done(0);
    done.start(0);
    for (uint32_t t = 0; t <= kBrushingMs; t += 1000) done.news(t);
    CHECK_FALSE(done.over(kBrushingMs + BrushingRun::kShineMs - 1));
    CHECK(done.over(kBrushingMs + BrushingRun::kShineMs));
}

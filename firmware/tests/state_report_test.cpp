#include <doctest/doctest.h>

#include "services/state_report.h"

using namespace cubey;

TEST_CASE("Cubey tells the server its battery when it changes, and again on each new connection") {
    StateReport report;
    CHECK(report.next({73, true}, 1) == std::optional<std::string>(R"({"type":"state","battery":73,"charging":true})"));
    CHECK_FALSE(report.next({73, true}, 1));
    CHECK(report.next({72, true}, 1));
    CHECK(report.next({72, false}, 1) == std::optional<std::string>(R"({"type":"state","battery":72,"charging":false})"));
    CHECK_FALSE(report.next({72, false}, 1));
    CHECK(report.next({72, false}, 2)); // the server has forgotten
}

TEST_CASE("a battery it can't read is told as none") {
    StateReport report;
    CHECK(report.next({}, 1) == std::optional<std::string>(R"({"type":"state"})"));
}

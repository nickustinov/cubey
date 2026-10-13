#pragma once

#include <optional>
#include <string>

#include "services/battery.h"

namespace cubey {

// What Cubey tells the server about itself: its battery, when it changes and on each new connection (the server has
// forgotten), for Cubey to say when she asks.
class StateReport {
public:
    std::optional<std::string> next(Battery battery, unsigned session);

private:
    std::string last_;
    unsigned session_ = 0;
};

} // namespace cubey

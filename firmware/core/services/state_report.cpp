#include "services/state_report.h"

#include <cstdio>

namespace cubey {

std::optional<std::string> StateReport::next(Battery battery, unsigned session) {
    char json[80];
    if (battery.percent < 0) snprintf(json, sizeof json, R"({"type":"state"})");
    else snprintf(json, sizeof json, R"({"type":"state","battery":%d,"charging":%s})", battery.percent, battery.charging ? "true" : "false");
    if (json == last_ && session == session_) return std::nullopt;
    last_ = json;
    session_ = session;
    return last_;
}

} // namespace cubey

#include "services/settings_menu.h"

#include <algorithm>

namespace cubey {

void SettingsMenu::known(std::vector<std::string> languages, Settings settings, bool fresh) {
    languages_ = std::move(languages);
    known_ = std::move(settings);
    fresh_ = fresh;
    if (!open_) shown_ = known_;
}

SettingsMenu::Event SettingsMenu::keys(const Keys &keys, uint32_t now) {
    Event event = Event::None;
    const bool alone = keys.plus != keys.minus;
    if (alone && (keys.plus != plus_ || keys.minus != minus_)) held_since_ = now, fired_ = false; // a hold begins
    if (alone && !fired_ && now - held_since_ >= kHoldMs) {
        fired_ = true;
        if (keys.minus) event = open_ ? close() : open(now);
        else if (!open_) event = Event::Wifi;
    }
    plus_ = keys.plus, minus_ = keys.minus;
    if (open_ && !keys.plus && !keys.minus && now - last_ > kIdleMs) event = close();
    return event;
}

SettingsMenu::Event SettingsMenu::offer(uint32_t now) {
    if (!fresh_ || offered_ || open_) return Event::None;
    offered_ = true;
    return open(now);
}

SettingsMenu::Event SettingsMenu::open(uint32_t now) {
    if (languages_.empty() || open_) return Event::None;
    open_ = true, last_ = now;
    return Event::Opened;
}

void SettingsMenu::step(int line, int by, uint32_t now) {
    const std::vector<std::string> &all = line == 2 ? themes_ : languages_;
    if (!open_ || line < 0 || line > 2 || all.empty()) return;
    last_ = now;
    std::string *lines[3] = {&shown_.language, &shown_.learns, &shown_.theme};
    const int n = static_cast<int>(all.size());
    auto at = std::find(all.begin(), all.end(), *lines[line]);
    int i = at == all.end() ? 0 : static_cast<int>(at - all.begin());
    do i = (i + by + n) % n;
    while (line < 2 && all[i] == *lines[1 - line] && n > 1);
    *lines[line] = all[i];
}

void SettingsMenu::brighten(int percent, uint32_t now) {
    if (!open_) return;
    last_ = now;
    shown_.brightness = std::clamp((percent + 5) / 10 * 10, 10, 100);
}

SettingsMenu::Event SettingsMenu::close() {
    if (!open_) return Event::None;
    open_ = false;
    return Event::Closed;
}

} // namespace cubey

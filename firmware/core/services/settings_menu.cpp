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
    return opening(now, true);
}

SettingsMenu::Event SettingsMenu::open(uint32_t now) { return opening(now, false); }

SettingsMenu::Event SettingsMenu::opening(uint32_t now, bool first) {
    if (languages_.empty() || open_) return Event::None;
    open_ = true, first_ = first, last_ = now, page_ = 0;
    return Event::Opened;
}

void SettingsMenu::step(int line, int by, uint32_t now) {
    const std::vector<std::string> &all = line == 1 ? themes_ : languages_;
    if (!open_ || line < 0 || line > 1 || all.empty()) return;
    last_ = now;
    std::string &shown = line == 0 ? shown_.language : shown_.theme;
    const int n = static_cast<int>(all.size());
    const auto at = std::find(all.begin(), all.end(), shown);
    const int i = at == all.end() ? 0 : static_cast<int>(at - all.begin());
    shown = all[(i + by + n) % n];
}

void SettingsMenu::brighten(int percent, uint32_t now) {
    if (!open_) return;
    last_ = now;
    shown_.brightness = std::clamp((percent + 5) / 10 * 10, 10, 100);
}

void SettingsMenu::set_volume(int percent, uint32_t now) {
    if (!open_) return;
    last_ = now;
    shown_.volume = std::clamp((percent + 5) / 10 * 10, 10, 100);
}

void SettingsMenu::set_hold(bool hold, uint32_t now) {
    if (!open_) return;
    last_ = now;
    shown_.hold = hold;
}

void SettingsMenu::to_page(int page, uint32_t now) {
    if (!open_ || page < 0 || page > 1) return;
    last_ = now;
    page_ = page;
}

SettingsMenu::Event SettingsMenu::close() {
    if (!open_) return Event::None;
    open_ = false;
    return Event::Closed;
}

} // namespace cubey

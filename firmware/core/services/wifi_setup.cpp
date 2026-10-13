#include "services/wifi_setup.h"

#include <algorithm>

#include "hal/hal.h"

namespace cubey {

void WifiSetup::open(bool welcome, uint32_t now) {
    networks_.clear();
    top_ = 0;
    scanning_ = true;
    hal::wifi_scan();
    to(welcome ? Screen::Welcome : Screen::Networks, now);
    touched_ = now;
}

void WifiSetup::back() {
    if (screen_ == Screen::Password) screen_ = Screen::Networks;
    else close();
}

bool WifiSetup::follow(uint32_t now) {
    if (!is_open()) return false;
    if (now - touched_ >= kIdleMs && screen_ != Screen::Joining) return close(), true;
    bool changed = false;
    if (scanning_ && hal::wifi_networks(networks_)) scanning_ = false, changed = true;
    const uint32_t on = now - since_;
    switch (screen_) {
    case Screen::Joining: {
        const Wifi state = hal::wifi();
        if (state == Wifi::Joined) to(Screen::Joined, now), changed = true;
        else if (state == Wifi::Refused || state == Wifi::NotFound || state == Wifi::Failed || on >= kJoinMostMs) why_ = state, to(Screen::Failed, now), changed = true;
        break;
    }
    case Screen::Joined:
        if (on >= kDoneMs) close(), changed = true;
        break;
    case Screen::Failed:
        if (on >= kDoneMs) to(Screen::Password, now), changed = true;
        break;
    default: break;
    }
    return changed;
}

void WifiSetup::tap(uint32_t now) {
    if (screen_ == Screen::Welcome && tapped(now)) to(Screen::Networks, now);
}

void WifiSetup::scroll(int by, uint32_t now) {
    touched_ = now;
    top_ = std::clamp(top_ + by, 0, std::max(0, static_cast<int>(networks_.size()) - kShown));
}

bool WifiSetup::tapped(uint32_t now) {
    touched_ = now;
    if (tapped_ && now - tapped_ < kBounceMs) return false;
    tapped_ = now ? now : 1;
    return true;
}

void WifiSetup::pick(int shown, uint32_t now) {
    if (!tapped(now)) return;
    const int at = top_ + shown;
    if (screen_ != Screen::Networks || shown < 0 || shown >= kShown || at >= static_cast<int>(networks_.size())) return;
    ssid_ = networks_[at].ssid;
    password_.clear();
    page_ = keyboard::Page::Small;
    if (networks_[at].open) return join(now);
    to(Screen::Password, now);
}

void WifiSetup::press(int col, int row, uint32_t now) {
    if (!tapped(now) || screen_ != Screen::Password) return;
    using keyboard::Does, keyboard::Page;
    const keyboard::Key key = keyboard::key(page_, col, row);
    switch (key.does) {
    case Does::Type:
        if (password_.size() < kMost) password_ += key.text;
        if (page_ == Page::Capitals) page_ = Page::Small; // shift is for one letter
        break;
    case Does::Shift: page_ = page_ == Page::Capitals ? Page::Small : Page::Capitals; break;
    case Does::Symbols: page_ = page_ == Page::Symbols ? Page::Small : Page::Symbols; break;
    case Does::Back:
        if (!password_.empty()) password_.pop_back(); // the keyboard's are all ASCII
        break;
    case Does::Join: join(now); break;
    case Does::Nothing: break;
    }
}

void WifiSetup::join(uint32_t now) {
    hal::wifi_join(ssid_, password_);
    to(Screen::Joining, now);
}

} // namespace cubey

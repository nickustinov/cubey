#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace cubey {

// What a Cubey runs, as its hello tells the server: its version (git describe), its build (the start of its ELF's
// hash: unique to each build) and the build of an update it rolled back from (none: ""). A platform without updates
// (the simulator) has no build.
struct Firmware {
    std::string version, build, failed;
};

// An update as the platform fetches it into its other slot: not begun, fetching (how far), done (it restarts into
// it), or failed.
enum class UpdateState : uint8_t { None, Fetching, Done, Failed };
struct UpdateProgress {
    UpdateState state = UpdateState::None;
    int percent = 0;
};

// An update the server offers (session.ts): it waits until Cubey is free (awake, nobody talking, no routine), then
// starts (the Happy Mac shows, ui/happy_mac.h, and the platform fetches it); done, a moment to see it, then the
// restart into it; failed, a moment, then Cubey as it was, and that build is not tried again until it restarts.
class Updater {
public:
    static constexpr uint32_t kDoneMs = 2500;   // it winks, before the restart
    static constexpr uint32_t kFailedMs = 5000; // it is sad, before Cubey's eyes again
    enum class Do : uint8_t { Nothing, Start, Restart, End };

    void offer(const std::string &build);
    bool waiting() const { return !build_.empty() && !started_; } // no talk opens meanwhile
    bool running() const { return started_; }
    const std::string &build() const { return build_; }
    const std::string &failed() const { return failed_; } // the build of the last that failed
    // free: Cubey is free for it; progress: the platform's; call every step
    Do step(bool free, const UpdateProgress &progress, uint32_t now);

private:
    std::string build_, failed_;
    bool started_ = false, restarted_ = false;
    std::optional<uint32_t> ended_at_; // done or failed, since
};

} // namespace cubey

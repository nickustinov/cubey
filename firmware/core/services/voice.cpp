#include "services/voice.h"

#include <cstdio>
#include <string>

#include "hal/hal.h"
#include "services/message.h"
#include "services/ticks.h"

namespace cubey {

constexpr uint32_t kAnswerTimeoutMs = 15000;  // the server never started speaking
constexpr uint32_t kQuietCloseMs = 30000;     // nobody spoke: the talk ends, and Cubey sleeps
constexpr uint32_t kHeldQuietCloseMs = 60000; // ...held to talk, nobody held its screen
constexpr uint32_t kRetryMs = 5000;           // after a failed turn, before listening again
constexpr unsigned kMessagesPerTick = 4;      // leaves time for drawing between them

bool Voice::open(uint32_t now, bool again, const std::string &with) {
    if (!hal::link_connected() || open_) return false;
    hal::link_send(std::string(R"({"type":"open")") + (again ? R"(,"again":true)" : "") + (with.empty() ? "" : "," + with) + "}");
    open_ = true;
    ended_ = TalkEnd::None;
    state_ = VoiceState::Thinking; // the hello is on its way
    done_ = last_ = false;
    pause_ms_ = 0;
    asked_at_ = quiet_since_ = now;
    return true;
}

void Voice::close() {
    if (open_ && hal::link_connected()) hal::link_send(R"({"type":"close"})");
    end(TalkEnd::None);
}

void Voice::end(TalkEnd why) {
    hal::mic_stream(false);
    hal::speaker_stop();
    state_ = VoiceState::Idle;
    open_ = false;
    ended_ = why;
    dropping_ = false;
    tap_.clear();
}

void Voice::listen() {
    hal::link_send(hold_ ? R"({"type":"listen","hold":true})" : R"({"type":"listen"})");
    hal::mic_stream(true);
    state_ = VoiceState::Listening;
    if (!tap_.empty()) hal::link_send(tapped_message(tap_));
    tap_.clear();
}

bool Voice::tap(const std::string &id) {
    if (!open_ || state_ == VoiceState::Thinking) return false;
    if (state_ == VoiceState::Listening) hal::link_send(tapped_message(id));
    else tap_ = id;
    return true;
}

bool Voice::press(uint32_t now) {
    if (!open_ || !hold_ || state_ == VoiceState::Listening) return false;
    if (state_ != VoiceState::Idle) { // what Cubey says, or is about to: dropped for her turn
        hal::speaker_stop();
        dropping_ = true;
        done_ = last_ = false; // an interrupted goodbye is no goodbye
    }
    quiet_since_ = now;
    listen();
    return true;
}

void Voice::release(uint32_t now) {
    if (!open_ || !hold_ || state_ != VoiceState::Listening) return;
    hal::link_send(R"({"type":"release"})");
    hal::mic_stream(false);
    state_ = VoiceState::Thinking;
    asked_at_ = quiet_since_ = now;
}

void Voice::rest(uint32_t now, uint32_t pause_ms) {
    state_ = VoiceState::Idle;
    idle_since_ = now;
    pause_ms_ = pause_ms;
}

void Voice::tick(uint32_t now, const std::function<void(const ServerMessage &, const std::string &)> &other) {
    if (!hal::link_connected()) {
        if (open_) end(TalkEnd::Lost); // the server has lost the talk
        return;
    }
    if (greeted_ != hal::link_session()) {
        if (open_) end(TalkEnd::Lost); // a new connection: the server has forgotten the talk
        char hello[64];
        snprintf(hello, sizeof hello, R"({"type":"hello","speaker_rate":%d})", hal::speaker_rate());
        hal::link_send(hello);
        greeted_ = hal::link_session();
    }

    std::string json;
    for (unsigned i = 0; i < kMessagesPerTick && hal::link_receive(json); i++) {
        const ServerMessage msg = parse_message(json);
        if (msg.type == "talk_over") {
            talk_over_ = msg.on;
            continue;
        }
        const bool talks = msg.type == "heard" || msg.type == "said" || msg.type == "failed" || msg.type == "listening" ||
                           msg.type == "interrupted";
        if (!talks) {
            if (other) other(msg, json);
            continue;
        }
        if (!open_) continue; // what was on its way for a talk that has ended
        if (msg.type == "listening" && dropping_) {
            hal::speaker_stop(); // all of the speech before it was the answer she held its screen over
            dropping_ = false;
        } else if (msg.type == "interrupted" && (state_ == VoiceState::Thinking || state_ == VoiceState::Speaking)) {
            // She talked over it: all of its speech came before, and the server listens to her already.
            hal::speaker_stop();
            done_ = last_ = false; // an interrupted goodbye is no goodbye
            quiet_since_ = now;
            hal::mic_stream(true);
            state_ = VoiceState::Listening;
        } else if (msg.type == "heard" && state_ == VoiceState::Listening) {
            hal::mic_stream(false);
            state_ = VoiceState::Thinking;
            asked_at_ = quiet_since_ = now;
        } else if (msg.type == "said") {
            done_ = true;
            last_ = msg.last;
            pause_ms_ = 0;
        } else if (msg.type == "failed") {
            done_ = true;
            last_ = false;
            pause_ms_ = kRetryMs;
        }
    }
    if (!open_) return;
    if (dropping_) hal::speaker_stop();

    const bool busy = hal::speaker_busy();
    const bool over = talk_over_ && !hold_ && hal::hears_over_speech(); // she may talk over it (held: she holds its screen)
    if (state_ == VoiceState::Thinking && busy) {
        state_ = VoiceState::Speaking;
        if (over) { // the server listens
            hal::link_send(R"({"type":"overhear"})");
            hal::mic_stream(true);
            hal::voice_over_speech(); // from before it spoke
            voiced_ = false;
        }
    }
    // Her voice over its speech, as the platform's detector hears it: the server stops it and hears her turn.
    if (state_ == VoiceState::Speaking && over && hal::voice_over_speech() && !voiced_) {
        hal::link_send(R"({"type":"voice"})");
        voiced_ = true;
    }
    if (state_ == VoiceState::Speaking && busy) asked_at_ = now;
    const bool answering = state_ == VoiceState::Thinking || state_ == VoiceState::Speaking;
    if (answering && done_ && !busy) {
        done_ = false;
        quiet_since_ = now;
        if (last_) { // the goodbye has played
            hal::mic_stream(false);
            state_ = VoiceState::Idle;
            open_ = false;
            ended_ = TalkEnd::Goodbye;
            return;
        }
        rest(now, pause_ms_);
    } else if (answering && !busy && since(now, asked_at_) > kAnswerTimeoutMs) {
        hal::link_send(R"({"type":"stop"})");
        rest(now, 0);
    }
    const bool waits = state_ == VoiceState::Idle || (state_ == VoiceState::Listening && !hold_); // held: she talks
    if (waits && since(now, quiet_since_) > (hold_ ? kHeldQuietCloseMs : kQuietCloseMs)) {
        close();
        ended_ = TalkEnd::Quiet;
        return;
    }
    if (state_ != VoiceState::Idle) return;
    if (!hold_) {
        if (since(now, idle_since_) >= pause_ms_) listen();
    } else if (!tap_.empty()) { // held to talk, her tap is her turn
        hal::link_send(tapped_message(tap_));
        tap_.clear();
        state_ = VoiceState::Thinking;
        asked_at_ = quiet_since_ = now;
    }
}

} // namespace cubey

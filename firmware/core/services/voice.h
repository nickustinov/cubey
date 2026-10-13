#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "services/message.h"

namespace cubey {

enum class VoiceState : uint8_t { Idle, Listening, Thinking, Speaking };
// How the last talk ended: not yet (or closed here), its goodbye played, a quiet stretch, or the server gone.
enum class TalkEnd : uint8_t { None, Goodbye, Quiet, Lost };

// The talk with Cubey on the server (protocol in server/src/session.ts). Runs on the UI thread; the platform streams
// the mic and plays the speech. Opened, Cubey says hello; then, hands-free, the device listens, the server ends each
// turn when the child pauses, Cubey answers and the device listens again, until the server says the answer was its
// goodbye. Half a quiet minute, or losing the server, end the talk too (ended()). The mic rests while Cubey thinks;
// while it speaks it streams where the platform cancels its echo, the platform's detector says when she talks over it
// ("voice"), and the server stops it and hears her ("interrupted").
// Held to talk (its menu's), the device waits for her instead: it listens while she holds its screen, stopping Cubey
// if it speaks, and her turn is over when she lets go; a quiet minute ends the talk.
class Voice {
public:
    // false without a server, or while a talk is open; again: the server lost the talk (a new connection), and it
    // goes on without a hello; with: what happened instead, as JSON fields (a timer's end)
    bool open(uint32_t now, bool again = false, const std::string &with = "");
    void close(); // the talk is over here: Cubey sleeps
    // Reads the server's messages and follows the speaker; call every frame or so. Messages that aren't the talk's
    // (show the battery, a picture) go to `other` as they came, talk or no talk.
    void tick(uint32_t now, const std::function<void(const ServerMessage &, const std::string &json)> &other = {});

    // She tapped a picture (her word game's): her answer while listening, kept while the question is still being
    // asked; false while Cubey thinks (the pictures on show belong to the last question) or with no talk.
    bool tap(const std::string &id);

    // Something goes on (she brushes): nobody speaking is no quiet while to close the talk for.
    void stay(uint32_t now) { quiet_since_ = now; }

    // Held to talk, or hands-free (its menu's).
    void set_hold(bool hold) { hold_ = hold; }
    bool holds() const { return hold_; }
    // Held to talk: she holds its screen, and it listens (Cubey's speech, if any, stopped); false with no talk, while
    // listening already, or hands-free. She lets go: her turn is over.
    bool press(uint32_t now);
    void release(uint32_t now);

    // Her turn: it listens to her, or held to talk, waits for her to hold its screen.
    bool her_turn() const { return open_ && (state_ == VoiceState::Listening || (hold_ && state_ == VoiceState::Idle)); }
    bool is_open() const { return open_; }
    VoiceState state() const { return state_; }
    TalkEnd ended() const { return ended_; }

private:
    void end(TalkEnd why); // the mic off, the speech stopped, the talk over; nothing sent
    void listen();
    void rest(uint32_t now, uint32_t pause_ms);

    VoiceState state_ = VoiceState::Idle;
    bool open_ = false;
    unsigned greeted_ = 0;     // the connection the hello went to; each new one gets its own
    bool done_ = false;        // the server has said all of the answer (or failed)
    bool last_ = false;        // ...and it was the goodbye
    TalkEnd ended_ = TalkEnd::None;
    bool voiced_ = false;      // the server has heard she talks over this answer
    bool talk_over_ = true;    // she may talk over Cubey, as the server says on each connection (CUBEY_TALK_OVER)
    bool hold_ = false;        // she holds its screen to talk
    bool dropping_ = false;    // held as Cubey spoke: its speech still on its way is dropped, until the server listens
    uint32_t asked_at_ = 0;    // speech should follow: the talk opened, she paused, or speech was still playing
    uint32_t quiet_since_ = 0; // nobody has spoken since
    uint32_t idle_since_ = 0;
    uint32_t pause_ms_ = 0;    // wait this long in Idle before listening again
    std::string tap_;          // a tap kept until listening starts
};

} // namespace cubey

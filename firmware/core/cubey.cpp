#include "cubey.h"

#include <cstdio>
#include <optional>
#include <string>

#include "hal/hal.h"
#include "lvgl.h"
#include "services/brushing.h"
#include "services/dice.h"
#include "services/settings_menu.h"
#include "services/wifi_setup.h"
#include "services/lesson.h"
#include "services/mixer.h"
#include "services/power_button.h"
#include "services/shake.h"
#include "services/spell.h"
#include "services/state_report.h"
#include "services/timer.h"
#include "services/ticks.h"
#include "services/upright.h"
#include "services/voice.h"
#include "ui/battery.h"
#include "ui/cards.h"
#include "ui/count.h"
#include "ui/dial.h"
#include "ui/die.h"
#include "ui/dots.h"
#include "ui/eyes.h"
#include "ui/fine.h"
#include "ui/letter.h"
#include "ui/menu.h"
#include "ui/mouth.h"
#include "ui/picture.h"
#include "ui/spell.h"
#include "ui/wifi.h"

namespace cubey {

static Voice voice;
static PowerButton power;
static lv_obj_t *eyes_shown = nullptr; // awake, the eyes on screen
static bool again = false;             // the talk was lost with the server: it goes on once it's back, without a hello
static lv_timer_t *stepping = nullptr; // step(): often awake, seldom asleep
constexpr uint32_t kStepMs = 30, kStepAsleepMs = 200;
static StateReport state_report;
static lv_obj_t *battery_shown = nullptr; // over the eyes while Cubey tells its battery
static uint32_t battery_since = 0;
constexpr uint32_t kBatteryShowMs = 6000; // ponytail: about its answer's length; until it has played, if they run long
static std::optional<BrushingRun> brushing; // brushing her teeth, Cubey's mouth in place of its eyes
static lv_obj_t *mouth_shown = nullptr;
static std::optional<TimerRun> timer; // a timer, its dial in place of Cubey's eyes, no talk while it runs
static std::string timer_for;         // what for, as the server named it
static lv_obj_t *dial_shown = nullptr;
static std::string open_with; // what happened, for the talk that opens next to begin with (a timer's end)
static lv_obj_t *cards_shown = nullptr; // her word game: its pictures, over Cubey's eyes when there are any
static lv_obj_t *die_shown = nullptr;   // the die she rolls, in place of Cubey's eyes; no talk while it is out
static lv_obj_t *letter_shown = nullptr; // a game with letters: the letter she needs, big, in place of Cubey's eyes
static lv_obj_t *count_shown = nullptr;  // how many dots: the dots she counts, in place of Cubey's eyes
static lv_obj_t *screen_shown = nullptr; // a picture Cubey drew or text it writes, in place of its eyes for a while
static bool screen_moves = false;        // text scrolling past
static uint32_t screen_since = 0, screen_ms = 0;
constexpr uint32_t kScreenMs = 8000; // ponytail: a few seconds, about its answer's length
static std::optional<SpellRun> spelling; // a word Cubey spells, its letters as its voice says them
static lv_obj_t *spell_shown = nullptr;  // in place of its eyes from its first letter, like a drawing
static SettingsMenu settings_menu{dots::themes()}; // the grown-ups' menu: the languages and theme, as the server has them
static lv_obj_t *menu_shown = nullptr; // over everything while it is open
static WifiSetup wifi_setup;            // a grown-up joining Cubey to a network
static lv_obj_t *wifi_shown = nullptr;  // its screens, over everything
static lv_obj_t *lost_shown = nullptr;  // "no server": on a network, the server not answering a while
static uint32_t lost_since = 0;         // on a network, no server since (0: it answers)
constexpr uint32_t kLostMs = 60000;     // longer than the server takes to restart
static bool wifi_offered = false;       // since it woke, it opened by itself: not again
static int brightness_shown = 100;     // the screen's, as last set
static std::string asked;               // "menu" or "wifi": a grown-up asked Cubey for it, to open once it has spoken
static uint32_t woke_at = 0;
constexpr uint32_t kWifiFailedMs = 20000; // the network kept not joined this long after waking: the setup
static std::optional<DiceRoll> dice;    // its last roll
static bool dice_told = true;           // the server has heard the last roll's number
static uint32_t dice_since = 0;         // the last roll
constexpr uint32_t kDiceIdleMs = 10 * 60000; // no roll this long: the game is over, Cubey sleeps
static Shake shake;
static std::optional<uint32_t> finding_since; // Cubey asked her to find something: the talk waits while she looks
constexpr uint32_t kFindMs = 3 * 60000;
static Upright upright;       // which way is up, from the motion sensor

// A screen of its own colour: black asleep, the theme's ground awake.
static lv_obj_t *new_screen(uint32_t colour) {
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(colour), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    return scr;
}

static void load(lv_obj_t *screen) {
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

// The screen's theme: all of it drawn again in its colours (asleep, it wakes in them).
static void theme(const std::string &name) {
    if (!dots::set_theme(name) || power.asleep()) return;
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(dots::ground()), 0);
    lv_obj_invalidate(lv_screen_active());
}

static void stopped_brushing(); // she stopped it: the server hears, for Cubey to know

// A routine's picture shows in place of the eyes (a game's, the battery, brushing...): Cubey's own gives way to it.
static bool routine_shown() {
    return battery_shown || mouth_shown || dial_shown || cards_shown || die_shown || letter_shown || count_shown;
}
static void on_power(PowerAction action);

// What Cubey drew, or a word it spells once its first letter shows, in place of its eyes.
static bool drawing_shown() { return screen_shown || (spelling && spelling->shown(lv_tick_get()) > 0); }

// Asleep: an empty black screen, the screen off, the talk over (brushing too), the steps seldom.
static void sleep_screen() {
    stopped_brushing();
    audio::music(false);
    voice.close();
    again = false;
    brushing.reset();
    timer.reset();
    open_with.clear();
    dice.reset();
    finding_since.reset();
    spelling.reset();
    eyes_shown = battery_shown = mouth_shown = dial_shown = cards_shown = die_shown = letter_shown = count_shown = screen_shown = spell_shown = menu_shown = wifi_shown = lost_shown = nullptr;
    lost_since = 0;
    settings_menu.close();
    wifi_setup.close();
    wifi_offered = false;
    load(new_screen(0x000000));
    hal::screen(false);
    lv_timer_set_period(stepping, kStepAsleepMs);
}

// Awake: the eyes; the talk opens once the server is there.
static void wake_screen() {
    woke_at = lv_tick_get();
    lv_timer_set_period(stepping, kStepMs);
    hal::screen(true);
    lv_obj_t *scr = new_screen(dots::ground());
    eyes_shown = eyes::create(scr);
    battery_shown = mouth_shown = dial_shown = cards_shown = die_shown = letter_shown = count_shown = screen_shown = spell_shown = menu_shown = wifi_shown = lost_shown = nullptr;
    lost_since = 0;
    settings_menu.close();
    wifi_setup.close();
    wifi_offered = false;
    spelling.reset();
    dice.reset();
    finding_since.reset();
    brushing.reset();
    timer.reset();
    load(scr);
}

// Her tap on Cubey's mouth: the two minutes start (the server hears at the next step).
static void brushing_tapped() {
    if (brushing) brushing->start(lv_tick_get());
}

static void end_brushing() {
    if (mouth_shown) lv_obj_delete(mouth_shown);
    mouth_shown = nullptr;
    brushing.reset();
    audio::music(false);
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, false);
}

static void stopped_brushing() {
    if (brushing && !brushing->finished() && hal::link_connected()) hal::link_send(R"({"type":"brushing.stopped"})");
}

// The timer is over, or she stopped it: its dial goes, Cubey's eyes come back, and the talk opens with what happened.
static void end_timer(const char *how) {
    if (dial_shown) lv_obj_delete(dial_shown);
    dial_shown = nullptr;
    timer.reset();
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, false);
    open_with = std::string(R"("timer":")") + how + R"(","for":")" + timer_for + R"(")";
}

// Her word game is over (she, the button or the server ended it): its pictures go, Cubey's eyes come back.
static void end_lesson() {
    if (cards_shown) lv_obj_delete(cards_shown);
    cards_shown = nullptr;
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, false);
}

// Luck, stirred by the moment (her tap or shake, a message's coming): LVGL's starts the same at every boot.
static uint32_t lucky(uint32_t below) { return lv_rand(0, below - 1); }
static void stir(uint32_t now) { lv_rand_set_seed((lv_rand(0, 0x7FFFFFFF) ^ now) | 1); }

// A roll: the die tumbles (not while it still does).
static void roll_die(uint32_t now) {
    if (!die_shown || (dice && !dice->landed(now))) return;
    stir(now);
    dice.emplace(now, lucky);
    dice_told = false;
    dice_since = now;
}

// The dice are put away (the button): Cubey's eyes come back, and the talk opens with what happened.
static void end_dice() {
    if (die_shown) lv_obj_delete(die_shown);
    die_shown = nullptr;
    dice.reset();
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, false);
    open_with = R"("dice":"stopped")";
}

static void end_screen() {
    if (!screen_shown) return;
    lv_obj_delete(screen_shown);
    screen_shown = nullptr;
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, routine_shown() || drawing_shown());
}

static void end_spell() {
    if (!spell_shown) return;
    lv_obj_delete(spell_shown);
    spell_shown = nullptr;
    spelling.reset();
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, routine_shown() || drawing_shown());
}

// The grown-ups' menu is closed: it goes, and what changed goes to the server, which opens the talk anew in it.
static void end_menu() {
    if (menu_shown) lv_obj_delete(menu_shown);
    menu_shown = nullptr;
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, routine_shown() || drawing_shown());
    if (!settings_menu.to_send() || !hal::link_connected()) return;
    char msg[160];
    snprintf(msg, sizeof msg, R"({"type":"settings","language":"%s","learns":"%s","theme":"%s","brightness":%d})", settings_menu.language().c_str(),
             settings_menu.learns().c_str(), settings_menu.theme().c_str(), settings_menu.brightness());
    hal::link_send(msg);
}

// The Wi-Fi setup: its screens over everything, the talk closed until it is done.
static void open_wifi(bool welcome, uint32_t now) {
    if (wifi_setup.is_open() || settings_menu.is_open()) return;
    voice.close();
    wifi_setup.open(welcome, now);
    wifi_shown = wifi_view::create(lv_screen_active(), &wifi_setup, now);
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, true);
}

static void end_wifi() {
    if (wifi_shown) lv_obj_delete(wifi_shown);
    wifi_shown = nullptr;
    wifi_setup.close();
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, routine_shown() || drawing_shown());
}

// A press of the button, awake: back from a routine to Cubey's eyes, or from its eyes to sleep.
static PowerAction back() {
    if (wifi_setup.is_open()) {
        wifi_setup.back();
        if (!wifi_setup.is_open()) end_wifi();
        return PowerAction::None;
    }
    if (settings_menu.close() == SettingsMenu::Event::Closed) {
        end_menu();
        return PowerAction::None;
    }
    if (screen_shown || spell_shown) {
        end_screen();
        end_spell();
        return PowerAction::None;
    }
    if (die_shown) {
        end_dice();
        return PowerAction::None;
    }
    if (cards_shown) {
        if (hal::link_connected()) hal::link_send(R"({"type":"lesson.stopped"})");
        end_lesson();
        return PowerAction::None;
    }
    if (timer) {
        end_timer("stopped");
        return PowerAction::None;
    }
    if (brushing) {
        stopped_brushing();
        end_brushing();
        return PowerAction::None;
    }
    if (!battery_shown) return power.sleep();
    lv_obj_delete(battery_shown);
    battery_shown = nullptr;
    lv_obj_set_hidden(eyes_shown, false);
    return PowerAction::None;
}

// Her word game: its pictures over all of the screen in place of Cubey's eyes, as the server sends them; a tap on one
// is her answer.
static void lesson_message(const ServerMessage &msg, const std::string &json) {
    if (msg.type == "lesson.end") return end_lesson();
    if (msg.type == "lesson.start" && !cards_shown && !brushing && !timer)
        cards_shown = cards::create(lv_obj_get_parent(eyes_shown), [](const std::string &id) { return voice.tap(id); });
    if (!cards_shown) return;
    if (msg.type == "lesson.show") cards::show(lesson_show(json).value_or(std::vector<uint8_t>{}));
    if (msg.type == "lesson.choose") cards::choose(lesson_choose(json).value_or(std::vector<LessonPicture>{}));
    lv_obj_set_hidden(eyes_shown, cards::showing());
}

// A game with letters: the letter she needs now, big; none, the game is over and Cubey's eyes come back. In the chain
// the words scroll in first as they are said: hers as her turn ends, Cubey's going on from it once written. Only over
// the eyes: the server ends the game before another routine shows.
static void show_letter(const ServerMessage &msg, uint32_t now) {
    if (letter_shown && msg.hers.empty() && !msg.word.empty() && letter::go_on(letter_shown, msg.word, msg.letter, now)) return;
    if (letter_shown) lv_obj_delete(letter_shown);
    letter_shown = nullptr;
    const bool other = routine_shown();
    if (auto shown = letter::start(msg.hers, msg.word, msg.letter); !shown.words.empty() && !other)
        letter_shown = letter::create(lv_obj_get_parent(eyes_shown), std::move(shown), now);
    lv_obj_set_hidden(eyes_shown, other || letter_shown);
}

// How many dots: so many at random places, the number before first; none, the game is over and Cubey's eyes come back.
// Only over the eyes, as the letters.
static void show_count(const ServerMessage &msg, uint32_t now) {
    const int was = count_shown ? count::number(count_shown) : 0;
    if (count_shown) lv_obj_delete(count_shown);
    count_shown = nullptr;
    const bool other = routine_shown();
    stir(now);
    if (msg.n > 0 && !other) count_shown = count::create(lv_obj_get_parent(eyes_shown), count::place(msg.n, was, lucky), now);
    lv_obj_set_hidden(eyes_shown, other || count_shown);
}

// Something Cubey shows on its screen, in place of its eyes for a while: a picture it drew, or text, still or
// scrolling past (whole passes). Only over the eyes.
static void show_screen(const ServerMessage &msg, uint32_t now) {
    end_screen();
    end_spell();
    if (routine_shown()) return;
    screen_ms = kScreenMs;
    screen_moves = false;
    if (!msg.rows.empty()) screen_shown = picture::create(lv_obj_get_parent(eyes_shown), msg.rows);
    else if (letter::Shown text = letter::text(msg.text); !text.words.empty()) {
        const uint32_t pass = letter::length(text.words[0]);
        if (pass) screen_ms = (kScreenMs + pass - 1) / pass * pass, screen_moves = true;
        screen_shown = letter::create(lv_obj_get_parent(eyes_shown), std::move(text), now);
    }
    screen_since = now;
    lv_obj_set_hidden(eyes_shown, screen_shown);
}

// A word Cubey spells, before its speech: its letters big, one by one as its voice says them (spell.at), from its
// first in place of its eyes; then all of it passes. Only over the eyes, as a drawing, which it replaces.
static void spell_word(const ServerMessage &msg, uint32_t now) {
    end_spell();
    if (routine_shown()) return;
    letter::Shown shown = letter::spelling(msg.word);
    if (shown.words.empty()) return;
    end_screen();
    spelling.emplace(static_cast<int>(shown.words.size()) - 1, letter::length(shown.words.back()));
    spell_shown = spell::create(lv_obj_get_parent(eyes_shown), std::move(shown), &*spelling, now);
    lv_obj_set_hidden(spell_shown, true);
}

// The word spelled: timed from when Cubey's speech began to play; once its turn is over (spoken, or not), any letter
// not timed shows. Over, or a routine's picture shows, Cubey's eyes are back.
static void follow_spell(uint32_t now) {
    if (!spelling) return;
    const VoiceState state = voice.state();
    if (state == VoiceState::Speaking) spelling->speaking(now);
    else if (!voice.is_open() || state == VoiceState::Listening || state == VoiceState::Idle) spelling->spoken(now);
    if (routine_shown() || spelling->over(now)) return end_spell();
    const bool shows = spelling->shown(now) > 0;
    lv_obj_set_hidden(spell_shown, !shows);
    if (shows) lv_obj_set_hidden(eyes_shown, true);
    spell::show(spell_shown, now);
}

static void follow_screen(uint32_t now) {
    if (!screen_shown) return;
    if (routine_shown() || since(now, screen_since) >= screen_ms) return end_screen();
    if (screen_moves) letter::show(screen_shown, now);
}

// The routines, as the server says: asked about its battery, Cubey shows it in place of its eyes while it says how
// much is left; brushing, its mouth, until the two minutes and the praise are over, or she stops; her word game.
static void server_message(const ServerMessage &msg, const std::string &json, uint32_t now) {
    if (msg.type == "reset") return hal::wipe();
    if (msg.type == "settings") { // to its hello, asleep too
        if (msg.brightness) hal::brightness(brightness_shown = msg.brightness);
        if (!msg.theme.empty() && !settings_menu.is_open()) theme(msg.theme);
        return settings_menu.known(msg.languages, {msg.language, msg.learns, msg.theme, msg.brightness ? msg.brightness : 100}, msg.fresh);
    }
    if (!eyes_shown) return;
    if (msg.type == "menu" || msg.type == "wifi") {
        asked = msg.type;
        return;
    }
    if (msg.type.rfind("lesson.", 0) == 0) return lesson_message(msg, json);
    if (msg.type == "find") finding_since = now;
    if (msg.type == "letter") return show_letter(msg, now);
    if (msg.type == "count") return show_count(msg, now);
    if (msg.type == "show") return show_screen(msg, now);
    if (msg.type == "spell") return spell_word(msg, now);
    if (msg.type == "spell.at") {
        if (spelling && msg.ms >= 0) spelling->said(msg.i, static_cast<uint32_t>(msg.ms));
        return;
    }
    if (msg.type == "dice" && !brushing && !timer && !cards_shown) {
        if (battery_shown) lv_obj_delete(battery_shown);
        battery_shown = nullptr;
        if (!die_shown) die_shown = die::create(lv_obj_get_parent(eyes_shown), [] { roll_die(lv_tick_get()); });
        lv_obj_set_hidden(eyes_shown, true);
        return roll_die(now);
    }
    if (msg.type == "brushing.stop") return end_brushing();
    if (msg.type == "timer" && msg.minutes > 0 && !brushing) {
        if (battery_shown) lv_obj_delete(battery_shown);
        if (dial_shown) lv_obj_delete(dial_shown);
        battery_shown = nullptr;
        timer.emplace(now, static_cast<uint32_t>(msg.minutes));
        timer_for = msg.kind;
        dial_shown = dial::create(lv_obj_get_parent(eyes_shown), &*timer, dial::for_named(msg.kind));
        lv_obj_set_hidden(eyes_shown, true);
        return;
    }
    if (msg.type == "brushing" && !brushing) {
        if (battery_shown) lv_obj_delete(battery_shown);
        battery_shown = nullptr;
        brushing.emplace(now);
        mouth_shown = mouth::create(lv_obj_get_parent(eyes_shown), &*brushing, brushing_tapped);
        lv_obj_set_hidden(eyes_shown, true);
    }
    if (msg.type != "battery" || brushing || timer || hal::battery().percent < 0) return;
    if (battery_shown) lv_obj_delete(battery_shown);
    battery_shown = battery::create(lv_obj_get_parent(eyes_shown), hal::battery());
    lv_obj_set_hidden(eyes_shown, true);
    battery_since = now;
}

// Brushing: the talk stays open while she brushes, the beat plays; each part, the end, or no tap, the server hears,
// for Cubey to say.
static void follow_brushing(uint32_t now) {
    if (!brushing) return;
    voice.stay(now);
    audio::music(brushing->started() && !brushing->finished());
    const int news = brushing->news(now);
    if (news && hal::link_connected()) {
        char msg[48];
        if (news == BrushingRun::kDone) snprintf(msg, sizeof msg, R"({"type":"brushing.done"})");
        else if (news == BrushingRun::kMissed) snprintf(msg, sizeof msg, R"({"type":"brushing.missed"})");
        else snprintf(msg, sizeof msg, R"({"type":"brushing.part","part":%d})", news);
        hal::link_send(msg);
    }
    if (brushing->over(now)) end_brushing();
}

// The dice: once Cubey has said it rolls, the talk is over (nobody listens to the family at the board game); each
// roll's number goes to the server once it has landed and Cubey is quiet, for Cubey to say it, nobody listening. A
// shake or a tap rolls again; a long while without a roll, Cubey sleeps.
static void follow_dice(uint32_t now, bool shaken) {
    if (!die_shown) return;
    if (voice.is_open() && voice.state() == VoiceState::Listening) voice.close();
    if (shaken) roll_die(now);
    if (!dice) return;
    die::show(die_shown, dice->frame(now));
    if (!dice_told && dice->landed(now) && !voice.is_open() && hal::link_connected()) {
        char msg[48];
        snprintf(msg, sizeof msg, R"({"type":"dice.rolled","n":%d})", dice->number());
        hal::link_send(msg);
        dice_told = true;
    }
    if (since(now, dice_since) > kDiceIdleMs) on_power(power.sleep());
}

// She looks for something Cubey asked her to find: the talk waits for her a few minutes, not half of one.
static void follow_finding(uint32_t now) {
    if (!finding_since) return;
    if (since(now, *finding_since) > kFindMs) return finding_since.reset();
    voice.stay(now);
}

// Her word game never ends by itself: the talk stays open while she thinks, however long; it is over with the talk.
static void follow_lesson(uint32_t now) {
    if (!cards_shown) return;
    if (!voice.is_open()) return end_lesson();
    voice.stay(now);
}

static void follow_battery(uint32_t now) {
    if (!battery_shown || since(now, battery_since) < kBatteryShowMs) return;
    lv_obj_delete(battery_shown);
    battery_shown = nullptr;
    lv_obj_set_hidden(eyes_shown, false);
}

static void on_power(PowerAction action) {
    if (action == PowerAction::Back) action = back();
    if (action == PowerAction::Sleep) sleep_screen();
    if (action == PowerAction::Wake) wake_screen();
}

// The grown-ups' menu, opened: over everything, the talk closed until it closes.
static void show_menu() {
    voice.close();
    menu_shown = menu::create(lv_screen_active(), &settings_menu);
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, true); // their blinks would redraw the screen under it
}

// A grown-up asked Cubey for its menu or its Wi-Fi setup: it opens once Cubey has said so.
static void follow_asked(uint32_t now) {
    if (asked.empty() || (voice.is_open() && voice.state() != VoiceState::Listening)) return;
    if (power.asleep() || settings_menu.is_open() || wifi_setup.is_open()) return asked.clear();
    if (asked == "wifi") open_wifi(false, now);
    else if (settings_menu.open(now) == SettingsMenu::Event::Opened) show_menu();
    asked.clear();
}

// The grown-ups' menu: "–" held alone opens it, or a grown-up asking Cubey for it (follow_asked); a new Cubey's opens
// by itself, once its Wi-Fi is set up and the server has said its languages, before Cubey's first word.
static void follow_menu(const Keys &keys, uint32_t now) {
    if (power.asleep()) return;
    SettingsMenu::Event event = settings_menu.keys(keys, now);
    if (wifi_setup.is_open()) return; // its own keys
    if (event == SettingsMenu::Event::None) event = settings_menu.offer(now);
    switch (event) {
    case SettingsMenu::Event::Wifi: open_wifi(false, now); break;
    case SettingsMenu::Event::Opened: show_menu(); break;
    case SettingsMenu::Event::Closed: return end_menu();
    case SettingsMenu::Event::None: break;
    }
    if (menu_shown && !settings_menu.is_open()) return end_menu(); // OK tapped
    if (!menu_shown) return;
    theme(settings_menu.theme()); // its theme and brightness shown as they are set
    if (settings_menu.brightness() != brightness_shown) hal::brightness(brightness_shown = settings_menu.brightness());
    menu::show(menu_shown, settings_menu);
}

// On a network, the server not answering a minute (down, or this Cubey blocked): "no server" says so, in place of shut
// eyes and nothing else; gone once it answers, or the Wi-Fi setup opens.
static void follow_server(uint32_t now) {
    const bool lost = !power.asleep() && hal::wifi() == Wifi::Joined && !hal::link_connected() && !wifi_setup.is_open();
    if (!lost) lost_since = 0;
    else if (!lost_since) lost_since = now ? now : 1;
    const bool show = lost_since && since(now, lost_since) >= kLostMs;
    if (show == !!lost_shown) return;
    if (lost_shown) {
        lv_obj_delete(lost_shown);
        lost_shown = nullptr;
        if (eyes_shown) lv_obj_set_hidden(eyes_shown, routine_shown() || drawing_shown());
        return;
    }
    fine::Frame f;
    f.fill(dots::Dot::Off);
    fine::centred(f, "no", 16, 2, dots::Dot::Half);
    fine::centred(f, "server", 32, 2, dots::Dot::Half);
    lost_shown = fine::create(lv_screen_active(), f);
    if (eyes_shown) lv_obj_set_hidden(eyes_shown, true);
}

// The Wi-Fi setup: by itself once Cubey wakes with no network (none kept, or the one kept not joined a while), or "+"
// held alone (follow_menu); "+" and "–" scroll its networks. Joined, it closes and the talk opens.
static void follow_wifi(const Keys &keys, uint32_t now) {
    if (power.asleep()) return;
    const Wifi state = hal::wifi();
    if (!wifi_offered && (state == Wifi::Unset || (state != Wifi::Joining && state != Wifi::Joined && since(now, woke_at) >= kWifiFailedMs))) {
        wifi_offered = true;
        open_wifi(true, now);
    }
    if (!wifi_setup.is_open()) return;
    if (const int by = keys.plus_pressed - keys.minus_pressed) wifi_setup.scroll(by, now);
    wifi_setup.follow(now);
    if (!wifi_setup.is_open()) return end_wifi();
    wifi_view::show(wifi_shown, wifi_setup, now);
}

// Awake, a talk is always open while the server is there and has said its languages (but while a timer runs, or the
// grown-ups' menu or the Wi-Fi setup is open): its goodbye, or half a quiet minute, put Cubey to sleep; lost with the
// server, it goes on once it is back.
static void follow_talk(uint32_t now) {
    if (power.asleep() || voice.is_open() || timer || die_shown || settings_menu.is_open() || wifi_setup.is_open() || !settings_menu.ready()) return;
    switch (voice.ended()) {
    case TalkEnd::Goodbye:
    case TalkEnd::Quiet: return on_power(power.sleep());
    case TalkEnd::Lost: again = true; break;
    case TalkEnd::None: break;
    }
    if (voice.open(now, again, open_with)) again = false, open_with.clear();
}

// A timer: once Cubey has said it's on, the talk is over (Gemini neither listens nor bills while it runs) and the
// screen stays on; 5 minutes and 1 minute before its end the server is asked for a reminder, and at its end Cubey is
// back, asking whether she has finished.
static void follow_timer(uint32_t now) {
    if (!timer) return;
    if (voice.is_open() && voice.state() == VoiceState::Listening) voice.close();
    const int news = timer->news(now);
    if (news == TimerRun::kOver) return end_timer("done");
    if (news && hal::link_connected()) {
        char msg[96];
        snprintf(msg, sizeof msg, R"({"type":"timer.left","minutes":%d,"for":"%s"})", news, timer_for.c_str());
        hal::link_send(msg);
    }
}

// The picture stays upright however Cubey is turned; asleep, there is nothing to turn.
static void follow_upright(Gravity down, uint32_t now) {
    if (power.asleep()) return;
    const int turn = upright.turn(down, now);
    if (turn == dots::turn()) return;
    dots::set_turn(turn);
    cards::set_turn(turn);
    lv_obj_invalidate(lv_screen_active());
}

// Follows the power button, the talk and the battery shown; with no server for 45 seconds, Cubey sleeps. The battery
// goes to the server as it changes.
static void step(lv_timer_t *) {
    const uint32_t now = lv_tick_get();
    on_power(power.press(hal::power_button(), now));
    on_power(power.idle(now, voice.is_open() || timer || die_shown || settings_menu.is_open() || wifi_setup.is_open()));
    voice.tick(now, [now](const ServerMessage &msg, const std::string &json) { server_message(msg, json, now); });
    if (hal::link_connected())
        if (auto report = state_report.next(hal::battery(), hal::link_session())) hal::link_send(*report);
    const Keys keys = hal::keys();
    follow_asked(now);
    follow_menu(keys, now);
    follow_wifi(keys, now);
    follow_server(now);
    follow_talk(now);
    follow_battery(now);
    follow_brushing(now);
    follow_timer(now);
    follow_lesson(now);
    follow_finding(now);
    const Gravity down = power.asleep() ? Gravity{} : hal::gravity(); // asleep, nothing turns or rolls
    follow_dice(now, shake.shaken(down, now));
    if (letter_shown) letter::show(letter_shown, now);
    if (count_shown) count::show(count_shown, now);
    follow_screen(now);
    follow_spell(now);
    follow_upright(down, now);
    if (eyes_shown) eyes::set_shut(eyes_shown, !hal::link_connected()); // nobody to talk to
}

void start() {
    stepping = lv_timer_create(step, kStepMs, nullptr);
    wake_screen();
}

} // namespace cubey

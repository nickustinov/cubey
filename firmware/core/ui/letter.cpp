#include "ui/letter.h"

#include <algorithm>
#include <optional>

namespace cubey::letter {

namespace {

constexpr int kScale = 2;
constexpr int kTop = (dots::kBoard - font::kCapitalRows * kScale) / 2 - font::kCapitalTop * kScale; // capitals in the middle

int wide(const font::Letter &letter) { return static_cast<int>(letter.rows[0].size()); }

// Where the word's first column is with its letter i in the middle (the font's column between letters, two dots).
int stop(const Word &word, int i) {
    int before = 0;
    for (int j = 0; j < i; j++) before += (wide(word.letters[j]) + 1) * kScale;
    return (dots::kBoard - wide(word.letters[i]) * kScale) / 2 - before;
}

// A word in capitals, stopping on its last `stop_on`; none if empty, without it, or with a letter the font hasn't.
std::optional<Word> drawn(const std::string &utf8, const std::string &stop_on) {
    Word word{{}, -1};
    for (const std::string &name : font::split(utf8)) {
        auto letter = font::letter(font::upper(name));
        if (!letter) return std::nullopt;
        if (name == stop_on) word.at = static_cast<int>(word.letters.size());
        word.letters.push_back(std::move(*letter));
    }
    if (word.at < 0) return std::nullopt;
    return word;
}

// All of a word across, a column between its letters.
int across(const Word &word) {
    int w = -1;
    for (const font::Letter &letter : word.letters) w += wide(letter) + 1;
    return w * kScale;
}

Frame one(const Word &word, int index, uint32_t ms) {
    if (word.at < 0) { // text: still if it fits, else scrolling past, again and again
        const uint32_t pass = length(word);
        if (!pass) return {index, (dots::kBoard - across(word)) / 2, dots::Dot::On, false};
        return {index, dots::kBoard - static_cast<int>(ms % pass / word.dot_ms), dots::Dot::On, false};
    }
    const int end = stop(word, word.at);
    if (word.letters.size() == 1) return {index, end, dots::Dot::Off, false};
    const int start = word.from >= 0 ? stop(word, word.from) : dots::kBoard;
    if (word.from >= 0) {
        if (ms < kDimMs) return {index, start, dots::Dot::Dim, true};
        ms -= kDimMs;
    }
    const uint32_t travel = static_cast<uint32_t>(start - end) * word.dot_ms;
    if (ms < travel) return {index, start - static_cast<int>(ms / word.dot_ms), dots::Dot::On, false};
    ms -= travel;
    return {index, end, ms < kHoldMs ? dots::Dot::On : ms < kHoldMs + kDimMs ? dots::Dot::Dim : dots::Dot::Off, false};
}

struct Board {
    Shown shown;
    uint32_t since;
    Frame frame;
};

Board *board_of(lv_obj_t *obj) { return static_cast<Board *>(lv_obj_get_user_data(obj)); }
Board *board_of(lv_event_t *e) { return board_of(static_cast<lv_obj_t *>(lv_event_get_current_target(e))); }

} // namespace

Shown start(const std::string &hers, const std::string &cubeys, const std::string &stop_on) {
    auto alone = font::letter(font::upper(stop_on));
    if (!alone) return {};
    std::optional<Word> word = drawn(hers.empty() ? cubeys : hers, stop_on);
    if (word && !hers.empty()) word->from = 0, word->dot_ms = kHersDotMs;
    return {{word ? std::move(*word) : Word{{std::move(*alone)}, 0}}, {0}, stop_on};
}

bool go_on(Shown &shown, uint32_t ms, const std::string &cubeys, const std::string &stop_on) {
    const std::vector<std::string> names = font::split(cubeys);
    auto word = names.empty() || names[0] != shown.ends_on ? std::nullopt : drawn(cubeys, stop_on);
    if (!word) return false;
    word->from = 0;
    shown.starts.push_back(std::max(ms, shown.starts.back() + length(shown.words.back())));
    shown.words.push_back(std::move(*word));
    shown.ends_on = stop_on;
    return true;
}

Shown text(const std::string &utf8) {
    Word word{{}, -1};
    for (const std::string &name : font::split(utf8))
        if (const font::Letter *letter = font::letter(font::upper(name))) word.letters.push_back(*letter);
    if (word.letters.empty()) return {};
    return {{std::move(word)}, {0}, ""};
}

uint32_t length(const Word &word) {
    if (word.at < 0) return across(word) <= dots::kBoard ? 0 : static_cast<uint32_t>(dots::kBoard + across(word)) * word.dot_ms;
    if (word.letters.size() == 1) return 0;
    const int start = word.from >= 0 ? stop(word, word.from) : dots::kBoard;
    return (word.from >= 0 ? kDimMs : 0) + static_cast<uint32_t>(start - stop(word, word.at)) * word.dot_ms + kHoldMs + kDimMs;
}

Frame frame(const Shown &shown, uint32_t ms) {
    int i = static_cast<int>(shown.words.size()) - 1;
    while (i > 0 && ms < shown.starts[i]) i--;
    return one(shown.words[i], i, ms - shown.starts[i]);
}

dots::Dot dot(const Shown &shown, const Frame &frame, int col, int row) {
    const Word &word = shown.words[frame.word];
    int x = col - frame.left;
    if (x < 0 || row < kTop) return dots::Dot::Off;
    for (int i = 0; i < static_cast<int>(word.letters.size()); i++) {
        const font::Letter &letter = word.letters[i];
        if (x < wide(letter) * kScale) {
            const int y = (row - kTop) / kScale - letter.top;
            if (y < 0 || y >= static_cast<int>(letter.rows.size()) || letter.rows[y][x / kScale] != '#') return dots::Dot::Off;
            if (frame.arriving) return i == word.from ? dots::Dot::On : dots::Dot::Dim;
            return i == word.at ? dots::Dot::On : frame.rest;
        }
        x -= (wide(letter) + 1) * kScale;
        if (x < 0) return dots::Dot::Off; // between letters
    }
    return dots::Dot::Off;
}

lv_obj_t *create(lv_obj_t *parent, Shown shown, uint32_t now) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(obj, false);
    const Frame first = frame(shown, 0);
    lv_obj_set_user_data(obj, new Board{std::move(shown), now, first});
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t *e) {
            const Board *board = board_of(e);
            dots::draw(e, [&](int col, int row) { return dot(board->shown, board->frame, col, row); });
        },
        LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, [](lv_event_t *e) { delete board_of(e); }, LV_EVENT_DELETE, nullptr);
    return obj;
}

bool go_on(lv_obj_t *letter, const std::string &cubeys, const std::string &stop_on, uint32_t now) {
    Board *board = board_of(letter);
    return go_on(board->shown, now - board->since, cubeys, stop_on);
}

void show(lv_obj_t *letter, uint32_t now) {
    Board *board = board_of(letter);
    const Frame next = frame(board->shown, now - board->since);
    const Frame &was = board->frame;
    if (next.word == was.word && next.left == was.left && next.rest == was.rest && next.arriving == was.arriving) return;
    board->frame = next;
    lv_obj_invalidate(letter);
}

} // namespace cubey::letter

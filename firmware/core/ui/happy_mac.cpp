#include "ui/happy_mac.h"

#include <algorithm>

namespace cubey::happy_mac {

namespace {

using dots::Dot;

constexpr int kLeft = 14, kTop = 1; // the icon's top left on the board: 32 dots across, 41 down
constexpr int kEyes[2] = {10, 21};  // its eyes' columns, in the icon
constexpr int kSlot = 29;           // the floppy slot's row
constexpr int kBarTop = 46, kBarLeft = 7, kBarRight = 52, kBarBottom = 52;
// A floppy, its shutter half lit, its label dim; '.' hides what is behind it.
constexpr int kDiskWide = 11, kDiskTall = 9, kDiskLeft = 16, kDiskUnder = 31;
const char *const kDisk[kDiskTall] = {
    "###########", "#..+++++..#", "#..+..++..#", "#..+++++..#", "#.........#", "#.-------.#", "#.-------.#", "#.-------.#", "###########",
};

enum class Eye : uint8_t { Open, Shut, Crossed };

void set(fine::Frame &f, int col, int row, Dot as) { fine::set(f, kLeft + col, kTop + row, as); }

// The Macintosh: its case, its screen lit, its light and its floppy slot, and its foot.
void macintosh(fine::Frame &f) {
    for (int col = 1; col < 31; col++) set(f, col, 0, Dot::On), set(f, col, 35, Dot::On), set(f, col, 34, Dot::Dim);
    for (int row = 1; row < 35; row++) set(f, 0, row, Dot::On), set(f, 31, row, Dot::On);
    for (int row = 36; row < 41; row++)
        for (int col = 1; col < 31; col++) set(f, col, row, row == 40 || col == 1 || col == 30 ? Dot::On : Dot::Dim);
    for (int col = 4; col < 28; col++) set(f, col, 4, Dot::Half), set(f, col, 22, Dot::Half);
    for (int row = 5; row < 22; row++) {
        set(f, 4, row, Dot::Half), set(f, 27, row, Dot::Half);
        for (int col = 5; col < 27; col++) set(f, col, row, Dot::Dim);
    }
    for (int row = 28; row < 31; row++)
        for (int col = 3; col < 6; col++) set(f, col, row, Dot::Half);
    for (int col = 16; col < 27; col++) set(f, col, kSlot, Dot::On), set(f, col, kSlot + 1, Dot::Dim);
}

// Its face: its eyes, its nose, and a smile (or a frown).
void face(fine::Frame &f, const Eye eyes[2], bool smiles) {
    for (int i = 0; i < 2; i++) {
        const int col = kEyes[i];
        switch (eyes[i]) {
        case Eye::Open:
            for (int row = 9; row <= 11; row++) set(f, col, row, Dot::On);
            break;
        case Eye::Shut:
            for (int c = col - 1; c <= col + 1; c++) set(f, c, 11, Dot::On);
            break;
        case Eye::Crossed:
            for (int d = -1; d <= 1; d++) set(f, col + d, 10 + d, Dot::On), set(f, col + d, 10 - d, Dot::On);
            break;
        }
    }
    for (int row = 9; row < 15; row++) set(f, 15, row, Dot::On);
    set(f, 14, 14, Dot::On);
    const int ends = smiles ? 17 : 18, line = smiles ? 18 : 17;
    set(f, 11, ends, Dot::On), set(f, 18, ends, Dot::On);
    for (int col = 12; col < 18; col++) set(f, col, line, Dot::On);
}

// A floppy going into the slot, `ms` into its turn: under it a moment, rising into it a row at a time (what is in, hidden
// above it), then gone a while.
void floppy(fine::Frame &f, uint32_t ms) {
    const uint32_t at = ms % kDiskMs;
    if (at >= 1400) return;
    const int top = kDiskUnder - (at < 400 ? 0 : static_cast<int>((at - 400) / 100));
    for (int y = 0; y < kDiskTall; y++) {
        if (top + y <= kSlot) continue;
        for (int x = 0; x < kDiskWide; x++) {
            const char c = kDisk[y][x];
            set(f, kDiskLeft + x, top + y, c == '#' ? Dot::On : c == '+' ? Dot::Half : c == '-' ? Dot::Dim : Dot::Off);
        }
    }
}

// The progress bar: lit as far as it has come, a barber pole turning in the rest; failed, all dim.
void bar(fine::Frame &f, int percent, uint32_t ms, bool failed) {
    const Dot line = failed ? Dot::Dim : Dot::On;
    for (int col = kBarLeft; col <= kBarRight; col++) fine::set(f, col, kBarTop, line), fine::set(f, col, kBarBottom, line);
    for (int row = kBarTop; row <= kBarBottom; row++) fine::set(f, kBarLeft, row, line), fine::set(f, kBarRight, row, line);
    const int inside = (kBarRight - 2) - (kBarLeft + 2) + 1; // 42 columns, a dot clear of its frame
    const int filled = kBarLeft + 2 + (inside * std::clamp(percent, 0, 100) + 50) / 100;
    const int turned = static_cast<int>(ms / 150);
    for (int row = kBarTop + 2; row <= kBarBottom - 2; row++)
        for (int col = kBarLeft + 2; col < kBarRight - 1; col++) {
            if (col < filled) fine::set(f, col, row, line);
            else if (!failed && (col + row + 6 - turned % 6) % 6 < 2) fine::set(f, col, row, Dot::Dim);
        }
}

} // namespace

fine::Frame paint(UpdateState state, int percent, uint32_t ms) {
    fine::Frame f;
    f.fill(Dot::Off);
    macintosh(f);
    const bool blinks = ms % kBlinkEveryMs >= kBlinkEveryMs - kBlinkMs;
    switch (state) {
    case UpdateState::Done: {
        const Eye wink[2] = {Eye::Open, Eye::Shut};
        face(f, wink, true);
        break;
    }
    case UpdateState::Failed: {
        const Eye crossed[2] = {Eye::Crossed, Eye::Crossed};
        face(f, crossed, false);
        break;
    }
    case UpdateState::None:
    case UpdateState::Fetching: {
        const Eye eyes[2] = {blinks ? Eye::Shut : Eye::Open, blinks ? Eye::Shut : Eye::Open};
        face(f, eyes, true);
        floppy(f, ms);
        break;
    }
    }
    bar(f, percent, ms, state == UpdateState::Failed);
    return f;
}

lv_obj_t *create(lv_obj_t *parent) { return fine::create(parent, paint(UpdateState::None, 0, 0)); }

void show(lv_obj_t *view, UpdateState state, int percent, uint32_t ms) { fine::show(view, paint(state, percent, ms)); }

} // namespace cubey::happy_mac

#pragma once

#include "ui/letter.h"

// Big letters on the dot board at a moment (ui/letter.h), as tests look at them.
namespace cubey::letter_board {

// How many of the board's dots show so.
inline int count(const letter::Shown &shown, const letter::Frame &frame, dots::Dot as) {
    int n = 0;
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++) n += letter::dot(shown, frame, col, row) == as;
    return n;
}

// Whether two moments light the same dots fully.
inline bool same_lit(const letter::Shown &a, const letter::Frame &fa, const letter::Shown &b, const letter::Frame &fb) {
    for (int row = 0; row < dots::kBoard; row++)
        for (int col = 0; col < dots::kBoard; col++)
            if ((letter::dot(a, fa, col, row) == dots::Dot::On) != (letter::dot(b, fb, col, row) == dots::Dot::On)) return false;
    return true;
}

} // namespace cubey::letter_board

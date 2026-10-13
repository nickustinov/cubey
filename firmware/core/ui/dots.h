#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lvgl.h"

// The board of dots everything on Cubey's screen is drawn with: square dots over the whole 480 x 480 screen, each of
// them 2 by 2 small ones (nearly as fine as the Wi-Fi setup's board), every one faintly there as on an old LED panel,
// the lit ones full, half or dim, in its theme's colours.
namespace cubey::dots {

constexpr int kBoard = 24; // dots across and down
constexpr int kSplit = 2;  // small dots across and down each

enum class Dot : uint8_t { Off, Dim, Half, On };

// The theme, as the server says (its admin page's): amber, as old LED panels; green, a phosphor screen's; red and blue, LED
// panels'; vfd, a vacuum fluorescent display's; white, ink on paper. True if the colours changed (none such: amber).
bool set_theme(const std::string &name);
std::vector<std::string> themes(); // their names, for the menu
uint32_t colour(Dot d); // a dot's, as lit
uint32_t ground();      // the screen's under the dots

// Draws the board over the object an LV_EVENT_DRAW_MAIN is for: dot(col, row) says how each dot is lit, upright as
// she sees it, however the screen is turned (set_turn); each of them `split` by `split` small ones.
// A board of another size (the Wi-Fi setup's finer one, its dots whole) likewise.
void draw(lv_event_t *e, const std::function<Dot(int col, int row)> &dot, int board = kBoard, int split = kSplit);
// Redraws only those dots of the picture, its cells (col1, row1) to (col2, row2), wherever they are on the screen.
void invalidate(lv_obj_t *obj, int col1, int row1, int col2, int row2, int board = kBoard);

// The board's picture, turned so many quarters clockwise on the screen, to stay upright as Cubey is turned
// (services/upright.h); and which of its dots shows at the screen's (col, row).
void set_turn(int quarters);
int turn();
struct Cell {
    int col, row;
};
Cell turned(int quarters, int col, int row, int board = kBoard);

} // namespace cubey::dots

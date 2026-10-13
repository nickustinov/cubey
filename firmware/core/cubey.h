#pragma once

// Cubey: a small cube for a child of 2–6 that she talks with hands-free. Its screen is two LED-matrix eyes. It
// wakes to Cubey's hello; her goodbye, or a quiet while, puts it to sleep; a press of the button wakes it.
namespace cubey {

// Call once after LVGL is initialised and a 480 x 480 display exists: Cubey's eyes, and its talk once the server
// is there.
void start();

} // namespace cubey

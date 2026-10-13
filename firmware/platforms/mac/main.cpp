// Cubey simulator for macOS.
//
//   cubey-sim                          window: the mouse is a finger; p is the power button (shift+p held long), b the
//                                      pretend battery a step emptier (then charging), r turns the pretend cube a
//                                      quarter clockwise, s shakes it, = and - are the board's "+" and "–" (held
//                                      5 s: the Wi-Fi setup, the grown-ups' menu). Talks to the server at CUBEY_SERVER (default
//                                      ws://127.0.0.1:8788/device).
//   cubey-sim --screenshot out.png [--online] [--battery 30|30+] [--turn 1] [--message JSON ...] [--tap X,Y ...] [--power short|long] [--keys plus|minus|hold-plus|hold-minus] [--no-wifi] [--ms 1000]
//                                      the screen, rendered headless to a PNG; --online pretends the server is there
//                                      (otherwise the eyes are shut); --message arrives as if the server sent it;
//                                      messages, taps, presses and keys (hold-plus, hold-minus: held 5 s) in
//                                      the order given, then --ms of time; --no-wifi: no network kept; --battery:
//                                      its charge, + while charging; --turn: the cube turned so many quarters clockwise
//                                      (the picture turns back upright)
#include <SDL2/SDL.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "cubey.h"
#include "lvgl.h"
#include "mac_hal.h"
#define LODEPNG_NO_COMPILE_CPP
#include "src/libs/lodepng/lodepng.h"

using namespace cubey;

constexpr int32_t kSize = 480;

// The pretend battery, a step emptier each press, then charging, then full again.
static void next_battery() {
    static const Battery steps[] = {{50, false}, {20, false}, {8, false}, {8, true}, {100, false}, {80, false}};
    static size_t at = 0;
    mac_set_battery(steps[at++ % std::size(steps)]);
}

// Keys are watched before LVGL's SDL driver polls them, on the same thread.
static int on_key(void *, SDL_Event *e) {
    if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
        const Uint8 *held = SDL_GetKeyboardState(nullptr);
        mac_set_keys(held[SDL_SCANCODE_EQUALS], held[SDL_SCANCODE_MINUS]);
    }
    if (e->type != SDL_KEYDOWN || e->key.repeat) return 0;
    if (e->key.keysym.sym == SDLK_p) mac_press(SDL_GetModState() & KMOD_SHIFT ? PowerPress::Long : PowerPress::Short);
    if (e->key.keysym.sym == SDLK_b) next_battery();
    if (e->key.keysym.sym == SDLK_r) mac_turn(1);
    if (e->key.keysym.sym == SDLK_s) mac_shake();
    return 0;
}

static int run_window() {
    lv_display_t *disp = lv_sdl_window_create(kSize, kSize);
    lv_sdl_window_set_title(disp, "Cubey");
    lv_sdl_mouse_create();
    SDL_AddEventWatch(on_key, nullptr);
    const char *server = getenv("CUBEY_SERVER");
    mac_hal_init(server ? server : "ws://127.0.0.1:8788/device");
    start();
    while (true) {
        const uint32_t ms = lv_timer_handler();
        usleep(LV_MIN(ms, 10u) * 1000);
    }
}

static bool write_png(const char *path) {
    lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if (!snap) return false;
    const uint32_t w = snap->header.w, h = snap->header.h;
    std::string rgba(w * h * 4, '\0');
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *row = snap->data + y * snap->header.stride;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *px = row + x * 4; // B, G, R, A
            uint8_t *out = reinterpret_cast<uint8_t *>(&rgba[(y * w + x) * 4]);
            for (int c = 0; c < 3; c++) out[c] = px[2 - c];
            out[3] = 255;
        }
    }
    lv_draw_buf_destroy(snap);
    unsigned char *png = nullptr;
    size_t size = 0;
    if (lodepng_encode32(&png, &size, reinterpret_cast<const unsigned char *>(rgba.data()), w, h) != 0) return false;
    FILE *f = fopen(path, "wb");
    const bool ok = f && fwrite(png, 1, size, f) == size;
    if (f) fclose(f);
    lv_free(png);
    return ok;
}

static uint32_t fake_ms = 0;

static void run_ms(int ms) {
    for (int t = 0; t < ms; t += 20) {
        fake_ms += 20;
        lv_timer_handler();
    }
}

static int run_screenshot(int argc, char **argv) {
    const char *out = nullptr;
    int ms = 1000;
    struct Step {
        const char *message = nullptr;       // a message from the server
        PowerPress press = PowerPress::None; // or the power button
        lv_point_t tap{-1, -1};              // or a finger's tap
        const char *keys = nullptr;          // or the board's keys, pressed
    };
    std::vector<Step> steps;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--screenshot") out = argv[++i];
        else if (a == "--online") mac_pretend_online();
        else if (a == "--message") steps.push_back({argv[++i]});
        else if (a == "--power") steps.push_back({nullptr, strcmp(argv[++i], "long") == 0 ? PowerPress::Long : PowerPress::Short});
        else if (a == "--ms") ms = atoi(argv[++i]);
        else if (a == "--keys") steps.push_back({nullptr, PowerPress::None, {-1, -1}, argv[++i]});
        else if (a == "--no-wifi") mac_wifi_unset();
        else if (a == "--turn") mac_turn(atoi(argv[++i]));
        else if (int x, y; a == "--tap" && i + 1 < argc && sscanf(argv[i + 1], "%d,%d", &x, &y) == 2) steps.push_back({nullptr, PowerPress::None, {x, y}}), i++;
        else if (a == "--battery") {
            const std::string b = argv[++i];
            mac_set_battery({std::stoi(b), b.back() == '+'});
        }
    }

    lv_tick_set_cb([] { return fake_ms; });
    lv_display_t *disp = lv_display_create(kSize, kSize);
    alignas(64) static uint8_t buf[kSize * kSize * 2];
    lv_display_set_buffers(disp, buf, nullptr, sizeof buf, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, [](lv_display_t *d, const lv_area_t *, uint8_t *) { lv_display_flush_ready(d); });
    start();
    static lv_indev_data_t finger{};
    lv_indev_t *pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, [](lv_indev_t *, lv_indev_data_t *data) { *data = finger; });
    run_ms(200);
    for (const auto &step : steps) {
        if (step.message) mac_receive(step.message);
        else if (step.keys) {
            const bool plus = strcmp(step.keys, "plus") == 0 || strcmp(step.keys, "hold-plus") == 0;
            mac_set_keys(plus, !plus);
            run_ms(strncmp(step.keys, "hold-", 5) == 0 ? 5100 : 100);
            mac_set_keys(false, false);
        } else if (step.tap.x >= 0) {
            finger.point = step.tap;
            finger.state = LV_INDEV_STATE_PRESSED;
            run_ms(100);
            finger.state = LV_INDEV_STATE_RELEASED;
        } else mac_press(step.press);
        run_ms(100);
    }
    run_ms(ms);
    if (!write_png(out)) return fprintf(stderr, "could not write %s\n", out), 1;
    printf("wrote %s\n", out);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0); // line by line, also when piped to a log
    lv_init();
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--screenshot") == 0) return run_screenshot(argc, argv);
    return run_window();
}

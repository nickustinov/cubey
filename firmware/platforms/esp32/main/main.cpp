// Cubey on the board: the board's file brings up its display and touch, and runs LVGL on its own task; the core's
// runtime lives in LVGL timers on that task, so it starts under the display lock.
#include "board.h"
#include "cubey.h"
#include "nvs_flash.h"

extern "C" void app_main() {
    if (nvs_flash_init() != ESP_OK) { // Wi-Fi keeps its calibration here
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    cubey::board::update_start(); // a new firmware on trial
    cubey::board::display_start();
    cubey::board::power_start(); // on the board's I2C bus
    cubey::board::keys_start();
    cubey::board::audio_start();
    cubey::board::network_start();
    cubey::board::display_lock();
    cubey::start();
    cubey::board::display_unlock();
    cubey::board::backlight(true);
}

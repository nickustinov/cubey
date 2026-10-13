// The board's power (from Lapsa and Puce). The CPU runs at 240 MHz while the screen is on or the audio awake, and
// sleeps lightly between its tasks when it is dark and quiet, Wi-Fi waking for the access point's beacons. The
// AXP2101 on the board's I2C bus reads the battery for the core (the charge from the chip's fuel gauge, and whether
// it is charging), charges at 400 mA, and reports the PWR button's short and long presses; held longer still, the
// chip cuts the power itself.
// Registers as in XPowersLib (Waveshare's AXP2101 demo): 0x00 status 1, 0x01 status 2, 0x18 gauge, 0x62 charge
// current, 0x68 battery detection, 0xA4 the charge in percent; the PWR button: 0x41 (enable) and 0x49 (status) bits
// 3 (short press), 2 (long press), 6 and 7 (USB removed, plugged in), 0x27 the long press time.
#include <atomic>

#include "board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/hal.h"

namespace {

const char *TAG = "power";
constexpr uint16_t kAddress = 0x34;
constexpr uint8_t kStatus1 = 0x00, kStatus2 = 0x01, kGauge = 0x18, kChargeCurrent = 0x62, kBatteryDetect = 0x68, kPercent = 0xA4;
constexpr uint8_t kKeyTimes = 0x27, kIrqEnable2 = 0x41, kIrqStatus2 = 0x49;
constexpr uint8_t kShortPress = 1 << 3, kLongPress = 1 << 2, kUsbRemoved = 1 << 6, kUsbInserted = 1 << 7;
constexpr uint8_t kLongPress2s = 2 << 4; // 0x27 bits 5..4: 1, 1.5, 2 or 2.5 s
// 400 mA, as in Waveshare's demo for its LCD-4B: the chip's 200 mA took 15 hours for a 3000 mAh cell. 0x62's code:
// 25 mA steps up to 200 mA (8), then 100 mA steps.
constexpr uint8_t kCharge400mA = 10;

i2c_master_dev_handle_t pmu = nullptr;
std::atomic<int> battery_state{-1}; // percent and charging bit, sampled together
std::atomic<cubey::PowerPress> pending_press{cubey::PowerPress::None};
esp_pm_lock_handle_t display_speed = nullptr, audio_speed = nullptr;
bool screen_on = true; // owned by the UI thread

bool read(uint8_t reg, uint8_t &value) { return pmu && i2c_master_transmit_receive(pmu, &reg, 1, &value, 1, 50) == ESP_OK; }

void write(uint8_t reg, uint8_t value) {
    const uint8_t out[] = {reg, value};
    if (pmu) i2c_master_transmit(pmu, out, sizeof out, 50);
}

void set_bit(uint8_t reg, int bit) {
    uint8_t value = 0;
    if (read(reg, value)) write(reg, static_cast<uint8_t>(value | 1 << bit));
}

void sample_battery() {
    uint8_t s1 = 0, s2 = 0, percent = 0;
    battery_state = read(kStatus1, s1) && (s1 & 1 << 3) && read(kStatus2, s2) && read(kPercent, percent) && percent <= 100
                        ? percent | ((s2 >> 5 == 1) ? 256 : 0)
                        : -1;
}

// Presses every 100 ms, the battery every five seconds, off the UI thread.
void power_task(void *) {
    for (unsigned ticks = 0;; ticks++) {
        uint8_t status = 0;
        if (read(kIrqStatus2, status) && status) {
            write(kIrqStatus2, status);
            // Unplugging USB can set the short-press bit too; it is not a button press.
            if (!(status & (kUsbRemoved | kUsbInserted))) {
                if (status & kLongPress) pending_press = cubey::PowerPress::Long;
                else if (status & kShortPress) {
                    auto expected = cubey::PowerPress::None;
                    pending_press.compare_exchange_strong(expected, cubey::PowerPress::Short);
                }
            }
        }
        if (ticks % 50 == 0) sample_battery();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

} // namespace

namespace cubey::board {

void power_start() {
    // Light sleep between the steps while the screen is dark and the audio parked: the display's and the audio's
    // locks hold the CPU at full speed otherwise.
    const esp_pm_config_t speeds = {240, 80, true};
    ESP_ERROR_CHECK(esp_pm_configure(&speeds));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "display", &display_speed));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "audio", &audio_speed));
    ESP_ERROR_CHECK(esp_pm_lock_acquire(display_speed)); // the screen starts on

    i2c_device_config_t device = {};
    device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device.device_address = kAddress;
    device.scl_speed_hz = 400000;
    i2c_master_bus_handle_t bus = i2c();
    if (!bus || i2c_master_bus_add_device(bus, &device, &pmu) != ESP_OK) {
        ESP_LOGE(TAG, "no AXP2101");
        pmu = nullptr;
        return;
    }
    set_bit(kBatteryDetect, 0); // tell a battery from none
    set_bit(kGauge, 3);         // the fuel gauge, for the percent
    uint8_t times = 0, irqs = 0, current = 0;
    if (read(kChargeCurrent, current)) write(kChargeCurrent, static_cast<uint8_t>((current & 0xE0) | kCharge400mA));
    if (read(kKeyTimes, times)) write(kKeyTimes, static_cast<uint8_t>((times & ~0x30) | kLongPress2s)); // well before the hard cut
    if (read(kIrqEnable2, irqs)) write(kIrqEnable2, irqs | kShortPress | kLongPress);
    write(kIrqStatus2, kShortPress | kLongPress); // presses from before the start don't count
    uint8_t s1 = 0, s2 = 0, percent = 0;
    if (read(kStatus1, s1) && read(kStatus2, s2) && read(kPercent, percent))
        ESP_LOGI(TAG, "USB power %s, battery %s, %s, %d%%", s1 & 1 << 5 ? "yes" : "no", s1 & 1 << 3 ? "yes" : "no",
                 s2 >> 5 == 1 ? "charging" : s2 >> 5 == 2 ? "discharging" : "standby", percent);
    sample_battery();
    ESP_ERROR_CHECK(xTaskCreate(power_task, "power", 3072, nullptr, 2, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

void audio_performance(bool active) {
    ESP_ERROR_CHECK(active ? esp_pm_lock_acquire(audio_speed) : esp_pm_lock_release(audio_speed));
}

} // namespace cubey::board

namespace cubey::hal {

Battery battery() {
    const int state = battery_state;
    return state < 0 ? Battery{} : Battery{state & 255, bool(state & 256)};
}

PowerPress power_button() { return pending_press.exchange(PowerPress::None); }

void screen(bool on) {
    if (screen_on == on) return;
    screen_on = on;
    ESP_LOGI(TAG, "screen %s", on ? "on" : "off");
    if (on) {
        ESP_ERROR_CHECK(esp_pm_lock_acquire(display_speed));
        board::backlight(true);
    } else {
        board::backlight(false);
        ESP_ERROR_CHECK(esp_pm_lock_release(display_speed));
    }
}

void brightness(int percent) { board::brightness(percent); }

} // namespace cubey::hal

// A Waveshare ESP32-S3-Touch-AMOLED-2.16: Waveshare's BSP for the I2C bus and the CST9220 touch, LVGL on its own
// task (esp_lvgl_adapter), and set up here: the CO5300 AMOLED over QSPI, because the BSP's panel copies every
// strip LVGL draws in PSRAM into internal RAM to send it, which runs out (ESP-IDF 6 can send from PSRAM
// directly); and the ES8311 and ES7210 codecs, because the BSP's I2S reads the ES7210 as stereo, which leaves out
// its third input: the speaker's echo, wired back for cancelling it. Pins as the BSP's (BSP_LCD_*, BSP_I2S_*).
#include <algorithm>
#include <atomic>
#include <iterator>
#include <tuple>

#include "board.h"
#include "hal/hal.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/spi_master.h"
#include "esp_codec_dev_defaults.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace {

constexpr uint16_t kLines = 50; // LVGL draws in strips of this many lines, two in PSRAM (as the BSP)

esp_lcd_panel_handle_t panel = nullptr;
esp_lcd_panel_io_handle_t panel_io = nullptr;

// Waveshare's sequence for this panel (the BSP's): sleep out, its vendor page, SPI mode, RGB565, the tearing
// line, brightness control at full, the 480 x 480 window, the picture turned (MADCTL 0xA0), display on.
const uint8_t kNone[] = {0x00}, kPage20[] = {0x20}, kReg19[] = {0x10}, kReg1C[] = {0xA0}, kPage0[] = {0x00},
              kSpiMode[] = {0x80}, kRgb565[] = {0x55}, kTearing[] = {0x00}, kBrightnessOn[] = {0x20},
              kFull[] = {0xFF}, kWindow[] = {0x00, 0x00, 0x01, 0xDF}, kTurned[] = {0xA0};
const co5300_lcd_init_cmd_t kInit[] = {
    {0x11, kNone, 0, 600},   {0xFE, kPage20, 1, 0},  {0x19, kReg19, 1, 0},        {0x1C, kReg1C, 1, 0},
    {0xFE, kPage0, 1, 0},    {0xC4, kSpiMode, 1, 0}, {0x3A, kRgb565, 1, 0},       {0x35, kTearing, 1, 0},
    {0x53, kBrightnessOn, 1, 0}, {0x51, kFull, 1, 0}, {0x63, kFull, 1, 0},        {0x2A, kWindow, 4, 0},
    {0x2B, kWindow, 4, 0},   {0x36, kTurned, 1, 0},  {0x29, kNone, 0, 600},
};

esp_lcd_panel_io_handle_t start_panel() {
    spi_bus_config_t spi = {};
    spi.sclk_io_num = BSP_LCD_PCLK;
    spi.data0_io_num = BSP_LCD_DATA0;
    spi.data1_io_num = BSP_LCD_DATA1;
    spi.data2_io_num = BSP_LCD_DATA2;
    spi.data3_io_num = BSP_LCD_DATA3;
    spi.max_transfer_sz = BSP_LCD_H_RES * kLines * 2;
    spi.dma_burst_size = 64; // the most PSRAM takes (the driver's 32 let the panel's SPI outrun PSRAM)
    ESP_ERROR_CHECK(spi_bus_initialize(BSP_LCD_SPI_NUM, &spi, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io_config = {}; // CO5300_PANEL_IO_QSPI_CONFIG's, which is C only
    io_config.cs_gpio_num = BSP_LCD_CS;
    io_config.dc_gpio_num = GPIO_NUM_NC;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 3; // as the BSP
    io_config.lcd_cmd_bits = 32;
    io_config.lcd_param_bits = 8;
    io_config.flags.quad_mode = true;
    io_config.flags.psram_dma_direct = true;
    esp_lcd_panel_io_handle_t io = nullptr;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(BSP_LCD_SPI_NUM), &io_config, &io));
    static co5300_vendor_config_t vendor = {};
    vendor.init_cmds = kInit;
    vendor.init_cmds_size = std::size(kInit);
    vendor.flags.use_qspi_interface = 1;
    esp_lcd_panel_dev_config_t config = {};
    config.reset_gpio_num = BSP_LCD_RST;
    config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    config.bits_per_pixel = 16;
    config.vendor_config = &vendor;
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io, &config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, false)); // its memory is noise until the core's first screen
    return io;
}

// The CO5300 takes only areas that start on an even pixel and are an even number wide and high.
void even_area(lv_event_t *e) {
    auto *area = static_cast<lv_area_t *>(lv_event_get_param(e));
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

// For bring-up, every 10 s: how many frames the screen drew, and how long they took (the slowest, and on average).
int64_t frame_from = 0, frames_from = 0, frames_us = 0, slowest_us = 0;
int frames = 0;
void frame_started(lv_event_t *) { frame_from = esp_timer_get_time(); }
void frame_drawn(lv_event_t *) {
    const int64_t now = esp_timer_get_time(), took = now - frame_from;
    frames++, frames_us += took, slowest_us = std::max(slowest_us, took);
    if (now - frames_from < 10'000'000) return;
    ESP_LOGI("display", "%d frames in 10 s, %lld ms each on average, the slowest %lld ms", frames, frames ? frames_us / frames / 1000 : 0, slowest_us / 1000);
    frames_from = now, frames = 0, frames_us = slowest_us = 0;
}

// The codecs' I2S bus: the speaker's ES8311 in standard I2S, the mic's ES7210 in TDM, its inputs in slots 0–3 in
// the order MIC1, MIC3, MIC2, MIC4 (the speaker's echo is MIC3: slot 1). esp_codec_dev sets the rate, the slots
// and their widths as each codec opens, the speaker's matched to the mic's on the shared clock.
const audio_codec_data_if_t *audio_bus() {
    static const audio_codec_data_if_t *bus = [] {
        i2s_chan_handle_t tx = nullptr, rx = nullptr;
        i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
        chan.auto_clear = true; // as the BSP: no old samples replayed
        ESP_ERROR_CHECK(i2s_new_channel(&chan, &tx, &rx));
        i2s_std_config_t out = {};
        out.clk_cfg.sample_rate_hz = 16000;
        out.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
        out.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        out.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
        out.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
        out.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
        out.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
        out.slot_cfg.ws_width = 16;
        out.slot_cfg.bit_shift = true;
        out.gpio_cfg = {BSP_I2S_MCLK, BSP_I2S_SCLK, BSP_I2S_LCLK, BSP_I2S_DOUT, I2S_GPIO_UNUSED, {}};
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &out));
        i2s_tdm_config_t in = {};
        in.clk_cfg.sample_rate_hz = 16000;
        in.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
        in.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        in.clk_cfg.bclk_div = 8;
        in.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
        in.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
        in.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
        in.slot_cfg.slot_mask = static_cast<i2s_tdm_slot_mask_t>(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3);
        in.slot_cfg.ws_width = I2S_TDM_AUTO_WS_WIDTH;
        in.slot_cfg.bit_shift = true;
        in.slot_cfg.total_slot = I2S_TDM_AUTO_SLOT_NUM;
        in.gpio_cfg = {BSP_I2S_MCLK, BSP_I2S_SCLK, BSP_I2S_LCLK, I2S_GPIO_UNUSED, BSP_I2S_DSIN, {}};
        ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx, &in));
        ESP_ERROR_CHECK(i2s_channel_enable(tx));
        ESP_ERROR_CHECK(i2s_channel_enable(rx));
        audio_codec_i2s_cfg_t i2s = {};
        i2s.port = CONFIG_BSP_I2S_NUM;
        i2s.rx_handle = rx;
        i2s.tx_handle = tx;
        return audio_codec_new_i2s_data(&i2s);
    }();
    return bus;
}

const audio_codec_ctrl_if_t *codec_control(uint8_t address) {
    audio_codec_i2c_cfg_t i2c = {};
    i2c.port = BSP_I2C_NUM;
    i2c.addr = address;
    i2c.bus_handle = cubey::board::i2c();
    return audio_codec_new_i2c_ctrl(&i2c);
}

esp_codec_dev_handle_t codec(esp_codec_dev_type_t type, const audio_codec_if_t *chip) {
    if (!chip || !audio_bus()) return nullptr;
    esp_codec_dev_cfg_t config = {};
    config.dev_type = type;
    config.codec_if = chip;
    config.data_if = audio_bus();
    return esp_codec_dev_new(&config);
}

} // namespace

namespace cubey::board {

void display_start() {
    esp_lcd_panel_io_handle_t io = panel_io = start_panel();

    // ESP_LV_ADAPTER_DEFAULT_CONFIG() leaves a member out, which C++ doesn't allow; its values, but:
    esp_lv_adapter_config_t lvgl = {};
    lvgl.task_stack_size = 16 * 1024; // the core's screens run on this task
    lvgl.task_core_id = 1;            // Wi-Fi and the link on core 0
    lvgl.task_priority = ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY;
    lvgl.tick_period_ms = ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS;
    lvgl.task_min_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MIN_DELAY_MS;
    lvgl.task_max_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MAX_DELAY_MS;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&lvgl));

    esp_lv_adapter_display_config_t display = {};
    display.panel = panel;
    display.panel_io = io;
    display.profile.interface = ESP_LV_ADAPTER_PANEL_IF_OTHER;
    display.profile.rotation = ESP_LV_ADAPTER_ROTATE_0;
    display.profile.hor_res = BSP_LCD_H_RES;
    display.profile.ver_res = BSP_LCD_V_RES;
    display.profile.buffer_height = kLines;
    display.profile.use_psram = true;
    display.profile.require_double_buffer = true;
    display.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    lv_display_t *lv_display = esp_lv_adapter_register_display(&display);
    lv_display_add_event_cb(lv_display, even_area, LV_EVENT_INVALIDATE_AREA, nullptr);
    lv_display_add_event_cb(lv_display, frame_started, LV_EVENT_REFR_START, nullptr);
    lv_display_add_event_cb(lv_display, frame_drawn, LV_EVENT_REFR_READY, nullptr);

    bsp_display_cfg_t touch_turn = {};
    touch_turn.touch_flags = {.swap_xy = 1, .mirror_x = 0, .mirror_y = 1}; // as bsp_display_start(), for the panel's turn
    esp_lcd_touch_handle_t touch = nullptr;
    ESP_ERROR_CHECK(bsp_touch_new(&touch_turn, &touch));
    esp_lv_adapter_touch_config_t pointer = {};
    pointer.disp = lv_display;
    pointer.handle = touch;
    pointer.scale.x = pointer.scale.y = 1.0f;
    pointer.multi_touch.mode = ESP_LV_ADAPTER_TOUCH_MODE_SINGLE;
    pointer.multi_touch.pointers = 1;
    esp_lv_adapter_register_touch(&pointer);

    ESP_ERROR_CHECK(esp_lv_adapter_start());
}

void display_lock() { esp_lv_adapter_lock(-1); } // a negative timeout waits; 0 would not
void display_unlock() { esp_lv_adapter_unlock(); }

static uint8_t shown_level = 255; // the panel's brightness: its sleep forgets it

// The CO5300's brightness (0x51), sent as the driver sends its commands over QSPI (opcode 0x02, the command in the
// middle bytes); on the LVGL task, between its strips (as Puce's).
static void send_brightness() {
    if (panel_io) esp_lcd_panel_io_tx_param(panel_io, 0x02 << 24 | 0x51 << 8, &shown_level, 1);
}

void brightness(int percent) {
    shown_level = static_cast<uint8_t>(255 * std::clamp(percent, 0, 100) / 100);
    send_brightness();
}

// An AMOLED: its pixels go dark, and the panel sleeps in its own low-power mode; waking, it is drawn anew and told
// its brightness again (it comes out of sleep at full).
void backlight(bool on) {
    if (on) esp_lcd_panel_disp_sleep(panel, false);
    esp_lcd_panel_disp_on_off(panel, on);
    if (on) send_brightness();
    else esp_lcd_panel_disp_sleep(panel, true);
}

i2c_master_bus_handle_t i2c() { return bsp_i2c_init() == ESP_OK ? bsp_i2c_get_handle() : nullptr; }

esp_codec_dev_handle_t speaker_codec() {
    es8311_codec_cfg_t es8311 = {}; // as the BSP's
    es8311.ctrl_if = codec_control(ES8311_CODEC_DEFAULT_ADDR);
    es8311.gpio_if = audio_codec_new_gpio();
    es8311.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311.pa_pin = BSP_POWER_AMP_IO;
    es8311.use_mclk = true;
    es8311.hw_gain.pa_voltage = 5.0f;
    es8311.hw_gain.codec_dac_voltage = 3.3f;
    return codec(ESP_CODEC_DEV_TYPE_OUT, es8311.ctrl_if ? es8311_codec_new(&es8311) : nullptr);
}

esp_codec_dev_handle_t mic_codec() {
    es7210_codec_cfg_t es7210 = {};
    es7210.ctrl_if = codec_control(ES7210_CODEC_DEFAULT_ADDR);
    es7210.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    return codec(ESP_CODEC_DEV_TYPE_IN, es7210.ctrl_if ? es7210_codec_new(&es7210) : nullptr);
}
void amplifier(bool) {} // the ES8311 switches it (GPIO 46) as the speaker opens and closes

} // namespace cubey::board

namespace {

// The QMI8658 on the shared bus (0x6B, or 0x6A with its SA0 low; it answers 0x05 to WHO_AM_I), as QST's and
// Waveshare's drivers set it up: the address counting up as it's read (CTRL1 0x60), the accelerometer at
// +-4 g and 62.5 Hz (CTRL2 0x17), the gyroscope off (CTRL7 0x01): which way is down is all Cubey needs. nullptr
// without one.
i2c_master_dev_handle_t motion_sensor() {
    i2c_master_bus_handle_t bus = cubey::board::i2c();
    for (const uint16_t address : {0x6B, 0x6A}) {
        i2c_device_config_t device = {};
        device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device.device_address = address;
        device.scl_speed_hz = 400000;
        i2c_master_dev_handle_t imu = nullptr;
        if (!bus || i2c_master_bus_add_device(bus, &device, &imu) != ESP_OK) return nullptr;
        const uint8_t who = 0x00;
        uint8_t is = 0;
        if (i2c_master_transmit_receive(imu, &who, 1, &is, 1, 50) == ESP_OK && is == 0x05) {
            const uint8_t setup[][2] = {{0x02, 0x60}, {0x03, 0x17}, {0x08, 0x01}};
            for (const auto &reg : setup) i2c_master_transmit(imu, reg, 2, 50);
            ESP_LOGI("board", "motion sensor at 0x%02X", address);
            return imu;
        }
        i2c_master_bus_rm_device(imu);
    }
    ESP_LOGW("board", "no motion sensor: the picture stays as it is");
    return nullptr;
}

// How the sensor sits under the picture. It is on the board's back, so its axes are mirrored as she sees them; held
// upright, the first guess (neither) showed the timer upside down, and the only mirror that does that flips up and
// down: mirrored, and turned two quarters (2026-10-10). ponytail: the sides not yet seen on the board. If the picture
// shows turned clockwise by k quarters, set kSensorTurn to k more; if it turns the wrong way as Cubey is turned,
// flip kSensorMirrored.
constexpr int kSensorTurn = 2;
constexpr bool kSensorMirrored = true;

} // namespace

namespace {

// The keys as marked on the board: "+" (IO18) and "–" (BOOT, GPIO0) pull their pins low (as Puce's). Read every 10 ms
// on a timer of the system's (no task of their own: internal RAM is short), a press counted once it has held two
// reads: the UI read them once a step, and a press between two steps (the screen busy drawing) was lost, 9 in 10
// (2026-10-12).
constexpr const char *TAG = "keys";
std::atomic<bool> plus_held{false}, minus_held{false};
std::atomic<int> plus_presses{0}, minus_presses{0};

constexpr gpio_num_t kPlus = GPIO_NUM_18, kMinus = GPIO_NUM_0;

void read_keys(void *) {
    static bool was[2] = {}, read[2] = {}; // the timer's own
    for (int i = 0; i < 2; i++) {
        const bool down = !gpio_get_level(i ? kMinus : kPlus);
        if (down == read[i] && down != was[i]) { // the same two reads in a row: no bounce
            was[i] = down;
            (i ? minus_held : plus_held) = down;
            if (down) ++(i ? minus_presses : plus_presses);
        }
        read[i] = down;
    }
}

} // namespace

namespace cubey::board {

void keys_start() {
    gpio_config_t pins = {};
    pins.pin_bit_mask = 1ULL << kPlus | 1ULL << kMinus;
    pins.mode = GPIO_MODE_INPUT;
    pins.pull_up_en = GPIO_PULLUP_ENABLE; // the board pulls them up too
    if (gpio_config(&pins) != ESP_OK) {
        ESP_LOGE(TAG, "no keys");
        return;
    }
    esp_timer_create_args_t every = {};
    every.callback = read_keys;
    every.name = "keys";
    esp_timer_handle_t timer = nullptr;
    if (esp_timer_create(&every, &timer) != ESP_OK || esp_timer_start_periodic(timer, 10'000) != ESP_OK) ESP_LOGE(TAG, "no keys");
}

} // namespace cubey::board

namespace cubey::hal {

// At rest the accelerometer reads 1 g up, away from the ground: down is the other way.
Gravity gravity() {
    static const i2c_master_dev_handle_t imu = motion_sensor();
    const uint8_t first = 0x35; // AX_L: x, y, z, little-endian
    uint8_t raw[6];
    if (!imu || i2c_master_transmit_receive(imu, &first, 1, raw, sizeof raw, 20) != ESP_OK) return {};
    const auto axis = [&](int i) { return -static_cast<float>(static_cast<int16_t>(raw[2 * i + 1] << 8 | raw[2 * i])); };
    float x = kSensorMirrored ? -axis(0) : axis(0), y = axis(1);
    for (int k = 0; k < kSensorTurn; k++) std::tie(x, y) = std::make_tuple(-y, x); // a quarter clockwise
    return {x, y, axis(2)};
}

Keys keys() {
    return {plus_held, minus_held, plus_presses.exchange(0), minus_presses.exchange(0)};
}

} // namespace cubey::hal

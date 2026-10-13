// The board's updates over the air (hal::update): the build the server offers, fetched from its /firmware/<build>.bin
// (http(s):// where the link is ws(s)://, with the link's headers: who this Cubey is) into the app slot not running,
// with the link closed meanwhile (one TLS connection at a time leaves internal RAM enough), each flash write in its
// turn with the display's DMA (board_amoled_2_16.cpp). The bootloader then boots it on trial: it is kept once it
// talks with the server (hal::firmware_good); restarted before that (it crashed), or still not there a few minutes
// on, the bootloader goes back to the one before, and its hello names the build that failed, for the server not to
// offer it again.
#include <atomic>
#include <string>

#include "board.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/hal.h"

namespace {

const char *TAG = "update";
constexpr int kTaskStack = 10 * 1024;            // TLS, as the link's (network.cpp)
constexpr uint64_t kTrialUs = 5 * 60 * 1000000ull; // a new firmware's time to reach the server
constexpr int kChunk = 4096;                       // fetched and written at a time: a flash sector

std::atomic<cubey::UpdateState> state{cubey::UpdateState::None};
std::atomic<int> percent{0};

// A build as the server knows it: the start of the ELF's hash (firmware.ts).
std::string build_of(const esp_app_desc_t &desc) {
    std::string hex;
    for (int i = 0; i < 8; i++) hex += "0123456789abcdef"[desc.app_elf_sha256[i] >> 4], hex += "0123456789abcdef"[desc.app_elf_sha256[i] & 15];
    return hex;
}

// The firmware's address beside the link's: wss://host/device -> https://host/firmware/<build>.bin.
std::string url_for(const std::string &build) {
    std::string url = CONFIG_CUBEY_SERVER_URL;
    if (url.rfind("ws", 0) == 0) url.replace(0, 2, "http");
    const size_t path = url.find('/', url.find("://") + 3);
    return url.substr(0, path) + "/firmware/" + build + ".bin";
}

// The link's header lines, on the fetch's request.
esp_err_t with_headers(esp_http_client_handle_t client) {
    const std::string &lines = cubey::board::server_headers();
    for (size_t at = 0; at < lines.size();) {
        const size_t end = lines.find("\r\n", at), colon = lines.find(": ", at);
        if (end == std::string::npos || colon == std::string::npos || colon > end) break;
        esp_http_client_set_header(client, lines.substr(at, colon - at).c_str(), lines.substr(colon + 2, end - colon - 2).c_str());
        at = end + 2;
    }
    return ESP_OK;
}

// The build from the server into the slot not running, a chunk at a time: each flash write in its turn with the
// panel's DMA (board_amoled_2_16.cpp), never the network's reading; then that slot boots next.
esp_err_t fetch_into_slot(const std::string &url) {
    esp_http_client_config_t http = {};
    http.url = url.c_str();
    http.crt_bundle_attach = esp_crt_bundle_attach;
    http.timeout_ms = 15000;
    http.buffer_size = kChunk;
    esp_http_client_handle_t client = esp_http_client_init(&http);
    auto *chunk = static_cast<char *>(heap_caps_malloc(kChunk, MALLOC_CAP_SPIRAM));
    if (!client || !chunk) {
        free(chunk);
        if (client) esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    with_headers(client);
    esp_err_t err = esp_http_client_open(client, 0);
    const int64_t size = err == ESP_OK ? esp_http_client_fetch_headers(client) : -1;
    if (err == ESP_OK && esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "the server answered %d", esp_http_client_get_status_code(client));
        err = ESP_ERR_NOT_FOUND;
    }
    const esp_partition_t *slot = esp_ota_get_next_update_partition(nullptr);
    esp_ota_handle_t ota = 0;
    if (err == ESP_OK) {
        cubey::board::flash_writing(true);
        err = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &ota); // erased as it is written, not all at once
        cubey::board::flash_writing(false);
    }
    for (int64_t read = 0; err == ESP_OK;) {
        const int n = esp_http_client_read(client, chunk, kChunk);
        if (n <= 0) {
            if (n < 0 || !esp_http_client_is_complete_data_received(client)) err = ESP_FAIL;
            break;
        }
        cubey::board::flash_writing(true);
        err = esp_ota_write(ota, chunk, n);
        cubey::board::flash_writing(false);
        read += n;
        if (size > 0) percent = static_cast<int>(100 * read / size);
    }
    if (ota && err == ESP_OK) err = esp_ota_end(ota); // the image checked
    else if (ota) esp_ota_abort(ota);
    if (err == ESP_OK) {
        cubey::board::flash_writing(true);
        err = esp_ota_set_boot_partition(slot);
        cubey::board::flash_writing(false);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(chunk);
    return err;
}

void fetch(void *arg) {
    const std::string build = *static_cast<std::string *>(arg);
    delete static_cast<std::string *>(arg);
    cubey::board::link_pause(true);
    vTaskDelay(pdMS_TO_TICKS(500)); // the link closes on its own task
    const std::string url = url_for(build);
    ESP_LOGI(TAG, "fetching %s", url.c_str());
    const esp_err_t err = fetch_into_slot(url);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s fetched: it boots next, on trial", build.c_str());
        percent = 100;
        state = cubey::UpdateState::Done;
    } else {
        ESP_LOGE(TAG, "%s not fetched: %s", build.c_str(), esp_err_to_name(err));
        state = cubey::UpdateState::Failed;
        cubey::board::link_pause(false);
    }
    vTaskDelete(nullptr);
}

bool on_trial() {
    esp_ota_img_states_t running = ESP_OTA_IMG_UNDEFINED;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &running) == ESP_OK && running == ESP_OTA_IMG_PENDING_VERIFY;
}

} // namespace

namespace cubey::board {

void update_start() {
    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "running %s (build %s)%s", desc->version, build_of(*desc).c_str(), on_trial() ? ", on trial" : "");
    if (!on_trial()) return;
    static esp_timer_handle_t trial = nullptr;
    esp_timer_create_args_t args = {};
    args.callback = [](void *) {
        if (!on_trial()) return;
        ESP_LOGE(TAG, "never reached the server: back to the firmware before");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    };
    args.name = "trial";
    ESP_ERROR_CHECK(esp_timer_create(&args, &trial));
    ESP_ERROR_CHECK(esp_timer_start_once(trial, kTrialUs));
}

} // namespace cubey::board

namespace cubey::hal {

Firmware firmware() {
    const esp_app_desc_t *desc = esp_app_get_description();
    Firmware firmware{desc->version, build_of(*desc), ""};
    esp_app_desc_t failed = {};
    if (const esp_partition_t *bad = esp_ota_get_last_invalid_partition(); bad && esp_ota_get_partition_description(bad, &failed) == ESP_OK)
        firmware.failed = build_of(failed);
    return firmware;
}

void firmware_good() {
    if (!on_trial()) return;
    ESP_LOGI(TAG, "talks with the server: kept");
    esp_ota_mark_app_valid_cancel_rollback();
}

void update(const std::string &build) {
    if (state == UpdateState::Fetching) return;
    state = UpdateState::Fetching;
    percent = 0;
    if (xTaskCreatePinnedToCore(fetch, "update", kTaskStack, new std::string(build), 3, nullptr, 0) != pdPASS) state = UpdateState::Failed;
}

UpdateProgress update_progress() { return {state, percent}; }

void restart() { esp_restart(); }

} // namespace cubey::hal

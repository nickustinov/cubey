// The board's network: the Wi-Fi it keeps (in flash, the last its setup screens joined; at first sdkconfig.local's),
// its scans and joins for the setup (services/wifi_setup.h), and the server link
// over one WebSocket (esp_websocket_client, ws:// to the Mac or wss:// to Fly), which reconnects on its own. Text
// frames wait in the inbox for the core's UI task; binary frames are speech, for the speaker (audio.cpp). Sending
// goes through one FIFO and one task, so the UI and the mic never wait on a socket.
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "board.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal/hal.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "services/link_inbox.h"

namespace {

const char *TAG = "network";
constexpr int kReconnectMs = 2000;
// A task that does TLS (the link over wss:// to Fly): mbedtls's handshake and the certificate check need more
// than the 4–6 KB stacks tasks usually get (Espressif's HTTPS examples use 8 KB; Lapsa needed 10).
constexpr int kTaskStack = 10 * 1024;

std::mutex mu; // short inbox copies only; never held during socket I/O
cubey::LinkInbox inbox(384 * 1024, 512 * 1024, 16); // her word game's pictures come in messages of up to ~150 KB
esp_websocket_client_handle_t server = nullptr;
std::atomic<bool> connected{false};
std::atomic<bool> online{false};  // Wi-Fi has an address
std::atomic<unsigned> session{0}; // counts connections to the server
std::atomic<bool> restart_link{false};
std::atomic<bool> paused{false}, stop_link{false}; // an update fetches over its own connection (update.cpp)
std::atomic<cubey::Wifi> wifi_state{cubey::Wifi::Unset};
// The network kept, and one being tried (under mu): kept only once joined, so a wrong password never loses the one
// it had (it went back to that); network_task applies them, waiting for the last to let go.
struct Login {
    std::string ssid, password;
};
Login kept, trying;
int tries = 0;               // the one being tried, so far (under mu)
constexpr int kTries = 3;    // a handshake on a mesh may fail once
std::atomic<bool> want_join{false}, want_back{false};
std::atomic<bool> want_scan{false}; // the setup asked: network_task starts it (again, while joining is in its way)
std::vector<cubey::Network> scanned; // under mu
bool scan_done = false;              // under mu

// One FIFO keeps listen -> mic -> stop in order. Producers only copy; the client handle belongs to
// network_task. Storage is allocated once in PSRAM.
constexpr size_t kPacketBytes = cubey::board::kAudioPacketMost * sizeof(int16_t); // a mic frame, or a small JSON message
constexpr size_t kOutboxCount = 24;
struct Packet {
    unsigned session;
    uint32_t queued_at;
    uint16_t size;
    bool audio;
    uint8_t data[kPacketBytes];
};
QueueHandle_t outbox = nullptr;
std::atomic<uint32_t> last_traffic{0};

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

// Mic sound dropped for being late or finding the queue full: the link stays (Gemini misses a moment).
std::atomic<uint32_t> late_audio{0};

void drop_audio() {
    if (late_audio++ % 50 == 0) ESP_LOGW(TAG, "mic sound late: %lu packets dropped so far", static_cast<unsigned long>(late_audio.load()));
}

void reconnect(const char *reason) {
    if (connected.exchange(false)) ESP_LOGW(TAG, "%s; reconnecting", reason);
    restart_link = true;
}

void enqueue(const void *data, size_t bytes, bool audio) {
    if (!connected || !outbox) return;
    if (bytes > kPacketBytes) return reconnect("outgoing message too large");
    Packet packet{};
    packet.session = session;
    packet.queued_at = now_ms();
    packet.size = static_cast<uint16_t>(bytes);
    packet.audio = audio;
    memcpy(packet.data, data, bytes);
    if (xQueueSend(outbox, &packet, 0) != pdTRUE) audio ? drop_audio() : reconnect("outgoing queue full");
}

void on_link(void *, esp_event_base_t, int32_t id, void *event_data) {
    const auto *data = static_cast<esp_websocket_event_data_t *>(event_data);
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "server link connected");
        {
            std::lock_guard<std::mutex> lock(mu);
            inbox.clear();
        }
        cubey::hal::speaker_stop();
        session++;
        connected = true;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        if (connected) ESP_LOGW(TAG, "server link lost, retrying");
        connected = false;
        {
            std::lock_guard<std::mutex> lock(mu);
            inbox.clear();
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        if (data->error_handle.esp_ws_handshake_status_code == 401) ESP_LOGE(TAG, "the server wants a device token: CONFIG_CUBEY_DEVICE_TOKEN");
        break;
    case WEBSOCKET_EVENT_DATA: {
        if (!connected) break;
        cubey::LinkInbox::Result result;
        {
            std::lock_guard<std::mutex> lock(mu);
            result = inbox.receive(data->op_code, data->fin, data->payload_offset, data->payload_len, data->data_ptr, data->data_len);
        }
        if (result == cubey::LinkInbox::Result::Overflow) reconnect("incoming queue full or message too large");
        if (result == cubey::LinkInbox::Result::Speech) {
            last_traffic = now_ms();
            if (!cubey::board::speech_received(reinterpret_cast<const uint8_t *>(data->data_ptr), data->data_len))
                reconnect("speech buffer full");
        }
        break;
    }
    default: break;
    }
}

std::string kept_str(nvs_handle_t nvs, const char *key) {
    char value[65] = {};
    size_t size = sizeof value;
    return nvs_get_str(nvs, key, value, &size) == ESP_OK ? value : "";
}

// Who this Cubey is, to the server: its chip's own number, and a key of its own, made once, kept in flash, that no
// other can send (the server keeps it at its first meeting): one firmware for every Cubey, each told apart.
std::string device_headers() {
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);
    char id[13];
    snprintf(id, sizeof id, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    std::string key;
    if (nvs_handle_t nvs; nvs_open("cubey", NVS_READWRITE, &nvs) == ESP_OK) {
        key = kept_str(nvs, "key");
        if (key.empty()) {
            uint8_t random[24];
            esp_fill_random(random, sizeof random);
            for (uint8_t b : random) key += "0123456789abcdef"[b >> 4], key += "0123456789abcdef"[b & 15];
            nvs_set_str(nvs, "key", key.c_str());
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    return std::string("X-Cubey-Device: ") + id + "\r\nX-Cubey-Key: " + key + "\r\n";
}

// The token, if the server wants one, and who it is: on the link, and on an update's fetch.
const std::string &headers() {
    static const std::string headers = (strlen(CONFIG_CUBEY_DEVICE_TOKEN) ? std::string("Authorization: Bearer ") + CONFIG_CUBEY_DEVICE_TOKEN + "\r\n" : "") + device_headers();
    return headers;
}

void close_link() {
    connected = false;
    if (!server) return;
    esp_websocket_client_stop(server);
    esp_websocket_client_destroy(server);
    server = nullptr;
}

void start_link() {
    close_link();
    xQueueReset(outbox);
    esp_websocket_client_config_t config = {};
    config.uri = CONFIG_CUBEY_SERVER_URL;
    config.reconnect_timeout_ms = kReconnectMs;
    config.enable_close_reconnect = true;
    config.network_timeout_ms = 10000;
    config.crt_bundle_attach = esp_crt_bundle_attach; // wss:// (Fly): the server's certificate, checked
    config.task_stack = kTaskStack;
    config.task_core_id_set = true;
    config.task_core_id = 0; // the link must not displace the speaker and the UI on core 1
    config.buffer_size = 4096;
    config.headers = headers().c_str();
    server = esp_websocket_client_init(&config);
    ESP_ERROR_CHECK(server ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_websocket_register_events(server, WEBSOCKET_EVENT_ANY, on_link, nullptr));
    ESP_ERROR_CHECK(esp_websocket_client_start(server));
}

// A network's config, fresh: its name and password, and the mesh's settings. Nothing kept from the last (a copy of it
// kept the node the roaming app had picked, and a new network's password was refused there, 2026-10-12).
wifi_config_t config_for(const Login &login) {
    wifi_config_t wifi = {};
    strlcpy(reinterpret_cast<char *>(wifi.sta.ssid), login.ssid.c_str(), sizeof wifi.sta.ssid);
    strlcpy(reinterpret_cast<char *>(wifi.sta.password), login.password.c_str(), sizeof wifi.sta.password);
    // A mesh has several access points with one name: the fast scan joined the first it heard (channel 1, a far
    // node at -75 dBm, and speech stuttered); scan them all and join the strongest.
    wifi.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    // The mesh may steer it to a better node (802.11k/v, MBO), handed over without a new handshake (802.11r).
    wifi.sta.rm_enabled = wifi.sta.btm_enabled = wifi.sta.mbo_enabled = wifi.sta.ft_enabled = 1;
    wifi.sta.listen_interval = 3; // beacons slept through in modem sleep (WIFI_PS_MAX_MODEM): about 300 ms
    return wifi;
}

// Joins a network: a scan stopped, what joining before let go of (its config can't change while it connects), its
// config set.
void apply(const Login &login) {
    wifi_config_t wifi = config_for(login);
    esp_wifi_scan_stop();
    for (int tries = 0; tries < 20; tries++) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (esp_wifi_set_config(WIFI_IF_STA, &wifi) == ESP_OK) break;
    }
    ESP_LOGI(TAG, "joining %s", login.ssid.c_str());
    esp_wifi_connect();
}

void keep(const Login &login) {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK) return;
    nvs_set_str(nvs, "ssid", login.ssid.c_str());
    nvs_set_str(nvs, "password", login.password.c_str());
    nvs_commit(nvs);
    nvs_close(nvs);
}


// Sends the FIFO and owns (re)starting the link.
void network_task(void *) {
    bool awake = false;
    Packet packet;
    while (true) {
        if (stop_link.exchange(false)) close_link();
        if (!paused && restart_link.exchange(false) && online) start_link();
        if (want_scan && esp_wifi_scan_start(nullptr, false) == ESP_OK) want_scan = false;
        if (want_join.exchange(false) || want_back.exchange(false)) {
            Login login;
            {
                std::lock_guard<std::mutex> lock(mu);
                login = trying.ssid.empty() ? kept : trying;
            }
            apply(login);
        }
        if (xQueueReceive(outbox, &packet, pdMS_TO_TICKS(50)) == pdTRUE && connected && packet.session == session) {
            // Stale mic sound is no use to Gemini: dropped rather than sent late, and the talk goes on (the link
            // restarting for it made Puce's owl start over, hello and all, while it spoke and heard at once).
            if (packet.audio && now_ms() - packet.queued_at > 200) {
                drop_audio();
                continue;
            }
            last_traffic = now_ms();
            if (!awake && esp_wifi_set_ps(WIFI_PS_NONE) == ESP_OK) awake = true;
            const char *bytes = reinterpret_cast<const char *>(packet.data);
            // A send that times out drops the link (the client aborts it): a slow moment waits instead, and the
            // mic sound queued meanwhile goes stale and is dropped.
            const int sent = packet.audio ? esp_websocket_client_send_bin(server, bytes, packet.size, pdMS_TO_TICKS(2000))
                                          : esp_websocket_client_send_text(server, bytes, packet.size, pdMS_TO_TICKS(1000));
            if (sent != packet.size) reconnect("incomplete network write");
        }
        // Modem sleep when idle, waking for every third beacon (a message from the server waits a moment); streaming
        // speech must not wait for the access point's DTIM interval.
        const bool active = connected && now_ms() - last_traffic.load() < 15000;
        if (active != awake && esp_wifi_set_ps(active ? WIFI_PS_NONE : WIFI_PS_MAX_MODEM) == ESP_OK) awake = active;
    }
}

// Reconnecting after a drop is Espressif's roaming app's (sdkconfig.defaults): it skips a node that keeps failing
// and moves to a better one before the signal is gone.
// A network kept is joined from the start; one it can't join (a wrong password, not there) is Failed, for the setup.
void on_wifi(void *, esp_event_base_t base, int32_t id, void *event_data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START && wifi_state != cubey::Wifi::Unset) esp_wifi_connect();
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const uint8_t reason = static_cast<wifi_event_sta_disconnected_t *>(event_data)->reason;
        ESP_LOGW(TAG, "Wi-Fi lost (reason %d)", reason);
        online = false;
        cubey::Wifi failed = cubey::Wifi::Joining;
        switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT: failed = cubey::Wifi::Refused; break;
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD: failed = cubey::Wifi::NotFound; break;
        case WIFI_REASON_CONNECTION_FAIL: failed = cubey::Wifi::Failed; break;
        default: break;
        }
        std::lock_guard<std::mutex> lock(mu);
        if (failed != cubey::Wifi::Joining && !trying.ssid.empty()) {
            if (++tries < kTries) { // again, from the start
                want_join = true;
            } else { // the one tried, not joined: back to the one kept
                wifi_state = failed;
                trying = {};
                want_back = !kept.ssid.empty();
            }
        } else if (failed != cubey::Wifi::Joining) {
            wifi_state = failed;
        } else if (wifi_state == cubey::Wifi::Joined) {
            wifi_state = cubey::Wifi::Joining; // the roaming app joins again
        }
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        std::vector<wifi_ap_record_t> records(n);
        if (n) esp_wifi_scan_get_ap_records(&n, records.data());
        std::vector<cubey::Network> found;
        for (const wifi_ap_record_t &r : records) { // strongest first (the scan's order), each name once
            const std::string ssid(reinterpret_cast<const char *>(r.ssid));
            if (ssid.empty() || std::any_of(found.begin(), found.end(), [&](const cubey::Network &f) { return f.ssid == ssid; })) continue;
            found.push_back({ssid, r.rssi, r.authmode == WIFI_AUTH_OPEN});
        }
        std::lock_guard<std::mutex> lock(mu);
        scanned = std::move(found);
        scan_done = true;
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_ap_record_t ap = {};
        esp_wifi_sta_get_ap_info(&ap);
        ESP_LOGI(TAG, "Wi-Fi connected: node " MACSTR ", channel %d, %d dBm", MAC2STR(ap.bssid), ap.primary, ap.rssi);
        online = true;
        wifi_state = cubey::Wifi::Joined;
        {
            std::lock_guard<std::mutex> lock(mu);
            if (!trying.ssid.empty()) keep(kept = trying), trying = {}; // joined: kept from now on
        }
        // Moved to another node with the same address, the server link holds and the talk goes on.
        if (!connected || static_cast<ip_event_got_ip_t *>(event_data)->ip_changed) restart_link = true;
    }
}

} // namespace

namespace cubey::board {

void network_start() {
    static StaticQueue_t queue_state;
    auto *queue_storage = static_cast<uint8_t *>(heap_caps_malloc(kOutboxCount * sizeof(Packet), MALLOC_CAP_SPIRAM));
    ESP_ERROR_CHECK(queue_storage ? ESP_OK : ESP_ERR_NO_MEM);
    outbox = xQueueCreateStatic(kOutboxCount, sizeof(Packet), queue_storage, &queue_state);
    ESP_ERROR_CHECK(outbox ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, nullptr));
    // The network it keeps; none yet, the one esp_wifi kept before (its own flash, until 2026-10-12), or
    // sdkconfig.local's; else none: the setup. esp_wifi keeps its config in RAM from now on: only one joined is kept.
    if (nvs_handle_t nvs; nvs_open("wifi", NVS_READONLY, &nvs) == ESP_OK) {
        kept = {kept_str(nvs, "ssid"), kept_str(nvs, "password")};
        nvs_close(nvs);
    }
    if (wifi_config_t was = {}; kept.ssid.empty() && esp_wifi_get_config(WIFI_IF_STA, &was) == ESP_OK)
        kept = {reinterpret_cast<const char *>(was.sta.ssid), reinterpret_cast<const char *>(was.sta.password)};
    if (kept.ssid.empty()) kept = {CONFIG_CUBEY_WIFI_SSID, CONFIG_CUBEY_WIFI_PASSWORD};
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi = config_for(kept);
    wifi_state = kept.ssid.empty() ? cubey::Wifi::Unset : cubey::Wifi::Joining;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(network_task, "network_tx", kTaskStack, nullptr, 4, nullptr, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "joining %s; the server at %s", wifi.sta.ssid[0] ? reinterpret_cast<const char *>(wifi.sta.ssid) : "nothing yet (the setup)", CONFIG_CUBEY_SERVER_URL);
}

void link_send_audio(const int16_t *pcm, size_t samples) { enqueue(pcm, samples * sizeof(int16_t), true); }

void link_pause(bool pause) {
    paused = pause;
    if (pause) stop_link = true;
    else restart_link = true;
}

const std::string &server_headers() { return headers(); }

} // namespace cubey::board

namespace cubey::hal {

bool link_connected() { return connected; }
unsigned link_session() { return session; }

void link_send(const std::string &json) { enqueue(json.data(), json.size(), false); }

bool link_receive(std::string &json) {
    std::lock_guard<std::mutex> lock(mu);
    return inbox.pop(json);
}

Wifi wifi() { return wifi_state; }

void wifi_scan() {
    {
        std::lock_guard<std::mutex> lock(mu);
        scan_done = false;
    }
    want_scan = true;
}

bool wifi_networks(std::vector<Network> &networks) {
    std::lock_guard<std::mutex> lock(mu);
    if (!scan_done) return false;
    networks = scanned;
    return true;
}

// Tried on network_task; kept once joined.
void wipe() {
    ESP_LOGW(TAG, "full reset: all it keeps forgotten, starting again");
    esp_wifi_stop();
    nvs_flash_erase(); // its network, its key, the Wi-Fi's calibration
    esp_restart();
}

void wifi_join(const std::string &ssid, const std::string &password) {
    {
        std::lock_guard<std::mutex> lock(mu);
        trying = {ssid, password}, tries = 0;
    }
    wifi_state = Wifi::Joining;
    want_join = true;
}

} // namespace cubey::hal

// The board's speaker and mic, through its codecs (board.h): the ES8311 and the ES7210 mic ADC share one I2S
// bus, so both run at 16 kHz, 16-bit (as Waveshare's own firmware). The speaker task plays the server's speech
// from a buffer in PSRAM, 20 ms at a time, with the core's music mixed in under it; the mic task reads the first
// mic and, while the core streams, sends it to the server. The speaker's echo comes with the mic (board::kEchoReference): esp-sr's audio front end cancels
// it, so the server hears her over Cubey's speech (hears_over_speech), and its voice detector, louder than the room
// (services/talk_over.h), hears her talk over it: the speech ducks (services/duck.h). esp-sr's AGC adds the mic's digital gain (board::kMicDigitalDb). A
// second after all three have stopped, the amplifier and both codecs are parked (services/audio_idle.h); speech,
// music or the mic wakes them, and speech waits until they settle.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "board.h"
#include "esp_afe_sr_models.h"
#include "esp_agc.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/hal.h"
#include "services/audio_idle.h"
#include "services/duck.h"
#include "services/talk_over.h"
#include "services/mixer.h"
#include "services/speech_buffer.h"

namespace {

const char *TAG = "audio";
constexpr int kRate = 16000;
constexpr size_t kFrame = kRate / 50;     // 20 ms, mono
constexpr size_t kSpeechMax = 30 * kRate; // a long answer, buffered
constexpr uint32_t kPrebufferMs = 120;    // cushions bursts without delaying short clips indefinitely

esp_codec_dev_handle_t speaker = nullptr;
esp_codec_dev_handle_t mic = nullptr;
std::mutex codec_mu; // the codecs open or close only between mic reads
std::atomic<bool> codecs_open{false}; // changed under codec_mu
TaskHandle_t speaker_handle = nullptr;
TaskHandle_t mic_handle = nullptr;

// Speech waiting to play: a ring in PSRAM, filled by the network task, drained by the speaker task.
std::mutex mu;
cubey::SpeechBuffer speech(nullptr, 0, 0, 0);
std::atomic<bool> playing{false};
std::atomic<bool> streaming{false};
std::atomic<uint32_t> write_errors{0}, read_errors{0}, overflows{0};

// The audio front end: the echo cancelled, her voice heard (its detector on what is left, from the model
// partition) and the speech ducked while she talks over it; then the mic's digital gain, in 10 ms frames.
const esp_afe_sr_iface_t *afe_iface = nullptr;
esp_afe_sr_data_t *afe = nullptr;
// The detector's floor, before the gain: her voice over the speech was -35 to -48 dBFS, what the cancelling left of
// Puce's owl -59 to -69 (at -75 it ducked the owl for its own voice, 2026-10-10). Above it, louder than the room by
// TalkOver::kAboveDb: a TV across the room ducked Cubey and stopped it at its words (2026-10-11).
constexpr float kVoiceDbfs = -55;
cubey::TalkOver talk_over(kVoiceDbfs); // the front end's task's
std::atomic<bool> ducked{false};
std::atomic<bool> talked_over{false}; // she talks over Cubey (the server stops it), since the core last asked
void *agc = nullptr;
constexpr size_t kAgcFrame = kRate / 100;
std::atomic<float> peak_mic{0}, peak_echo{0}, peak_clean{0}; // the loudest while speech played, since the last report

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

float loudness(const int16_t *pcm, size_t n) {
    float sum = 0; // the S3 has a single-precision FPU; double uses software arithmetic
    for (size_t i = 0; i < n; i++) sum += float(pcm[i]) * pcm[i];
    return n ? std::min(1.0f, std::sqrt(sum / n) / 5000.0f) : 0;
}

esp_codec_dev_sample_info_t format(int channels, uint16_t mask) {
    esp_codec_dev_sample_info_t fs{};
    fs.sample_rate = kRate;
    fs.channel = channels;
    fs.channel_mask = mask;
    fs.bits_per_sample = 16;
    return fs;
}

void raise(std::atomic<float> &peak, float level) {
    if (level > peak) peak = level; // only the mic task raises it; a report in between may lose one
}

// Mic PCM to the server, in packets of a frame at most.
void send(const int16_t *pcm, size_t n) {
    constexpr size_t most = cubey::board::kAudioPacketMost;
    for (size_t at = 0; at < n; at += most) cubey::board::link_send_audio(pcm + at, std::min(most, n - at));
}

// While speech plays, for tuning: the loudest mic, echo and cancelled mic, how much quieter the cancelled mic is
// than the mic (their mean squares, summed frame by frame), the speech the detector heard (hers, a TV's), the room's
// background, and how often she was heard over it.
std::atomic<float> speech_mic{0}, speech_clean{0};
std::atomic<uint32_t> speech_frames{0}, voiced_frames{0}, heard_frames{0}, ducks{0};
std::atomic<int> speech_mic_peak{0}, speech_echo_peak{0}; // the largest sample: 32767 is clipping
std::atomic<float> voiced_dbfs{-100}, heard_dbfs{-100}, background_dbfs{-100}; // the loudest speech, the loudest hers

float square(const int16_t *pcm, size_t n, size_t step = 1) {
    float sum = 0;
    for (size_t i = 0; i < n; i++) sum += float(pcm[i * step]) * pcm[i * step];
    return sum / n;
}

// The mic and the echo, interleaved, as they go in.
void measure_in(const int16_t *both, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (std::abs(both[2 * i]) > speech_mic_peak) speech_mic_peak = std::abs(both[2 * i]);
        if (std::abs(both[2 * i + 1]) > speech_echo_peak) speech_echo_peak = std::abs(both[2 * i + 1]);
    }
    speech_mic = speech_mic + square(both, n, 2);
    ++speech_frames;
    raise(peak_mic, std::min(1.0f, std::sqrt(square(both, n, 2)) / 5000));
    raise(peak_echo, std::min(1.0f, std::sqrt(square(both + 1, n, 2)) / 5000));
}

// The cancelled mic as it comes out, this loud (dBFS); voice: the detector hears speech; heard: hers.
void measure_out(const int16_t *clean, size_t n, bool voice, bool heard, float dbfs) {
    speech_clean = speech_clean + square(clean, n);
    raise(peak_clean, loudness(clean, n));
    if (voice) ++voiced_frames, raise(voiced_dbfs, dbfs);
    if (heard) ++heard_frames, raise(heard_dbfs, dbfs);
    background_dbfs = talk_over.background();
}

bool resume_audio() {
    std::lock_guard<std::mutex> lock(codec_mu);
    cubey::board::audio_performance(true);
    auto out = format(2, 0), in = format(cubey::board::kMicChannels, cubey::board::kMicSlots);
    if (esp_codec_dev_open(speaker, &out) != ESP_CODEC_DEV_OK || esp_codec_dev_open(mic, &in) != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(mic);
        esp_codec_dev_close(speaker);
        cubey::board::audio_performance(false);
        return false;
    }
    esp_codec_dev_set_out_vol(speaker, cubey::board::kSpeakerVolume);
    if (cubey::board::kEchoReference) { // ES7210 gains by its inputs: the mic is MIC1 (as Lapsa's while quiet), the echo MIC3
        esp_codec_dev_set_in_channel_gain(mic, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), cubey::board::kMicGainDb);
        esp_codec_dev_set_in_channel_gain(mic, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), cubey::board::kEchoGainDb);
    } else
        esp_codec_dev_set_in_gain(mic, cubey::board::kMicGainDb);
    codecs_open = true;
    cubey::board::amplifier(true);
    if (mic_handle) xTaskNotifyGive(mic_handle);
    return true;
}

void park_audio() {
    std::lock_guard<std::mutex> lock(codec_mu);
    cubey::board::amplifier(false); // the 4B's has its own switch: closing the ES8311 alone doesn't turn it off
    esp_codec_dev_close(mic);
    esp_codec_dev_close(speaker);
    codecs_open = false;
    cubey::board::audio_performance(false);
}

void speaker_task(void *) {
    static int16_t mono[kFrame], stereo[2 * kFrame];
    float gain = 1; // the speech's: ducked while she talks over it
    cubey::AudioIdle idle;
    while (true) {
        bool pending;
        {
            std::lock_guard<std::mutex> lock(mu);
            pending = speech.samples() > 0;
        }
        if (!idle.update(now_ms(), streaming, pending, cubey::audio::sounding())) {
            if (idle.awake()) {
                park_audio();
                idle.parked();
                ESP_LOGI(TAG, "standby: amplifier and codecs off");
            }
            playing = false;
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)); // speech or the mic wakes it; the music within 100 ms
            continue;
        }
        if (!idle.awake()) {
            if (!resume_audio()) {
                ++write_errors;
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            idle.woke(now_ms());
        }
        size_t got = 0;
        if (idle.settled(now_ms())) {
            std::lock_guard<std::mutex> lock(mu);
            got = speech.read(mono, kFrame, now_ms());
            playing = got > 0;
        }
        std::fill(mono + got, mono + kFrame, 0);
        gain = cubey::duck_gain(gain, ducked);
        for (size_t i = 0; i < got; i++) mono[i] = static_cast<int16_t>(mono[i] * gain);
        cubey::audio::mix(mono, kFrame, kRate, got > 0);
        for (size_t i = 0; i < kFrame; i++) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
        if (esp_codec_dev_write(speaker, stereo, sizeof stereo) != ESP_CODEC_DEV_OK) {
            ++write_errors;
            vTaskDelay(pdMS_TO_TICKS(20)); // a failed codec must not turn this high-priority task into a spin loop
        }
    }
}

std::vector<int16_t> waiting, gained; // the mic for its digital gain, a part frame waiting for more (one task's)

// The mic to the server while the core streams it, with its digital gain.
void stream(const int16_t *pcm, size_t n) {
    if (!streaming) { // read anyway, so a new turn starts with fresh sound
        waiting.clear();
        return;
    }
    if (!agc) {
        send(pcm, n);
        return;
    }
    waiting.insert(waiting.end(), pcm, pcm + n);
    const size_t whole = waiting.size() / kAgcFrame * kAgcFrame;
    gained.resize(whole);
    for (size_t at = 0; at < whole; at += kAgcFrame)
        esp_agc_process(agc, waiting.data() + at, gained.data() + at, kAgcFrame, kRate);
    send(gained.data(), whole);
    waiting.erase(waiting.begin(), waiting.begin() + whole);
}

// Reads the mic (and the echo with it) and feeds the audio front end; without one, streams the first mic.
void mic_task(void *) {
    const size_t n = afe ? afe_iface->get_feed_chunksize(afe) : kFrame;
    std::vector<int16_t> both(2 * n), voice(n);
    while (true) {
        int result = ESP_CODEC_DEV_OK;
        bool awake;
        {
            std::lock_guard<std::mutex> lock(codec_mu);
            awake = codecs_open;
            if (awake) result = esp_codec_dev_read(mic, both.data(), both.size() * sizeof(int16_t));
        }
        if (!awake) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // the speaker task wakes it once the codecs are back
            continue;
        }
        if (result != ESP_CODEC_DEV_OK) {
            ++read_errors;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (afe) {
            if (playing) measure_in(both.data(), n);
            afe_iface->feed(afe, both.data());
            continue;
        }
        for (size_t i = 0; i < n; i++) voice[i] = both[2 * i];
        stream(voice.data(), n);
    }
}

// What the audio front end makes of it: the cancelled mic to the server, and her voice over the speech ducks it.
void afe_task(void *) {
    while (true) {
        if (!codecs_open) { // nothing is fed while the codecs are parked: waiting on it, esp-sr warns every 200 ms
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const afe_fetch_result_t *got = afe_iface->fetch(afe);
        if (!got || !got->data || got->data_size <= 0) continue;
        const size_t n = got->data_size / sizeof(int16_t);
        const bool voice = got->vad_state == VAD_SPEECH;
        // Her voice over the speech, only while the server listens
        const bool heard = talk_over.frame(playing && streaming, voice, got->data_volume, static_cast<uint32_t>(n * 1000 / kRate));
        if (playing) measure_out(got->data, n, voice, heard, got->data_volume);
        if (heard && !ducked) ++ducks;
        ducked = heard;
        if (talk_over.talking_over()) talked_over = true;
        stream(got->data, n);
    }
}

// Every 10 s, for bring-up, off the tasks that feed I2S: the echo cancelling while speech played, codec errors,
// dropped speech and memory.
void diagnostics_task(void *) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        if (const uint32_t frames = speech_frames.exchange(0)) {
            const float mic = speech_mic.exchange(0), clean = speech_clean.exchange(0);
            ESP_LOGI(TAG, "while speaking (%lu frames): loudest mic/echo/cancelled %.2f/%.2f/%.2f, largest samples mic/echo %d/%d, cancelled %.1f dB under the mic; speech in %lu frames, at most %.1f dBFS, the room %.1f dBFS; hers in %lu, at most %.1f dBFS, ducked %lu times",
                     static_cast<unsigned long>(frames), peak_mic.exchange(0), peak_echo.exchange(0), peak_clean.exchange(0),
                     speech_mic_peak.exchange(0), speech_echo_peak.exchange(0), 10 * std::log10((mic + 1) / (clean + 1)),
                     static_cast<unsigned long>(voiced_frames.exchange(0)), voiced_dbfs.exchange(-100), background_dbfs.load(),
                     static_cast<unsigned long>(heard_frames.exchange(0)), heard_dbfs.exchange(-100), static_cast<unsigned long>(ducks.exchange(0)));
        }
        ESP_LOGI(TAG, "errors out/in %lu/%lu, overflow %lu; internal free/min %u/%u, PSRAM free %u",
                 static_cast<unsigned long>(write_errors.exchange(0)), static_cast<unsigned long>(read_errors.exchange(0)),
                 static_cast<unsigned long>(overflows.exchange(0)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    }
}

} // namespace

namespace cubey::board {

void audio_start() {
    auto *storage = static_cast<uint8_t *>(heap_caps_malloc(kSpeechMax * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    speaker = speaker_codec();
    mic = mic_codec();
    if (kEchoReference) {
        // As xiaozhi's: esp-sr's full-duplex front end, low cost, nothing but the cancelling and the detector.
        srmodel_list_t *models = esp_srmodel_init("model");
        afe_config_t *config = afe_config_init("MR", models, AFE_TYPE_FD, AFE_MODE_LOW_COST);
        config->aec_init = true;
        config->aec_mode = AEC_MODE_FD_LOW_COST;
        config->aec_filter_length = 4;
        // Very aggressive: what the cancelling left of Puce's owl, Soniox heard as words of hers and stopped it (at
        // Espressif's default level it did, 2026-10-10); the ducking lets her voice through instead.
        config->aec_nlp_level = AEC_NLP_LEVEL_VERYAGGR;
        config->se_init = config->ns_init = config->wakenet_init = config->agc_init = false;
        config->vad_init = true;
        config->vad_model_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, nullptr);
        config->vad_energy_threshold = kVoiceDbfs;
        config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM; // in internal RAM the cancelling took 85 KB, leaving TLS 9 KB
        config->afe_perferred_core = 1;
        afe_iface = esp_afe_handle_from_config(config);
        afe = afe_iface ? afe_iface->create_from_config(config) : nullptr;
        if (!afe) ESP_LOGE(TAG, "no audio front end: the mic rests while Cubey speaks");
        else
            ESP_LOGI(TAG, "audio front end in %d-sample frames, voice detector %s; internal RAM free %u",
                     afe_iface->get_feed_chunksize(afe), config->vad_model_name ? config->vad_model_name : "WebRTC's",
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
        afe_config_free(config);
    }
    if (kMicDigitalDb) { // fixed digital gain, limited below full scale (WebRTC's AGC mode 3)
        agc = esp_agc_open(AGC_MODE_3, kRate);
        if (agc) set_agc_config(agc, kMicDigitalDb, 1, 3);
        else ESP_LOGE(TAG, "no digital gain: the mic is %d dB quiet", kMicDigitalDb);
    }
    if (!storage || !speaker || !mic) {
        ESP_LOGE(TAG, "no audio");
        free(storage);
        return;
    }
    speech = SpeechBuffer(storage, kSpeechMax * sizeof(int16_t), kRate * sizeof(int16_t) * kPrebufferMs / 1000, kPrebufferMs);
    amplifier(false); // parked until there is something to hear or listen to
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(speaker_task, "speaker", 4096, nullptr, 7, &speaker_handle, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(mic_task, "mic", 4096, nullptr, 6, &mic_handle, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    if (afe) ESP_ERROR_CHECK(xTaskCreatePinnedToCore(afe_task, "afe", 6144, nullptr, 5, nullptr, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(diagnostics_task, "audio_stats", 3072, nullptr, 1, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

bool speech_received(const uint8_t *data, size_t len) {
    std::lock_guard<std::mutex> lock(mu);
    if (speech.write(data, len, now_ms()) == len) {
        if (speaker_handle) xTaskNotifyGive(speaker_handle);
        return true;
    }
    ++overflows;
    return false; // reconnect instead of silently truncating or misaligning speech
}

} // namespace cubey::board

namespace cubey::hal {

void mic_stream(bool on) {
    streaming = on;
    if (on && speaker_handle) xTaskNotifyGive(speaker_handle);
}

bool hears_over_speech() { return afe != nullptr; }

bool voice_over_speech() { return talked_over.exchange(false); }

int speaker_rate() { return kRate; }

bool speaker_busy() {
    std::lock_guard<std::mutex> lock(mu);
    return speech.samples() > 0 || playing;
}

void speaker_stop() {
    std::lock_guard<std::mutex> lock(mu);
    speech.clear();
}

} // namespace cubey::hal

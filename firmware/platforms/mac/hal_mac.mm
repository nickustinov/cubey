// Mac simulator platform: the server link over an NSURLSession WebSocket; mic and speaker through AVAudioEngine,
// its voice processing cancelling the speaker's echo (as Puce's board's esp-sr does), so she can talk over the speech.
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <deque>
#include <mutex>

#include "hal/hal.h"
#include "lvgl.h"
#include "mac_hal.h"
#include "services/mixer.h"
#include "services/talk_over.h"

namespace {

constexpr int kMicRate = 16000;
constexpr int kSpeakerRate = 24000;

std::mutex mu;                 // guards socket, inbox and speech
NSURLSessionWebSocketTask *socket;
std::deque<std::string> inbox; // text messages from the server
std::deque<int16_t> speech;    // speech samples waiting to play
std::atomic<bool> connected{false};
std::atomic<unsigned> session{0}; // counts connections
std::atomic<bool> streaming{false};
std::atomic<bool> playing{false};     // speech in the speaker's last buffer
std::atomic<bool> talked_over{false}; // her voice over the speech, since the core last asked
// ponytail: her voice is the cleaned mic louder than this and the room while speech plays (services/talk_over.h; what
// macOS's voice processing leaves of the speaker is far quieter), no speech detector; one if a room's noise trips it.
constexpr float kVoiceDbfs = -40;
NSURL *server_url;
AVAudioEngine *engine;

void set_connected(bool on) {
    if (on && !connected) session++;
    if (connected.exchange(on) != on) printf("server link: %s\n", on ? "connected" : "lost, retrying every 2 s");
}

void connect();

void reconnect_later(NSURLSessionWebSocketTask *task) {
    {
        std::lock_guard<std::mutex> lock(mu);
        if (socket != task) return; // someone already reconnected
        socket = nil;
    }
    set_connected(false);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{ connect(); });
}

void listen(NSURLSessionWebSocketTask *task) {
    [task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage *msg, NSError *error) {
        if (error) return reconnect_later(task);
        set_connected(true);
        std::lock_guard<std::mutex> lock(mu);
        if (msg.type == NSURLSessionWebSocketMessageTypeString) {
            inbox.emplace_back(msg.string.UTF8String);
        } else {
            const auto *pcm = static_cast<const int16_t *>(msg.data.bytes);
            speech.insert(speech.end(), pcm, pcm + msg.data.length / 2);
        }
        listen(task);
    }];
}

void connect() {
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:server_url];
    if (const char *token = getenv("CUBEY_DEVICE_TOKEN"))
        [request setValue:[@"Bearer " stringByAppendingString:@(token)] forHTTPHeaderField:@"Authorization"];
    // Who it is, as a board says (network.cpp): one simulator, its key CUBEY_DEVICE_KEY's or its own.
    const char *key = getenv("CUBEY_DEVICE_KEY");
    [request setValue:@"simulator" forHTTPHeaderField:@"X-Cubey-Device"];
    [request setValue:@(key ? key : "simulator") forHTTPHeaderField:@"X-Cubey-Key"];
    NSURLSessionWebSocketTask *task = [NSURLSession.sharedSession webSocketTaskWithRequest:request];
    {
        std::lock_guard<std::mutex> lock(mu);
        socket = task;
    }
    [task resume];
    listen(task);
    [task sendPingWithPongReceiveHandler:^(NSError *error) {
        if (!error) set_connected(true); // failures surface in listen()
    }];
}

void send(NSURLSessionWebSocketMessage *msg) {
    NSURLSessionWebSocketTask *task;
    {
        std::lock_guard<std::mutex> lock(mu);
        task = socket;
    }
    [task sendMessage:msg completionHandler:^(NSError *) {}]; // failures surface in listen()
}

// The engine's audio threads: the mic sends straight to the server, the speaker drains the speech queue and the
// core's mixer adds the music.
void on_mic(AVAudioConverter *convert, AVAudioPCMBuffer *heard) {
    if (!streaming) return;
    AVAudioFormat *sent = convert.outputFormat;
    AVAudioPCMBuffer *pcm = [[AVAudioPCMBuffer alloc] initWithPCMFormat:sent
                                                           frameCapacity:heard.frameLength * sent.sampleRate / heard.format.sampleRate + 1];
    __block bool given = false;
    [convert convertToBuffer:pcm error:nil withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount, AVAudioConverterInputStatus *status) {
        *status = given ? AVAudioConverterInputStatus_NoDataNow : AVAudioConverterInputStatus_HaveData;
        return given ? nil : (given = true, heard);
    }];
    send([[NSURLSessionWebSocketMessage alloc] initWithData:[NSData dataWithBytes:pcm.int16ChannelData[0] length:pcm.frameLength * 2]]);
    static cubey::TalkOver talk_over(kVoiceDbfs); // this thread's
    double sum = 0;
    for (AVAudioFrameCount i = 0; i < pcm.frameLength; i++) sum += double(pcm.int16ChannelData[0][i]) * pcm.int16ChannelData[0][i];
    const double dbfs = 10 * std::log10(sum / std::max(1u, pcm.frameLength) / (32768.0 * 32768.0) + 1e-12);
    talk_over.frame(playing, true, static_cast<float>(dbfs), static_cast<uint32_t>(pcm.frameLength * 1000 / kMicRate));
    if (talk_over.talking_over()) talked_over = true;
}

OSStatus on_speaker(AVAudioFrameCount frames, AudioBufferList *buffers) {
    auto *out = static_cast<float *>(buffers->mBuffers[0].mData);
    int16_t pcm[480]; // 20 ms
    for (AVAudioFrameCount at = 0; at < frames;) {
        const size_t n = std::min<size_t>(frames - at, std::size(pcm));
        size_t i = 0;
        {
            std::lock_guard<std::mutex> lock(mu);
            for (; i < n && !speech.empty(); i++) {
                pcm[i] = speech.front();
                speech.pop_front();
            }
        }
        std::fill(pcm + i, pcm + n, 0);
        playing = i > 0;
        cubey::audio::mix(pcm, n, kSpeakerRate, i > 0);
        for (size_t k = 0; k < n; k++) out[at + k] = pcm[k] / 32768.0f;
        at += n;
    }
    return noErr;
}

void start_audio() {
    engine = [[AVAudioEngine alloc] init];
    AVAudioInputNode *mic = engine.inputNode;
    NSError *error = nil;
    if (![mic setVoiceProcessingEnabled:YES error:&error]) printf("no echo cancelling: %s\n", error.localizedDescription.UTF8String);
    AVAudioFormat *speech_format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:kSpeakerRate channels:1];
    AVAudioSourceNode *speaker = [[AVAudioSourceNode alloc] initWithFormat:speech_format
        renderBlock:^OSStatus(BOOL *, const AudioTimeStamp *, AVAudioFrameCount frames, AudioBufferList *buffers) { return on_speaker(frames, buffers); }];
    [engine attachNode:speaker];
    [engine connect:speaker to:engine.mainMixerNode format:speech_format];
    AVAudioFormat *heard = [mic outputFormatForBus:0];
    AVAudioFormat *sent = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:kMicRate channels:1 interleaved:YES];
    AVAudioConverter *convert = [[AVAudioConverter alloc] initFromFormat:heard toFormat:sent];
    convert.channelMap = @[@0]; // voice processing gives the cleaned mic in each of the device's channels
    [mic installTapOnBus:0 bufferSize:heard.sampleRate / 50 format:heard block:^(AVAudioPCMBuffer *buffer, AVAudioTime *) { on_mic(convert, buffer); }];
    if (![engine startAndReturnError:&error]) printf("no audio: %s\n", error.localizedDescription.UTF8String);
}

} // namespace

void mac_hal_init(const char *url) {
    server_url = [NSURL URLWithString:@(url)];
    start_audio();
    printf("server link: connecting to %s\n", url);
    connect();
}

static std::atomic<cubey::PowerPress> pressed{cubey::PowerPress::None};
static std::atomic<int> pretend_battery{80}; // percent, +1000 while charging

void mac_press(cubey::PowerPress press) { pressed = press; }
static std::atomic<bool> plus_held{false}, minus_held{false};
static cubey::Wifi pretend_wifi = cubey::Wifi::Joined; // the UI thread's
static std::string pretend_password;
static uint32_t joining_from = 0, scan_from = 0;
void mac_wifi_unset() { pretend_wifi = cubey::Wifi::Unset; }
static std::atomic<int> plus_presses{0}, minus_presses{0};
void mac_set_keys(bool plus, bool minus) {
    if (plus && !plus_held) ++plus_presses;
    if (minus && !minus_held) ++minus_presses;
    plus_held = plus, minus_held = minus;
}
void mac_set_battery(cubey::Battery battery) { pretend_battery = battery.percent + (battery.charging ? 1000 : 0); }
static std::atomic<int> turned{0}; // the pretend cube, quarters clockwise
static uint32_t pretend_speech_until = 0; // the UI thread's: Cubey's pretend speech plays until then
void mac_turn(int quarters) { turned = (turned + quarters) & 3; }
static std::atomic<int> shaking{0}; // readings still to swing
void mac_shake() { shaking = 20; }

void mac_pretend_online() { connected = true, session = 1; }
void mac_pretend_speech(uint32_t ms) { pretend_speech_until = lv_tick_get() + ms; }

void mac_receive(const char *json) {
    std::lock_guard<std::mutex> lock(mu);
    inbox.emplace_back(json);
}

namespace cubey::hal {

bool link_connected() { return connected; }
unsigned link_session() { return session; }

void link_send(const std::string &json) { send([[NSURLSessionWebSocketMessage alloc] initWithString:@(json.c_str())]); }

bool link_receive(std::string &json) {
    std::lock_guard<std::mutex> lock(mu);
    if (inbox.empty()) return false;
    json = std::move(inbox.front());
    inbox.pop_front();
    return true;
}

// ponytail: the mic runs all along, and its sound goes to the server only while on (a packet already on its way
// when it goes off is the server's to drop, as the board's late ones).
void mic_stream(bool on) { streaming = on; }

bool hears_over_speech() { return true; }

bool voice_over_speech() { return talked_over.exchange(false); }

int speaker_rate() { return kSpeakerRate; }

bool speaker_busy() {
    std::lock_guard<std::mutex> lock(mu);
    return !speech.empty() || static_cast<int32_t>(pretend_speech_until - lv_tick_get()) > 0;
}

void speaker_stop() {
    std::lock_guard<std::mutex> lock(mu);
    speech.clear();
}

Battery battery() {
    const int b = pretend_battery;
    return {b % 1000, b >= 1000};
}

PowerPress power_button() { return pressed.exchange(PowerPress::None); }
Keys keys() { return {plus_held, minus_held, plus_presses.exchange(0), minus_presses.exchange(0)}; }

// A pretend Wi-Fi (the Mac's own is the simulator's): a few networks found half a second after a scan is asked
// for; joining takes a second and a half, "wrong" is the wrong password and "gone" a network not there.
Wifi wifi() {
    if (pretend_wifi == Wifi::Joining && lv_tick_get() - joining_from >= 1500) pretend_wifi = pretend_password == "wrong" ? Wifi::Refused : pretend_password == "gone" ? Wifi::NotFound : Wifi::Joined;
    return pretend_wifi;
}
void wifi_scan() { scan_from = lv_tick_get(); }

// The simulator keeps nothing: it ends (--no-wifi starts it as flashed clean).
void wipe() {
    printf("full reset: run it again\n");
    exit(0);
}
bool wifi_networks(std::vector<Network> &networks) {
    if (lv_tick_get() - scan_from < 500) return false;
    networks = {{"Home", -42, false}, {"Home-5G", -55, false}, {"Kaimiņi", -68, false}, {"TP-Link_4F2A", -74, false},
                {"Kafejnīca ar garu vārdu", -80, true}, {"iPhone (Nick)", -83, false}};
    return true;
}
void wifi_join(const std::string &ssid, const std::string &password) {
    printf("Wi-Fi: joining %s\n", ssid.c_str());
    pretend_wifi = Wifi::Joining, pretend_password = password, joining_from = lv_tick_get();
}

// Turned a quarter clockwise, its left edge is up and its right edge down; shaken, the pull swings strong and weak.
Gravity gravity() {
    static const Gravity downs[] = {{0, 1, 0}, {1, 0, 0}, {0, -1, 0}, {-1, 0, 0}};
    const Gravity down = downs[turned];
    if (shaking <= 0) return down;
    const float pull = shaking-- % 2 ? 2.5f : 0.2f;
    return {down.x * pull, down.y * pull, 0};
}
void screen(bool) {} // no backlight: the core's black is the screen off
void brightness(int) {} // ponytail: a window doesn't dim; the board's panel does

} // namespace cubey::hal

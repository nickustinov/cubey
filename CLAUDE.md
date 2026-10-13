# Cubey

A small talking cube for children aged 2–6. Cubey, a kind and curious friend with two glowing eyes on a board of dots, hears the child through Soniox and answers with a language model in Soniox's voice (or speech to speech on Gemini Live), and does a few things besides talking: brushing, timers, games, drawing. This file is the guide for developing it, for people and coding agents alike.

- `README.md`: what Cubey does, every voice command, setup and deployment.
- `docs/design.md`: the decisions behind it and why. Read it before changing how Cubey talks, shows or behaves; add to it when you make a decision of the same weight.
- `CONTRIBUTING.md`: how to propose a change.

## Principles

A parenting tool, not a screen, a toy or a chatbot. Every change keeps these:

- **Back to real life.** A talk ends with the child's goodbye or half a minute of quiet; Cubey never asks them to stay.
- **The parent is in the loop.** Every exchange goes to the parents' log word for word; the big questions (death, babies, bodies, faith, family matters, war, their health, someone hurting them) are handed to the parents.
- **Warm and calm.** Nothing that invites staring. No text for the child but what Cubey writes as it says it (a word spelled, a sum): eyes, pictures and voice.
- **Private.** No audio is kept. Provider keys never go on a device.

## Repository map

```
server/                Bun + TypeScript: the talk, the models, the device protocol, the admin page
  src/server.ts        HTTP and WebSocket entry point (/device, /admin, /health)
  src/session.ts       one connected device; the protocol is documented at the top
  src/talk.ts          what a talk is to the session, and routineMessage(): a routine as a device message
  src/cascade.ts       the talk through Soniox (soniox.ts) and a model (llm.ts)
  src/models.ts        the models for each job (talk, games, drawing), chosen on the admin page
  src/live.ts          the talk on Gemini Live (tool calls, moving to a new connection on GoAway)
  src/cubey.ts         Cubey's prompt for either, reading its JSON answer, the drawing prompt
  src/spelling.ts      a word Cubey spells: its letters found in Soniox's timing of its speech
  src/lesson.ts        the word game's prompt and turns; lesson-game.ts its plan; curriculum.ts its content
  src/memory.ts        what Cubey knows of the child (memory.md); child.ts the child's type, languages, themes
  src/fleet.ts         the Cubeys, each by chip id and its own key; data under data/devices/<id>/
  src/history.ts       the talks before (SQLite); log.ts the parents' log
  src/admin.ts         the admin page; spending.ts and prices.ts what each Cubey costs
  scripts/             log, latency and lesson-pictures tools (bun run …)
firmware/              C++17 + LVGL 9.6
  core/                plain C++17 and LVGL, no platform includes
    cubey.cpp          the runtime: awake or asleep, the talk, routines, the buttons
    services/          logic without drawing: voice turns (hands-free or held to talk), talk-over detection, timers, brushing, dice, spelling, menu, Wi-Fi setup, keyboard
    ui/                drawing on the dot board (dots.h): eyes, ear, mouth, dial, die, letters, spelling, cards, menu, Wi-Fi; font.cpp the dot font
  hal/hal.h            what core needs from a platform
  platforms/mac/       the simulator (SDL window, AVAudioEngine with echo cancelling, headless screenshots)
  platforms/esp32/     the board: Waveshare ESP32-S3-Touch-AMOLED-2.16 (esp-sr echo cancelling, CO5300 panel)
  tests/               doctest unit tests against a fake HAL
content/lesson/        the word game's curriculum and its pictures (Fluent Emoji, MIT: LICENSES.md)
docs/                  design notes and README screenshots
```

## Setup

Requirements: macOS (the simulator uses AVAudioEngine), Homebrew, [Bun](https://bun.sh) 1.4+, and for the board ESP-IDF 6.1.

```sh
brew install cmake ninja sdl2
cp server/.env.example server/.env        # fill in SONIOX_API_KEY, and GEMINI_API_KEY or OPENAI_API_KEY
cd server && bun install
```

`server/.env` holds no child: each Cubey gets to know its own by talking. For the board, clone ESP-IDF 6.1 into `~/esp/esp-idf-v6.1` and run `./install.sh esp32s3`, then put the board's server and token in the git-ignored `firmware/platforms/esp32/sdkconfig.local`:

```
CONFIG_CUBEY_SERVER_URL="ws://<your-mac>.local:8788/device"
CONFIG_CUBEY_DEVICE_TOKEN="<CUBEY_DEVICE_TOKEN from server/.env>"
```

With `HOST=0.0.0.0` and `CUBEY_DEVICE_TOKEN` in `server/.env` so the board can reach the Mac's server, or `wss://<your-app>.fly.dev/device` for a deployed one. Leave the Wi-Fi empty: the cube joins a network on its own screens. After changing `sdkconfig.local`, `rm firmware/platforms/esp32/sdkconfig` so the build picks it up.

## Commands

```sh
cmake -S firmware -B build/firmware -G Ninja && cmake --build build/firmware   # simulator + tests
ctest --test-dir build/firmware --output-on-failure                             # firmware tests
build/firmware/platforms/mac/cubey-sim                                           # simulator window
build/firmware/platforms/mac/cubey-sim --screenshot build/shots/eyes.png [--online] [--battery 30|30+] [--turn 1] [--message JSON] [--tap X,Y] [--hold X,Y] [--power short|long] [--keys plus|minus|hold-plus|hold-minus] [--speech MS] [--no-wifi] [--ms 1000]

cd server && bun run dev                                                         # server on ws://127.0.0.1:8788/device
cd server && bun test && bunx tsc --noEmit                                       # server tests and types
cd server && bun run log [count]                                                 # the parents' log
cd server && bun run lesson-pictures                                             # fetch the word game's pictures into content/lesson/pictures/
cd server && bun run latency [runs] ["question"]                                # the child's last word to Cubey's first sound, through the running server

fly deploy                                                                       # the server to Fly.io (fly.toml)
fly logs                                                                         # its log

. ~/esp/esp-idf-v6.1/export.sh && idf.py -C firmware/platforms/esp32 -B build/esp32 build             # the board build
idf.py -C firmware/platforms/esp32 -B build/esp32 -p /dev/cu.usbmodem* flash monitor                   # flash the board and watch its log
```

In the simulator window the mouse is a finger, `p` the power button (`shift+p` held long), `=` and `-` the board's "+" and "–" keys (held 5 s: the Wi-Fi setup, the grown-ups' menu), `b` the pretend battery, `r` turns the pretend cube a quarter and `s` shakes it. `--message` takes any server-to-device message from the protocol in `session.ts`, so every screen can be rendered without a server:

```sh
build/firmware/platforms/mac/cubey-sim --screenshot build/shots/timer.png --online --message '{"type":"timer","minutes":40,"for":"cartoons"}'
```

## How we work

- **Tests first** for all logic: `bun test` beside each server module (`*.test.ts`), doctest in `firmware/tests/` against `fake_hal`. Providers are pretended in tests; nothing calls a real API.
- **Look at the screen.** Review every screen change with `--screenshot` and open the PNG.
- **Keep the prompt short.** A game played in the talk has one line in Cubey's prompt, and its rules only while the child plays it (`Playing` in `cubey.ts`); a routine with a talk of its own (the word game) has its own prompt.
- **Let code judge what models can't.** A model sees words, not letters: the server spells and judges the games with letters. Anything that must be exact (a letter, a count, a range) is checked in code, not trusted to the model.
- **One dot font.** Any text or digits on screen (a timer's digits, a game's letter, what Cubey writes) are drawn on the dot board in `firmware/core/ui/font.cpp`.
- **`core/` stays portable.** Plain C++17 and LVGL; anything platform-specific goes behind `hal/hal.h`.
- **Document the protocol.** A new message between device and server goes in the comment at the top of `server/src/session.ts`.
- **Secrets** live in `server/.env` (git-ignored). Provider keys never go on a device.
- **Style.** Match the surrounding code: comments explain what a thing is and why, in plain sentences; no dead code. In prose, en dashes (–), never em dashes, and sentence-case headings.

## Adding a routine

A routine is anything Cubey does besides talk. It travels from the model's answer to the dot board like this:

1. **Prompt** (`server/src/cubey.ts`): a line telling Cubey when to set it, under the right heading; a game's rules go in `Playing`, only while it's played. Add its name to `ROUTINES` and read any fields it needs in `readAnswer`.
2. **Message** (`server/src/talk.ts`): what `routineMessage` sends the device for it. Anything more (a game's state between turns) goes in `cascade.ts`.
3. **Protocol** (`server/src/session.ts`): document the message, and any message back from the device.
4. **Gemini Live** (`server/src/live.ts`), optionally: a tool for it.
5. **Device**: parse it in `firmware/core/services/message.cpp`, handle it in `firmware/core/cubey.cpp`, keep any timing or state in `core/services/`, draw it in `core/ui/` on the dot board.
6. **Tests**: `cubey.test.ts`, `cascade.test.ts` and `session.test.ts` on the server; a `firmware/tests/<name>_test.cpp` for the device; then a `--screenshot` with `--message`.
7. **Docs**: a row in the routines table of `docs/design.md`, and its voice command in `README.md`.

## Gotchas

- **Providers' terms.** Gemini's terms forbid apps used by under-18s, and OpenAI's need checking for a child's use: an open question (`docs/design.md`).
- **Soniox.** Some projects allow one TTS stream at a time: don't share one with another app. A TTS stream with no text for about 10 s is dropped, so the voice connects when the child's pause is heard.
- **Gemini Live** bills the whole talk so far again on every turn, with no caching: the context is capped (`CONTEXT_MOST` in `live.ts`), and each connection's cost is logged when it ends. Gemini ends a connection after about ten minutes, with a GoAway first: the talk moves to a new one between turns.
- **macOS** asks for microphone access for the terminal the simulator runs in, and the firewall may ask whether Bun may accept connections (the board's).
- **LVGL 9.6** moved its public headers to `include/lvgl/` and deprecated `lv_obj_add_flag` (use `lv_obj_set_clickable` and friends). Don't load a new screen inside the old screen's own event: LVGL grows that screen's event list while it walks it, a use-after-free; `lv_async_call` it. An AddressSanitizer build finds such things: `cmake -S firmware -B build/asan -G Ninja -DCMAKE_CXX_FLAGS=-fsanitize=address -DCMAKE_C_FLAGS=-fsanitize=address -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address`.
- **The board's audio.** The ES7210 is read in TDM (`board_amoled_2_16.cpp`, not the BSP's stereo), its inputs in slots ordered MIC1, MIC3, MIC2, MIC4: the speaker's echo, wired to MIC3, is slot 1, for esp-sr to cancel. While the speaker sounds the mic is at 12 dB (more clips on the echo, which then can't be cancelled), while quiet 37.5 dB. esp-sr's echo canceller lives in PSRAM: in internal RAM it left TLS too little.
- **The board's display.** The BSP sends LVGL's strips from PSRAM by copying each into internal RAM, which runs out ("Failed to allocate priv TX buffer"): `board_amoled_2_16.cpp` sets the CO5300 panel up itself, its DMA reading PSRAM directly. That BSP's `bsp_display_lock(0)` doesn't wait, and its config macros are C only.
- **The board's internal RAM.** On the board an allocation under 1 KB comes from internal RAM, which Wi-Fi, TLS and the display need; 1 KB and more goes to PSRAM (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`). Keep lasting data in a few large allocations, not many small ones: the dot font keeps its letters' rows as views of its glyphs' text in one store (`ui/font.cpp`).
- **The board's link.** It sends `overhear` when its speech starts playing, often after the server has sent all of it (Soniox makes speech faster than it plays), so the server can't tie overhearing to its own speaking. A late mic packet is dropped, never a reason to reconnect: a reconnect loses the talk.
- **The power button** can't be held for long gestures: its chip reports a press and a 2 s hold, and cuts the power at 4 to 10 s. That's why the menus use "+" and "–".
- **Heritage.** Cubey grew out of two earlier projects by the same author, Puce and Lapsa. Code comments mention them ("Puce's owl's") as the origin of some code; neither is needed to build or run Cubey.

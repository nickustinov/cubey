# Cubey – design notes

The decisions behind Cubey and why they were made. Read this before changing how Cubey talks, what it shows or what it does; when you make a decision of the same weight, add it here.

## Principles

| Decision | Why |
| --- | --- |
| A parenting tool, not a screen, a toy or a chatbot. Conversation comes first; everything else is a routine Cubey does in its own voice. | A perfect talk first, then more. |
| A talk ends with the child's goodbye or half a minute of quiet. Cubey never asks them to stay, come back or talk more, never says it loves or will miss them, keeps no secrets and makes no promises. | Back to real life. |
| Every exchange goes to the parents' log word for word. The big questions (death, babies, bodies, faith and family traditions, family matters, war and scary news, why people differ, the child's own health, someone hurting them) are not answered but handed to the parents and marked. | The parent stays in the loop. |
| Warm and calm, nothing that invites staring. No text for the child but what Cubey writes as it says it (a word spelled, a sum). | Eyes, pictures and voice are enough for 2–6. |
| No audio is kept. Provider keys live on the server only; a device holds only its own key. | Private and safe by construction. |
| A parent sets nothing up: Cubey gets to know the child by talking (their name, their age), as a friend would. | No app; the cube and the server are all there is. |

## Hearing and speaking

| Decision | Why |
| --- | --- |
| Two ways, chosen by `CUBEY_TALK`. **cascade** (default): the child's voice to Soniox speech-to-text, the text to a model answering in JSON, its words spoken by Soniox text-to-speech as they are written (`cascade.ts`). **live**: speech to speech on Gemini Live (`live.ts`), with only the battery, brushing and timers. | Gemini Live understood a small child's Latvian poorly; Soniox hears it well. |
| The model answers in JSON: what it says first, then `bye`, `for_parents`, a `routine` and its fields. Its words stream to the voice as they are written; a voice cue is never cut in two. | One answer carries both speech and actions, and speech starts before the answer is finished. |
| OpenAI's JSON mode is not used; the history of JSON answers keeps the model writing JSON. | JSON mode added about 180 ms to the first word; plain history made the model drift into plain sentences. |
| Soniox decides when the child's turn ends, and only then is the model asked. An answer begun unspoken once their words stopped changing for 300 ms was tried and dropped. | Turn-detection models have no Latvian or children's model. The early answer was seldom the one spoken (Soniox gives the last word only just before it ends the turn), and cost a third of the model's calls. |
| Cubey's prompt keeps its rules first and what changes turn by turn last: the time of day, the battery, what's on the screen, the game being played. The word game's does the same. | A model's cache holds only the start that requests share. With the time of day on the second line, Gemini cached nothing and GPT-6 Sol 5%. |
| In the cascade, Cubey's words carry Soniox voice cues (`[warm]`, `[curious]`, `[whispering]`, `[giggles]`, `[pause]`…), *stress*, "..." and little sounds written as words (Hmm, Ooh). Only a fixed palette, nothing sad or scary. | An unknown tag may be read aloud. |
| Gemini Live: the context is capped at 5,000 tokens, trimmed to 3,000; the prompt and the 10 latest exchanges from before always stay. When Gemini ends a connection (about ten minutes, with a GoAway first), the talk moves to a new one with its resumption handle at the next quiet moment. | Gemini bills the whole context again every turn, with no caching; a long talk shouldn't stop. |

## Talking over Cubey

| Decision | Why |
| --- | --- |
| While Cubey speaks, a device that cancels its own echo keeps streaming the mic (`overhear`). The device's voice detector judges: a quarter of a second of the child's voice, 10 dB louder than the room as measured over the last 3 s while Cubey speaks (`services/talk_over.h`). Then it sends `voice` and ducks Cubey; the server has Soniox hear the last second of mic and stops Cubey at the child's first word (`interrupted`). No word within 2 s, Cubey goes on. | Soniox's words alone stopped Cubey on its own echo; the room's level keeps a TV from counting as the child. |
| `CUBEY_TALK_OVER=0` turns it off: the mic rests while Cubey speaks. | A very loud room can still fool the detector. |

## Prompt and memory

| Decision | Why |
| --- | --- |
| The prompt stays short. Each game has one line in it, and its rules are added only while the child plays it (`Playing` in `cubey.ts`). The word game has its own prompt. | Otherwise the prompt grows with every routine. |
| Cubey knows its routines and now and then offers one when it fits the moment (brushing at bedtime, a game when the child is bored), as a question, starting only after a yes; never one it doesn't have. | A routine offered at the right moment gets used. |
| A story is told whole in one answer: a beginning, something that happens, a happy end. | Otherwise models tell a sentence and ask a question, over and over. |
| What Cubey knows of the child is `memory.md` per device (`memory.ts`): name, gender, birthday, the language it speaks and the one it teaches, the household's time zone, brightness and theme. What the child or a grown-up tells it is kept (`child` in the answer); a grown-up can edit it on the admin page. Not known yet, Cubey guesses no gender, speaks as to a 4-year-old and asks the name or age now and then. | A parent sets nothing up. |
| A child's talks before and word-game progress follow their `id` (their first name as first known), so a new name keeps them. | "Call me Nick" shouldn't wipe what Cubey knows. |

## Models by job

| Decision | Why |
| --- | --- |
| A model for each job (talk; games, including the word game; drawing), chosen on the admin page for all Cubeys and used from the next turn (`models.ts`). Only priced models are offered, and only those whose provider key is set; the others are greyed out. Defaults: Gemini 3.5 Flash-Lite for talk and games, GPT-6 Astra for drawing when there's an OpenAI key, else Flash-Lite; a chosen model whose key is gone gives way to the default. The turn that starts a game is the talk's. Reasoning as low as each model allows. | Flash-Lite is the quickest and cheapest, and fine in English; the server judges the letters games' words itself, where fast models praised words on the wrong letter. In a contest of ten models drawing ten things, GPT-6 Astra drew best and Flash-Lite drew blobs. |

## Screen

| Decision | Why |
| --- | --- |
| A board of 24 by 24 square dots over the whole black screen, every dot faintly there as on an old LED panel, each drawn as 2 by 2 small dots. Everything (eyes, letters, pictures, dials) is drawn on it (`ui/dots.h`). The Wi-Fi setup and menu use a finer 60 by 60 board in the same look. | Old school: a child sees a face, not a screen. |
| Two amber eyes (5 by 7 dots, corners cut) that blink every 2.5–7 s, glance aside now and then and squint sometimes. Offline the eyes stay shut; asleep the screen is off. | Alive without inviting staring. Round eyes with pupils read as holes. |
| The picture turns a quarter at a time to stay upright, from the accelerometer: once down is 15° past half-way to the next quarter and has held 400 ms; lying flat it keeps its way. One place does it (`dots::set_turn`), so every screen follows. | A cube has no top. |
| All text and digits use one dot font (`ui/font.cpp`), with Latvian letters drawn in its style and capitals and signs added in the same style. | One alphabet of dots. |
| Colour themes per Cubey: amber, green, red, blue, VFD, white. Chosen on its menu or the admin page. | A look of its own for each home. |
| Each kind of dot is a small pre-drawn picture, and a run of alike dots in a row is drawn as one tiled image. | Much faster on the board than a square per dot. |

## Routines

What Cubey does besides talk, whenever the child asks in any words: a `routine` field of the cascade's answer (a tool on Gemini Live), shown on the dot board in place of the eyes. The server turns it into a device message (`routineMessage` in `talk.ts`); the device draws it (`firmware/core/ui/`).

| Routine | Decision | Why |
| --- | --- | --- |
| Battery | The device reports its charge on each connection and as it changes; asked, Cubey says the percentage and whether it's charging while a battery of dots shows for 6 s. A device that can't tell says so. | The simplest routine, first. |
| Brushing | Cubey's mouth shows; the two minutes start at the child's tap (so they can fetch their toothbrush first), in four parts of 30 s round the mouth, a toothbrush scrubbing on the beat, teeth shining as they go. The device keeps the time and tells the server each part; Cubey says where to brush next and praises the end. No tap for a minute, it's off. | A routine a child seeks out. |
| Timers | 1–180 minutes, for something (cartoons, tidying, bath, eating, sleep, reading, playing, other): a little picture, the minutes left, a bar round the board. The talk ends once it's on (nobody listens or pays while it runs); reminders at 5 and 1 minute left; at the end Cubey asks whether they've finished. | Ending screen time calmly, asked for by voice. A ring of 60 seconds looked like minutes racing by, so the bar runs over the whole timer. |
| Dice | A die tumbles and lands within 1.1 s; shake or tap rolls again; Cubey says each number once, nobody listening. Ten minutes without a roll, it sleeps. Each roll is seeded by the moment of the tap or shake. | Board games with the family. LVGL's random numbers start the same at every boot. |
| Finding things | Cubey asks for one safe thing in the room at a time, easy for the child's age, and keeps the talk open up to three minutes while they look; it believes what they found (it can't see). Never knives, the stove, medicine, sockets, high shelves, windows or outside. | Real-world play, away from the screen. |
| Word chain, words on one letter | The server spells and judges: whether the child's word begins with exactly the right letter (a letter with a mark is its own), and that no word is said twice. The model only says which word the child meant (`hers`), so "Aitā." is read as "aita". The letter shows big; in the chain the words scroll in as they are said. Who gives up loses (Cubey never does). | A model sees words, not letters: it praised words on the wrong letter. |
| How many dots | 1 to 10 dots at random places; right, a new number (the old one shown big first); wrong, they count together. The server only ever sends the new number. | Sending the same picture again would give the answer away. |
| Drawing and writing | The talk's model names what to show (`show`, in English, on any turn, first in its answer); a drawing model makes it: a 24 by 24 picture with three shades, or text in the dot font (a sum, a number), still if it fits, else scrolling. It shows for 8 s. A word the child wants written goes to spelling instead. While it is being drawn (5 to 14 s), faint dots come and go in the board's middle like a sketch forming, each sketch its own; a drawing that comes to nothing brings the eyes back. | English avoids mistranslation ("suns" is Latvian for dog). Naming it first means the picture is on its way while Cubey speaks. |
| Spelling words | The model writes the word first (`spell`, before what it says), then says it, its letters each alone, and the word again ("Cat: c, a, t. Cat!"). The server spells it and sends it to the device at once; Soniox times each character it speaks (asked only for an answer that spells), and a letter of the word said alone, in order, is that letter said (`spelling.ts`). The device lights each letter big the moment it plays, counted from when its speech began: the word slides on to it, those before dim; then the whole word passes twice. A letter never timed shows when the speech is over. On the smart model while played. | The child sees how a word is written as they hear it. Timestamps put each letter on its sound, where a steady pace would drift; the server spells, as a model sees words, not letters. |
| The word game | Cubey teaches words of another language with picture cards (Fluent Emoji, full screen), from a fixed curriculum, remembering progress in Leitner boxes (SQLite). Each step is said in the child's language with the target words spoken in the target language. It never ends by itself: silence asks again; it ends when the child wants something else, says goodbye or presses the button. Cascade only. | Learning without a test: the game ends only when the child ends it. |
| For grown-ups | Brightness (10–100%), the menu and the Wi-Fi setup by voice; the menus open once Cubey has said so, and the talk is over until they close. | The settings a parent would look for, without the hidden keys. |

## Grown-ups' settings

| Decision | Why |
| --- | --- |
| The menu is hidden from the child: "–" held 5 s opens and closes it. Three lines (the language Cubey speaks, the one it teaches, the theme) stepped by tapping arrows, a brightness slider, OK. On close, changes go to the server, which keeps them and opens the talk anew. | Not by voice for languages: a child could switch them by accident. A both-keys hold was too hard to find. |
| Wi-Fi is set up on the cube: "+" held 5 s, or by itself when there's no network. A list of networks by signal, and a password on a keyboard of dots (ten keys by four, shift, symbols). The board keeps the network in flash. | Taken to a new home, set up on the cube itself. The power button can't do it: its chip cuts power on a long hold. |

## Several Cubeys and the admin page

| Decision | Why |
| --- | --- |
| One firmware for every Cubey. Each sends its chip's id and a key it made once and keeps in flash, with the fleet's `CUBEY_DEVICE_TOKEN`; met for the first time, the server keeps the key's hash, and only that key will do from then on (`fleet.ts`). A grown-up may block it. | Many cubes, nothing configured per cube. |
| Each Cubey has its own child, talks, word game and parents' log under `data/devices/<id>/`. | Each home is separate. |
| A new Cubey: "HI!", its Wi-Fi on its own screens, then its language menu opens by itself, then Cubey introduces itself and asks the child's name. | Set up on the cube, by anyone. |
| `/admin` behind `CUBEY_ADMIN_PASSWORD`, with a cookie only the server can make (HttpOnly, Secure, SameSite=Strict): every Cubey, its child, its talks with the big questions marked, what it cost, and a full reset (with a confirmation) that wipes it on the server and, if it's on, the cube itself. Graphs are SVG drawn on the server, with no script on a signed-in page. | No app. Seeing what to make cheaper. |
| What each Cubey spends is counted: every model's tokens, the seconds Soniox heard and spoke, Gemini Live's connections (`spending.ts`, prices in `prices.ts`). | Cost is visible per home and per kind. |

## Power

| Decision | Why |
| --- | --- |
| Awake, a press of the power button is a step back: from a routine to the eyes, from the eyes to sleep. A long press sleeps. Asleep, a press wakes it to Cubey's hello. Without the server for 45 s, it sleeps. | A press mid-brushing that slept straight away surprised the child. |

## The board

| Decision | Why |
| --- | --- |
| Waveshare ESP32-S3-Touch-AMOLED-2.16: esp-sr cancels the echo and ducks Cubey 14 dB while the child talks over it, the speaker at full volume. | Echo cancelling on the device is what makes talking over Cubey possible. |
| The codecs stay awake 3 s after the mic rests. | Parked a second after, they made Cubey's first words wait 0.5 s to wake them. |
| Mesh Wi-Fi: 802.11k/v, MBO and 802.11r on; Espressif's roaming app reconnects after a drop and moves to a stronger node below −67 dBm. | A far node stuttered; a home with a mesh shouldn't drop a talk. |
| TLS, Wi-Fi and lwIP buffers in PSRAM, and AES in software. | Internal RAM ran out mid-talk and broke the link. |

## The server

| Decision | Why |
| --- | --- |
| Bun on Fly.io: one shared-cpu-1x machine with 256 MB, always on (each Cubey keeps a WebSocket open), SQLite and the logs on a 1 GB volume (`CUBEY_DATA=/data`). | Small, cheap and enough. |

## Open questions

- Gemini's terms forbid apps used by under-18s, and OpenAI's need checking for a child's use. Settle the providers before any child uses Cubey.
- On Gemini Live, the child's words come from Gemini's transcript, which can arrive after its answer; then they are logged with the next turn (`live.ts`).

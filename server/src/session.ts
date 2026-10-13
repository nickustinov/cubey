// One connected device, over its WebSocket: Cubey's talk with the child (live.ts). It wakes to Cubey's hello, then
// listens hands-free and Cubey answers, until a goodbye (hers, said back by Cubey) or a quiet while puts it to sleep.
//   device -> server: {"type":"hello", speaker_rate}  the rate it plays speech at
//                     {"type":"open", again?, timer?, for?, dice?}   Cubey woke: its hello; again: the device lost the
//                                                       server mid-talk and goes on, without a hello; timer: "done"
//                                                       or "stopped", the timer for `for` is, and Cubey says so
//                                                       instead; dice: "stopped", put away with the button
//                     {"type":"listen"}, then mic PCM   she may talk (16 kHz mono)
//                     {"type":"overhear"}, then mic PCM Cubey's speech plays: she may talk over it
//                     {"type":"voice"}                  its own detector heard her voice over Cubey's speech
//                     {"type":"stop"}                   what Cubey is saying is dropped (an answer that never came)
//                     {"type":"close"}                  the talk is over: Cubey sleeps
//                     {"type":"state", battery?, charging?} its battery (0–100), when it changes and on each
//                                                       connection; none: it can't tell
//                     {"type":"brushing.part", part}    brushing: a part has begun (1–4; her tap begins the first)
//                     {"type":"brushing.done"}, {"type":"brushing.missed"}, {"type":"brushing.stopped"}   the two
//                                                       minutes are up; no tap to start; she stopped it (the button)
//                     {"type":"timer.left", minutes, for}   a timer has this many minutes left: Cubey reminds her,
//                                                       once, nobody listening (the talk is over while it runs)
//                     {"type":"tapped", id}             her word game: she tapped this picture, as her answer
//                     {"type":"lesson.stopped"}         her word game: she stopped it (the button)
//                     {"type":"dice.rolled", n}         the die landed on n (1–6): Cubey says it, once, nobody
//                                                       listening (no talk while the die is out)
//   device -> server: {"type":"settings", language, learns, theme, brightness}   its menu: the language Cubey speaks,
//                                                       the one it teaches, its screen's theme and brightness
//                                                       (memory.ts); the talk opens anew in it
//   server -> device: {"type":"settings", language, learns, languages, new, brightness, theme}   to its hello, and
//                                                       once they are set: for its menu, with the languages Cubey
//                                                       speaks; new: not set up yet, its menu opens by itself;
//                                                       brightness: its screen's, in percent (10–100), a grown-up's;
//                                                       theme: its screen's colours (its menu, the admin page)
//   server -> device: {"type":"talk_over", on}          to its hello: whether she may talk over Cubey (CUBEY_TALK_OVER);
//                                                       off, the mic rests while Cubey speaks and nothing ducks it
//                     {"type":"listening"}              every listen's
//                     {"type":"heard"}                  she paused: the mic can rest
//                     binary speech PCM at the device's rate
//                     {"type":"said", heard, said, last} spoken; last: its goodbye, then Cubey sleeps
//                     {"type":"interrupted"}            she talked over it: the speech stops, the mic goes on as her turn
//                     {"type":"failed", reason}         the turn failed: the device listens again after a moment
//                     {"type":"battery"}                Cubey was asked about its battery: it shows it
//                     {"type":"brushing"}               brushing: its mouth shows, the time starting at her tap
//                     {"type":"brushing.stop"}          she wants to stop brushing
//                     {"type":"timer", minutes, for}    a timer: its clock shows, and the talk ends until it does
//                     {"type":"lesson.start"}           her word game begins: the button stops it, quiet doesn't
//                     {"type":"lesson.show", picture?}  its step's picture (base64 PNG) over all of the screen; none:
//                                                       Cubey's eyes; before the step's speech
//                     {"type":"lesson.choose", pictures: [{id, picture}]}   two or three pictures to tap
//                     {"type":"lesson.end"}             her word game is over: Cubey's eyes
//                     {"type":"dice"}                   the die: it shows and rolls (again, if it shows already); the
//                                                       talk ends once Cubey has spoken
//                     {"type":"find"}                   she looks for something: the talk waits a few minutes
//                     {"type":"letter", letter?, word?, hers?}   a game with letters: the letter she needs, big;
//                                                       none: the game is over, Cubey's eyes. In the chain, scrolling
//                                                       in first: hers, her word as her turn ends, stopping on the
//                                                       letter Cubey's begins with; word, Cubey's once written, going
//                                                       on from that letter
//                     {"type":"count", n?}              how many dots: n dots to count, the number before shown big
//                                                       first; none: the game is over, Cubey's eyes
//                     {"type":"show", rows? | text?}    Cubey draws or writes, in place of its eyes for a few seconds:
//                                                       a picture's 24 rows of 24 ('#' lit, '+' dim), or text in the
//                                                       dot font, still if it fits, else scrolling past
//                     {"type":"menu"}, {"type":"wifi"}  a grown-up asked for its menu (languages, theme,
//                                                       brightness), or its Wi-Fi setup: it opens once Cubey has
//                                                       spoken, the talk over
//                     {"type":"reset"}                  a full reset (admin.ts): it forgets all it keeps (its Wi-Fi,
//                                                       its key) and starts again, as flashed clean
// A talk lasts until the next open, a close or a disconnect.
import { type Child, LANGUAGES } from "./child";
import { brushingNote, DICE_PUT_AWAY, diceNote, LESSON_STOPPED, TIMER_FOR, type TimerFor, timerNote } from "./cubey";
import type { Battery, Opened, Talk, Talks, TalkOut } from "./talk";
import type { Exchanged } from "./log";
import type { Learned } from "./memory";
import type { Spent } from "./prices";

export type Settings = { language: string; learns: string; theme?: string };
// new: a new Cubey's, not set up yet; theme: its screen's colours (child.ts)
export type Known = Settings & { new: boolean; brightness: number; theme: string };
export type SessionDeps = {
  child: () => Child;
  // what the device's menu sets, and the brightness a grown-up asks Cubey for (memory.ts)
  settings: { get(): Known; set(settings: Settings): boolean; brighten(percent: number): void };
  cubey: Talks; // through Soniox and a model, or on Gemini Live (server.ts)
  send: { text(msg: object): void; audio(pcm: Buffer): void };
  onExchange?: (exchanged: Exchanged) => void; // for the parents' log and the history
  talkOver?: boolean; // she may talk over Cubey (CUBEY_TALK_OVER, on unless 0)
  spent?: (spent: Spent) => void; // what its talks cost (spending.ts)
  learn?: (learned: Learned) => Child; // what the child told Cubey, kept in its memory (memory.ts)
};

const kindOf = (kind: unknown): TimerFor => (TIMER_FOR.includes(kind as TimerFor) ? (kind as TimerFor) : "other");

export class Session {
  private rate = 24000;
  private talk?: Talk;
  private battery?: Battery;

  constructor(private deps: SessionDeps) {}

  // To its hello, and when they change (its menu, by voice, on the admin page).
  sendSettings() {
    this.deps.send.text({ type: "settings", ...this.deps.settings.get(), languages: LANGUAGES });
  }

  onText(raw: string) {
    let msg: {
      type?: unknown; speaker_rate?: unknown; again?: unknown; battery?: unknown; charging?: unknown; part?: unknown; timer?: unknown;
      for?: unknown; minutes?: unknown; id?: unknown; dice?: unknown; n?: unknown; language?: unknown; learns?: unknown; theme?: unknown;
      brightness?: unknown;
    };
    try {
      msg = JSON.parse(raw);
    } catch {
      return;
    }
    switch (msg.type) {
      case "hello":
        this.rate = Number(msg.speaker_rate) || this.rate;
        this.deps.send.text({ type: "talk_over", on: this.deps.talkOver !== false });
        return this.sendSettings();
      case "settings":
        if (!this.deps.settings.set({ language: String(msg.language), learns: String(msg.learns), ...(typeof msg.theme === "string" && { theme: msg.theme }) })) return;
        if (typeof msg.brightness === "number") this.deps.settings.brighten(msg.brightness);
        this.talk?.close(); // in the language before: the device opens it anew
        this.talk = undefined;
        return this.sendSettings();
      case "open": {
        const note = msg.timer === "done" || msg.timer === "stopped" ? timerNote(kindOf(msg.for), msg.timer) : msg.dice === "stopped" ? DICE_PUT_AWAY : undefined;
        return this.open({ again: msg.again === true, ...(note ? { note } : {}) });
      }
      case "timer.left":
        if (typeof msg.minutes === "number" && msg.minutes > 0) this.open({ note: timerNote(kindOf(msg.for), msg.minutes), once: true });
        return;
      case "listen":
        if (!this.talk) return this.deps.send.text({ type: "failed", reason: "no talk" });
        return this.talk.listen();
      case "overhear":
        return this.talk?.overhear();
      case "voice":
        return this.talk?.voice();
      case "stop":
        return this.talk?.stop();
      case "close":
        return this.close();
      case "state":
        this.battery = typeof msg.battery === "number" ? { percent: msg.battery, charging: msg.charging === true } : undefined;
        return;
      case "brushing.part":
        if (msg.part === 1 || msg.part === 2 || msg.part === 3 || msg.part === 4) this.talk?.tell(brushingNote(msg.part));
        return;
      case "brushing.done":
        return this.talk?.tell(brushingNote("done"));
      case "brushing.missed":
        return this.talk?.tell(brushingNote("missed"));
      case "brushing.stopped":
        return this.talk?.tell(brushingNote("stopped"));
      case "dice.rolled":
        if (Number.isInteger(msg.n) && (msg.n as number) >= 1 && (msg.n as number) <= 6) this.open({ note: diceNote(msg.n as number), once: true });
        return;
      case "tapped":
        if (typeof msg.id === "string" && msg.id) this.talk?.tapped(msg.id);
        return;
      case "lesson.stopped":
        return this.talk?.tell(LESSON_STOPPED);
    }
  }

  private open(opened: Opened) {
    this.close();
    const { text, audio } = this.deps.send;
    const out: TalkOut = {
      rate: this.rate, text, audio, exchange: (e) => this.deps.onExchange?.(e), battery: () => this.battery, spent: (s) => this.deps.spent?.(s),
      learn: (learned) => this.deps.learn?.(learned) ?? this.deps.child(),
      brighten: (percent) => (this.deps.settings.brighten(percent), this.sendSettings(), this.deps.child()),
    };
    this.talk = this.deps.cubey.open(this.deps.child(), opened, out);
  }

  onAudio(pcm: Uint8Array) {
    if (this.talk?.hearing()) this.talk.audio(pcm);
  }

  close() {
    this.talk?.close();
    this.talk = undefined;
  }
}

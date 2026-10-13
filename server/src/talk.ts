// A talk with Cubey, as the session drives it (session.ts), whichever way Cubey hears and speaks: through Soniox and a
// model (cascade.ts), or on Gemini Live (live.ts). Both send the device the same messages.
import type { Child } from "./child";
import type { Learned } from "./memory";
import { type Battery, type Routine, TIMER_FOR, type TimerFor } from "./cubey";
import type { Exchanged } from "./log";
import type { Spent } from "./prices";

export type { Battery };
// again: the device lost the server mid-talk and goes on, without a hello; note: what happened instead of her waking
// it (a timer's end); once: Cubey says its word on it and the talk ends, nobody listening (a timer's reminder).
export type Opened = { again?: boolean; note?: string; once?: boolean };
// What the talk sends the device (at its speaker's rate), each exchange, for the log and the history, and what it
// knows of the device.
// spent: what the talk cost (its models' tokens, the audio heard and spoken), for its Cubey's spending (spending.ts);
// learn: what the child told Cubey (their name, girl or boy, their age), kept in its Cubey's memory: the child, now.
export type TalkOut = {
  rate: number; text(msg: object): void; audio(pcm: Buffer): void; exchange(exchanged: Exchanged): void; battery(): Battery | undefined;
  spent(spent: Spent): void; learn(learned: Learned): Child; brighten(percent: number): Child;
};
export type Talk = {
  listen(): void;
  overhear(): void; // its speech plays: she may talk over it
  voice(): void; // the device's own detector heard her voice over Cubey's speech: she talks over it
  hearing(): boolean; // her mic is wanted
  audio(pcm: Uint8Array): void;
  stop(): void; // what it is saying is dropped
  tell(note: string): void; // what happened (brushing goes on), for Cubey to say something about
  tapped(id: string): void; // she tapped a picture (her word game's)
  close(): void;
};
export type Talks = { open(child: Child, opened: Opened, out: TalkOut): Talk };

export const TIMER_MOST = 180; // minutes

// A timer's minutes and what it is for, checked; undefined for no sensible length.
export function timerOf(minutes: unknown, kind: unknown): { minutes: number; for: TimerFor } | undefined {
  const m = Number(minutes);
  if (!Number.isInteger(m) || m < 1 || m > TIMER_MOST) return undefined;
  return { minutes: m, for: TIMER_FOR.includes(kind as TimerFor) ? (kind as TimerFor) : "other" };
}

// What a routine shows or stops on the device: its battery (if it knows it), Cubey's mouth to brush with, the end of
// brushing, a timer's dial (nothing for a timer of no sensible length), her word game, the die, finding something
// (the device waits for her while she looks), the letter she needs (none, nothing), in the chain with Cubey's word,
// how many dots (none, nothing).
export function routineMessage(routine: Routine, args: { minutes?: unknown; for?: unknown; letter?: string; word?: string; number?: number }, battery: Battery | undefined): object | undefined {
  switch (routine) {
    case "battery": return battery ? { type: "battery" } : undefined;
    case "brushing": return { type: "brushing" };
    case "stop_brushing": return { type: "brushing.stop" };
    case "timer": {
      const timer = timerOf(args.minutes, args.for);
      return timer && { type: "timer", ...timer };
    }
    case "lesson": return { type: "lesson.start" };
    case "dice": return { type: "dice" };
    case "find": return { type: "find" };
    case "chain":
    case "letters": return args.letter ? { type: "letter", letter: args.letter, ...(args.word && { word: args.word }) } : undefined;
    case "count": return args.number ? { type: "count", n: args.number } : undefined;
    case "spell": return undefined; // its word as it is written, before its speech (cascade.ts)
    case "brightness": return undefined; // with its settings (session.ts)
    case "menu": return { type: "menu" };
    case "wifi": return { type: "wifi" };
  }
}

import { expect, test } from "bun:test";
import type { Child } from "./child";
import type { Opened, Talk, TalkOut } from "./talk";
import { brushingNote, DICE_PUT_AWAY, diceNote, LESSON_STOPPED, timerNote } from "./cubey";
import { Session } from "./session";

const anna: Child = { id: "Anna", name: "Anna", ageMonths: 42, timeZone: "Europe/Lisbon", gender: "girl", language: "lv", learns: "en", parents: "mum_dad" };

// A session with a pretend Cubey: what the talk was asked, and what the device was sent.
function session(talkOver?: boolean) {
  const did: string[] = [];
  const opened: { opened: Opened; rate: number }[] = [];
  const sent: object[] = [];
  let hearing = false;
  let out: TalkOut | undefined;
  const talk: Talk = {
    listen: (hold) => void did.push(hold ? "listen hold" : "listen"),
    release: () => void did.push("release"),
    overhear: () => void did.push("overhear"),
    voice: () => void did.push("voice"),
    hearing: () => hearing,
    audio: (pcm) => void did.push(`audio ${pcm.length}`),
    stop: () => void did.push("stop"),
    tell: (note) => void did.push(`tell ${note}`),
    tapped: (id) => void did.push(`tapped ${id}`),
    close: () => void did.push("close"),
  };
  const settings = { language: "lv", new: true, brightness: 100, volume: 100, theme: "green", hold: false };
  const s = new Session({
    child: () => anna,
    settings: {
      get: () => settings, set: (to) => to.language !== "xx" && (Object.assign(settings, to, { new: false }), true),
      brighten: (percent) => void (settings.brightness = percent),
      turn: (percent) => void (settings.volume = percent),
      colour: (theme) => void (settings.theme = theme),
    },
    cubey: { open: (_, o, to) => (opened.push({ opened: o, rate: to.rate }), (out = to), talk) },
    send: { text: (msg) => void sent.push(msg), audio: () => {} },
    talkOver,
  });
  return { s, did, opened, sent, settings, hear: (on: boolean) => void (hearing = on), out: () => out! };
}

test("the device opens Cubey's talk at its speaker's rate; again after a lost connection, it goes on", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "hello", speaker_rate: 16000 }));
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "open", again: true }));
  expect(t.opened).toEqual([{ opened: { again: false }, rate: 16000 }, { opened: { again: true }, rate: 16000 }]);
  expect(t.did).toEqual(["close"]); // a new open ends the talk before
});

test("the device hears whether she may talk over Cubey as it says hello: yes, unless CUBEY_TALK_OVER=0", () => {
  const on = session(), off = session(false);
  on.s.onText(JSON.stringify({ type: "hello", speaker_rate: 16000 }));
  off.s.onText(JSON.stringify({ type: "hello", speaker_rate: 16000 }));
  expect(on.sent[0]).toEqual({ type: "talk_over", on: true });
  expect(off.sent[0]).toEqual({ type: "talk_over", on: false });
});

test("to its hello the device hears its languages for its menu; the menu's change is kept, the talk opening anew in it", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "hello", speaker_rate: 16000 }));
  expect(t.sent[1]).toEqual({ type: "settings", language: "lv", new: true, brightness: 100, volume: 100, theme: "green", hold: false, languages: ["en", "fr", "de", "es", "it", "pt", "nl", "pl", "sv", "da", "no", "fi", "et", "ru", "uk", "lv", "lt"] });
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "settings", language: "en" }));
  expect(t.settings).toMatchObject({ language: "en", new: false });
  expect(t.did).toEqual(["close"]); // the talk in the old language
  expect(t.sent.at(-1)).toEqual({ type: "settings", language: "en", new: false, brightness: 100, volume: 100, theme: "green", hold: false, languages: ["en", "fr", "de", "es", "it", "pt", "nl", "pl", "sv", "da", "no", "fi", "et", "ru", "uk", "lv", "lt"] });
  t.s.onText(JSON.stringify({ type: "settings", language: "xx" })); // refused: as it was
  expect(t.settings).toMatchObject({ language: "en", new: false });
  t.s.onText(JSON.stringify({ type: "settings", language: "en", theme: "white", brightness: 40 })); // its theme and brightness, from its menu
  expect(t.settings).toMatchObject({ theme: "white", brightness: 40 });
  t.s.onText(JSON.stringify({ type: "settings", language: "en", hold: true })); // she holds its screen to talk
  expect(t.settings).toMatchObject({ hold: true });
});

test("asked for another colour: kept, and to the device with its settings, the talk going on", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.out().colour("red");
  expect(t.settings.theme).toBe("red");
  expect(t.sent.at(-1)).toMatchObject({ type: "settings", theme: "red" });
  expect(t.did).toEqual([]);
});

test("asked to be louder or quieter: kept, and to the device with its settings, the talk going on; set on its menu, kept", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.out().turn(60);
  expect(t.settings.volume).toBe(60);
  expect(t.sent.at(-1)).toMatchObject({ type: "settings", volume: 60 });
  expect(t.did).toEqual([]);
  t.s.onText(JSON.stringify({ type: "settings", language: "lv", volume: 30 }));
  expect(t.settings.volume).toBe(30);
});

test("a grown-up asks Cubey for a brighter or dimmer screen: kept, and to the device with its settings, the talk going on", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.out().brighten(60);
  expect(t.settings.brightness).toBe(60);
  expect(t.sent.at(-1)).toMatchObject({ type: "settings", brightness: 60 });
  expect(t.did).toEqual([]);
});

test("its turns go to the talk: listening, talking over it, a stop, and the mic only while the talk hears her", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "listen" }));
  t.s.onAudio(new Uint8Array(640)); // not hearing: dropped
  t.hear(true);
  t.s.onAudio(new Uint8Array(640));
  t.s.onText(JSON.stringify({ type: "overhear" }));
  t.s.onText(JSON.stringify({ type: "voice" }));
  t.s.onText(JSON.stringify({ type: "stop" }));
  expect(t.did).toEqual(["listen", "audio 640", "overhear", "voice", "stop"]);
});

test("she holds its screen as she talks: it listens until she lets go", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "listen", hold: true }));
  t.s.onText(JSON.stringify({ type: "release" }));
  expect(t.did).toEqual(["listen hold", "release"]);
});

test("the talk's messages and speech reach the device as they are", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.out().text({ type: "heard" });
  expect(t.sent).toEqual([{ type: "heard" }]);
});

test("closed by the device (Cubey sleeps) or by the connection, the talk ends; listening with no talk fails", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "close" }));
  t.s.onText(JSON.stringify({ type: "listen" }));
  expect(t.did).toEqual(["close"]);
  expect(t.sent).toEqual([{ type: "failed", reason: "no talk" }]);
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.close();
  expect(t.did).toEqual(["close", "close"]);
});

test("the device's battery, as it last said, is there for Cubey to tell", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  expect(t.out().battery()).toBeUndefined();
  t.s.onText(JSON.stringify({ type: "state", battery: 73, charging: true }));
  expect(t.out().battery()).toEqual({ percent: 73, charging: true });
  t.s.onText(JSON.stringify({ type: "state" })); // it can't tell any more
  expect(t.out().battery()).toBeUndefined();
});

test("as she brushes, the device's word of each part, and of the end, goes to the talk for Cubey to say", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "brushing.part", part: 2 }));
  t.s.onText(JSON.stringify({ type: "brushing.done" }));
  t.s.onText(JSON.stringify({ type: "brushing.missed" }));
  t.s.onText(JSON.stringify({ type: "brushing.stopped" }));
  t.s.onText(JSON.stringify({ type: "brushing.part", part: 9 })); // not a part
  expect(t.did).toEqual([`tell ${brushingNote(2)}`, `tell ${brushingNote("done")}`, `tell ${brushingNote("missed")}`, `tell ${brushingNote("stopped")}`]);
});

test("her word game: a picture she tapped is her answer; the button stops the game, for Cubey to say something about", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open" }));
  t.s.onText(JSON.stringify({ type: "tapped", id: "cat" }));
  t.s.onText(JSON.stringify({ type: "tapped" })); // no picture
  t.s.onText(JSON.stringify({ type: "lesson.stopped" }));
  expect(t.did).toEqual(["tapped cat", `tell ${LESSON_STOPPED}`]);
});

test("each roll of the die is a word once, its number, while nobody listens; put away, the talk opens with that", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "dice.rolled", n: 4 }));
  t.s.onText(JSON.stringify({ type: "dice.rolled", n: 7 })); // no die's
  t.s.onText(JSON.stringify({ type: "dice.rolled" }));
  t.s.onText(JSON.stringify({ type: "open", dice: "stopped" }));
  expect(t.opened.map((o) => o.opened)).toEqual([{ note: diceNote(4), once: true }, { again: false, note: DICE_PUT_AWAY }]);
});

test("the timer's end, or its stop, opens the talk with what happened; a reminder is a word once, while nobody listens", () => {
  const t = session();
  t.s.onText(JSON.stringify({ type: "open", timer: "done", for: "cartoons" }));
  t.s.onText(JSON.stringify({ type: "open", timer: "stopped", for: "bath" }));
  t.s.onText(JSON.stringify({ type: "timer.left", minutes: 5, for: "cartoons" }));
  expect(t.opened.map((o) => o.opened)).toEqual([
    { again: false, note: timerNote("cartoons", "done") },
    { again: false, note: timerNote("bath", "stopped") },
    { note: timerNote("cartoons", 5), once: true },
  ]);
});

test("a message that isn't one is left alone", () => {
  const t = session();
  t.s.onText("not json");
  t.s.onText(JSON.stringify({ type: "dance" }));
  expect(t.sent).toEqual([]);
});

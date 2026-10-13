import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { type CascadeDeps, cascadeTalks, TALK_OVER_WORD_MS } from "./cascade";
import type { Child } from "./child";
import { LESSON_STOPPED } from "./cubey";
import type { Earlier } from "./history";
import { LessonProgress, LessonWords } from "./lesson";
import { Lessons } from "./lesson-game";
import type { Exchanged } from "./log";
import type { Spent } from "./prices";
import type { Battery, Opened } from "./talk";

const anna: Child = { id: "Anna", name: "Anna", ageMonths: 42, timeZone: "Europe/Lisbon", gender: "girl", language: "lv", learns: "en", parents: "mum_dad" };
const before: Earlier[] = [{ at: new Date().toISOString(), heard: "Kur ir saule?", said: "Aiz mākoņiem." }];
const json = (say: string, extra: object = {}) => JSON.stringify({ say, bye: false, for_parents: false, ...extra });

// Cubey's talk through a pretend Soniox and pretend models, for a device playing at 16 kHz. answers: what the models
// write, in turn, either of them (each streamed in three pieces); asked says which was; the word game's translations
// the model writes by itself.
async function talk(opened: Opened = {}, answers: string[] = [json("Čau, Anna!")], battery: Battery | null = { percent: 73, charging: false }) {
  const texts: any[] = [];
  const exchanges: Exchanged[] = [];
  let audio = 0;
  const asked: { system: string; messages: { role: string; text: string }[]; model: "talk" | "games" | "draw" }[] = [];
  const spent: Spent[] = [];
  const model = (name: "talk" | "games" | "draw"): CascadeDeps["complete"] => async ({ system, messages, onText, spent: spend }) => {
    asked.push({ system, messages: [...messages], model: name });
    spend?.({ what: "think", model: name, input: 100, output: 10 });
    const raw = answers.shift();
    if (raw === undefined) throw new Error("no answer");
    const third = Math.ceil(raw.length / 3);
    for (let i = 0; i < raw.length; i += third) onText?.(raw.slice(i, i + third));
    return raw;
  };
  const listeners: {
    pcm: number; onHeard: () => void; onWords?: (soFar: string) => void; onPause?: unknown; text: string; cancelled: boolean;
    also?: string[]; waitMs?: number;
  }[] = [];
  const voices: { said: string[]; languages: (string | undefined)[]; cancelled: boolean; rate: number; timed: boolean }[] = [];
  let heardNext = "Kāpēc debesis ir zilas?";
  const learned: object[] = [];
  const brightened: number[] = [];
  const logged: string[] = [];
  const deps: CascadeDeps = {
    history: { recent: () => before },
    listen: (opts) => {
      const { onHeard, onWords, also, waitMs } = opts;
      const l = { pcm: 0, onHeard, onWords, onPause: (opts as { onPause?: unknown }).onPause, text: heardNext, cancelled: false, also, waitMs };
      listeners.push(l);
      return { send: (pcm) => void (l.pcm += pcm.length), finish: async () => l.text, cancel: () => void (l.cancelled = true) };
    },
    // Asked to, it times what it says as Soniox would: a character every 0.1 s.
    speak: ({ sampleRate, onAudio, timestamps, onCharacters }) => {
      const v = { said: [] as string[], languages: [] as (string | undefined)[], cancelled: false, rate: sampleRate, timed: false };
      voices.push(v);
      let at = 0;
      return {
        say: async (text, language) => {
          v.said.push(text), v.languages.push(language);
          if (v.said.length === 1) v.timed = timestamps?.() ?? false; // as its stream opens, with the first text
          if (v.timed) onCharacters?.([...text], [...text].map(() => (at += 0.1) - 0.1));
        },
        end: async () => onAudio(Buffer.alloc(4)),
        cancel: () => void (v.cancelled = true),
      };
    },
    complete: model("talk"),
    games: model("games"),
    draw: model("draw"),
    lessons: undefined as unknown as Lessons, // below: it shares the model
    log: (line) => void logged.push(line),
  };
  const db = new Database(":memory:");
  deps.lessons = new Lessons({
    progress: new LessonProgress(db), words: new LessonWords(db), language: () => "en",
    picture: async (name, size) => `png:${name}:${size}`,
    complete: async (opts) => {
      if (!opts.system.startsWith("Translate")) return deps.complete(opts);
      const asked = JSON.parse(opts.messages[0].text) as Record<string, string>;
      return JSON.stringify(Object.fromEntries(Object.entries(asked).map(([id, en]) => [id, en])));
    },
  });
  const live = cascadeTalks(deps).open(anna, opened, {
    rate: 16000, text: (m) => void texts.push(m), audio: () => void audio++, exchange: (e) => void exchanges.push(e), battery: () => battery ?? undefined,
    spent: (s) => void spent.push(s),
    learn: (what) => (learned.push(what), { ...anna, ...(what.name && { name: what.name }), ...(what.gender && { gender: what.gender }) }),
    brighten: (percent) => (brightened.push(percent), { ...anna, brightness: percent }),
  });
  await Bun.sleep(5);
  // She says something: the device listens and streams the mic, Soniox hears her pause.
  const ask = async (heard = "Kāpēc debesis ir zilas?", ...answer: string[]) => {
    answers.push(...answer);
    heardNext = heard;
    live.listen();
    live.audio(new Uint8Array(640));
    listeners.at(-1)!.onHeard();
    await Bun.sleep(5);
  };
  const types = () => texts.map((t) => t.type);
  return { live, ask, texts, types, exchanges, asked, listeners, voices, audio: () => audio, answers, deps, learned, spent, brightened, logged };
}

test("Cubey's hello: the model is told what happened, in Cubey's prompt with its battery, and its words are spoken as written", async () => {
  const t = await talk();
  expect(t.asked[0].system).toContain("a machine writes down what she says");
  expect(t.asked[0].system).toContain("Your battery has 73% left.");
  expect(t.asked[0].system).toContain("Aiz mākoņiem.");
  expect(t.asked[0].messages).toEqual([{ role: "user", text: "(Anna has just woken you.)" }]);
  expect(t.voices[0].rate).toBe(16000);
  expect(t.voices[0].said.join("")).toBe("Čau, Anna!");
  expect(t.voices[0].said.length).toBe(2); // a word at a time
  expect(t.audio()).toBe(1);
  expect(t.texts).toEqual([{ type: "said", heard: "", said: "Čau, Anna!", last: false }]);
  expect(t.exchanges).toEqual([{ heard: "(Anna has just woken you.)", said: "Čau, Anna!", forParents: false }]);
});

test("going on after a lost connection, no hello; what happened instead of her waking it, as the hello", async () => {
  const again = await talk({ again: true });
  expect(again.asked).toEqual([]);
  expect(again.texts).toEqual([{ type: "said", heard: "", said: "", last: false }]);
  const done = await talk({ note: "(The timer for cartoons is over.)" });
  expect(done.asked[0].messages).toEqual([{ role: "user", text: "(The timer for cartoons is over.)" }]);
});

test("she talks: the device listens, her mic goes to Soniox; once her turn is over the model gets the talk so far and her words", async () => {
  const t = await talk();
  await t.ask("Kāpēc debesis ir zilas?", json("Jo saule izkliedē zilo."));
  expect(t.types()).toEqual(["said", "listening", "heard", "said"]);
  expect(t.listeners[0].pcm).toBe(640);
  expect(t.asked[1].messages).toEqual([
    // Its answers as the JSON it writes: given as plain sentences, GPT-6 Sol answered in them a third of the time.
    { role: "user", text: "(Anna has just woken you.)" }, { role: "assistant", text: '{"say":"Čau, Anna!"}' },
    { role: "user", text: "Kāpēc debesis ir zilas?" },
  ]);
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "Kāpēc debesis ir zilas?", said: "Jo saule izkliedē zilo.", last: false });
  expect(t.exchanges.at(-1)).toEqual({ heard: "Kāpēc debesis ir zilas?", said: "Jo saule izkliedē zilo.", forParents: false });
});

test("a pause with nothing said: the device listens on, the model isn't asked", async () => {
  const t = await talk();
  await t.ask("");
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "", said: "", last: false });
  expect(t.asked.length).toBe(1);
});

test("its goodbye ends the talk; a question for her parents is marked for them", async () => {
  const t = await talk();
  await t.ask("Kur rodas bērni?", json("Pajautā mammai!", { for_parents: true }));
  expect(t.exchanges.at(-1)).toMatchObject({ heard: "Kur rodas bērni?", forParents: true });
  await t.ask("Atā", json("Atā!", { bye: true }));
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "Atā", said: "Atā!", last: true });
});

test("its routines: the battery, brushing and its end, a timer, shown on the device as Cubey speaks", async () => {
  const t = await talk();
  await t.ask("Cik tev baterija?", json("Septiņdesmit trīs procenti.", { routine: "battery" }));
  expect(t.types().slice(-2)).toEqual(["battery", "said"]);
  await t.ask("Gribu tīrīt zobus", json("Paņem birstīti!", { routine: "brushing" }));
  expect(t.texts.at(-2)).toEqual({ type: "brushing" });
  await t.ask("Negribu vairs", json("Labi.", { routine: "stop_brushing" }));
  expect(t.texts.at(-2)).toEqual({ type: "brushing.stop" });
  await t.ask("Taimeri uz 40 minūtēm multenēm", json("Labi!", { routine: "timer", minutes: 40, for: "cartoons" }));
  expect(t.texts.at(-2)).toEqual({ type: "timer", minutes: 40, for: "cartoons" });
  await t.ask("Uz tūkstoš", json("Par ilgu.", { routine: "timer", minutes: 1000, for: "cartoons" }));
  expect(t.types().at(-2)).toBe("heard"); // no timer of no sensible length
  await t.ask("Met kauliņu!", json("Metam!", { routine: "dice" }));
  expect(t.texts.at(-2)).toEqual({ type: "dice" });
  await t.ask("Spēlēsim meklēt!", json("Atrodi kaut ko sarkanu!", { routine: "find" }));
  expect(t.texts.at(-2)).toEqual({ type: "find" });
  await t.ask("Padari ekrānu tumšāku", json("Labi.", { routine: "brightness", number: 30 }));
  expect(t.brightened).toEqual([30]); // kept, and to the device with its settings (session.ts)
  expect(t.types().at(-2)).toBe("heard");
  await t.ask("Atver valodu izvēlni", json("Lūdzu!", { routine: "menu" }));
  expect(t.texts.at(-2)).toEqual({ type: "menu" });
  expect(t.asked.at(-1)!.system).toContain("brightness is 30%"); // as it is now
  await t.ask("Pievieno Wi-Fi", json("Lūdzu!", { routine: "wifi" }));
  expect(t.texts.at(-2)).toEqual({ type: "wifi" });
});

test("she talks over it: the device hears her voice (its own detector), Soniox her first word, Cubey stops; her turn begins with the moment before", async () => {
  const t = await talk();
  await t.ask("Kāpēc?", json("Jo saule ir karsta un spoža."));
  t.live.overhear();
  expect(t.live.hearing()).toBe(true);
  expect(t.listeners.length).toBe(1); // nothing transcribed while Cubey only speaks
  for (let i = 0; i < 60; i++) t.live.audio(new Uint8Array(640)); // 1.2 s of the mic as it speaks
  t.answers.push(json("Jā?"));
  t.live.voice();
  expect(t.types().at(-1)).toBe("said"); // a voice is not yet a word: Cubey goes on
  const turn = t.listeners.at(-1)!;
  turn.onWords?.("pa");
  expect(t.types().at(-1)).toBe("interrupted"); // her first word: the device stops what it still plays
  turn.onWords?.("pagai");
  expect(t.types().filter((type) => type === "interrupted")).toHaveLength(1);
  expect(turn.pcm).toBe(32000); // the last second before her voice was heard
  t.live.audio(new Uint8Array(640));
  expect(turn.pcm).toBe(32640); // then on
  turn.text = "saule pagaidi"; // Cubey's own word heard back first
  turn.onHeard();
  await Bun.sleep(5);
  expect(t.asked.at(-1)!.messages.at(-1)).toEqual({ role: "user", text: "pagaidi" });
  expect(t.texts.at(-1)).toMatchObject({ type: "said", heard: "pagaidi", said: "Jā?" });
});

test("a voice over Cubey with no word of hers in it (an \"ooh\", a knock, the room): Cubey goes on, and listens over it again", async () => {
  const t = await talk();
  await t.ask("Kāpēc?", json("Jo saule ir karsta un spoža."));
  t.live.overhear();
  t.live.voice();
  const doubted = t.listeners.at(-1)!;
  await Bun.sleep(TALK_OVER_WORD_MS + 50);
  expect(doubted.cancelled).toBe(true);
  expect(t.types()).not.toContain("interrupted");
  expect(t.live.hearing()).toBe(true); // overhearing again
  doubted.onWords?.("pa"); // too late: nothing
  expect(t.types()).not.toContain("interrupted");
});

test("her voice heard while nothing is being said is no talking over", async () => {
  const t = await talk();
  t.live.voice();
  expect(t.types()).not.toContain("interrupted");
});

test("told what happened as she listens (brushing goes on): the device hears it as an answer, and Cubey says something about it", async () => {
  const t = await talk();
  t.answers.push(json("Tagad otrā pusē!"));
  t.live.listen();
  t.live.tell("(Brushing: now the top, on the other side.)");
  await Bun.sleep(5);
  expect(t.listeners.at(-1)!.cancelled).toBe(true);
  expect(t.types().slice(-3)).toEqual(["listening", "heard", "said"]);
  expect(t.texts.at(-1)).toMatchObject({ heard: "", said: "Tagad otrā pusē!" });
  expect(t.exchanges.at(-1)).toEqual({ heard: "(Brushing: now the top, on the other side.)", said: "Tagad otrā pusē!", forParents: false });
});

test("a word once (a timer's reminder): said, then nothing listens", async () => {
  const t = await talk({ note: "(5 minutes left.)", once: true }, [json("Vēl piecas minūtes!")]);
  expect(t.texts).toEqual([{ type: "said", heard: "", said: "Vēl piecas minūtes!", last: false }]);
  t.live.listen();
  expect(t.listeners).toEqual([]);
});

test("a word once starts no routine: the die's number said with the dice routine set doesn't roll it again", async () => {
  const t = await talk({ note: "(The die shows 4.)", once: true }, [json("Oho, četri!", { routine: "dice" })]);
  expect(t.texts).toEqual([{ type: "said", heard: "", said: "Oho, četri!", last: false }]); // no {type: "dice"}: it looped
});

test("stopped, the rest of its answer goes unsaid; closed with what happened unanswered, it is in the history all the same", async () => {
  const t = await talk();
  t.live.tell("(Brushing: she stopped it.)");
  t.live.close();
  await Bun.sleep(5);
  expect(t.voices.at(-1)!.cancelled).toBe(true);
  expect(t.exchanges.at(-1)).toEqual({ heard: "(Brushing: she stopped it.)", said: "", forParents: false });
});

test("a model that fails, or writes nothing usable: the turn fails, and the device listens again after a moment", async () => {
  const t = await talk();
  await t.ask("Kāpēc?", "nonsense");
  expect(t.texts.at(-1)).toMatchObject({ type: "failed" });
  await t.ask("Kāpēc?"); // no answer at all
  expect(t.texts.at(-1)).toMatchObject({ type: "failed" });
});

// Answers begun at her pause, before her turn ended, were seldom the ones spoken, and cost a third of the model's
// calls (2026-10-13).
test("the model is asked once her turn is over, never before: nothing listens for her pauses", async () => {
  const t = await talk();
  await t.ask("Kāpēc debesis ir zilas?", json("Jo saule izkliedē zilo."));
  expect(t.listeners.every((l) => !l.onPause)).toBe(true);
  expect(t.asked.length).toBe(2); // the hello, and her turn
});

const step = (say: string, extra: object = {}) => JSON.stringify({ say, result: "", stop: false, for_parents: false, ...extra });

// Cubey's talk, she has asked for the word game and Cubey has said it'll play.
async function playing() {
  const t = await talk();
  await t.ask("Gribu mācīties angliski", json("Spēlēsim vārdu spēli! Vai esi gatava?", { routine: "lesson" }));
  return t;
}

test("she asks to learn English (or says parrot): Cubey says it'll play, and the device hears the word game begin", async () => {
  const t = await talk();
  expect(t.asked[0].system).toContain("Learning English");
  expect(t.asked[0].system).toContain('says "parrot"');
  await t.ask("Gribu mācīties angliski", json("Spēlēsim vārdu spēli! Vai esi gatava?", { routine: "lesson" }));
  expect(t.types()).toEqual(["said", "listening", "heard", "lesson.start", "said"]);
});

test("in the word game her words are its: the step's picture before its speech, in two languages; Soniox hears English too", async () => {
  const t = await playing();
  await t.ask("Jā!", step("[excited] Klausies: {Hello}! {Hello}!"));
  expect(t.listeners.at(-1)).toMatchObject({ also: ["en"], waitMs: 8000 });
  expect(t.asked.at(-1)!.system).toContain("you teach her English");
  expect(t.asked.at(-1)!.messages).toEqual([{ role: "user", text: "Jā!" }]);
  expect(t.types().slice(-4)).toEqual(["listening", "heard", "lesson.show", "said"]);
  expect(t.texts.at(-2)).toEqual({ type: "lesson.show", picture: "png:Waving hand:256" });
  expect(t.voices.at(-1)!.said).toEqual(["[excited] Klausies: ", "Hello! Hello!"]);
  expect(t.voices.at(-1)!.languages).toEqual(["lv", "en"]);
  expect(t.exchanges.at(-1)).toEqual({ heard: "Jā!", said: "[excited] Klausies: Hello! Hello!", forParents: false });
});

test("in the word game she says nothing: the step once more, then Cubey waits for her, never ending the game", async () => {
  const t = await playing();
  await t.ask("Jā!", step("{Hello}!"));
  await t.ask("", step("Vēlreiz: {Hello}!"));
  expect(t.asked.at(-1)!.messages.at(-1)).toEqual({ role: "user", text: "(Anna said nothing)" });
  expect(t.listeners.at(-1)!.waitMs).toBe(8000);
  const asked = t.asked.length;
  await t.ask("");
  expect(t.listeners.at(-1)!.waitMs).toBeUndefined(); // it waits as long as it takes
  expect(t.asked.length).toBe(asked);
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "", said: "", last: false });
  expect(t.types()).not.toContain("lesson.end");
});

test("in the word game a tap is her answer: the device hears it as her pause, and the game goes on", async () => {
  const t = await playing();
  await t.ask("Jā!", step("{Hello}!"));
  t.answers.push(step("Jā, {hello}!"));
  t.live.listen();
  t.live.tapped("cat");
  await Bun.sleep(5);
  expect(t.listeners.at(-1)!.cancelled).toBe(true);
  expect(t.types().slice(-4)).toEqual(["listening", "heard", "lesson.show", "said"]);
});

test("she wants to stop the game, or something else: it ends, and Cubey's usual talk answers her words", async () => {
  const t = await playing();
  await t.ask("Gribu tīrīt zobus", step("", { stop: true }), json("Labi! Ejam tīrīt zobus.", { routine: "brushing" }));
  expect(t.types().slice(-4)).toEqual(["heard", "lesson.end", "brushing", "said"]);
  expect(t.asked.at(-1)!.messages.at(-1)).toEqual({ role: "user", text: "(The word game is over: she wanted to stop.) Gribu tīrīt zobus" });
  await t.ask("Kāpēc?", json("Tāpēc."));
  expect(t.listeners.at(-1)!.also).toBeUndefined(); // no game any more
});

test("the button stops the game: Cubey's usual talk says something about it", async () => {
  const t = await playing();
  await t.ask("Jā!", step("{Hello}!"));
  t.live.listen();
  t.answers.push(json("Labi, pietiek angļu valodas!"));
  t.live.tell(LESSON_STOPPED);
  await Bun.sleep(5);
  expect(t.types().slice(-4)).toEqual(["listening", "heard", "lesson.end", "said"]);
  expect(t.asked.at(-1)!.system).toContain("a machine writes down"); // Cubey's own prompt
});

test("taps outside the word game are nothing", async () => {
  const t = await talk();
  t.live.listen();
  t.live.tapped("cat");
  await Bun.sleep(5);
  expect(t.types()).toEqual(["said", "listening"]);
});

test("a word chain: the letter she needs is the last of Cubey's word, as the server spells it; the server judges hers", async () => {
  const t = await talk();
  await t.ask("Spēlēsim vārdu ķēdi!", json("Vāvere! Tev uz E!", { routine: "chain", word: "Vāvere" }));
  expect(t.texts.at(-2)).toEqual({ type: "letter", letter: "e", word: "vāvere" }); // its word scrolls in, then her letter
  await t.ask("Egle", json("Egle! Man: ezis. Tev uz Z!", { routine: "chain", hers: "egle", word: "ezis", spelled: "e-z-i-s" }));
  expect(t.asked.at(-1)!.system).toContain('She said egle (e-g-l-e), on "e".');
  // Shown as it is said: her word as her turn ends (it scrolls while Cubey thinks), Cubey's once written, before it
  // speaks; nothing again at the end.
  expect(t.texts.slice(-4)).toEqual([
    { type: "heard" }, { type: "letter", letter: "e", hers: "egle" }, { type: "letter", letter: "s", word: "ezis" }, // not z
    { type: "said", heard: "Egle", said: "Egle! Man: ezis. Tev uz Z!", last: false },
  ]);
  await t.ask("Mamma", json("Malacis! Man: mango.", { routine: "chain", word: "mango" })); // praised all the same
  expect(t.asked.at(-1)!.system).toContain('She needed a word on "s" and said mamma');
  expect(t.types().at(-2)).toBe("heard"); // not on s: the letter stays, whatever the model wrote
  await t.ask("Suns", json("Suns! Man: sēne. Tev uz E!", { routine: "chain", hers: "suns", word: "sēne" }));
  expect(t.asked.at(-1)!.system).toContain('She said suns (s-u-n-s), on "s".');
  expect(t.texts.slice(-3, -1)).toEqual([{ type: "letter", letter: "s", hers: "suns" }, { type: "letter", letter: "e", word: "sēne" }]);
  await t.ask("Egle", json("Malacis! Man: ezis.", { routine: "chain", word: "ezis" })); // on e, but said already
  expect(t.asked.at(-1)!.system).toContain("not said in this game (vāvere, egle, ezis, suns, sēne)");
  expect(t.asked.at(-1)!.system).toContain("She said egle, but it was said already");
  expect(t.types().at(-2)).toBe("heard"); // the letter stays
  await t.ask("Pietiek", json("Labi, beidzam!"));
  expect(t.texts.at(-2)).toEqual({ type: "letter" }); // over: the eyes come back
  await t.ask("Kāpēc?", json("Tāpēc."));
  expect(t.asked.at(-1)!.system).not.toContain("Now: a word chain");
  await t.ask("Vēlreiz!", json("Egle! Tev uz E!", { routine: "chain", word: "egle" })); // a new game: its words anew
  await t.ask("Ezis", json("Sula! Tev uz A!", { routine: "chain", hers: "ezis", word: "sula" }));
  expect(t.asked.at(-1)!.system).toContain("not said in this game (egle)");
});

// On Fly (2026-10-12): from words on O to a chain she begins: "o" stayed hers to need, "Skola" was judged off it, and
// nothing showed.
test("from words on a letter to a chain she begins: no letter carried over, her first word and Cubey's shown", async () => {
  const t = await talk();
  await t.ask("Vārdi uz O!", json("Tev uz O!", { routine: "letters", letter: "o" }));
  await t.ask("Nē, ķēdi!", json("Sāc tu!", { routine: "chain" }));
  expect(t.texts.at(-2)).toEqual({ type: "letter" }); // its letter goes: the eyes, until there is a word
  await t.ask("Skola", json("Skola! Man: ape. Tev uz E!", { routine: "chain", hers: "skola", word: "ape" }));
  expect(t.asked.at(-1)!.system).toContain("She said skola (s-k-o-l-a). If she meant a word, be glad");
  expect(t.texts.slice(-3, -1)).toEqual([{ type: "letter", letter: "a", hers: "skola" }, { type: "letter", letter: "e", word: "ape" }]);
  await t.ask("Egle", json("Egle! Man: ezis.", { routine: "chain", hers: "egle", word: "ezis" }));
  expect(t.asked.at(-1)!.system).toContain("not said in this game (skola, ape)");
});

// On Fly (2026-10-12): Soniox wrote "Aitā." for her "aita", and the chain went on from ā; "Eh." went on to h.
test("in the chain her word is what the model says she meant: shown and gone on from as meant; no word, the letter stays", async () => {
  const t = await talk();
  await t.ask("Spēlēsim vārdu ķēdi!", json("Aka! Tev uz A!", { routine: "chain", word: "aka" }));
  await t.ask("Aitā.", json("Jā, aita! Man: ābols.", { routine: "chain", hers: "aita", word: "ābols" }));
  expect(t.texts.slice(-4, -1)).toEqual([{ type: "heard" }, { type: "letter", letter: "a", hers: "aita" }, { type: "letter", letter: "s", word: "ābols" }]);
  await t.ask("Eh.", json("Tas nav vārds! Kas ir uz s?", { routine: "chain", hers: "", word: "" }));
  expect(t.types().slice(-2)).toEqual(["heard", "said"]); // nothing to show: the s stays
  expect(t.asked.at(-1)!.system).toContain("not said in this game (aka, aita, ābols)");
  await t.ask("Sēta", json("Jā!", { routine: "chain", hers: "Zēta", word: "aka" })); // not on s, as written: not hers
  expect(t.types().slice(-2)).toEqual(["heard", "said"]);
  await t.ask("Suns", json("Suns!", { routine: "chain", hers: "suns" })); // hers, but no word of Cubey's: hers again
  expect(t.texts.slice(-3, -1)).toEqual([{ type: "letter", letter: "s", hers: "suns" }, { type: "letter", letter: "s" }]);
  expect(t.asked.length).toBeGreaterThan(0);
});

test("words on one letter: the letter she chose stays on the screen", async () => {
  const t = await talk();
  await t.ask("Nosauc vārdus uz A!", json("Sāc tu!", { routine: "letters", letter: "A" }));
  expect(t.texts.at(-2)).toEqual({ type: "letter", letter: "a" });
  await t.ask("Auto", json("Viens!", { routine: "letters", letter: "a" }));
  expect(t.asked.at(-1)!.system).toContain('Of what she said, on "a": auto (a-u-t-o).');
});

test("its screen: the talk's model says what to show, the drawing one draws or writes it, and it goes to the device", async () => {
  const t = await talk();
  await t.ask("Uzzīmē sirsniņu!", json("Lūk!", { show: "sirsniņa" }), '{"picture": ["#"]}');
  expect(t.asked.at(-1)!.model).toBe("draw");
  expect(t.asked.at(-1)!.messages.at(-1)!.text).toContain("sirsniņa");
  expect(t.texts).toContainEqual({ type: "show", rows: [...Array(11).fill(".".repeat(24)), `${".".repeat(11)}#${".".repeat(12)}`, ...Array(12).fill(".".repeat(24))] });
  await t.ask("Uzraksti manu vārdu!", json("Lūk!", { show: "ANNA" }), '{"text": "Anna"}');
  expect(t.texts).toContainEqual({ type: "show", text: "Anna" });
  const shown = t.texts.length, asked = t.asked.length;
  await t.ask("Un vēl?", json("Hmm!", { show: "" })); // nothing named: nothing drawn
  expect(t.asked.length).toBe(asked + 1);
  expect(t.texts.slice(shown).map((m) => m.type)).not.toContain("show");
});

// On Fly (2026-10-12): GPT-6 Astra drew in 5 to 14 s, and the sun and a car she asked for were dropped as her next turn began.
test("a drawing that comes after her next turn has begun shows all the same; one asked for since replaces one not drawn yet", async () => {
  const t = await talk();
  const drawing: ((raw: string) => void)[] = [];
  t.deps.draw = () => new Promise((resolve) => drawing.push(resolve));
  await t.ask("Uzzīmē mašīnu!", json("Tūlīt uzzīmēšu!", { show: "a car" }));
  await t.ask("Ātri!", json("Jā!"));
  expect(t.asked.at(-1)!.system).toContain("On your screen now: a car, still being drawn: not there yet."); // she asks where it is
  drawing[0]('{"text": "CAR"}');
  await Bun.sleep(5);
  expect(t.texts).toContainEqual({ type: "show", text: "CAR" });
  await t.ask("Kur?", json("Te!"));
  expect(t.asked.at(-1)!.system).toContain("On your screen now: a car, drawn.");
  await t.ask("Uzzīmē sauli!", json("Tūlīt!", { show: "the sun" }));
  await t.ask("Nē, mēnesi!", json("Labi!", { show: "the moon" }));
  const shown = t.texts.length;
  drawing[2]('{"text": "MOON"}');
  drawing[1]('{"text": "SUN"}');
  await Bun.sleep(5);
  expect(t.texts.slice(shown).filter((m) => m.type === "show")).toEqual([{ type: "show", text: "MOON" }]);
});

test("while it draws, the device shows a sketch forming; a drawing that comes to nothing brings its eyes back", async () => {
  const t = await talk();
  const drawing: ((raw: string) => void)[] = [];
  t.deps.draw = () => new Promise((resolve) => drawing.push(resolve));
  await t.ask("Uzzīmē kaķi!", json("Zīmēju!", { show: "a cat" }));
  expect(t.types().slice(-3)).toEqual(["heard", "drawing", "said"]); // as soon as it is named
  drawing[0]("Sorry, I can't.");
  await Bun.sleep(5);
  expect(t.texts.at(-1)).toEqual({ type: "show" }); // nothing to show: the sketch goes
  t.deps.draw = async () => {
    throw new Error("busy");
  };
  await t.ask("Uzzīmē suni!", json("Zīmēju!", { show: "a dog" }));
  expect(t.texts.filter((m) => m.type === "show")).toEqual([{ type: "show" }, { type: "show" }]);
});

test("what each turn spent, for its Cubey's spending: its model's tokens, her audio heard, Cubey's spoken", async () => {
  const t = await talk();
  t.spent.length = 0;
  await t.ask("Kāpēc debesis ir zilas?", json("Tāpēc!"));
  expect(t.spent).toContainEqual({ what: "think", model: "talk", input: 100, output: 10 });
  expect(t.spent).toContainEqual({ what: "listen", seconds: 640 / 32000 }); // a frame of her mic
  expect(t.spent).toContainEqual({ what: "speak", seconds: 4 / 32000 });    // Cubey's pretend sound, at 16 kHz
});

test("what she tells Cubey of herself is kept, and the talk goes on with it: call me Nick", async () => {
  const t = await talk();
  await t.ask("Sauc mani par Niku!", json("Labi, Nik!", { child: { name: "Niks", gender: "boy" } }));
  expect(t.learned).toEqual([{ name: "Niks", gender: "boy" }]);
  await t.ask("Kā mani sauc?", json("Tevi sauc Niks!"));
  expect(t.asked.at(-1)!.system).toContain("talks with Niks about anything he likes");
  await t.ask("Labi.", json("Jā!", { child: {} })); // nothing learned
  expect(t.learned).toHaveLength(1);
});

test("the games with words are the games' model's while she plays them; the talk, and the turn that starts one, the quick one's", async () => {
  const t = await talk();
  expect(t.asked.at(-1)!.model).toBe("talk"); // the hello
  await t.ask("Spēlēsim vārdu ķēdi!", json("Vāvere! Tev uz E!", { routine: "chain", word: "vāvere" }));
  expect(t.asked.at(-1)!.model).toBe("talk");
  await t.ask("Egle", json("Egle! Man: ezis.", { routine: "chain", hers: "egle", word: "ezis" }));
  expect(t.asked.at(-1)!.model).toBe("games");
  await t.ask("Pietiek", json("Labi!"));
  expect(t.asked.at(-1)!.model).toBe("games"); // still in the game as she said it
  await t.ask("Kāpēc?", json("Tāpēc."));
  expect(t.asked.at(-1)!.model).toBe("talk");
  await t.ask("Skaitīsim punktus!", json("Cik?", { routine: "count", number: 3 }));
  await t.ask("Trīs", json("Jā!", { routine: "count", number: 4 }));
  expect(t.asked.at(-1)!.model).toBe("talk"); // counting is easy
});

test("how many dots: each new number shows (the same again would give the answer away); over, the eyes", async () => {
  const t = await talk();
  await t.ask("Skaitīsim punktus!", json("Cik punktu tu redzi?", { routine: "count", number: 3 }));
  expect(t.texts.at(-2)).toEqual({ type: "count", n: 3 });
  await t.ask("Divi", json("Skaitīsim kopā!", { routine: "count", number: 3 }));
  expect(t.asked.at(-1)!.system).toContain("On your screen now: 3 dots");
  expect(t.types().at(-2)).toBe("heard"); // the same dots
  await t.ask("Trīs", json("Jā, trīs! Un tagad?", { routine: "count", number: 5 }));
  expect(t.texts.at(-2)).toEqual({ type: "count", n: 5 });
  await t.ask("Punktus uz burtu A!", json("Vārdi uz A!", { routine: "letters", letter: "a" }));
  expect(t.texts.slice(-3, -1)).toEqual([{ type: "count" }, { type: "letter", letter: "a" }]); // its dots go first
  await t.ask("Skaitīsim!", json("Cik?", { routine: "count", number: 2 }));
  expect(t.texts.slice(-3, -1)).toEqual([{ type: "letter" }, { type: "count", n: 2 }]);
  await t.ask("Pietiek", json("Labi!"));
  expect(t.texts.at(-2)).toEqual({ type: "count" });
  await t.ask("Skaitīsim vēl!", json("Cik?", { routine: "count", number: 2 })); // a new game: shown again
  expect(t.texts.at(-2)).toEqual({ type: "count", n: 2 });
});

test("spelling a word: the server spells it to the device before Cubey speaks, then times each letter as Cubey says it alone", async () => {
  const t = await talk();
  expect(t.voices.at(-1)!.timed).toBe(false); // its voice times nothing it isn't spelling
  const spelt = JSON.stringify({ spell: "Kaķis", say: "Kaķis: k, a, ķ, i, s. Kaķis!", routine: "spell" });
  await t.ask("Kā raksta kaķis?", spelt);
  expect(t.voices.at(-1)!.timed).toBe(true);
  expect(t.texts.slice(-8)).toEqual([
    { type: "heard" },
    { type: "spell", word: "kaķis" },
    { type: "spell.at", i: 0, ms: 700 },
    { type: "spell.at", i: 1, ms: 1000 },
    { type: "spell.at", i: 2, ms: 1300 },
    { type: "spell.at", i: 3, ms: 1600 },
    { type: "spell.at", i: 4, ms: 1900 },
    { type: "said", heard: "Kā raksta kaķis?", said: "Kaķis: k, a, ķ, i, s. Kaķis!", last: false },
  ]);
  expect(t.logged).toContain("spelled kaķis: 5 of 5 letters timed"); // to see on the board's server whether Soniox's timing finds them
  await t.ask("Un suns?", JSON.stringify({ spell: "suns jā", say: "Suns: s, u, n, s.", routine: "spell" }));
  expect(t.asked.at(-1)!.model).toBe("games"); // a game with words, while she plays it
  expect(t.asked.at(-1)!.system).toContain("Now: spelling words");
  expect(t.texts).toContainEqual({ type: "spell", word: "suns" }); // one word
  await t.ask("Pietiek!", json("Labi!"));
  await t.ask("Kāpēc?", json("Tāpēc."));
  expect(t.asked.at(-1)!.model).toBe("talk");
  expect(t.asked.at(-1)!.system).not.toContain("Now: spelling words");
});

test("another routine ends a game with letters first", async () => {
  const t = await talk();
  await t.ask("Spēlēsim!", json("Tev uz A!", { routine: "letters", letter: "a" }));
  await t.ask("Met kauliņu!", json("Metam!", { routine: "dice" }));
  expect(t.types().slice(-4)).toEqual(["heard", "letter", "dice", "said"]);
});

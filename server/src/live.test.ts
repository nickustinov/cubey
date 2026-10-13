import { expect, test } from "bun:test";
import { MediaModality } from "@google/genai";
import type { Child } from "./child";
import type { Earlier } from "./history";
import { type Connect, type LiveHeard, liveTalks as cubey } from "./live";
import type { Battery, Opened } from "./talk";
import type { Exchanged } from "./log";

const anna: Child = { id: "Anna", name: "Anna", ageMonths: 42, timeZone: "Europe/Lisbon", gender: "girl", language: "lv", learns: "en", parents: "mum_dad" };
const before: Earlier[] = [{ at: new Date().toISOString(), heard: "Kur ir saule?", said: "Aiz mākoņiem." }];

// What Gemini says, as its messages come.
const speech = (text: string): LiveHeard => ({
  serverContent: { modelTurn: { parts: [{ inlineData: { data: Buffer.alloc(4800).toString("base64") } }] }, outputTranscription: { text } },
});
const words = (text: string): LiveHeard => ({ serverContent: { inputTranscription: { text } } });
const over: LiveHeard = { serverContent: { turnComplete: true } };
const cut: LiveHeard = { serverContent: { interrupted: true } };
const call = (name: string, args: Record<string, unknown> = {}): LiveHeard => ({ toolCall: { functionCalls: [{ id: `${name}-1`, name, args }] } });
const resumable = (handle: string): LiveHeard => ({ sessionResumptionUpdate: { newHandle: handle, resumable: true } });
const goAway: LiveHeard = { goAway: { timeLeft: "10s" } };
// What Gemini billed for a turn: the talk so far in, its answer out.
const usage = (text: number, audio: number, out: number): LiveHeard => ({
  usageMetadata: {
    promptTokensDetails: [{ modality: MediaModality.TEXT, tokenCount: text }, { modality: MediaModality.AUDIO, tokenCount: audio }],
    responseTokensDetails: [{ modality: MediaModality.AUDIO, tokenCount: out }],
  },
});

// Cubey's talk on a pretend Gemini Live, for a device playing at 16 kHz.
async function talk(opened: Opened = {}, earlier = before, battery: Battery | null = { percent: 73, charging: false }) {
  const sent: unknown[] = []; // to Gemini: her audio's bytes, notes, calls answered, a close
  const connections: { system: string; language: string; handle?: string; tell: (msg: LiveHeard) => void; drop: (why: string) => void }[] = [];
  const connect: Connect = async ({ system, language, handle, onMessage, onClose }) => {
    connections.push({ system, language, handle, tell: onMessage, drop: onClose });
    return {
      audio: (pcm) => void sent.push({ audio: pcm.length }),
      note: (text, turnComplete) => void sent.push({ note: text, turnComplete }),
      answer: (calls) => void sent.push({ answered: calls.map((c) => (c.result ? [c.name, c.result] : c.name)) }),
      close: () => (sent.push("close"), onClose("closed 1000")), // as Gemini's: closing it closes it
    };
  };
  const logged: string[] = [];
  const spent: object[] = [];
  const texts: any[] = [];
  const exchanges: Exchanged[] = [];
  let audio = 0;
  const live = cubey({ connect, history: { recent: () => earlier }, log: (line) => void logged.push(line) })
    .open(anna, opened, {
      rate: 16000, text: (m) => void texts.push(m), audio: (pcm) => void (audio += pcm.length), exchange: (e) => void exchanges.push(e),
      battery: () => battery ?? undefined, spent: (s) => void spent.push(s), learn: () => anna, brighten: () => anna, turn: () => anna, colour: () => anna,
    });
  await Bun.sleep(1);
  const gemini = async (...msgs: LiveHeard[]) => {
    msgs.forEach((m) => connections.at(-1)!.tell(m));
    await Bun.sleep(1); // what goes back to Gemini
  };
  // She says something: the device listens and streams the mic.
  const ask = async (heard = "Kāpēc debesis ir zilas?") => {
    live.listen();
    await Bun.sleep(1);
    live.audio(new Uint8Array(640));
    await Bun.sleep(1);
    await gemini(words(heard));
  };
  const types = () => texts.map((t) => t.type);
  return { live, gemini, ask, texts, types, exchanges, sent, connections, logged, audio: () => audio, drop: (why: string) => connections.at(-1)!.drop(why) };
}

test("Cubey's hello: Gemini Live is told what happened, in Cubey's prompt and her language, and speaks at the device's rate", async () => {
  const t = await talk();
  expect(t.connections[0].system).toContain("You are Cubey");
  expect(t.connections[0].system).toContain("Always speak Latvian");
  expect(t.connections[0].system).toContain("Aiz mākoņiem.");
  expect(t.connections[0].language).toBe("lv");
  expect(t.connections[0].handle).toBeUndefined();
  expect(t.sent).toEqual([{ note: "(Anna has just woken you.)", turnComplete: true }]);
  await t.gemini(speech("Čau, Anna!"), over);
  expect(t.texts).toEqual([{ type: "said", heard: "", said: "Čau, Anna!", last: false }]);
  expect(t.audio()).toBeGreaterThan(3000); // two samples for three, at 16 kHz
  expect(t.audio()).toBeLessThan(3200);
  expect(t.exchanges).toEqual([{ heard: "(Anna has just woken you.)", said: "Čau, Anna!", forParents: false }]);
});

test("meeting her for the first time, Cubey says who it is; going on after a lost connection, no hello", async () => {
  expect((await talk({}, [])).sent).toEqual([{ note: "(Anna has met you for the first time: say hello and who you are.)", turnComplete: true }]);
  const again = await talk({ again: true });
  expect(again.sent).toEqual([]);
  expect(again.texts).toEqual([{ type: "said", heard: "", said: "", last: false }]);
});

test("she talks: her mic goes to Gemini, the first of its answer tells the device it heard her, then its speech and what was said", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  expect(t.live.hearing()).toBe(false);
  await t.ask();
  expect(t.live.hearing()).toBe(true);
  expect(t.sent.at(-1)).toEqual({ audio: 640 });
  await t.gemini(speech("Jo saule"), speech(" izkliedē zilo."));
  expect(t.types()).toEqual(["said", "listening", "heard"]);
  expect(t.live.hearing()).toBe(false);
  await t.gemini(over);
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "Kāpēc debesis ir zilas?", said: "Jo saule izkliedē zilo.", last: false });
  expect(t.exchanges.at(-1)).toEqual({ heard: "Kāpēc debesis ir zilas?", said: "Jo saule izkliedē zilo.", forParents: false });
});

test("its goodbye and the parents' questions come as calls, answered silently; the goodbye ends the link", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask("Kur rodas bērni?");
  await t.gemini(call("for_parents"), speech("Pajautā mammai!"), over);
  expect(t.sent).toContainEqual({ answered: ["for_parents"] });
  expect(t.exchanges.at(-1)).toMatchObject({ heard: "Kur rodas bērni?", forParents: true });
  expect(t.texts.at(-1)).toMatchObject({ type: "said", last: false });
  await t.ask("Atā");
  await t.gemini(speech("Atā!"), call("bye"), over);
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "Atā", said: "Atā!", last: true });
  expect(t.sent.at(-1)).toBe("close");
});

test("she talks over it: Gemini stops, the device is told, and her words are the next turn", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask();
  await t.gemini(speech("Jo saule"));
  t.live.overhear();
  expect(t.live.hearing()).toBe(true);
  await t.gemini(cut, over);
  expect(t.types().slice(-1)).toEqual(["interrupted"]);
  expect(t.exchanges.at(-1)).toMatchObject({ heard: "Kāpēc debesis ir zilas?", said: "Jo saule" });
  expect(t.live.hearing()).toBe(true);
  await t.gemini(words("Pagaidi!"), speech("Jā?"), over);
  expect(t.types().slice(-3)).toEqual(["interrupted", "heard", "said"]);
  expect(t.texts.at(-1)).toMatchObject({ heard: "Pagaidi!", said: "Jā?" });
  // Over the end of its speech, all sent but still playing: an interruption too.
  t.live.overhear();
  await t.gemini(words("Vēl!"), speech("Labi!"));
  expect(t.types().slice(-2)).toEqual(["interrupted", "heard"]);
});

test("stopped (the device gave up on it), the rest of its turn is dropped; a turn nobody asked for is dropped too", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask();
  await t.gemini(speech("Jo"));
  const audio = t.audio();
  t.live.stop();
  expect(t.live.hearing()).toBe(false);
  await t.gemini(speech(" saule"), over);
  expect(t.audio()).toBe(audio);
  expect(t.types()).toEqual(["said", "listening", "heard"]);
  await t.gemini(speech("Hm?"), over); // Gemini on its own
  expect(t.audio()).toBe(audio);
  expect(t.types().length).toBe(3);
  await t.ask("Vēlreiz");
  await t.gemini(speech("Jā."), over);
  expect(t.texts.at(-1)).toMatchObject({ type: "said", heard: "Vēlreiz", said: "Jā." });
});

test("Gemini gone: the turn under way fails, and the next listen picks the talk up where it was, with no hello", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over, resumable("talk-1"));
  await t.ask();
  t.drop("1011 deadline");
  expect(t.types().slice(-2)).toEqual(["heard", "failed"]);
  expect(t.live.hearing()).toBe(false);
  await t.ask();
  expect(t.connections.length).toBe(2);
  expect(t.connections[1].handle).toBe("talk-1");
  expect(t.sent.filter((s: any) => s.note).length).toBe(1);
  await t.gemini(speech("Jā."), over);
  expect(t.texts.at(-1)).toMatchObject({ type: "said", said: "Jā." });
});

test("Gemini ends its connections now and then: the talk moves to a new one where it was, between turns, without a word lost", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), resumable("talk-1"), over);
  await t.ask();
  await t.gemini(speech("Jo"), goAway); // mid-answer: it waits for the end of the turn, and where the talk is after it
  expect(t.connections.length).toBe(1);
  await t.gemini(speech(" saule."), over, resumable("talk-2"));
  expect(t.connections.length).toBe(2);
  expect(t.connections[1].handle).toBe("talk-2");
  expect(t.sent).toContain("close"); // the old one
  expect(t.texts.at(-1)).toMatchObject({ type: "said", said: "Jo saule." });
  await t.ask("Un mēness?");
  expect(t.sent.at(-1)).toEqual({ audio: 640 }); // to the new one
  await t.gemini(speech("Arī."), over);
  expect(t.texts.at(-1)).toMatchObject({ type: "said", heard: "Un mēness?", said: "Arī." });
  expect(t.types()).not.toContain("failed");
});

test("listening while she hasn't said a word yet, it moves at once; once she talks, after Cubey's answer", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), resumable("talk-1"), over);
  t.live.listen();
  await t.gemini(goAway);
  expect(t.connections.length).toBe(2);
  expect(t.connections[1].handle).toBe("talk-1");
  await t.gemini(words("Kāpēc"), resumable("talk-2"), goAway);
  expect(t.connections.length).toBe(2);
  await t.gemini(speech("Tāpēc."), over);
  expect(t.connections.length).toBe(2); // no word yet of where the talk is after the answer
  t.live.listen(); // its answer played: a quiet moment
  expect(t.connections.length).toBe(3);
  expect(t.connections[2].handle).toBe("talk-2");
});

test("closed with what happened still unanswered (she stopped brushing as Cubey fell asleep), it is in the history all the same", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  t.live.tell("(Brushing: she stopped it.)");
  t.live.close();
  expect(t.exchanges.at(-1)).toEqual({ heard: "(Brushing: she stopped it.)", said: "", forParents: false });
});

test("closed: Gemini's link is closed, and nothing more reaches the device", async () => {
  const t = await talk();
  t.live.close();
  await Bun.sleep(1);
  expect(t.sent.at(-1)).toBe("close");
  await t.gemini(speech("Čau!"));
  t.live.listen();
  expect(t.texts).toEqual([]);
  expect(t.audio()).toBe(0);
});

test("what each connection cost, as Gemini counted what it billed, is logged when it ends", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over, usage(1800, 200, 100));
  await t.ask("Atā");
  await t.gemini(speech("Atā!"), call("bye"), over, usage(1900, 500, 100));
  expect(t.logged).toEqual(["Cubey on Gemini Live: 2 turns, 3700 text and 700 audio tokens in, 200 out: about $0.007"]);
  await t.ask("Vēl!"); // a connection of its own
  t.drop("1011 deadline");
  t.live.close();
  expect(t.logged).toHaveLength(1); // nothing billed, nothing logged
});

test("asked about its battery, Cubey looks it up: Gemini gets the device's charge, the device shows its battery, and the answer is one turn", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask("Cik tev ir baterija?");
  await t.gemini(call("battery"));
  expect(t.sent).toContainEqual({ answered: [["battery", { percent: 73, charging: false }]] });
  expect(t.types().slice(-2)).toEqual(["heard", "battery"]);
  await t.gemini(over); // its turn may end with the call: the answer it waited for comes next
  expect(t.types().at(-1)).toBe("battery");
  await t.gemini(speech("Man ir septiņdesmit trīs procenti."), over);
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "Cik tev ir baterija?", said: "Man ir septiņdesmit trīs procenti.", last: false });
  expect(t.exchanges.at(-1)).toEqual({ heard: "Cik tev ir baterija?", said: "Man ir septiņdesmit trīs procenti.", forParents: false });
});

test("a device that can't tell its battery: Gemini hears so, and nothing shows", async () => {
  const t = await talk({}, before, null);
  await t.gemini(speech("Čau!"), over);
  await t.ask("Cik tev ir baterija?");
  await t.gemini(call("battery"));
  expect(t.sent).toContainEqual({ answered: [["battery", { unknown: true }]] });
  expect(t.types()).not.toContain("battery");
});

test("she wants to brush: Gemini calls brushing and waits, the device shows its mouth, and Cubey asks her to get ready and tap, as one turn", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask("Gribu tīrīt zobus!");
  await t.gemini(call("brushing"));
  expect(t.sent).toContainEqual({ answered: [["brushing", { shown: true }]] });
  expect(t.types().slice(-2)).toEqual(["heard", "brushing"]);
  await t.gemini(over, speech("Paņem birstīti un pieskaries man!"), over);
  expect(t.texts.at(-1)).toEqual({ type: "said", heard: "Gribu tīrīt zobus!", said: "Paņem birstīti un pieskaries man!", last: false });
});

test("the device says a part has begun: Gemini is told, and says where next, heard by the device as an answer", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  t.live.listen();
  t.live.tell("(Brushing: now the top, on the other side.)");
  await Bun.sleep(1);
  expect(t.sent.at(-1)).toEqual({ note: "(Brushing: now the top, on the other side.)", turnComplete: true });
  await t.gemini(speech("Tagad otrā pusē!"), over);
  expect(t.types().slice(-3)).toEqual(["listening", "heard", "said"]);
  expect(t.exchanges.at(-1)).toEqual({ heard: "(Brushing: now the top, on the other side.)", said: "Tagad otrā pusē!", forParents: false });
});

test("she wants to stop brushing: Gemini calls stop_brushing, and the device stops", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask("Negribu vairs!");
  await t.gemini(call("stop_brushing"), speech("Labi!"), over);
  expect(t.sent).toContainEqual({ answered: ["stop_brushing"] });
  expect(t.types()).toContain("brushing.stop");
  expect(t.texts.at(-1)).toMatchObject({ type: "said", said: "Labi!" });
});

test("a timer, for so many minutes and for what: the device shows its clock, and Cubey says it's on, as one turn", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask("Ieslēdzam multeņu taimeri uz 40 minūtēm!");
  await t.gemini(call("timer", { minutes: 40, for: "cartoons" }));
  expect(t.sent).toContainEqual({ answered: [["timer", { started: true, minutes: 40 }]] });
  expect(t.texts.at(-1)).toEqual({ type: "timer", minutes: 40, for: "cartoons" });
  await t.gemini(over, speech("Labi, četrdesmit minūtes!"), over);
  expect(t.texts.at(-1)).toMatchObject({ type: "said", heard: "Ieslēdzam multeņu taimeri uz 40 minūtēm!", said: "Labi, četrdesmit minūtes!" });
});

test("a timer of no sensible length, or for what Cubey doesn't know: Gemini hears so, or it's for something else", async () => {
  const t = await talk();
  await t.gemini(speech("Čau!"), over);
  await t.ask("Taimeri uz tūkstoš minūtēm!");
  await t.gemini(call("timer", { minutes: 1000, for: "cartoons" }));
  expect(t.sent).toContainEqual({ answered: [["timer", { error: "a timer is 1 to 180 minutes" }]] });
  expect(t.types()).not.toContain("timer");
  await t.gemini(over, speech("Tas ir par ilgu."), over);
  await t.ask("Tad uz 10, mazgāties!");
  await t.gemini(call("timer", { minutes: 10, for: "swimming" }));
  expect(t.texts.at(-1)).toEqual({ type: "timer", minutes: 10, for: "other" });
});

test("what happened instead of her waking it (the timer's end); or a word once, while nobody listens (a reminder)", async () => {
  const done = await talk({ note: "(The cartoons timer is over.)" });
  expect(done.sent).toEqual([{ note: "(The cartoons timer is over.)", turnComplete: true }]);
  const once = await talk({ note: "(5 minutes left.)", once: true });
  await once.gemini(speech("Vēl piecas minūtes!"), over);
  expect(once.texts).toEqual([{ type: "said", heard: "", said: "Vēl piecas minūtes!", last: false }]);
  expect(once.sent.at(-1)).toBe("close");
  expect(once.exchanges).toEqual([{ heard: "(5 minutes left.)", said: "Vēl piecas minūtes!", forParents: false }]);
});

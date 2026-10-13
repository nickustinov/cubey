import { afterEach, describe, expect, test } from "bun:test";
import { Speaker, Transcriber } from "./soniox";

process.env.SONIOX_API_KEY ||= "test-key";

// A stand-in for Soniox's STT socket: answers the config with `tokens`, and a finalize with <fin>.
// `finalized` counts the finalize requests; `config` is the last config it got.
let finalized = 0;
let config: Record<string, unknown> = {};
function fakeStt(tokens: { text: string; is_final: boolean }[]) {
  finalized = 0;
  return Bun.serve({
    port: 0,
    fetch: (req, server) => (server.upgrade(req) ? undefined : new Response("no", { status: 403 })),
    websocket: {
      message(ws, msg) {
        if (typeof msg !== "string") return;
        const parsed = JSON.parse(msg);
        if (parsed.api_key) (config = parsed), ws.send(JSON.stringify({ tokens }));
        if (parsed.type === "finalize") finalized++, ws.send(JSON.stringify({ tokens: [{ text: "<fin>", is_final: true }] }));
      },
    },
  });
}

let server: ReturnType<typeof Bun.serve> | undefined;
afterEach(() => server?.stop(true));

const tick = () => Bun.sleep(50);
const url = () => server!.url.href.replace("http", "ws");

describe("Transcriber", () => {
  test("listens for her language and her name, and ends by itself when Soniox hears her stop", async () => {
    server = fakeStt([{ text: "Jā", is_final: true }, { text: "<end>", is_final: true }]);
    let ends = 0;
    const stt = new Transcriber({ language: "lv", terms: ["Anna"], url: url(), onEnd: () => ends++ });
    stt.send(new Uint8Array(640));
    await tick();
    expect(config).toMatchObject({ language_hints: ["lv"], context: { terms: ["Anna"] }, enable_endpoint_detection: true });
    expect(ends).toBe(1);
    expect(await stt.finish()).toBe("Jā");
    expect(finalized).toBe(0); // the text before <end> is already final: no round trip
  });

  test("tells what it hears so far, the guess included, as it changes", async () => {
    server = fakeStt([{ text: "Pa", is_final: true }, { text: "gaidi", is_final: false }]);
    const heard: string[] = [];
    const stt = new Transcriber({ language: "lv", url: url(), onEnd: () => {}, onWords: (soFar) => heard.push(soFar) });
    stt.send(new Uint8Array(640));
    await tick();
    expect(heard).toEqual(["Pagaidi"]);
    stt.cancel();
  });

  test("listens for another language she may speak too (the parrot's)", async () => {
    server = fakeStt([]);
    const stt = new Transcriber({ language: "lv", also: ["en"], url: url(), onEnd: () => {} });
    await tick();
    expect(config).toMatchObject({ language_hints: ["lv", "en"] });
    stt.cancel();
  });

  test("with waitMs, a turn where nobody speaks ends by itself, with nothing heard", async () => {
    server = fakeStt([]);
    let ends = 0;
    const stt = new Transcriber({ language: "lv", waitMs: 100, url: url(), onEnd: () => ends++ });
    stt.send(new Uint8Array(640));
    await Bun.sleep(200);
    expect(ends).toBe(1);
    expect(await stt.finish()).toBe("");
  });

  test("with waitMs, a child who started speaking is not cut off", async () => {
    server = fakeStt([{ text: "Ja", is_final: false }]);
    let ends = 0;
    const stt = new Transcriber({ language: "lv", waitMs: 100, url: url(), onEnd: () => ends++ });
    stt.send(new Uint8Array(640));
    await Bun.sleep(200);
    expect(ends).toBe(0);
    stt.cancel();
  });

  test("without a pause heard, finish() asks Soniox to finalize", async () => {
    server = fakeStt([{ text: "Jā", is_final: true }]);
    const stt = new Transcriber({ language: "lv", url: url(), onEnd: () => {} });
    stt.send(new Uint8Array(640));
    await tick();
    expect(await stt.finish()).toBe("Jā");
    expect(finalized).toBe(1);
  });

  test("measures how long Soniox waited after the last word, in her audio: from the word's end to the pause", async () => {
    let bytes = 0;
    server = Bun.serve<undefined>({
      port: 0,
      fetch: (req, server) => (server.upgrade(req) ? undefined : new Response("no", { status: 403 })),
      websocket: {
        message(ws, msg) {
          if (typeof msg === "string") return;
          bytes += msg.length;
          if (bytes === 48000) // 1.5 s of her mic: her word ended at 0.4 s
            ws.send(JSON.stringify({ tokens: [{ text: "Jā", is_final: true, start_ms: 100, end_ms: 400 }, { text: "<end>", is_final: true }] }));
        },
      },
    });
    const ended = Promise.withResolvers<void>();
    const stt = new Transcriber({ language: "lv", url: url(), onEnd: ended.resolve });
    for (let i = 0; i < 75; i++) stt.send(new Uint8Array(640));
    await ended.promise;
    expect(await stt.finish()).toBe("Jā");
    expect(stt.pauseMs).toBe(1100);
  });

  test("a pause before any words does not end the utterance", async () => {
    server = fakeStt([{ text: "<end>", is_final: true }]);
    let ends = 0;
    const stt = new Transcriber({ language: "lv", url: url(), onEnd: () => ends++ });
    stt.send(new Uint8Array(640));
    await tick();
    expect(ends).toBe(0);
    stt.cancel();
  });

  test("a refused connection ends the utterance and fails it, instead of throwing per frame", async () => {
    server = Bun.serve({ port: 0, fetch: () => new Response("no", { status: 403 }) });
    let ends = 0;
    const stt = new Transcriber({ language: "lv", url: url(), onEnd: () => ends++ });
    for (let i = 0; i < 5; i++) stt.send(new Uint8Array(640));
    await tick();
    expect(ends).toBe(1);
    await expect(stt.finish()).rejects.toThrow(/cannot connect/);
  });

  test("a cancelled utterance ends quietly", async () => {
    server = fakeStt([]);
    let ends = 0;
    const stt = new Transcriber({ language: "lv", url: url(), onEnd: () => ends++ });
    stt.cancel();
    await tick();
    expect(ends).toBe(0);
  });
});

// A stand-in for Soniox's TTS socket: one byte of audio per text chunk, then terminated at text_end; asked to, each
// chunk's characters timed with it, 0.1 s apart across the stream. `streams` records each stream's config and text,
// in order.
function fakeTts(streams: { voice: string; language: string; sample_rate: number; timestamps?: boolean; text: string }[]) {
  const open = new Map<string, (typeof streams)[number]>();
  return Bun.serve<undefined>({
    port: 0,
    fetch: (req, server) => (server.upgrade(req, { data: undefined }) ? undefined : new Response("no", { status: 403 })),
    websocket: {
      async message(ws, msg) {
        const m = JSON.parse(String(msg));
        if (m.api_key) {
          expect(open.size).toBe(0); // one stream at a time
          return void open.set(m.stream_id, { voice: m.voice, language: m.language, sample_rate: m.sample_rate, timestamps: m.return_timestamps, text: "" });
        }
        const stream = open.get(m.stream_id)!;
        const from = stream.text.length;
        stream.text += m.text;
        const characters = [...m.text as string];
        const timestamps = stream.timestamps && characters.length
          ? { characters, character_start_times_seconds: characters.map((_, i) => (from + i) / 10), character_end_times_seconds: characters.map((_, i) => (from + i + 1) / 10) }
          : undefined;
        ws.send(JSON.stringify({ audio: Buffer.from([1]).toString("base64"), stream_id: m.stream_id, ...(timestamps && { timestamps }) }));
        if (!m.text_end) return;
        await Bun.sleep(20);
        open.delete(m.stream_id);
        streams.push(stream);
        ws.send(JSON.stringify({ terminated: true, stream_id: m.stream_id }));
      },
    },
  });
}

describe("Speaker", () => {
  test("one stream in the voice, language and rate asked for, ended once the last audio is in", async () => {
    const streams: Parameters<typeof fakeTts>[0] = [];
    server = fakeTts(streams);
    let audio = 0;
    const speaker = new Speaker({ voice: "Owl", language: "lv", sampleRate: 24000, speed: 1, onAudio: () => audio++, url: url() });
    await speaker.say("Sveika! ");
    await speaker.say("Kā tev iet?");
    await speaker.end();
    expect(streams).toEqual([{ voice: "Owl", language: "lv", sample_rate: 24000, text: "Sveika! Kā tev iet?" }]);
    expect(audio).toBe(3);
  });

  test("a reply in two languages: a stream for each run, one after the other", async () => {
    const streams: Parameters<typeof fakeTts>[0] = [];
    server = fakeTts(streams);
    const speaker = new Speaker({ voice: "Alistair", language: "lv", sampleRate: 24000, speed: 1, onAudio: () => {}, url: url() });
    await speaker.say("Atkārto pēc manis: ");
    await speaker.say("cat", "en");
    await speaker.say("! Lieliski.");
    await speaker.end();
    expect(streams.map(({ language, text }) => ({ language, text }))).toEqual([
      { language: "lv", text: "Atkārto pēc manis: " },
      { language: "en", text: "cat" },
      { language: "lv", text: "! Lieliski." },
    ]);
  });

  test("asked as its stream opens, it times each character it speaks across the stream; not asked, nothing is timed", async () => {
    const streams: Parameters<typeof fakeTts>[0] = [];
    server = fakeTts(streams);
    let wanted = false;
    const heard: [string[], number[]][] = [];
    const opts = { voice: "Owl", language: "lv", sampleRate: 24000, speed: 1, onAudio: () => {}, url: url() };
    const speaker = new Speaker({ ...opts, timestamps: () => wanted, onCharacters: (c, s) => void heard.push([c, s]) });
    wanted = true; // known once the answer's word to spell is written, before its first words are said
    await speaker.say("K: ");
    await speaker.say("k!");
    await speaker.end();
    expect(streams[0].timestamps).toBe(true);
    expect(heard).toEqual([[["K", ":", " "], [0, 0.1, 0.2]], [["k", "!"], [0.3, 0.4]]]);

    const plain = new Speaker({ ...opts, onCharacters: (c, s) => void heard.push([c, s]) });
    await plain.say("Sveika!");
    await plain.end();
    expect(streams[1].timestamps).toBeUndefined();
    expect(heard).toHaveLength(2);
  });

  // It connects while the child is still talking; a refused connection must not crash the server before
  // anything is said (an unhandled rejection ends the process), and saying something then fails.
  test("a refused connection fails what is said, and nothing else", async () => {
    const unhandled: unknown[] = [];
    const note = (e: unknown) => unhandled.push(e);
    process.on("unhandledRejection", note);
    server = Bun.serve({ port: 0, fetch: () => new Response("no", { status: 403 }) });
    const speaker = new Speaker({ voice: "Owl", language: "lv", sampleRate: 24000, speed: 1, onAudio: () => {}, url: url() });
    await Bun.sleep(100);
    expect(unhandled).toEqual([]);
    await expect(speaker.say("Sveika!")).rejects.toThrow(/cannot connect/);
    process.off("unhandledRejection", note);
  });
});

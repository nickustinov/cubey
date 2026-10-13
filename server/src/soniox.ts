// Soniox real-time speech-to-text and text-to-speech over WebSocket.
// API shapes follow https://soniox.com/docs/api-reference (stt/websocket-api, tts/websocket-api).

const STT_URL = "wss://stt-rt.soniox.com/transcribe-websocket";
const TTS_URL = "wss://tts-rt.soniox.com/tts-websocket";
export const STT_MODEL = "stt-rt-v5";
export const TTS_MODEL = "tts-rt-v2";
export const MIC_RATE = 16000;

function apiKey(): string {
  const key = process.env.SONIOX_API_KEY;
  if (!key) throw new Error("SONIOX_API_KEY is not set (server/.env)");
  return key;
}

// Words as they count for her turn: small, without punctuation.
export const wordsOf = (text: string) => text.toLowerCase().match(/\p{L}+/gu) ?? [];

// Sends only while the socket is open: a cancelled turn may still try to send.
function send(ws: WebSocket, data: string | Uint8Array) {
  if (ws.readyState === WebSocket.OPEN) ws.send(data as string | Uint8Array<ArrayBuffer>);
}

// Rejects when the socket closes before it opens; the close code says why (the error event does not).
function opened(ws: WebSocket): Promise<void> {
  return new Promise((resolve, reject) => {
    ws.addEventListener("open", () => resolve(), { once: true });
    ws.addEventListener("close", (e) => reject(new Error(`cannot connect to ${ws.url} (${e.code} ${e.reason})`)), { once: true });
  });
}

// Soniox calls the end of her turn itself, from her pauses, intonation and words (soniox.com/docs/stt/rt/
// endpoint-detection), and waits at most this long when it isn't sure: its default, 2 s, felt lagged; at 0.7 s it
// cut her off mid-word ("Ja n…", 2026-10-10). On a clear Latvian sentence its own detection ended the turn 580 ms
// after the last word; its "lower latency" levels waited longer (1,040–1,320 ms). Children pause mid-sentence: a
// longer wait, or endpoint_sensitivity below 0, if it cuts her off.
const ENDPOINT = { max_endpoint_delay_ms: 1000 };
// Her words unchanged this long, she has paused: a moment to start on the answer before her turn is sure to be over
// (cascade.ts); Soniox still decides when it is.
const SETTLED_MS = 300;

// One utterance: connect when listening starts, stream PCM, finish() for the text. Soniox listens for the
// child to stop: onEnd is called once when she stops after saying something, or when the utterance fails
// (finish() then rejects with the reason). also: other languages she may speak (the parrot's). waitMs: if
// she says nothing this long, the utterance ends anyway, with nothing heard. onWords: what is heard so far, the
// guess included, each time it changes; onPause: what is heard so far once it hasn't changed for SETTLED_MS.
export class Transcriber {
  private ws: WebSocket;
  private ready: Promise<void>;
  private text = "";
  private words = ""; // what is heard so far, words only
  private done = Promise.withResolvers<string>();
  private ended = false;
  private endpointed = false;
  private heardSoFar = "";
  private settling?: ReturnType<typeof setTimeout>;
  private sentMs = 0; // her audio sent so far
  private lastWordEnd = 0; // where in it her last word ended, as Soniox heard it
  pauseMs = 0; // how long Soniox waited after the last word before it called the pause

  constructor(private opts: {
    language: string; also?: string[]; terms?: string[]; waitMs?: number; onEnd: () => void; onWords?: (soFar: string) => void;
    onPause?: (soFar: string) => void; url?: string;
  }) {
    this.ws = new WebSocket(opts.url ?? STT_URL);
    this.ready = opened(this.ws).then(() => {
      this.ws.send(JSON.stringify({
        api_key: apiKey(),
        model: STT_MODEL,
        audio_format: "pcm_s16le",
        sample_rate: MIC_RATE,
        num_channels: 1,
        language_hints: [opts.language, ...(opts.also ?? [])],
        ...(opts.terms?.length ? { context: { terms: opts.terms } } : {}),
        enable_endpoint_detection: true,
        ...ENDPOINT,
      }));
    });
    this.ready.catch((e) => this.fail(e));
    if (opts.waitMs) this.ready.then(() => setTimeout(() => !this.heardSoFar && this.end(), opts.waitMs), () => {});
    this.done.promise.catch(() => {}); // handled by finish(); a cancelled turn may never ask
    this.ws.addEventListener("close", () => this.fail(new Error("Soniox STT closed the connection")));
    this.ws.addEventListener("message", (e) => {
      const msg = JSON.parse(String(e.data));
      if (msg.error_code) return this.fail(new Error(`Soniox STT ${msg.error_type}: ${msg.error_message}`));
      let guess = "";
      for (const token of msg.tokens ?? []) {
        if (!token.is_final) {
          guess += token.text;
          continue;
        }
        if (token.text === "<fin>") return this.done.resolve(this.text.trim());
        if (token.text !== "<end>") {
          this.text += token.text;
          if (token.end_ms) this.lastWordEnd = token.end_ms;
        } else if (this.text.trim()) {
          // Everything before <end> is final: the text is complete, no finalize round trip needed.
          this.pauseMs = Math.round(this.sentMs - this.lastWordEnd);
          this.endpointed = true;
          this.done.resolve(this.text.trim());
          this.end();
        }
      }
      if (this.text + guess !== this.heardSoFar) {
        this.heardSoFar = this.text + guess;
        this.opts.onWords?.(this.heardSoFar);
        // Her pause counts from her words: Soniox's punctuation and finalizing of the same words put it off until her
        // turn was over, and the answer begun at it was hardly ever used (2026-10-11).
        const words = wordsOf(this.heardSoFar).join(" ");
        if (words !== this.words) {
          this.words = words;
          clearTimeout(this.settling);
          if (this.opts.onPause && words) this.settling = setTimeout(() => !this.ended && this.opts.onPause?.(this.heardSoFar), SETTLED_MS);
        }
      }
    });
  }

  // Queued until the socket is open; if it never opens, finish() reports why.
  send(pcm: Uint8Array) {
    this.sentMs += pcm.length / ((MIC_RATE * 2) / 1000);
    this.ready.then(() => send(this.ws, pcm), () => {});
  }

  // Soniox advises ~200 ms of silence before a manual finalize.
  async finish(): Promise<string> {
    this.ended = true;
    await this.ready;
    if (!this.endpointed) {
      send(this.ws, new Uint8Array((MIC_RATE * 2) / 5));
      send(this.ws, JSON.stringify({ type: "finalize" }));
    }
    try {
      return await this.done.promise;
    } finally {
      send(this.ws, new Uint8Array(0));
      this.ws.close();
    }
  }

  cancel() {
    this.ended = true;
    clearTimeout(this.settling);
    this.ws.close();
  }

  private end() {
    clearTimeout(this.settling);
    if (this.ended) return;
    this.ended = true;
    this.opts.onEnd();
  }

  private fail(e: Error) {
    this.done.reject(e); // no-op once the text is in
    this.end();
  }
}

export type SpeakerOpts = {
  voice: string;
  language: string;
  sampleRate: number;
  speed: number; // Soniox allows 0.7 to 1.3
  onAudio: (pcm: Buffer) => void;
  // Whether its stream times each character it speaks, asked as the stream opens (with the first text); and what is
  // timed: the characters as it reads them, and when each begins, in seconds into the stream.
  timestamps?: () => boolean;
  onCharacters?: (characters: string[], seconds: number[]) => void;
  url?: string;
};

// One spoken reply: connect early (while the answer is being made), start the stream with the first text,
// end after the last. A TTS stream that waits too long for text is dropped (about 10 s), so the stream's
// config goes out with the first text. A stream reads everything in its language: text in another (the
// parrot's words) ends it and starts the next, one stream at a time on the same connection.
export class Speaker {
  private ws: WebSocket;
  private ready: Promise<void>;
  private stream?: { id: string; language: string; done: PromiseWithResolvers<void> };

  constructor(private opts: SpeakerOpts) {
    this.ws = new WebSocket(opts.url ?? TTS_URL);
    this.ready = opened(this.ws);
    this.ready.catch(() => {}); // it connects before there is anything to say: the failure surfaces in say() and end()
    this.ws.addEventListener("message", (e) => {
      const msg = JSON.parse(String(e.data));
      if (msg.error_code) return this.stream?.done.reject(new Error(`Soniox TTS ${msg.error_type}: ${msg.error_message}`));
      if (msg.audio) this.opts.onAudio(Buffer.from(msg.audio, "base64"));
      if (msg.timestamps) this.opts.onCharacters?.(msg.timestamps.characters, msg.timestamps.character_start_times_seconds);
      if (msg.terminated && this.stream && msg.stream_id === this.stream.id) this.stream.done.resolve();
    });
  }

  private async open(language: string) {
    await this.ready;
    const done = Promise.withResolvers<void>();
    done.promise.catch(() => {}); // handled by close(); a cancelled reply may never ask
    this.stream = { id: crypto.randomUUID(), language, done };
    send(this.ws, JSON.stringify({
      api_key: apiKey(),
      model: TTS_MODEL,
      language,
      voice: this.opts.voice,
      audio_format: "pcm_s16le",
      sample_rate: this.opts.sampleRate,
      speed: this.opts.speed,
      stream_id: this.stream.id,
      ...(this.opts.timestamps?.() && { return_timestamps: true }),
    }));
  }

  // Resolves once the stream's last audio chunk has arrived.
  private async close() {
    const stream = this.stream!;
    send(this.ws, JSON.stringify({ text: "", text_end: true, stream_id: stream.id }));
    await stream.done.promise;
    this.stream = undefined;
  }

  async say(text: string, language = this.opts.language) {
    if (this.stream && this.stream.language !== language) await this.close();
    if (!this.stream) await this.open(language);
    send(this.ws, JSON.stringify({ text, text_end: false, stream_id: this.stream!.id }));
  }

  // Resolves once the last audio chunk has arrived.
  async end(): Promise<void> {
    try {
      if (!this.stream) await this.open(this.opts.language);
      await this.close();
    } finally {
      this.ws.close();
    }
  }

  cancel() {
    if (this.stream) send(this.ws, JSON.stringify({ stream_id: this.stream.id, cancel: true }));
    this.stream?.done.resolve();
    this.ws.close();
  }
}

// Whole-text synthesis: a pretend child's question for `bun run ask`.
export async function synthesize(text: string, opts: Omit<SpeakerOpts, "onAudio">): Promise<Buffer> {
  const chunks: Buffer[] = [];
  const speaker = new Speaker({ ...opts, onAudio: (pcm) => chunks.push(pcm) });
  await speaker.say(text);
  await speaker.end();
  return Buffer.concat(chunks);
}

export function wav(pcm: Buffer, sampleRate: number): Buffer {
  const header = Buffer.alloc(44);
  header.write("RIFF", 0);
  header.writeUInt32LE(36 + pcm.length, 4);
  header.write("WAVEfmt ", 8);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20); // PCM
  header.writeUInt16LE(1, 22); // mono
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(sampleRate * 2, 28);
  header.writeUInt16LE(2, 32);
  header.writeUInt16LE(16, 34);
  header.write("data", 36);
  header.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([header, pcm]);
}

// Cubey's talk on Gemini Live (gemini-3.8-live), speech to speech: her voice goes straight to the model and its own
// comes back, and Gemini hears her talk over it. Behind the device's protocol (session.ts): the first of an answer is
// "heard", the end of its turn "said", Gemini's cutting it short "interrupted". Its goodbye and the questions for the
// parents are calls of tools, and so are its routines: asked about its battery, it looks up the device's charge
// (Gemini waits for it) and the device shows its battery; brushing, the device shows its mouth and keeps the time,
// and what happens is told to Gemini as it goes (tell); a timer, the device shows its clock and the talk ends until
// the timer does (its reminders are a word once, while nobody listens). What was said both ways comes as Gemini's transcripts, for
// the parents' log and the history. Gemini ends each connection after about ten minutes, saying so first (GoAway): the talk moves to a new
// one where it was (its session resumption), at a quiet moment between turns.
import { Behavior, FunctionResponseScheduling, GoogleGenAI, type LiveConnectConfig, type LiveServerMessage, MediaModality, Modality, Type, type UsageMetadata } from "@google/genai";
import { type Child, nameOf } from "./child";
import { cubeyPrompt, helloNote, TIMER_FOR } from "./cubey";
import { type Earlier, earlierBlock } from "./history";
import type { Exchanged } from "./log";
import { Resampler } from "./resample";
import { type Opened, routineMessage, type Talk, type TalkOut, type Talks, TIMER_MOST, timerOf } from "./talk";

export const LIVE_MODEL = "gemini-3.8-live";
export const MIC_RATE = 16000; // the device's mic
const VOICE = "Sulafat"; // one of Gemini's own, "warm"
const SPEECH_RATE = 24000; // Gemini's, always
// The talk so far, which Gemini is given each turn and bills again (most of the cost, growing with every turn): past
// CONTEXT_MOST tokens it keeps only its end, CONTEXT_KEPT of them, cutting at her turns; the prompt (with the talks
// before) always stays. Gemini counts and trims late: billed, a talk went up to 7,500 tokens and back to 3,700
// (Puce, 2026-10-10), the last one to two and a half minutes of it at 25 tokens a second of speech.
const CONTEXT_MOST = 5000;
const CONTEXT_KEPT = 3000;
// What it charges, $ per 1M tokens (ai.google.dev/gemini-api/docs/pricing, 2026-10-09): text and audio in, audio out.
const PRICE = { text: 0.75, audio: 3, out: 12 };

const TOOLS = [{
  functionDeclarations: [
    { name: "bye", behavior: Behavior.NON_BLOCKING, description: "Your goodbye: she said goodbye or wants to stop." },
    { name: "for_parents", behavior: Behavior.NON_BLOCKING, description: "Her question is one for her parents, or she told you something unsafe." },
    {
      name: "battery", behavior: Behavior.BLOCKING, // 3.8's tools don't wait by default
      description: "How much charge your battery has left, in percent, and whether you are charging; your battery shows on your screen as you say it.",
    },
    {
      name: "brushing", behavior: Behavior.BLOCKING,
      description: "Brush her teeth with her: your mouth shows on your screen, and two minutes start when she taps you.",
    },
    { name: "stop_brushing", behavior: Behavior.NON_BLOCKING, description: "She wants to stop brushing." },
    {
      name: "timer", behavior: Behavior.BLOCKING,
      description: "A timer, for so many minutes: its clock shows on your screen until it is over.",
      parameters: {
        type: Type.OBJECT,
        properties: {
          minutes: { type: Type.INTEGER, description: `How long, 1 to ${TIMER_MOST} minutes.` },
          for: { type: Type.STRING, enum: [...TIMER_FOR], description: "What it is for." },
        },
        required: ["minutes", "for"],
      },
    },
  ],
}];

// What the talk takes from Gemini's messages.
export type LiveHeard = Pick<LiveServerMessage, "serverContent" | "toolCall" | "usageMetadata" | "goAway" | "sessionResumptionUpdate">;
export type LiveLink = {
  audio(pcm: Uint8Array): void; // her mic, 16 kHz
  note(text: string, turnComplete: boolean): void; // what happened; turnComplete: answer it now
  answer(calls: { id?: string; name?: string; result?: Record<string, unknown> }[]): void; // the tools called: a result, or silently
  close(): void;
};
// handle: where an earlier connection's talk was, to go on from it
export type Connect = (opts: {
  system: string; language: string; handle?: string; onMessage(msg: LiveHeard): void; onClose(why: string): void;
}) => Promise<LiveLink>;

// What a connection is set up with: Cubey's prompt, her language, and where an earlier connection's talk was.
export function liveConfig(system: string, language: string, handle?: string): LiveConnectConfig {
  return {
    responseModalities: [Modality.AUDIO],
    systemInstruction: system,
    speechConfig: { voiceConfig: { prebuiltVoiceConfig: { voiceName: VOICE } } },
    inputAudioTranscription: { languageCodes: [language] },
    outputAudioTranscription: {},
    // Her turn ends by Gemini's own detection, as in its playground: 1.5 s of silence (the owl's, for small children's
    // pauses mid-sentence) made every answer start late (2026-10-10).
    contextWindowCompression: { triggerTokens: String(CONTEXT_MOST), slidingWindow: { targetTokens: String(CONTEXT_KEPT) } },
    sessionResumption: { handle },
    tools: TOOLS,
  };
}

let gemini: GoogleGenAI | undefined;

export const connectGemini: Connect = async ({ system, language, handle, onMessage, onClose }) => {
  gemini ??= new GoogleGenAI({ apiKey: process.env.GEMINI_API_KEY });
  const session = await gemini.live.connect({
    model: LIVE_MODEL,
    config: liveConfig(system, language, handle),
    callbacks: {
      onmessage: onMessage,
      onerror: (e) => onClose(String(e.message ?? e)),
      onclose: (e) => onClose(`closed ${e.code} ${e.reason}`),
    },
  });
  return {
    audio: (pcm) => session.sendRealtimeInput({ audio: { data: Buffer.from(pcm).toString("base64"), mimeType: `audio/pcm;rate=${MIC_RATE}` } }),
    note: (text, turnComplete) => session.sendClientContent({ turns: [{ role: "user", parts: [{ text }] }], turnComplete }),
    // With a result, Gemini says what it learned; silently, it goes on as it was, without a word about the call.
    answer: (calls) => session.sendToolResponse({
      functionResponses: calls.map(({ id, name, result }) =>
        result ? { id, name, response: result } : { id, name, response: { result: "ok" }, scheduling: FunctionResponseScheduling.SILENT }),
    }),
    close: () => session.close(),
  };
};

type Deps = {
  connect?: Connect;
  history: { recent(child: string): Earlier[] };
  log?: (line: string) => void; // what each connection cost
};

// Gemini's own count of what it billed for the turns of one connection.
type Billed = { turns: number; text: number; audio: number; out: number };
function bill(billed: Billed, { promptTokensDetails, responseTokensDetails }: UsageMetadata) {
  const of = (details = promptTokensDetails, modality: MediaModality) =>
    (details ?? []).filter((d) => d.modality === modality).reduce((sum, d) => sum + (d.tokenCount ?? 0), 0);
  billed.turns++;
  billed.text += of(promptTokensDetails, MediaModality.TEXT);
  billed.audio += of(promptTokensDetails, MediaModality.AUDIO);
  billed.out += of(responseTokensDetails, MediaModality.AUDIO);
}
const cost = ({ text, audio, out }: Billed) => (text * PRICE.text + audio * PRICE.audio + out * PRICE.out) / 1e6;

export const liveTalks = (deps: Deps): Talks => ({ open: (child, opened, out) => new CubeyTalk(child, opened, out, deps) });

// One of its answers (or the hello), as Gemini says it. muted: nobody listens (stopped, or a turn of Gemini's own);
// looked_up: it called a routine that Gemini waits on, so its words come after.
type Turn = {
  asked?: string; said: string; forParents: boolean; bye?: boolean; muted: boolean; spoke: boolean; looked_up: boolean; speech: Resampler;
};

class CubeyTalk implements Talk {
  private link?: Promise<LiveLink>;
  private mic: "off" | "listening" | "overhearing" = "off";
  private turn?: Turn;
  private heard = ""; // her words since the last turn
  private hello?: string; // what happened, while Cubey's answer to it is on its way: the hello, or a note
  private handle?: string; // where the talk is, for a new connection to go on from
  private moving = false; // Gemini is ending this connection: the talk moves at the next quiet moment
  private closed = false;
  private once: boolean;

  constructor(private child: Child, opened: Opened, private out: TalkOut, private deps: Deps) {
    this.once = opened.once === true;
    const link = this.connect();
    if (opened.again) {
      out.text({ type: "said", heard: "", said: "", last: false }); // it listens on
      return;
    }
    const hello = (this.hello = opened.note ?? helloNote(child, !deps.history.recent(child.id).length));
    link.then((l) => l.note(hello, true), () => {});
  }

  listen() {
    if (this.closed) return;
    this.out.text({ type: "listening" });
    this.mic = "listening";
    if (!this.link) this.connect();
    else if (this.moving) this.move();
  }

  overhear() {
    if (!this.closed && this.link && this.mic === "off") this.mic = "overhearing";
  }

  voice() {} // Gemini hears her over Cubey itself
  tapped() {} // ponytail: no word game on Gemini Live (its two languages are the cascade's); add one if live comes back

  hearing() {
    return this.mic !== "off";
  }

  audio(pcm: Uint8Array) {
    if (this.mic !== "off") this.send((l) => l.audio(pcm));
  }

  stop() {
    if (this.turn) this.turn.muted = true;
    this.hello = undefined;
    this.mic = "off";
  }

  tell(note: string) {
    if (this.closed) return;
    this.hello = note;
    if (!this.link) this.connect();
    this.send((l) => l.note(note, true));
  }

  // What happened and was never answered (she stopped brushing as Cubey fell asleep) is still in the history.
  close() {
    if (this.hello !== undefined) this.out.exchange({ heard: this.hello, said: "", forParents: false });
    this.closed = true;
    this.unlink();
  }

  private connect(): Promise<LiveLink> {
    this.moving = false;
    const earlier = earlierBlock(this.deps.history.recent(this.child.id), nameOf(this.child));
    const system = cubeyPrompt(this.child, { earlier, now: new Date(), mode: "live" });
    const billed: Billed = { turns: 0, text: 0, audio: 0, out: 0 };
    const link: Promise<LiveLink> = (this.deps.connect ?? connectGemini)({
      system, language: this.child.language, handle: this.handle,
      onMessage: (msg) => {
        if (msg.usageMetadata) bill(billed, msg.usageMetadata);
        if (this.link === link) this.heardFrom(msg);
      },
      onClose: (why) => {
        if (billed.turns) {
          const { turns, text, audio, out } = billed;
          this.deps.log?.(`Cubey on Gemini Live: ${turns} turns, ${text} text and ${audio} audio tokens in, ${out} out: about $${cost(billed).toFixed(3)}`);
          this.out.spent({ what: "live", model: LIVE_MODEL, input: text + audio, output: out, usd: cost(billed) });
          billed.turns = 0; // closed once
        }
        if (this.link === link) this.lost(why);
      },
    });
    link.catch((e) => this.link === link && this.lost(String(e)));
    return (this.link = link);
  }

  private send(what: (link: LiveLink) => void) {
    this.link?.then(what, () => {});
  }

  private unlink() {
    this.send((l) => l.close());
    this.link = undefined;
  }

  // Nothing under way that a new connection would lose: no answer, and she hasn't started talking.
  private quiet() {
    return !this.turn && this.hello === undefined && this.mic !== "overhearing" && !this.heard;
  }

  // To a new connection where the talk is; the old one, closed, only logs what it cost.
  private move() {
    const old = this.link;
    this.connect();
    old?.then((l) => l.close(), () => {});
  }

  private heardFrom({ serverContent: content, toolCall, goAway, sessionResumptionUpdate: resumed }: LiveHeard) {
    if (this.closed) return;
    if (resumed?.resumable && resumed.newHandle) this.handle = resumed.newHandle;
    if (goAway) this.moving = true;
    if ((goAway || resumed) && this.moving && this.quiet()) return this.move();
    if (content?.inputTranscription?.text) this.heard += content.inputTranscription.text;
    const calls = toolCall?.functionCalls ?? [];
    const speech = (content?.modelTurn?.parts ?? []).flatMap((p) => (p.inlineData?.data ? [p.inlineData.data] : []));
    const said = content?.outputTranscription?.text ?? "";
    if (calls.length || speech.length || said) this.turn ??= this.begin();
    const turn = this.turn;
    if (turn) {
      turn.said += said;
      turn.spoke ||= speech.length > 0;
      if (!turn.muted) for (const data of speech) this.out.audio(turn.speech.push(Buffer.from(data, "base64")));
      for (const { name } of calls) {
        if (name === "bye") turn.bye = true;
        if (name === "for_parents") turn.forParents = true;
      }
    }
    if (calls.length) {
      const answered = calls.map(({ id, name, args }) => ({ id, name, result: this.routine(name, args ?? {}, turn) })); // now, not once sent
      this.send((l) => l.answer(answered));
    }
    if (content?.interrupted) this.interrupted();
    else if (content?.turnComplete) this.done();
  }

  // A routine's call: what the device does for it, and the result Gemini waits for (none: it goes on without one).
  private routine(name: string | undefined, args: Record<string, unknown>, turn?: Turn): Record<string, unknown> | undefined {
    const shown = !!turn && !turn.muted && !this.once; // a word once starts nothing (cascade.ts)
    if (name !== "battery" && name !== "brushing" && name !== "stop_brushing" && name !== "timer") return undefined;
    const message = routineMessage(name, args, this.out.battery());
    if (message && (shown || name === "stop_brushing")) this.out.text(message);
    switch (name) {
      case "battery": return this.waited(turn, this.out.battery() ?? { unknown: true }); // its charge for Gemini to tell
      case "brushing": return this.waited(turn, { shown: true }); // its mouth on the screen, waiting for her tap
      case "stop_brushing": return undefined;
      case "timer": // its dial on the screen, the talk over until it is (the device's)
        return this.waited(turn, timerOf(args.minutes, args.for) ? { started: true, minutes: Number(args.minutes) } : { error: `a timer is 1 to ${TIMER_MOST} minutes` });
    }
  }

  // A result Gemini waits for: its words come after it.
  private waited(turn: Turn | undefined, result: Record<string, unknown>) {
    if (turn) turn.looked_up = true;
    return result;
  }

  // Gemini starts to answer: the device hears so (over the end of its last answer, she interrupted it), and her mic
  // rests.
  private begin(): Turn {
    const asked = this.hello;
    this.hello = undefined;
    const muted = asked === undefined && this.mic === "off";
    if (this.mic !== "off") { // its answer, to her or to what happened, as the device listens
      if (this.mic === "overhearing") this.out.text({ type: "interrupted" });
      this.out.text({ type: "heard" });
    }
    this.mic = "off";
    return { asked, said: "", forParents: false, muted, spoke: false, looked_up: false, speech: new Resampler(SPEECH_RATE, this.out.rate) };
  }

  // The turn is over, or cut short: what was said, for the parents.
  // ponytail: her words are the ones written down by the end of its turn; Gemini's transcript of them can come
  // later (no order between the two), and then counts for her next. Wait for its `finished` if the log misses words.
  private end(): (Turn & { heard: string }) | undefined {
    const turn = this.turn;
    if (!turn) return;
    this.turn = undefined;
    const heard = turn.asked ?? this.heard.trim();
    if (turn.asked === undefined) this.heard = "";
    const said = turn.said.trim();
    if (heard || said) this.out.exchange({ heard, said, forParents: turn.forParents });
    return { ...turn, said, heard: turn.asked === undefined ? heard : "" };
  }

  private done() {
    if (this.turn?.looked_up && !this.turn.spoke) return; // its answer, once Gemini has what it looked up
    const turn = this.end();
    if (!turn || turn.muted) return;
    this.out.text({ type: "said", heard: turn.heard, said: turn.said, last: !!turn.bye });
    if (turn.bye || this.once) this.unlink(); // a listen after it connects again
  }

  // She talked over it: it stopped, and listens to her.
  private interrupted() {
    const turn = this.end();
    if (!turn || turn.muted) return;
    this.out.text({ type: "interrupted" });
    this.mic = "listening";
  }

  private lost(why: string) {
    if (this.closed) return;
    this.link = undefined;
    this.moving = false;
    const waiting = this.hello !== undefined || this.mic === "listening" || (this.turn !== undefined && !this.turn.muted);
    if (this.mic === "listening") this.out.text({ type: "heard" }); // her mic rests
    this.hello = this.turn = undefined;
    this.mic = "off";
    if (waiting) this.out.text({ type: "failed", reason: `Gemini Live: ${why}` });
  }
}

// Cubey's talk through Soniox (Puce's way): her voice to text (Soniox's speech-to-text, which hears the end of her turn
// itself), the text to a model (Gemini Flash-Lite, streaming, llm.ts) that answers in JSON (cubey.ts), and Cubey's
// words to speech as they are written (Soniox's text-to-speech). Behind the device's protocol (session.ts), as on
// Gemini Live (live.ts): "heard" at her pause, "said" when spoken, "interrupted" when she talked over it (the device's
// own voice detector hears her over Cubey's echo, cancelled: Soniox heard what was left of Cubey as words of hers).
// The model is asked once Soniox hears her turn end: answers begun at a pause in her words (pipecat's speculation gate)
// were seldom the ones spoken, and cost a third of the model's calls (2026-10-13).
// Her word game (routine "lesson", lesson-game.ts) is a turn of the talk too: her words, her taps and her silence go to
// the game, its pictures to the device, its words spoken in two languages; when she wants to stop (or the button stops
// it), Cubey's usual talk takes her words.
import { type Child, nameOf, pronouns } from "./child";
import { chainLetter, cubeyPrompt, type Drawing, drawPrompt, fieldAsWritten, helloNote, type Playing, readAnswer, readDrawing, sayingAsWritten, wordAsWritten, wordOn } from "./cubey";
import { type Earlier, earlierBlock } from "./history";
import { type Her, heardOf, languageName } from "./lesson";
import type { Game, Lessons } from "./lesson-game";
import type { Complete, Message } from "./llm";
import type { Learned } from "./memory";
import type { Spent } from "./prices";
import { Resampler } from "./resample";
import { wordsOf } from "./soniox";
import { SpokenLetters } from "./spelling";
import { type Opened, routineMessage, type Talk, type TalkOut, type Talks } from "./talk";

// Cubey's voice in each language: Soniox's voices each have an accent (Russell's American, Daniel's and Sebastian's
// British: tried on 2026-10-10), so in a language Soniox has voices of, one with its accent, warm and friendly, male as
// Russell (2026-10-13); Russell where it has none (Latvian, Lithuanian, Estonian, Finnish, the Nordic languages, Dutch).
// Each takes the cues in Cubey's words (cubey.ts); the word game's words in the language it teaches go in that one's.
const VOICES: Record<string, string> = { fr: "Julien", de: "Konrad", es: "Javier", it: "Marco", pt: "Rui", ru: "Vadim", uk: "Bohdan", pl: "Marek" };
export const voiceFor = (language: string) => VOICES[language] ?? "Russell";
const SPEED = 1.0; // Soniox allows 0.7 to 1.3; Russell at its own pace (1.1 until 2026-10-10)
// Its voice four semitones higher, as a small robot's (2026-10-13; a ring modulator, a tin can and an old game's crush
// were tried too): Soniox has no pitch, so it speaks slower by as much, and its speech is played back faster, its pace as
// before (resample.ts), a fraction of a millisecond later.
const PITCH = 1.25;
const MIC_BYTES_PER_SECOND = 16000 * 2; // the device's mic: 16 kHz, 16-bit
const TALK_MOST = 16; // messages of this talk the model is given: its last exchanges (the ones before are history)
const LESSON_TALK = 6; // of the word game: its last steps, so the model keeps to this one
const SCREEN_MS = 10_000; // a drawing shows on the device (8 s there, a little more for text scrolling past)
const NUDGE_MS = 8000; // she says nothing in the word game: Cubey asks once more, then waits for her as long as it takes

// pauseMs: how long it waited after her last word to call the pause
export type Listener = { send(pcm: Uint8Array): void; finish(): Promise<string>; cancel(): void; pauseMs?: number };
export type Voice = { say(text: string, language?: string): Promise<void>; end(): Promise<void>; cancel(): void };
export type CascadeDeps = {
  history: { recent(child: string): Earlier[] };
  // onHeard: her turn is over, or listening failed; onWords: what is heard so far; also: another language she may
  // speak; waitMs: nothing said this long, her turn is over all the same, with nothing heard
  listen: (opts: { child: Child; onHeard: () => void; onWords?: (soFar: string) => void; also?: string[]; waitMs?: number }) => Listener;
  // timestamps: whether to time what it says, asked with its first words; onCharacters: what it says as it times it
  speak: (opts: {
    voice: (language: string) => string; speed: number; language: string; sampleRate: number; onAudio: (pcm: Buffer) => void;
    timestamps?: () => boolean; onCharacters?: (characters: string[], seconds: number[]) => void;
  }) => Voice;
  complete: Complete;
  // The games' model, while she plays a game with words; and the one that draws and writes on Cubey's screen: each as
  // chosen on the admin page (models.ts).
  games: Complete;
  draw: Complete;
  lessons: Lessons; // her word game
  log?: (line: string) => void; // how long each answer took
};

export const cascadeTalks = (deps: CascadeDeps): Talks => ({ open: (child, opened, out) => new CascadeTalk(child, opened, out, deps) });

// What she said, without Cubey's own words heard back before hers.
const withoutEcho = (text: string, echo: Set<string>) => {
  const words = text.trim().split(/\s+/);
  let i = 0;
  while (i < words.length && wordsOf(words[i]).every((w) => echo.has(w))) i++;
  return words.slice(i).join(" ");
};
// While Cubey speaks, the mic's last moment, kept: the device hears her voice a little after she starts (its detector
// wants a moment of it, then the message travels), and her turn begins with it.
const RECENT_BYTES = 16000 * 2; // a second at 16 kHz
// The device heard her voice over Cubey's speech: Cubey stops at her first word, which must come this soon. A voice
// with no word in it ("ooh" at the pictures, a knock, the room) stopped Cubey mid-sentence (2026-10-10).
export const TALK_OVER_WORD_MS = 2000;

class CascadeTalk implements Talk {
  private generation = 0; // bumped by every stop; what an older turn would still send is dropped
  private listener?: Listener; // her turn
  private overhearing = false; // Cubey speaks, the device streams the mic: the moment before she talks over it
  private recent: Uint8Array[] = [];
  private doubted?: ReturnType<typeof setTimeout>; // her voice over Cubey, not yet a word
  private recentBytes = 0;
  private speech?: Voice;
  private saying = new Set<string>(); // the words being spoken, which the mic hears back
  private messages: Message[] = []; // this talk so far
  private asked?: string; // what happened, while Cubey's answer to it is on its way
  // Her word game, while she plays: the game (planned as it starts), the language it teaches, its exchanges, and
  // whether Cubey has asked once more after she said nothing (then it waits)
  private lesson?: { game: Promise<Game>; language: string; talk: Message[]; nudged: boolean };
  private playing?: Playing; // a game of the talk's, as its last answer said: its rules go to the model
  private letter?: string; // the letter she needs in a game with letters
  private dots?: number; // how many dots show in how many dots
  private used: string[] = []; // the words said in this word chain, spelled: none again
  private drawings = 0; // asked for: only the last is shown
  private listened = 0; // bytes of her audio sent to Soniox, not yet counted
  private spoken = 0; // bytes of Cubey's speech from it, not yet counted
  private spent = (spent: Spent) => this.out.spent(spent);
  private drawn?: { what: string; at?: number }; // the last, and when it went to the device (not yet: being drawn)
  private closed = false;
  private once: boolean;

  constructor(private child: Child, opened: Opened, private out: TalkOut, private deps: CascadeDeps) {
    this.once = opened.once === true;
    if (opened.again) {
      out.text({ type: "said", heard: "", said: "", last: false }); // it listens on
      return;
    }
    void this.answer(opened.note ?? helloNote(child, !deps.history.recent(child.id).length), true, this.generation);
  }

  listen() {
    if (this.closed) return;
    this.hush();
    this.out.text({ type: "listening" });
    this.hear();
  }

  // Her turn: Soniox hears it to its end (in the word game, the language taught too, and her turn ends when she says
  // nothing). echo: Cubey's words she talked over, left out of what she said before hers.
  // onWord: her first word, over Cubey's speech.
  private hear(echo?: Set<string>, onWord?: () => void): Listener {
    const lesson = this.lesson;
    const listener: Listener = (this.listener = this.deps.listen({
      child: this.child,
      onHeard: () => void (this.listener === listener && this.heard(listener, this.generation, echo)),
      ...(onWord && { onWords: (soFar: string) => void (this.listener === listener && soFar.trim() && onWord()) }),
      ...(lesson && { also: [lesson.language], ...(!lesson.nudged && { waitMs: NUDGE_MS }) }),
    }));
    return listener;
  }

  hearing() {
    return !!this.listener || this.overhearing;
  }

  audio(pcm: Uint8Array) {
    if (this.listener) return (this.listened += pcm.length), this.listener.send(pcm);
    if (!this.overhearing) return;
    this.recent.push(pcm);
    this.recentBytes += pcm.length;
    while (this.recentBytes - this.recent[0].length >= RECENT_BYTES) this.recentBytes -= this.recent.shift()!.length;
  }

  stop() {
    this.hush();
  }

  // A note is Cubey's to answer: the word game, if she played, is over.
  tell(note: string) {
    if (this.closed) return;
    const listening = this.hearing();
    this.hush();
    if (listening) this.out.text({ type: "heard" }); // its answer, as the device listens
    this.endLesson();
    void this.answer(note, true, this.generation);
  }

  // The audio heard and spoken since it was last counted: what Soniox charges for.
  private settle() {
    if (this.listened) this.spent({ what: "listen", seconds: this.listened / MIC_BYTES_PER_SECOND });
    if (this.spoken) this.spent({ what: "speak", seconds: this.spoken / (this.out.rate * 2) });
    this.listened = this.spoken = 0;
  }

  // What happened and was never answered (she stopped brushing as Cubey fell asleep) is still in the history.
  close() {
    this.settle();
    if (this.asked !== undefined) this.out.exchange({ heard: this.asked, said: "", forParents: false });
    this.closed = true;
    this.lesson = undefined;
    this.hush();
  }

  // She tapped a picture in the word game: her answer, as the device listens.
  tapped(id: string) {
    if (this.closed || !this.lesson) return;
    this.hush();
    this.out.text({ type: "heard" });
    void this.lessonTurn({ tapped: id }, this.generation);
  }

  // While Cubey speaks, the device streams the mic with its echo cancelled; its own voice detector says when she
  // talks over it (voice).
  overhear() {
    // The device says when it speaks: often after all of the speech was sent (Soniox makes it faster than it plays).
    if (this.closed || this.listener || this.overhearing) return;
    this.overhearing = true;
  }

  // Her voice over Cubey (the device ducks it as she talks): Soniox hears her from the mic's last moment before, and
  // at her first word Cubey stops and her turn goes on; no word soon, Cubey goes on and listens over it again.
  voice() {
    if (!this.overhearing) return;
    const before = this.recent, echo = this.saying;
    this.overhearing = false;
    this.recent = [];
    this.recentBytes = 0;
    const listener = this.hear(echo, () => this.talkedOver());
    for (const pcm of before) (this.listened += pcm.length), listener.send(pcm);
    this.doubted = setTimeout(() => {
      this.doubted = undefined;
      if (this.listener !== listener) return;
      listener.cancel();
      this.listener = undefined;
      this.overhearing = true;
    }, TALK_OVER_WORD_MS);
  }

  // Her first word over Cubey's speech: what is left of it is dropped, the device stops what it still plays.
  private talkedOver() {
    if (!this.doubted) return;
    clearTimeout(this.doubted);
    this.doubted = undefined;
    this.generation++;
    this.speech?.cancel();
    this.speech = undefined;
    this.out.text({ type: "interrupted" });
  }

  // The turn ends: listening and speech stop.
  private hush() {
    clearTimeout(this.doubted);
    this.doubted = undefined;
    this.generation++;
    this.listener?.cancel();
    this.speech?.cancel();
    this.listener = this.speech = undefined;
    this.overhearing = false;
    this.recent = [];
    this.recentBytes = 0;
  }

  // She paused: her words, then Cubey's answer; nothing said, the device listens again.
  // echo: the words of the speech she talked over, left out of what she said before hers
  private async heard(listener: Listener, generation: number, echo?: Set<string>) {
    this.listener = undefined;
    this.out.text({ type: "heard" });
    let heard: string;
    try {
      const finished = await listener.finish();
      heard = echo ? withoutEcho(finished, echo) : finished;
    } catch (e) {
      if (generation === this.generation) this.out.text({ type: "failed", reason: String(e) });
      return;
    }
    if (generation !== this.generation) return;
    if (this.lesson && (heard || !this.lesson.nudged)) return this.lessonTurn(heard ? { said: heard } : { quiet: true }, generation);
    if (!heard) return this.out.text({ type: "said", heard: "", said: "", last: false });
    await this.answer(heard, false, generation, listener.pauseMs);
  }

  // The model for her turn: the games' one while she plays a game with words.
  private model(): Complete {
    return this.playing === "chain" || this.playing === "letters" || this.playing === "spell" ? this.deps.games : this.deps.complete;
  }

  // What Cubey shows on its screen, as its answer named it (as soon as it is written): drawn or written by the drawing
  // model, then to the device whenever it is done (in 5 to 14 s it was, often into her next turn); not if another was
  // asked for since. Until then the device shows a sketch forming; nothing drawn, it shows nothing, and its eyes are back.
  private async draw(what: string) {
    const asked = ++this.drawings, start = Date.now();
    this.drawn = { what };
    if (!this.closed) this.out.text({ type: "drawing" });
    let drawing: Drawing | undefined;
    try {
      drawing = readDrawing(await this.deps.draw({ system: drawPrompt(this.child, what), messages: [{ role: "user", text: what }], spent: this.spent }));
    } catch (e) {
      this.deps.log?.(`no drawing of ${what}: ${e}`);
    }
    if (drawing) this.deps.log?.(`drew ${what} in ${Date.now() - start} ms${asked === this.drawings ? "" : ", another asked for since"}`);
    if (asked !== this.drawings) return;
    this.drawn = drawing && { what, at: Date.now() };
    if (this.closed) return;
    this.out.text(!drawing ? { type: "show" } : drawing.picture ? { type: "show", rows: drawing.picture } : { type: "show", text: drawing.text });
  }

  // What its screen shows of its drawing, for the model to know (it told her the sun was there, never drawn).
  private screen(): string | undefined {
    if (!this.drawn) return undefined;
    if (this.drawn.at === undefined) return `${this.drawn.what}, still being drawn: not there yet`;
    return Date.now() - this.drawn.at < SCREEN_MS ? `${this.drawn.what}, drawn` : undefined;
  }

  // What the model is given for an answer: Cubey's prompt, with its battery as the device last said, and the talk so far.
  private prompt(asked: string) {
    const battery = this.out.battery();
    const earlier = earlierBlock(this.deps.history.recent(this.child.id), nameOf(this.child));
    const lesson = this.child.learns ? languageName(this.child.learns) : undefined;
    const system = cubeyPrompt(this.child, {
      earlier, now: new Date(), mode: "cascade", battery, lesson, playing: this.playing, needed: this.letter, heard: asked, used: this.used, dots: this.dots, screen: this.screen(),
    });
    const messages: Message[] = [...this.messages.slice(-TALK_MOST), { role: "user", text: asked }];
    return { system, messages, battery };
  }

  // Cubey's answer to her words, or to what happened (note: its hello, brushing, a timer): the model writes it, its
  // words spoken as they come; then what it does on the device, the log, and "said" once all of it has played.
  private async answer(asked: string, note: boolean, generation: number, pauseMs?: number) {
    const started = performance.now();
    const since = () => Math.round(performance.now() - started);
    let firstAudio = 0, spoken = "", said = Promise.resolve();
    if (note) this.asked = asked;
    // The word Cubey spells (its first, as the server spells it), to the device before Cubey's speech: its voice then
    // times what it says, and each letter goes to the device as it is said alone ("Kaķis: k, a, ķ, i, s!").
    let letters: SpokenLetters | undefined;
    const spelled = wordAsWritten("spell", (word) => {
      if (generation !== this.generation) return;
      letters = new SpokenLetters([...word], (i, ms) => void (generation === this.generation && this.out.text({ type: "spell.at", i, ms })));
      this.out.text({ type: "spell", word });
    });
    const voice = this.speaking(generation, () => (firstAudio ||= since()), {
      timestamps: () => !!letters, onCharacters: (characters, seconds) => letters?.heard(characters, seconds),
    });
    const speak = sayingAsWritten((word) => {
      if (generation !== this.generation) return;
      spoken += word;
      for (const w of wordsOf(word)) this.saying.add(w);
      said = said.then(() => voice.say(word));
      said.catch(() => {}); // handled below
    });
    // The word chain shown as it is said: her word, as the model says she meant it (Soniox wrote "aitā" for "aita"; "eh"
    // is none), once written, on the letter she needed (any, to begin; the server's judgment of what Soniox heard and of
    // the model's, as a model praised any word), new to the game; then Cubey's, before its speech. The letter moves on
    // only from hers, and Cubey's.
    const on = this.playing === "chain" && !note ? wordOn(asked, this.letter) : undefined;
    const fresh = !!on && !this.used.includes(on);
    let hers: string | undefined, shown = false; // hers and Cubey's word, on the device
    const heard = wordAsWritten("hers", (meant) => {
      if (generation !== this.generation || (this.letter && meant[0] !== this.letter) || this.used.includes(meant)) return;
      hers = meant;
      this.out.text({ type: "letter", letter: chainLetter(meant), hers });
    });
    const theirs = wordAsWritten("word", (word) => {
      const letter = chainLetter(word);
      if (generation !== this.generation || !hers || !letter) return;
      this.out.text({ type: "letter", letter, word });
      shown = true;
    });
    const sketch = fieldAsWritten("show", (what) => void (what.trim() && this.draw(what.trim().slice(0, 40))));
    const say = (written: string) => (spelled(written), speak(written), sketch(written), fresh && (heard(written), theirs(written)));
    try {
      const { system, messages, battery } = this.prompt(asked);
      const raw = await this.model()({ system, messages, onText: say, spent: this.spent });
      if (generation !== this.generation) return;
      const written = since();
      const answer = readAnswer(raw, spoken);
      if (!answer) throw new Error(`no usable answer: ${raw.slice(0, 80)}`);
      this.asked = undefined;
      // Its answers as the JSON it writes: given as plain sentences, GPT-6 Sol answered in them a third of the time.
      this.messages.push({ role: "user", text: asked }, { role: "assistant", text: JSON.stringify({ say: answer.said }) });
      this.out.exchange({ heard: asked, said: answer.said, forParents: answer.forParents });
      // A word once (a die's number, a timer's reminder) starts nothing: the dice's number with the dice set again
      // rolled the die again, and again (2026-10-10).
      const routine = this.once ? undefined : answer.routine;
      const playing = routine === "find" || routine === "chain" || routine === "letters" || routine === "count" || routine === "spell" ? routine : undefined;
      const screen = (game?: Playing) => (game === "chain" || game === "letters" ? "letter" : game === "count" ? "count" : undefined);
      const took = shown ? hers : undefined; // her word, and Cubey's from it
      if (hers && !took && this.letter) this.out.text({ type: "letter", letter: this.letter }); // no word from it: hers again
      const waits = this.playing === "chain" && !!this.letter && !took;
      const letter = waits ? undefined : answer.letter; // a letter to show anew
      const dots = answer.number !== this.dots ? answer.number : undefined; // the same again would give her answer away
      // Over, or another game with nothing to show yet (a chain she begins): the eyes. A game's letter is its own.
      const was = screen(this.playing);
      if (was && playing !== this.playing && !(was === "letter" && letter)) this.out.text({ type: was });
      // The chain's words, none twice: anew with a new game, hers and Cubey's as it goes on.
      if (playing !== "chain" || this.playing !== "chain") this.used = [];
      if (playing === "chain") this.used.push(...[took, letter && answer.word].filter((w): w is string => !!w));
      this.letter = letter ?? (playing === this.playing ? this.letter : undefined);
      this.dots = playing === "count" ? (answer.number ?? this.dots) : undefined;
      this.playing = playing;
      // What the child told it first: the language they chose is the word game's. No language chosen, no game: Cubey
      // asks which.
      if (answer.learned) this.child = this.out.learn(answer.learned);
      const learns = routine === "lesson" ? this.child.learns : undefined;
      const message = routine && (routine !== "lesson" || learns) && routineMessage(routine, { ...answer, letter, number: dots }, battery);
      if (message && !(routine === "chain" && shown)) this.out.text(message);
      if (learns) this.startLesson(learns);
      // Its settings, as asked, if they change: kept, and to the device with them (session.ts).
      if (answer.brightness && answer.brightness !== (this.child.brightness ?? 100)) this.child = this.out.brighten(answer.brightness);
      if (answer.volume && answer.volume !== (this.child.volume ?? 100)) this.child = this.out.turn(answer.volume);
      if (answer.theme && answer.theme !== (this.child.theme ?? "amber")) this.child = this.out.colour(answer.theme);
      if (!spoken.trim()) said = said.then(() => voice.say(answer.said)); // none of it came as it was written
      await said;
      await voice.end();
      this.settle();
      if (letters) this.deps.log?.(`spelled ${letters.letters.join("")}: ${letters.found} of ${letters.letters.length} letters timed`);
      if (generation !== this.generation) return;
      this.speech = undefined;
      this.deps.log?.(`answer: ${pauseMs ?? "-"} ms her pause, ${written} ms written, ${firstAudio} ms to its first sound`);
      this.out.text({ type: "said", heard: note ? "" : asked, said: answer.said, last: answer.bye });
      if (this.once) this.closed = true; // a word once: nothing listens after it
    } catch (e) {
      if (generation !== this.generation) return;
      voice.cancel();
      this.speech = undefined;
      this.out.text({ type: "failed", reason: String(e) });
    }
  }

  // Cubey's voice for an answer, higher (PITCH): its sound goes to the device while the answer is still the talk's
  // (generation); timed, what it says as it times it (a word it spells), as it plays.
  private speaking(generation: number, onSound = () => {}, timed: Pick<Parameters<CascadeDeps["speak"]>[0], "timestamps" | "onCharacters"> = {}): Voice {
    this.saying = new Set();
    const higher = new Resampler(Math.round(this.out.rate * PITCH), this.out.rate);
    const { onCharacters } = timed;
    return (this.speech = this.deps.speak({
      ...timed,
      ...(onCharacters && { onCharacters: (characters: string[], seconds: number[]) => onCharacters(characters, seconds.map((at) => at / PITCH)) }),
      voice: voiceFor, speed: SPEED / PITCH, language: this.child.language, sampleRate: this.out.rate,
      onAudio: (pcm) => {
        this.spoken += pcm.length; // made (and paid for), if no longer wanted
        if (generation !== this.generation) return;
        const played = higher.push(pcm);
        if (!played.length) return;
        onSound();
        this.out.audio(played);
      },
    }));
  }

  // Her word game begins in the language she chose (Cubey has said so): it is planned while that plays, and her next
  // words are the game's.
  private startLesson(language: string) {
    const game = this.deps.lessons.start(this.child, language, this.spent);
    game.catch(() => {}); // its first turn plans it again
    this.lesson = { game, language, talk: [], nudged: false };
  }

  private endLesson() {
    if (!this.lesson) return;
    this.lesson = undefined;
    this.out.text({ type: "lesson.end" });
  }

  // A step of the word game for her answer: what to show goes to the device before the speech, its words are spoken in
  // two languages, and the device listens again. She wants to stop: Cubey's usual talk answers her words.
  private async lessonTurn(her: Her, generation: number) {
    const lesson = this.lesson!;
    lesson.nudged = "quiet" in her;
    try {
      const game = await lesson.game.catch(() => (lesson.game = this.deps.lessons.start(this.child, lesson.language, this.spent)));
      const turn = await this.deps.lessons.turn(game, { her, talk: lesson.talk.slice(-LESSON_TALK) });
      if (generation !== this.generation) return;
      if (turn.stop) {
        this.endLesson();
        const words = "said" in her ? ` ${her.said}` : "";
        return await this.answer(`(The word game is over: ${pronouns(this.child).she} wanted to stop.)${words}`, false, generation);
      }
      const heard = heardOf(this.child, her);
      lesson.talk.push({ role: "user", text: heard }, { role: "assistant", text: JSON.stringify({ say: turn.said }) });
      this.out.exchange({ heard, said: turn.log.said, forParents: turn.forParents });
      this.out.text(turn.show);
      const voice = this.speaking(generation);
      for (const w of wordsOf(turn.log.said)) this.saying.add(w);
      for (const run of turn.spoken) await voice.say(run.text, run.language);
      await voice.end();
      this.settle();
      if (generation !== this.generation) return;
      this.speech = undefined;
      this.out.text({ type: "said", heard: turn.log.heard, said: turn.log.said, last: false });
    } catch (e) {
      if (generation !== this.generation) return;
      this.speech?.cancel();
      this.speech = undefined;
      this.out.text({ type: "failed", reason: String(e) });
    }
  }
}

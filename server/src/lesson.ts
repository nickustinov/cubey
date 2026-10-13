// Cubey's word game (Puce's parrot's, from Lapsa): Cubey teaches her a foreign language, a step at a time, until she
// wants to stop; she asks for it in any words ("learning English", or the code word "parrot"). Half written, half AI.
// Ours: the curriculum (curriculum.ts: ideas from first words to tiny conversations, in order), the plan of each
// round, the pictures and the memory. The model's: the words in the target language (translated once per language and
// kept), saying each step warmly, hearing how close she got, and the tiny chat. The memory: each idea she met, per
// language, in Leitner boxes. A new one comes back the next day; a right answer on a new day moves it a box on (1, 2,
// 4, 8 days); a miss starts it again tomorrow. Right on two more days after she learned it: known.
import type { Database } from "bun:sqlite";
import { ageOf, type Child, nameOf, parentsOf, pronouns, speakFor } from "./child";
import { CURRICULUM, idea, type Idea } from "./curriculum";
import type { Complete, Message } from "./llm";

export const LESSON_ROUND = 7; // steps planned at a time: then the next round, from her memory as it is by then

// The languages it can teach: every language Soniox speaks (soniox.com/docs/tts/concepts/supported-languages).
export const LESSON_LANGUAGES = [
  "af", "sq", "ar", "az", "eu", "be", "bn", "bs", "bg", "ca", "zh", "hr", "cs", "da", "nl", "en", "et", "fi", "fr", "gl",
  "de", "el", "gu", "he", "hi", "hu", "id", "it", "ja", "kn", "kk", "ko", "lv", "lt", "mk", "ms", "ml", "mr", "no", "fa",
  "pl", "pt", "pa", "ro", "ru", "sr", "sk", "sl", "es", "sw", "sv", "tl", "ta", "te", "th", "tr", "uk", "ur", "vi", "cy",
];

export const languageName = (code: string) => new Intl.DisplayNames(["en"], { type: "language" }).of(code) ?? code;

// Today where she lives ("YYYY-MM-DD"): the memory's days are hers.
export const dayOf = (child: Child, now = new Date()) => new Intl.DateTimeFormat("sv", { timeZone: child.timeZone }).format(now);

const RESULTS = ["right", "missed", "tried"] as const;
export type Result = (typeof RESULTS)[number];
export type Memory = { due: string[]; seen: string[]; known: string[] }; // idea ids

const INTERVALS = [0, 1, 2, 4, 8, 16]; // days until the next review, by box
const KNOWN_BOX = 3;
const DUE_MOST = 6;

const addDays = (date: string, n: number) => {
  const d = new Date(`${date}T12:00:00Z`);
  d.setUTCDate(d.getUTCDate() + n);
  return d.toISOString().slice(0, 10);
};

export class LessonProgress {
  constructor(private db: Database) {
    db.run(`CREATE TABLE IF NOT EXISTS lesson_progress (
      child TEXT NOT NULL,
      language TEXT NOT NULL,
      idea TEXT NOT NULL,
      box INTEGER NOT NULL,
      due TEXT NOT NULL,
      moved TEXT NOT NULL,
      PRIMARY KEY (child, language, idea)
    )`);
  }

  // How an idea went today ("YYYY-MM-DD"): a new one joins, the rest move between boxes.
  record(child: string, language: string, id: string, result: "new" | Result, today: string) {
    const found = this.db.query("SELECT box, moved FROM lesson_progress WHERE child = ? AND language = ? AND idea = ?")
      .get(child, language, id) as { box: number; moved: string } | null;
    const set = (box: number, days: number) =>
      this.db.run("INSERT OR REPLACE INTO lesson_progress (child, language, idea, box, due, moved) VALUES (?, ?, ?, ?, ?, ?)",
        [child, language, id, box, addDays(today, days), today]);
    if (!found) {
      if (result !== "tried") set(result === "right" ? 2 : 1, result === "right" ? INTERVALS[2] : 1);
    } else if (result === "missed") {
      set(1, 1);
    } else if (result === "right" && found.moved !== today) {
      const box = Math.min(found.box + 1, INTERVALS.length - 1);
      set(box, INTERVALS[box]);
    }
  }

  forget(child: string) {
    this.db.run("DELETE FROM lesson_progress WHERE child = ?", [child]);
  }

  // What to review today (the longest waiting first), every idea she met, and what she knows.
  memory(child: string, language: string, today: string): Memory {
    const rows = this.db.query("SELECT idea, box, due FROM lesson_progress WHERE child = ? AND language = ? ORDER BY due, idea")
      .all(child, language) as { idea: string; box: number; due: string }[];
    return {
      due: rows.filter((r) => r.due <= today).slice(0, DUE_MOST).map((r) => r.idea),
      seen: rows.map((r) => r.idea),
      known: rows.filter((r) => r.box >= KNOWN_BOX).map((r) => r.idea),
    };
  }
}

// The ideas in a target language, as the model translated them: one wording per idea and key (the language, and for
// phrases about herself the child's grammatical gender: "en/girl").
export class LessonWords {
  constructor(private db: Database) {
    db.run(`CREATE TABLE IF NOT EXISTS lesson_translations (
      language TEXT NOT NULL,
      idea TEXT NOT NULL,
      text TEXT NOT NULL,
      PRIMARY KEY (language, idea)
    )`);
  }

  get(language: string, ids: string[]): Record<string, string> {
    const out: Record<string, string> = {};
    for (const id of ids) {
      const row = this.db.query("SELECT text FROM lesson_translations WHERE language = ? AND idea = ?").get(language, id) as { text: string } | null;
      if (row) out[id] = row.text;
    }
    return out;
  }

  set(language: string, words: Record<string, string>) {
    for (const [id, text] of Object.entries(words))
      this.db.run("INSERT OR REPLACE INTO lesson_translations (language, idea, text) VALUES (?, ?, ?)", [language, id, text]);
  }
}

// The ideas in the target language, as a native speaker says them to a small child.
export async function translate(opts: { language: string; ideas: Idea[]; child: Child; complete: Complete }): Promise<Record<string, string>> {
  const target = languageName(opts.language);
  const gender = opts.child.gender;
  const about = gender ? `a ${gender} would say them about ${gender === "girl" ? "herself" : "himself"}` : "a child would say them about themselves, in forms that fit a girl and a boy where it can";
  const system = `Translate each idea into ${target}, as a native speaker would say it to a young child learning ${target}. Single words in their everyday form, with an article only where ${target} needs one to sound natural; phrases short and natural, as ${about}; "…" stays for the part the child fills in. Answer with JSON only: {"<id>": "<${target}>"}.`;
  const asked = Object.fromEntries(opts.ideas.map((i) => [i.id, i.en]));
  const raw = await opts.complete({ system, messages: [{ role: "user", text: JSON.stringify(asked, null, 1) }] });
  const out: Record<string, string> = {};
  try {
    const got = JSON.parse(raw.slice(raw.indexOf("{"), raw.lastIndexOf("}") + 1)) as Record<string, unknown>;
    for (const id of Object.keys(asked)) if (typeof got[id] === "string" && got[id].trim()) out[id] = got[id].trim();
  } catch {}
  return out;
}

// A round of the game, one step for each exchange: the ideas due first, then the next new ones in the curriculum's
// order (from 3½ one of them a phrase, so phrases don't wait for every word before them), each taught and then
// practised the way its unit plays (tapping pictures, doing it, counting, saying it; a phrase is always said), and a
// tiny chat once she knows a few words. Rounds follow until she stops: the game never ends by itself.
export type Step = {
  kind: "review" | "teach" | "practise" | "chat";
  idea?: string;
  how?: "say" | "touch" | "act" | "count" | "talk";
  choices?: string[]; // the pictures she taps among
};

const REVIEWS_MOST = 3;
const PHRASES_FROM = 42; // months

export function planGame(opts: { memory: Memory; ageMonths: number; random?: () => number }): Step[] {
  const { memory, ageMonths } = opts;
  const random = opts.random ?? Math.random;
  const seen = new Set(memory.seen);
  const unseen = CURRICULUM.filter((i) => i.from <= ageMonths && !seen.has(i.id));
  const count = ageMonths < 36 ? 2 : 3;
  const phrase = ageMonths >= PHRASES_FROM ? unseen.find((i) => i.type === "phrase") : undefined;
  const fresh = phrase && !unseen.slice(0, count).includes(phrase)
    ? [...unseen.filter((i) => i !== phrase).slice(0, count - 1), phrase]
    : unseen.slice(0, count);
  const others = ageMonths < 36 ? 1 : 2; // pictures beside hers
  const shuffled = <T>(list: T[]) => list.map((x) => [random(), x] as const).sort((a, b) => a[0] - b[0]).map(([, x]) => x);

  // The pictures beside hers: her opposite first, then others from her unit she has met or meets today, then any she
  // has met.
  const choices = (target: Idea) => {
    const unit = CURRICULUM.filter((i) => i.unit === target.unit && i.id !== target.id && i.picture);
    const met = (i: Idea) => seen.has(i.id) || fresh.includes(i);
    const partner = target.activity === "opposites" ? unit.filter((i) => Math.abs(unit.indexOf(i) - unit.indexOf(target)) <= 1).slice(0, 1) : [];
    const pool = [...partner, ...shuffled(unit.filter(met)), ...shuffled(CURRICULUM.filter((i) => i.id !== target.id && i.picture && seen.has(i.id))), ...shuffled(unit)];
    const picked = [...new Set(pool.map((i) => i.id))].slice(0, others);
    return shuffled([target.id, ...picked]);
  };
  const tap = (i: Idea): Pick<Step, "how" | "choices"> => (i.picture ? { how: "touch", choices: choices(i) } : { how: "say" });
  const review = (id: string): Step => {
    const i = idea(id)!;
    if (i.type === "phrase") return { kind: "review", idea: id, how: "talk" };
    return { kind: "review", idea: id, ...(ageMonths < 48 ? tap(i) : { how: "say" }) };
  };
  const practise = (i: Idea): Step => {
    if (i.type === "phrase") return { kind: "practise", idea: i.id, how: "talk" };
    if (["touch", "colour", "opposites"].includes(i.activity)) return { kind: "practise", idea: i.id, ...tap(i) };
    return { kind: "practise", idea: i.id, how: i.activity === "act" ? "act" : i.activity === "count" ? "count" : "talk" };
  };

  const slots = LESSON_ROUND;
  const chat = ageMonths >= 48 && memory.known.length >= 3;
  const steps: Step[] = memory.due.slice(0, Math.min(REVIEWS_MOST, Math.max(0, slots - 2 * fresh.length))).map(review);
  fresh.forEach((i, k) => {
    steps.push({ kind: "teach", idea: i.id });
    if (k > 0) steps.push(practise(fresh[k - 1]));
  });
  if (fresh.length) steps.push(practise(fresh[fresh.length - 1]));
  // Time left: today's new ideas again, what she met before, and today's again as long as it takes.
  const again = [...fresh.map(practise), ...shuffled(memory.seen.filter((id) => !steps.some((s) => s.idea === id))).map(review)];
  for (let k = 0; steps.length < slots - (chat ? 1 : 0) && (k < again.length || fresh.length); k++)
    steps.push(k < again.length ? again[k] : practise(fresh[k % fresh.length]));
  if (chat) steps.push({ kind: "chat" });
  return steps.slice(0, slots);
}

// How her last answer went, as the game saw it: what she said (judged by the model), a tap (judged here), nothing at
// all (the step again), or a wrong tap she gets another go at (the step again).
export type Answered =
  | { said: string }
  | { tapped: string; right: boolean; again: boolean }
  | { quiet: true };

// This exchange's one job (step), and how she did at the last one (before): last, after the rules, which are alike each
// step (a model's cache holds only the start that requests share).
export function lessonPrompt(opts: {
  child: Child;
  step: Step;
  before?: Step;
  answered: Answered;
  language: string;
  words: Record<string, string>; // idea -> the target language
  known: string[];
}): string {
  const { child, step, before, words, answered } = opts;
  const { she, her, them } = pronouns(child);
  const name = nameOf(child), Name = child.name ?? "The child", age = ageOf(child);
  const target = languageName(opts.language), own = languageName(child.language);
  const said = (id: string) => `{${words[id] ?? idea(id)?.en ?? id}}`;
  const en = (id: string) => idea(id)?.en ?? id;
  // The card, where its picture stands for the idea (a frog for jump, an umbrella for under); a word's, a colour's or a
  // number's is the thing itself, and its emoji name would only mislead ("red circle" for a red ball).
  const card = (id: string) => {
    const i = idea(id);
    return i?.picture && ["action", "describe", "phrase"].includes(i.type) ? ` The card shows: ${i.picture.toLowerCase()}.` : "";
  };
  const job = (s: Step): string => {
    const w = s.idea ? said(s.idea) : "";
    const pics = s.choices ? ` The cards show: ${s.choices.map(en).join(", ")}.` : "";
    switch (s.kind) {
      case "teach":
        return `Teach a new ${idea(s.idea!)?.type === "phrase" ? "phrase" : "word"}: ${w} (${en(s.idea!)}).${card(s.idea!)} Say it clearly twice, then ask ${them} to say it after you.`;
      case "review":
      case "practise":
        if (s.how === "touch") return `Ask ${them} to tap ${w} on your screen.${pics}`;
        if (s.how === "say") return `Ask what this is in ${target}: the word is ${w} (${en(s.idea!)}).${card(s.idea!)}`;
        if (s.how === "act") return `Ask ${them} to do it with you: ${w}! Say ${w} again as ${she} does it.`;
        if (s.how === "count") return `Count with ${them} up to ${w}, or ask ${them} to show ${w} fingers.`;
        return `Use ${w} in a tiny exchange${en(s.idea!).includes("…") ? `, a word ${she} knows for the …` : ""}: say it to ${them}, and ask ${them} to say it back to you.`;
      case "chat":
        return `One tiny chat in ${target}, using only words ${she} knows (${opts.known.map(said).join(", ") || "the ones from today"}): a yes-or-no or one-word question.`;
    }
  };
  // The only target-language words this answer may say: the model strayed to others in Puce.
  const allowed = [...new Set([before?.idea, step.idea, ...(step.choices ?? []), ...(step.kind === "chat" ? opts.known : [])])]
    .filter((id): id is string => !!id).map(said);
  const how = (() => {
    if ("quiet" in answered) return before ? `${Name} said nothing: say it once more, cheerfully, and ask again. Never ask whether ${she} is there.` : "";
    if ("tapped" in answered) {
      if (answered.right) return `${Name} tapped the ${en(answered.tapped)}: right!`;
      if (answered.again) return `${Name} tapped the ${en(answered.tapped)}, but ${said(before!.idea!)} was asked for: say kindly that this one is ${said(answered.tapped)}, and ask once more.`;
      return `${Name} tapped the ${en(answered.tapped)} again, but ${said(before!.idea!)} was asked for: say ${said(before!.idea!)} cheerfully, and go on.`;
    }
    if (!before) return `What ${she} just said answers your offer to play: begin.`;
    return `What ${she} just said answers your last step, about ${said(before.idea ?? "")}: react in a few words, and judge it in result (right, missed or tried).`;
  })();
  return `You are Cubey, a small cube with two glowing eyes, ${name}'s friend. Now you play a word game with ${them}: you teach ${them} ${target}, one step at a time. ${Name} is a ${child.gender ?? "child"}${age ? ` of ${age}` : ""} who speaks ${own}. You are not chatting now: you play the game.

How you speak
- Everything you say is read aloud: only the words you say, no emoji, lists, headings or descriptions of actions. You may begin a sentence with one voice cue in square brackets, exactly as written, never another: [warm] [happy] [excited] [curious] [playful] [calm] [giggles].
- Explain in ${own}: ${speakFor(child)} Say the ${target} words clearly.
- Put every ${target} word or phrase in curly braces, and nothing else: they are spoken in ${target}, the rest in ${own} ("Atkārto pēc manis: {cat}! {Cat}!").
- ${child.gender ? `${Name} is a ${child.gender}: in ${own} and ${target}, use the ${child.gender === "girl" ? "feminine" : "masculine"} forms for ${name} (Latvian, French and Portuguese words change with it: "Vai tu esi ${child.gender === "girl" ? "gatava" : "gatavs"}?")` : `You don't know whether ${name} is a girl or a boy: in ${own} and ${target}, say it so it fits both where you can`}; masculine forms for yourself.
- Every try is good; never say "wrong". Praise briefly and differently each time, about what ${she} did (${she} said it, ${she} found it), or just go on; don't call ${them} clever or smart. Children this age say words imperfectly and the listening mishears them: count a try as right when it sounds close.

Always
- When ${she} wants to stop the game, says goodbye, or wants something else (to talk, to brush ${her} teeth, a timer, anything), in any words: set stop and leave say empty. You go back to talking with ${them} as usual, and answer ${them} there.
- If ${she} asks something about the game or its words, answer in a few words and go on with the game. Questions about death, where babies come from, bodies, God, family matters, war or ${child.name}'s own health: say warmly that it is a great question for ${parentsOf(child)}, and set for_parents. If ${she} says someone hurt ${them}, or something scary or unsafe happened: say gently to tell ${parentsOf(child)} right now, and set for_parents.
- Never ask where ${she} lives, ${her} full name or other personal details. No brands, apps, games, films or screens. No secrets, no promises; never say you love ${them} or will miss ${them}.

This exchange
${how ? `- ${how}
` : ""}- Now, and only this: ${job(step)}
- The only ${target} words in this answer: ${allowed.join(", ") || "none"}. Never teach or ask about another.

Answer with JSON only: {"say": "<what you say>", "result": "right|missed|tried or empty", "stop": false, "for_parents": false}`;
}

// What Cubey says, in runs by language: the words in braces are the target language. Punctuation stays with the word
// before it ("{Cat}! Lieliski!"); a voice cue before a run in the other language goes with that run, which it colours.
export type Spoken = { text: string; language: string };
const CUES_ONLY = /^(\s*\[[a-z ]+\]\s*)+$/;
export function spoken(said: string, own: string, target: string): Spoken[] {
  const runs: Spoken[] = [];
  let cues = "";
  said.split(/[{}]/).forEach((part, i) => {
    if (!part) return;
    const language = i % 2 ? target : own;
    let text = cues + part;
    cues = "";
    const last = runs.at(-1);
    if (last && last.language !== language) {
      const lead = /^[^\p{L}[]*/u.exec(text)![0];
      last.text += lead;
      text = text.slice(lead.length);
    }
    if (!text) return;
    if (CUES_ONLY.test(text)) return void (cues = text);
    if (last?.language === language) last.text += text;
    else runs.push({ text, language });
  });
  return runs; // a cue left at the end colours nothing
}

export type LessonAnswer = { said: string; result?: Result; forParents: boolean; stop?: true };

const CUES = new Set(["[warm]", "[happy]", "[excited]", "[curious]", "[playful]", "[calm]", "[giggles]"]); // the prompt's

// What Cubey says and how her last try went, or that she wants to stop; undefined for no usable answer.
export function readLesson(raw: string): LessonAnswer | undefined {
  try {
    const { say, result, for_parents, stop } = JSON.parse(raw.slice(raw.indexOf("{"), raw.lastIndexOf("}") + 1));
    if (stop === true) return { said: "", forParents: for_parents === true, stop };
    const said = typeof say === "string" ? say.replace(/\[[a-z ]+\]\s*/g, (cue) => (CUES.has(cue.trim()) ? cue : "")).trim() : "";
    if (said && said.length <= 600)
      return { said, ...((RESULTS as readonly unknown[]).includes(result) ? { result: result as Result } : {}), forParents: for_parents === true };
  } catch {}
  return undefined;
}

// Her answer: her words, a picture she tapped, or nothing at all; as the model reads it.
export type Her = { said: string } | { tapped: string } | { quiet: true };
export const heardOf = (child: Child, her: Her) =>
  "said" in her ? her.said : "tapped" in her ? `(${child.name ?? "The child"} tapped the ${idea(her.tapped)?.en ?? her.tapped})` : `(${child.name ?? "The child"} said nothing)`;

export async function askLesson(opts: Parameters<typeof lessonPrompt>[0] & { talk: Message[]; complete: Complete }): Promise<LessonAnswer> {
  const raw = await opts.complete({ system: lessonPrompt(opts), messages: [...opts.talk, { role: "user", text: heardOf(opts.child, opts.answered) }] });
  const answer = readLesson(raw);
  if (!answer) throw new Error(`no usable lesson answer: ${raw.slice(0, 80)}`);
  return answer;
}

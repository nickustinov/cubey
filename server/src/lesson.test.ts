import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { CURRICULUM, idea } from "./curriculum";
import {
  askLesson, dayOf, LESSON_ROUND, LessonProgress, lessonPrompt, LessonWords, planGame, readLesson, spoken, translate,
  type Memory, type Step,
} from "./lesson";

const anna = { id: "Anna", name: "Anna", ageMonths: 50, timeZone: "Europe/Riga", gender: "girl" as const, language: "lv" as const, learns: "en", parents: "mum_dad" as const };
const empty: Memory = { due: [], seen: [], known: [] };
const fixed = () => 0.5;
const seenUpTo = (id: string) => CURRICULUM.slice(0, CURRICULUM.findIndex((i) => i.id === id) + 1).map((i) => i.id);

test("a new idea comes back the next day; each right answer on a new day waits longer", () => {
  const progress = new LessonProgress(new Database(":memory:"));
  progress.record("Anna", "en", "apple", "new", "2026-09-29");
  expect(progress.memory("Anna", "en", "2026-09-29")).toEqual({ due: [], seen: ["apple"], known: [] });
  expect(progress.memory("Anna", "en", "2026-09-30").due).toEqual(["apple"]);
  progress.record("Anna", "en", "apple", "right", "2026-09-30");
  progress.record("Anna", "en", "apple", "right", "2026-09-30"); // twice a day counts once
  expect(progress.memory("Anna", "en", "2026-10-01").due).toEqual([]);
  expect(progress.memory("Anna", "en", "2026-10-02").due).toEqual(["apple"]); // two days on
  progress.record("Anna", "en", "apple", "right", "2026-10-02");
  expect(progress.memory("Anna", "en", "2026-10-05").due).toEqual([]);
  expect(progress.memory("Anna", "en", "2026-10-06")).toEqual({ due: ["apple"], seen: ["apple"], known: ["apple"] }); // right on two more days
});

test("a missed idea starts again tomorrow; a try alone changes nothing; each language and child apart", () => {
  const progress = new LessonProgress(new Database(":memory:"));
  progress.record("Anna", "en", "dog", "new", "2026-09-01");
  progress.record("Anna", "en", "dog", "right", "2026-09-02");
  progress.record("Anna", "en", "dog", "tried", "2026-09-04");
  expect(progress.memory("Anna", "en", "2026-09-04").due).toEqual(["dog"]);
  progress.record("Anna", "en", "dog", "missed", "2026-09-04");
  expect(progress.memory("Anna", "en", "2026-09-04").due).toEqual([]);
  expect(progress.memory("Anna", "en", "2026-09-05").due).toEqual(["dog"]);
  expect(progress.memory("Anna", "fr", "2026-09-05")).toEqual(empty);
  expect(progress.memory("Jānis", "en", "2026-09-05")).toEqual(empty);
});

test("the days are hers: today where she lives", () => {
  expect(dayOf(anna, new Date("2026-10-10T22:30:00Z"))).toBe("2026-10-11"); // past midnight in Riga
  expect(dayOf({ ...anna, timeZone: "Europe/Lisbon" }, new Date("2026-10-10T22:30:00Z"))).toBe("2026-10-10");
});

test("the words in the target language are kept, one per idea and language", () => {
  const words = new LessonWords(new Database(":memory:"));
  words.set("en", { cat: "cat", dog: "dog" });
  words.set("de", { cat: "die Katze" });
  expect(words.get("en", ["cat", "dog", "cow"])).toEqual({ cat: "cat", dog: "dog" });
  expect(words.get("de", ["cat"])).toEqual({ cat: "die Katze" });
});

test("translating: the ideas go to the model once, and only the asked ones come back", async () => {
  let asked = "";
  const complete = async (opts: { system: string; messages: { role: "user" | "assistant"; text: string }[] }) => {
    asked = opts.system + opts.messages[0].text;
    return 'Here: {"cat": " die Katze ", "dog": "der Hund", "moon": "der Mond", "cow": 7}';
  };
  const ideas = ["cat", "dog", "cow"].map((id) => CURRICULUM.find((i) => i.id === id)!);
  expect(await translate({ language: "de", ideas, child: anna, complete })).toEqual({ cat: "die Katze", dog: "der Hund" });
  expect(asked).toContain("German");
  expect(asked).toContain('"cat": "cat"');
});

test("a first round: the first ideas of the first unit, taught and practised; the next round follows", () => {
  const plan = planGame({ memory: empty, ageMonths: 50, random: fixed });
  expect(plan).toHaveLength(LESSON_ROUND);
  expect(plan.filter((s) => s.kind === "teach").map((s) => s.idea)).toEqual(["hello", "bye", "yes"]);
  expect(plan[0]).toEqual({ kind: "teach", idea: "hello" });
  expect(plan.findIndex((s) => s.kind === "practise" && s.idea === "hello")).toBeGreaterThan(plan.findIndex((s) => s.idea === "bye"));
  expect(plan.find((s) => s.kind === "practise")?.how).toBe("talk"); // hello and bye are said, not tapped
  expect(planGame({ memory: empty, ageMonths: 30, random: fixed })).toHaveLength(LESSON_ROUND);
});

test("the next game goes on where she stopped: the due ideas first, then the next new ones", () => {
  const memory: Memory = { due: ["cat", "dog"], seen: seenUpTo("dog"), known: [] };
  const plan = planGame({ memory, ageMonths: 30, random: fixed });
  expect(plan.slice(0, 2).map((s) => [s.kind, s.idea])).toEqual([["review", "cat"], ["review", "dog"]]);
  expect(plan.filter((s) => s.kind === "teach").map((s) => s.idea)).toEqual(["fish", "bird"]); // two new under 3
});

test("under 4 she taps the picture among others; from 4 she says the word", () => {
  const memory: Memory = { due: ["cat"], seen: seenUpTo("dog"), known: [] };
  const young = planGame({ memory, ageMonths: 30, random: fixed })[0];
  expect(young).toMatchObject({ kind: "review", idea: "cat", how: "touch" });
  expect(young.choices).toContain("cat");
  expect(young.choices).toHaveLength(2); // one other picture under 3
  expect(new Set(young.choices).size).toBe(2);
  const older = planGame({ memory, ageMonths: 50, random: fixed });
  expect(older[0]).toMatchObject({ kind: "review", idea: "cat", how: "say" });
  const practise = older.find((s) => s.kind === "practise")!;
  expect(practise.how).toBe("touch"); // pets: touch the picture
  expect(practise.choices).toHaveLength(3);
});

test("units for older children wait; when nothing new suits her age, the game reviews what she met", () => {
  const stageOne = CURRICULUM.filter((i) => i.from <= 24).map((i) => i.id);
  const plan = planGame({ memory: { due: [], seen: stageOne, known: [] }, ageMonths: 26, random: fixed });
  expect(plan.filter((s) => s.kind === "teach")).toEqual([]);
  expect(plan).toHaveLength(LESSON_ROUND);
  expect(plan.every((s) => s.kind === "review" && stageOne.includes(s.idea!))).toBe(true);
});

test("a tiny chat once she knows a few words, from 4", () => {
  const memory: Memory = { due: [], seen: seenUpTo("mouse"), known: ["cat", "dog", "fish"] };
  expect(planGame({ memory, ageMonths: 50, random: fixed }).at(-1)).toEqual({ kind: "chat" });
  expect(planGame({ memory, ageMonths: 40, random: fixed }).some((s) => s.kind === "chat")).toBe(false);
});

test("from 3½ every game teaches a phrase too; a phrase is said, never tapped", () => {
  const memory: Memory = { due: [], seen: seenUpTo("cat"), known: [] };
  const taught = (ageMonths: number) => planGame({ memory, ageMonths, random: fixed }).filter((s) => s.kind === "teach").map((s) => s.idea);
  expect(taught(44)).toEqual(["dog", "fish", "how_many"]);
  expect(taught(40)).toEqual(["dog", "fish", "bird"]);
  const plan = planGame({ memory, ageMonths: 44, random: fixed });
  expect(plan.filter((s) => s.idea === "how_many" && s.kind !== "teach").every((s) => s.how === "talk")).toBe(true);
  const due = planGame({ memory: { ...memory, due: ["how_many"] }, ageMonths: 44, random: fixed });
  expect(due[0]).toEqual({ kind: "review", idea: "how_many", how: "talk" });
});

const teach: Step = { kind: "teach", idea: "cat" };
const touch: Step = { kind: "practise", idea: "cat", how: "touch", choices: ["dog", "cat", "fish"] };
const words = { cat: "cat", dog: "dog", fish: "fish" };
const prompt = (more: Partial<Parameters<typeof lessonPrompt>[0]> = {}) =>
  lessonPrompt({ child: anna, step: teach, answered: { said: "Jā" }, language: "en", words, known: [], ...more });

test("the prompt: Cubey, the target language and hers, this exchange's one job with its word in braces", () => {
  expect(prompt()).toContain("You are Cubey");
  expect(prompt()).toContain("English");
  expect(prompt()).toContain("Latvian");
  expect(prompt()).toContain("{cat}");
  expect(prompt()).toContain("new word");
  expect(prompt()).toContain("answers your offer to play: begin");
  expect(prompt({ step: touch, before: teach })).toContain("tap {cat} on your screen");
  expect(prompt({ step: touch, before: teach })).toContain("The cards show: dog, cat, fish.");
});

test("only this step's words in the target language: Cubey doesn't stray to others", () => {
  expect(prompt()).toContain("The only English words in this answer: {cat}. Never teach or ask about another.");
  expect(prompt({ step: touch, before: teach })).toContain("The only English words in this answer: {cat}, {dog}, {fish}.");
  const chat = prompt({ step: { kind: "chat" }, before: teach, known: ["dog"] });
  expect(chat).toContain("The only English words in this answer: {cat}, {dog}.");
});

test("taps: right; wrong, with one more go; wrong again, and on", () => {
  expect(prompt({ step: touch, before: touch, answered: { tapped: "cat", right: true, again: false } })).toContain("Anna tapped the cat: right!");
  const again = prompt({ step: touch, before: touch, answered: { tapped: "dog", right: false, again: true } });
  expect(again).toContain("Anna tapped the dog, but {cat} was asked for: say kindly that this one is {dog}, and ask once more.");
  const on = prompt({ step: teach, before: touch, answered: { tapped: "fish", right: false, again: false } });
  expect(on).toContain("tapped the fish again, but {cat} was asked for: say {cat} cheerfully, and go on.");
});

test("she says nothing: the step once more, never a goodbye; the game never ends by itself", () => {
  const quiet = prompt({ step: teach, before: teach, answered: { quiet: true } });
  expect(quiet).toContain("Anna said nothing: say it once more, cheerfully, and ask again. Never ask whether she is there.");
  expect(quiet).not.toContain("goodbye to her");
  expect(prompt()).not.toContain("gone quiet");
});

test("stopping: she wants to stop, says goodbye or wants something else; Cubey's usual talk answers her", () => {
  expect(prompt()).toContain("wants something else (to talk, to brush her teeth, a timer, anything), in any words: set stop and leave say empty");
  expect(prompt()).toContain('"stop": false');
});

test("the card is told only where its picture stands for the idea (a frog for jump): a red ball is no red circle", () => {
  const teaching = (id: string) => prompt({ step: { kind: "teach", idea: id }, words: {} });
  expect(teaching("red")).not.toContain("circle");
  expect(teaching("jump")).toContain("The card shows: frog.");
  expect(teaching("whats_this")).toContain("new phrase");
});

test("praise is small and varied, about what she did; her forms and Cubey's; her family as it is", () => {
  expect(prompt()).toContain("differently each time");
  expect(prompt()).toContain("feminine forms for Anna");
  expect(prompt()).toContain("masculine forms for yourself");
  expect(prompt({ child: { ...anna, name: "Jānis", gender: "boy" } })).toContain("masculine forms for Jānis");
  expect(prompt({ child: { ...anna, parents: "dads" } })).toContain("a great question for her dads");
  expect(prompt()).toContain(`a girl of ${Math.floor(anna.ageMonths / 12)} years`);
});

test("its answer: what it says and how her last try went, or that she wants to stop; nothing usable is undefined", () => {
  const raw = 'Sure: {"say":"[excited] Atkārto: {cat}! [cheerful] {Cat}!","result":"right","for_parents":false}';
  expect(readLesson(raw)).toEqual({ said: "[excited] Atkārto: {cat}! {Cat}!", result: "right", forParents: false });
  expect(readLesson('{"say":"Hi!","result":"great"}')?.result).toBeUndefined();
  expect(readLesson('{"say":"","stop":true}')).toEqual({ said: "", forParents: false, stop: true });
  expect(readLesson("no json")).toBeUndefined();
  expect(readLesson('{"say":""}')).toBeUndefined();
});

test("asking: the job goes in the prompt; her words, silence and taps reach the model as words", async () => {
  const asked: { system: string; messages: { role: string; text: string }[] }[] = [];
  const complete = async (opts: { system: string; messages: { role: "user" | "assistant"; text: string }[] }) => (asked.push(opts), '{"say":"{Cat}!"}');
  const base = { child: anna, step: teach, language: "en", words, known: [], talk: [], complete };
  await askLesson({ ...base, answered: { quiet: true } });
  expect(asked[0].messages).toEqual([{ role: "user", text: "(Anna said nothing)" }]);
  await askLesson({ ...base, before: touch, answered: { tapped: "cat", right: true, again: false } });
  expect(asked[1].messages).toEqual([{ role: "user", text: "(Anna tapped the cat)" }]);
  await askLesson({ ...base, answered: { said: "Kaķis" } });
  expect(asked[2].messages).toEqual([{ role: "user", text: "Kaķis" }]);
  await expect(askLesson({ ...base, answered: { said: "?" }, complete: async () => "oops" })).rejects.toThrow("no usable lesson answer");
});

test("the words in braces are the target language, spoken in it; a cue goes with the run it colours", () => {
  expect(spoken("[excited] Atkārto pēc manis: {cat}! {Cat}!", "lv", "en")).toEqual([
    { text: "[excited] Atkārto pēc manis: ", language: "lv" },
    { text: "cat! Cat!", language: "en" }, // punctuation joins the word before; runs in one language join
  ]);
  expect(spoken("[excited] {Cat}! Lieliski!", "lv", "en")).toEqual([
    { text: "[excited] Cat! ", language: "en" }, // the cue colours the English word, not an empty Latvian run
    { text: "Lieliski!", language: "lv" },
  ]);
  expect(spoken("{Good morning}, Anna!", "lv", "en")).toEqual([
    { text: "Good morning, ", language: "en" },
    { text: "Anna!", language: "lv" },
  ]);
  expect(spoken("Lieliski!", "lv", "en")).toEqual([{ text: "Lieliski!", language: "lv" }]);
  expect(idea("cat")?.picture).toBe("Cat");
});

test("the word game's prompt: its rules first, alike each step; this step, its words and what she answered, last", () => {
  const a = prompt(), b = lessonPrompt({ child: anna, step: touch, before: teach, answered: { tapped: "dog", right: false, again: true }, language: "en", words, known: [] });
  let same = 0;
  while (a[same] === b[same]) same++;
  expect(same).toBeGreaterThan(a.indexOf("No secrets, no promises"));
  expect(a.indexOf("This exchange")).toBeGreaterThan(a.indexOf("Always"));
});

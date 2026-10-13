import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { LESSON_ROUND, LessonProgress, LessonWords } from "./lesson";
import { Lessons } from "./lesson-game";
import type { Complete } from "./llm";

const anna = { id: "Anna", name: "Anna", ageMonths: 30, timeZone: "Europe/Riga", gender: "girl" as const, language: "lv" as const, learns: "en", parents: "mum_dad" as const };
const now = () => new Date("2026-09-29T10:00:00Z");
const today = "2026-09-29";

// The model: translations as asked, and Cubey's reply for a game step (reply).
function model(reply = '{"say":"[excited] {Hello}! {Hello}!","result":"tried"}') {
  const calls = { translate: 0, game: 0, systems: [] as string[], heard: [] as string[] };
  const complete: Complete = async ({ system, messages }) => {
    if (system.startsWith("Translate")) {
      calls.translate++;
      const asked = JSON.parse(messages[0].text) as Record<string, string>;
      return JSON.stringify(Object.fromEntries(Object.entries(asked).map(([id, en]) => [id, `EN ${en}`])));
    }
    calls.game++;
    calls.systems.push(system);
    calls.heard.push(messages.at(-1)!.text);
    return reply;
  };
  return { calls, complete };
}

function lessons(complete: Complete) {
  const db = new Database(":memory:");
  const progress = new LessonProgress(db);
  const pictures: string[] = [];
  const game = new Lessons({
    progress, words: new LessonWords(db), complete, now,
    picture: async (name, size) => (pictures.push(`${name} ${size}`), `png:${name}:${size}`),
  });
  return { game, progress, pictures };
}

const said = (text: string) => ({ her: { said: text }, talk: [] });
const tapped = (id: string) => ({ her: { tapped: id }, talk: [] });
const quiet = { her: { quiet: true as const }, talk: [] };

test("a game starts from her memory, with the words in the target language translated once and kept", async () => {
  const { calls, complete } = model();
  const { game: lessons_ } = lessons(complete);
  const game = await lessons_.start(anna, "en");
  expect(game).toMatchObject({ language: "en", today, at: -1 });
  expect(game.plan[0]).toEqual({ kind: "teach", idea: "hello" });
  expect(game.words.hello).toBe("EN hello");
  expect(calls.translate).toBe(1);
  await lessons_.start(anna, "en");
  expect(calls.translate).toBe(1); // kept
});

test("teaching: the picture over all of the screen, the idea remembered as new, two languages, the parents see it", async () => {
  const { complete } = model();
  const { game: lessons_, progress, pictures } = lessons(complete);
  const game = await lessons_.start(anna, "en");
  const turn = await lessons_.turn(game, said("Jā!"));
  if (turn.stop) throw new Error("stopped");
  expect(turn.show).toEqual({ type: "lesson.show", picture: "png:Waving hand:256" });
  expect(pictures).toEqual(["Waving hand 256"]);
  expect(turn.spoken).toEqual([{ text: "[excited] Hello! Hello!", language: "en" }]);
  expect(progress.memory("Anna", "en", today).seen).toEqual(["hello"]);
  expect(turn.log).toEqual({ heard: "Jā!", said: "[excited] Hello! Hello!", words: [{ word: "EN hello", meaning: "hello", result: "new" }] });
  expect(game.at).toBe(0);
});

// A game whose first step asks her to tap the cat among others.
async function tapping(reply?: string) {
  const { calls, complete } = model(reply);
  const { game: lessons_, progress } = lessons(complete);
  progress.record("Anna", "en", "cat", "new", "2026-09-28");
  progress.record("Anna", "en", "dog", "new", "2026-09-28");
  const game = await lessons_.start(anna, "en");
  expect(game.plan[0]).toMatchObject({ kind: "review", idea: "cat", how: "touch" });
  const asking = await lessons_.turn(game, said("Jā"));
  if (asking.stop) throw new Error("stopped");
  expect(asking.show.type).toBe("lesson.choose");
  expect((asking.show as { pictures: { id: string }[] }).pictures.map((p) => p.id).sort()).toEqual([...game.plan[0].choices!].sort());
  return { calls, lessons_, progress, game };
}

test("a tap on the right picture counts as right, and the game goes on", async () => {
  const { lessons_, progress, game } = await tapping();
  const turn = await lessons_.turn(game, tapped("cat"));
  if (turn.stop) throw new Error("stopped");
  expect(turn.log.heard).toBe("tapped the cat");
  expect(turn.log.words[0]).toEqual({ word: "EN cat", meaning: "cat", result: "right" });
  expect(game.at).toBe(1);
  expect(progress.memory("Anna", "en", "2026-09-30").due).not.toContain("cat"); // right: two days on
});

test("a wrong tap gets one more go at the same pictures; right then is a try; wrong again is missed, and on", async () => {
  const { calls, lessons_, progress, game } = await tapping();
  const again = await lessons_.turn(game, tapped("dog"));
  if (again.stop) throw new Error("stopped");
  expect(game.at).toBe(0); // the same step
  expect(again.show.type).toBe("lesson.choose");
  expect(again.log.words).toEqual([]);
  expect(calls.systems.at(-1)).toContain("and ask once more");
  const right = await lessons_.turn(game, tapped("cat"));
  if (right.stop) throw new Error("stopped");
  expect(right.log.words[0]).toMatchObject({ result: "tried" });
  expect(game.at).toBe(1);

  const other = await tapping();
  await other.lessons_.turn(other.game, tapped("dog"));
  const missed = await other.lessons_.turn(other.game, tapped("dog"));
  if (missed.stop) throw new Error("stopped");
  expect(missed.log.words[0]).toMatchObject({ result: "missed" });
  expect(other.game.at).toBe(1);
  expect(other.progress.memory("Anna", "en", "2026-09-30").due).toContain("cat"); // missed: tomorrow again
});

test("she says nothing: the same step once more, nothing judged; never a goodbye, however long", async () => {
  const { calls, complete } = model();
  const { game: lessons_ } = lessons(complete);
  const game = await lessons_.start(anna, "en");
  await lessons_.turn(game, said("Jā"));
  for (let i = 0; i < 5; i++) {
    const turn = await lessons_.turn(game, quiet);
    if (turn.stop) throw new Error("stopped");
    expect(turn.log.words).toEqual([]);
    expect(game.at).toBe(0);
  }
  expect(calls.heard.at(-1)).toBe("(Anna said nothing)");
  expect(calls.systems.at(-1)).toContain("Anna said nothing: say it once more");
  expect(calls.systems.some((s) => s.includes("goodbye to her"))).toBe(false);
});

test("she wants to stop: the game says so and changes nothing", async () => {
  const { complete } = model('{"say":"","stop":true}');
  const { game: lessons_, progress } = lessons(complete);
  const game = await lessons_.start(anna, "en");
  expect(await lessons_.turn(game, said("Negribu vairs"))).toEqual({ stop: true, forParents: false });
  expect(game.at).toBe(-1);
  expect(progress.memory("Anna", "en", today).seen).toEqual([]);
});

test("no count: past its planned steps it plans the next round from her memory as it is by then, with new words", async () => {
  const { calls, complete } = model();
  const { game: lessons_ } = lessons(complete);
  const game = await lessons_.start(anna, "en");
  const taught = () => game.plan.filter((s) => s.kind === "teach").map((s) => s.idea);
  const first = taught();
  for (let i = 0; i <= LESSON_ROUND; i++) await lessons_.turn(game, said("Jā"));
  expect(game.plan).toHaveLength(2 * LESSON_ROUND);
  const next = taught().slice(first.length);
  expect(next.length).toBeGreaterThan(0);
  expect(next.some((id) => first.includes(id))).toBe(false);
  expect(calls.translate).toBe(2); // the new round's words
});

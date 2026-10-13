import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { JOBS, MODELS, Models } from "./models";
import { MODEL_PRICES } from "./prices";

const both = { GEMINI_API_KEY: "g", OPENAI_API_KEY: "o" };
const gemini = { GEMINI_API_KEY: "g" }, openai = { OPENAI_API_KEY: "o" };
const models = (keys: Record<string, string>, db = new Database(":memory:")) => new Models(db, keys);

test("every model offered has a price (what it costs shows on the admin page), and every job has models", () => {
  for (const m of MODELS) expect(MODEL_PRICES[m.id]).toBeDefined();
  for (const job of JOBS) expect(MODELS.some((m) => m.jobs.includes(job))).toBe(true);
});

test("by default Flash-Lite talks and plays the games; Astra draws when there is an OpenAI key, else Flash-Lite", () => {
  const all = models(both);
  expect(JOBS.map((job) => all.model(job))).toEqual(["gemini-3.5-flash-lite", "gemini-3.5-flash-lite", "gpt-6-astra"]);
  expect(JOBS.map((job) => models(gemini).model(job))).toEqual(["gemini-3.5-flash-lite", "gemini-3.5-flash-lite", "gemini-3.5-flash-lite"]);
  expect(JOBS.map((job) => models(openai).model(job))).toEqual(["gpt-6-sol", "gpt-6-sol", "gpt-6-astra"]);
});

test("a model chosen for a job is used from its next turn, and kept", () => {
  const db = new Database(":memory:");
  const all = models(both, db);
  expect(all.choose("games", "gpt-6-sol")).toBe(true);
  expect(all.model("games")).toBe("gpt-6-sol");
  expect(models(both, db).model("games")).toBe("gpt-6-sol");
  expect(models(both, db).model("talk")).toBe("gemini-3.5-flash-lite"); // the others as they were
});

test("only a model offered for the job, with its key, can be chosen", () => {
  const some = models(gemini);
  expect(some.choose("talk", "gpt-6-astra")).toBe(false); // it draws only: seconds before a word
  expect(some.choose("talk", "gpt-6-sol")).toBe(false); // no OpenAI key
  expect(some.choose("talk", "no-such-model")).toBe(false);
  expect(some.model("talk")).toBe("gemini-3.5-flash-lite");
});

test("its key gone, a chosen model gives way to the job's default, and stays chosen for when the key is back", () => {
  const db = new Database(":memory:");
  models(both, db).choose("talk", "gpt-6-sol");
  const now = models(gemini, db);
  expect(now.chosen("talk")).toBe("gpt-6-sol");
  expect(now.model("talk")).toBe("gemini-3.5-flash-lite");
  expect(models(both, db).model("talk")).toBe("gpt-6-sol");
});

test("what the admin page offers for a job: each model with whether its key is set, and which key it needs", () => {
  const drawing = models(gemini).offered("drawing");
  expect(drawing.map((o) => [o.model.id, o.available])).toEqual([
    ["gemini-3.5-flash-lite", true], ["gemini-3.1-flash-lite", true], ["gpt-6-sol", false], ["gpt-6-astra", false],
  ]);
  expect(drawing.find((o) => o.model.id === "gpt-6-astra")!.model.key).toBe("OPENAI_API_KEY");
  expect(models(gemini).offered("talk").map((o) => o.model.id)).not.toContain("gpt-6-astra");
});

test("with no model's key set, none can be used", () => {
  expect(models({}).usable()).toBe(false);
  expect(models(gemini).usable()).toBe(true);
  expect(() => models({}).model("talk")).toThrow(/key/);
});

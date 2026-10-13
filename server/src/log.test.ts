import { expect, test } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { ParentLog } from "./log";

test("every question and answer is kept for the parents, the big questions marked", () => {
  const log = new ParentLog(join(mkdtempSync(join(tmpdir(), "cubey-")), "data", "log.jsonl"));
  expect(log.read()).toEqual([]);
  log.add({ heard: "Kāpēc debesis ir zilas?", said: "Saule...", forParents: false }, new Date("2026-10-06T10:00:00Z"));
  log.add({ heard: "Kur rodas bērni?", said: "Lielisks jautājums mammai!", forParents: true }, new Date("2026-10-06T10:01:00Z"));
  expect(log.read()).toEqual([
    { at: "2026-10-06T10:00:00.000Z", question: "Kāpēc debesis ir zilas?", answer: "Saule..." },
    { at: "2026-10-06T10:01:00.000Z", question: "Kur rodas bērni?", answer: "Lielisks jautājums mammai!", for_parents: true },
  ]);
});

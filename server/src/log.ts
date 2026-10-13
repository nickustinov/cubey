// The parents' log: every question the child asked and what Cubey answered, word for word, the big questions
// marked for them. A JSON line each in log.jsonl in the data folder (db.ts) until a parents' app exists (`bun run
// log` shows it). No audio is kept.
import { appendFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { DATA } from "./db";

// One exchange: her words (or what happened, in brackets) and Cubey's answer; forParents: a big question.
export type Exchanged = { heard: string; said: string; forParents: boolean };
export type Entry = { at: string; question: string; answer: string; for_parents?: true };

const LOG_PATH = join(DATA, "log.jsonl");

export class ParentLog {
  constructor(private path = LOG_PATH) {}

  add({ heard, said, forParents }: Exchanged, at = new Date()) {
    const entry: Entry = { at: at.toISOString(), question: heard, answer: said, ...(forParents ? { for_parents: true as const } : {}) };
    mkdirSync(dirname(this.path), { recursive: true });
    appendFileSync(this.path, JSON.stringify(entry) + "\n");
  }

  read(): Entry[] {
    if (!existsSync(this.path)) return [];
    return readFileSync(this.path, "utf8").split("\n").filter(Boolean).map((line) => JSON.parse(line));
  }
}

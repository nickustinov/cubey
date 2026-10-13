// The models Cubey thinks with, a grown-up's choice on the admin page (admin.ts): one for each job, for all the Cubeys,
// kept in SQLite and used from the next turn. Only models with a price (prices.ts), and only those whose provider's key
// the server has; without a choice, or with the chosen one's key gone, each job's default.
import type { Database } from "bun:sqlite";

// talk: every turn of a talk; games: the games with words (the chain, words on one letter, spelling) and the word game
// with its translations; drawing: what Cubey draws or writes on its screen.
export const JOBS = ["talk", "games", "drawing"] as const;
export type Job = (typeof JOBS)[number];
export type Key = "GEMINI_API_KEY" | "OPENAI_API_KEY";
export type Model = { id: string; name: string; key: Key; jobs: readonly Job[] };

// GPT-6 Astra draws best, but takes about 6 s (2026-10-12): drawing only.
export const MODELS: Model[] = [
  { id: "gemini-3.5-flash-lite", name: "Gemini 3.5 Flash-Lite", key: "GEMINI_API_KEY", jobs: JOBS },
  { id: "gpt-6-sol", name: "GPT-6 Sol", key: "OPENAI_API_KEY", jobs: JOBS },
  { id: "gpt-6.1-sol", name: "GPT-6.1 Sol", key: "OPENAI_API_KEY", jobs: JOBS },
  { id: "gpt-6-astra", name: "GPT-6 Astra", key: "OPENAI_API_KEY", jobs: ["drawing"] },
];

// Each job's default, the first of these with its key: Flash-Lite, the quickest and cheapest, for talk and games;
// Astra for drawing (Flash-Lite drew a blob for a cat).
const DEFAULTS: Record<Job, string[]> = {
  talk: ["gemini-3.5-flash-lite", "gpt-6-sol"],
  games: ["gemini-3.5-flash-lite", "gpt-6-sol"],
  drawing: ["gpt-6-astra", "gemini-3.5-flash-lite"],
};

export class Models {
  // keys: the server's environment, for which providers' keys it has
  constructor(private db: Database, private keys: Record<string, string | undefined> = process.env) {
    db.run("CREATE TABLE IF NOT EXISTS models (job TEXT PRIMARY KEY, model TEXT NOT NULL)");
  }

  // What the admin page offers for a job: each model, and whether its key is set.
  offered(job: Job): { model: Model; available: boolean }[] {
    return MODELS.filter((m) => m.jobs.includes(job)).map((model) => ({ model, available: !!this.keys[model.key] }));
  }

  chosen(job: Job): string | undefined {
    return (this.db.query("SELECT model FROM models WHERE job = ?").get(job) as { model: string } | null)?.model;
  }

  // Only a model offered for the job, with its key.
  choose(job: Job, id: string): boolean {
    if (!this.usableFor(job, id)) return false;
    this.db.run("INSERT INTO models (job, model) VALUES (?, ?) ON CONFLICT (job) DO UPDATE SET model = excluded.model", [job, id]);
    return true;
  }

  // The model the job uses now: the one chosen, if it has its key; else its default.
  model(job: Job): string {
    const chosen = this.chosen(job);
    if (chosen && this.usableFor(job, chosen)) return chosen;
    const fallback = DEFAULTS[job].find((id) => this.usableFor(job, id));
    if (!fallback) throw new Error(`no model's key is set for ${job} (GEMINI_API_KEY or OPENAI_API_KEY)`);
    return fallback;
  }

  usable(): boolean {
    return MODELS.some((m) => !!this.keys[m.key]);
  }

  private usableFor(job: Job, id: string): boolean {
    return this.offered(job).some((o) => o.model.id === id && o.available);
  }
}

// What each Cubey has spent (prices.ts): every model's call, every turn's listening and speaking, kept in SQLite for
// the admin page (admin.ts): today, this month (UTC), all of it, and by what it was for.
import type { Database } from "bun:sqlite";
import { costOf, type Spent } from "./prices";

// One day's (UTC) of one Cubey's on one kind of thing (and model): its dollars, tokens and seconds of audio.
export type Spend = {
  day: string; device: string; what: Spent["what"]; model: string; usd: number; input: number; cached: number; output: number; seconds: number;
};
export type Spends = { today: number; month: number; total: number; tokens: { input: number; output: number }; by: Partial<Record<Spent["what"], number>> };

export class Spending {
  constructor(private db: Database) {
    db.run(`CREATE TABLE IF NOT EXISTS spending (
      device TEXT NOT NULL, at TEXT NOT NULL, what TEXT NOT NULL, model TEXT,
      input INTEGER NOT NULL DEFAULT 0, cached INTEGER NOT NULL DEFAULT 0, output INTEGER NOT NULL DEFAULT 0,
      seconds REAL NOT NULL DEFAULT 0, usd REAL NOT NULL)`);
    db.run("CREATE INDEX IF NOT EXISTS spending_device_at ON spending (device, at)");
  }

  add(device: string, spent: Spent, at = new Date()) {
    this.db.run("INSERT INTO spending (device, at, what, model, input, cached, output, seconds, usd) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", [
      device, at.toISOString(), spent.what, spent.model ?? null, spent.input ?? 0, spent.cached ?? 0, spent.output ?? 0, spent.seconds ?? 0, costOf(spent),
    ]);
  }

  // One Cubey's; none named, all of them.
  of(device: string | undefined, now = new Date()): Spends {
    const day = now.toISOString().slice(0, 10), month = day.slice(0, 7);
    const where = device === undefined ? "1 = 1" : "device = ?";
    const args = device === undefined ? [] : [device];
    const sum = (since: string) =>
      (this.db.query(`SELECT COALESCE(SUM(usd), 0) AS usd FROM spending WHERE ${where} AND at >= ?`).get(...args, since) as { usd: number }).usd;
    const all = this.db.query(`SELECT COALESCE(SUM(usd), 0) AS usd, COALESCE(SUM(input), 0) AS input, COALESCE(SUM(output), 0) AS output FROM spending WHERE ${where}`)
      .get(...args) as { usd: number; input: number; output: number };
    const by = this.db.query(`SELECT what, SUM(usd) AS usd FROM spending WHERE ${where} GROUP BY what`).all(...args) as { what: Spent["what"]; usd: number }[];
    return {
      today: sum(day), month: sum(`${month}-01`), total: all.usd, tokens: { input: all.input, output: all.output },
      by: Object.fromEntries(by.map((b) => [b.what, b.usd])),
    };
  }

  // Where it went since a day ("YYYY-MM-DD", UTC), for the admin page's graphs.
  breakdown(since: string): Spend[] {
    return this.db.query(`SELECT substr(at, 1, 10) AS day, device, what, COALESCE(model, '') AS model, SUM(usd) AS usd, SUM(input) AS input,
      SUM(cached) AS cached, SUM(output) AS output, SUM(seconds) AS seconds FROM spending WHERE at >= ? GROUP BY 1, 2, 3, 4 ORDER BY 1, 2, 3, 4`)
      .all(since) as Spend[];
  }
}

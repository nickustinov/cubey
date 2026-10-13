// What Cubey and the child said before, so every talk goes on from the last one instead of starting over: each
// exchange (and what happened: she woke Cubey) per child in SQLite (db.ts), and the latest few in each prompt with
// how long ago they were.
import type { Database } from "bun:sqlite";

// heard: her words, "" for nothing, or what happened in brackets ("(Anna has just woken you.)")
export type Earlier = { at: string; heard: string; said: string };

export const EARLIER_MOST = 10; // exchanges from before in a prompt: enough to go on from, few tokens
const SIDE_MOST = 160; // characters of each side of one

export class History {
  constructor(private db: Database) {
    db.run(`CREATE TABLE IF NOT EXISTS talk_history (
      child TEXT NOT NULL,
      at TEXT NOT NULL,
      heard TEXT NOT NULL,
      said TEXT NOT NULL
    )`);
    db.run("CREATE INDEX IF NOT EXISTS talk_history_by_child ON talk_history (child, at)");
  }

  add(child: string, heard: string, said: string, at = new Date()) {
    this.db.run("INSERT INTO talk_history (child, at, heard, said) VALUES (?, ?, ?, ?)", [child, at.toISOString(), heard, said]);
  }

  forget(child: string) {
    this.db.run("DELETE FROM talk_history WHERE child = ?", [child]);
  }

  // The latest, oldest first.
  recent(child: string, most = EARLIER_MOST): Earlier[] {
    const rows = this.db.query("SELECT at, heard, said FROM talk_history WHERE child = ? ORDER BY at DESC, rowid DESC LIMIT ?")
      .all(child, most) as Earlier[];
    return rows.reverse();
  }
}

export function ago(at: string, now = new Date()): string {
  const minutes = Math.floor((now.getTime() - new Date(at).getTime()) / 60000);
  const plural = (n: number, unit: string) => `${n} ${unit}${n === 1 ? "" : "s"} ago`;
  if (minutes < 2) return "just now";
  if (minutes < 60) return plural(minutes, "minute");
  if (minutes < 24 * 60) return plural(Math.floor(minutes / 60), "hour");
  if (minutes < 48 * 60) return "yesterday";
  return plural(Math.floor(minutes / (24 * 60)), "day");
}

const cut = (text: string) => (text.length > SIDE_MOST ? `${text.slice(0, SIDE_MOST - 1)}…` : text);

// What was said before, for a prompt, oldest first; nothing for a first meeting.
export function earlierBlock(earlier: Earlier[], name: string, now = new Date()): string {
  if (!earlier.length) return "";
  const lines = earlier.map(({ at, heard, said }) => {
    const hers = heard.startsWith("(") ? heard : heard ? `${name}: "${cut(heard)}"` : `${name} said nothing.`;
    return `- ${ago(at, now)}. ${hers} You: "${cut(said)}"`;
  });
  return `Your talks with ${name} before this one (oldest first): go on from them, as a friend who remembers.\n${lines.join("\n")}`;
}

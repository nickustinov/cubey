// The server's memory: Cubey's talks with her (history.ts). SQLite in the data folder: server/data (git-ignored) on
// this Mac, CUBEY_DATA (Fly's volume) elsewhere. No audio.
import { Database } from "bun:sqlite";
import { mkdirSync } from "node:fs";
import { dirname, join } from "node:path";

export const DATA = process.env.CUBEY_DATA ?? new URL("../data/", import.meta.url).pathname;

export function openDb(path = join(DATA, "cubey.db")): Database {
  mkdirSync(dirname(path), { recursive: true });
  const db = new Database(path, { create: true });
  db.run("PRAGMA journal_mode = WAL");
  return db;
}

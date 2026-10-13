// The Cubeys this server talks to, each by its chip's number (its id) and a key of its own it makes once: met for the
// first time, it is kept with its key (only its hash), and from then on only that key will do; a grown-up may block
// it, name it, or forget it (admin.ts). Each has its own child (memory.md) and parents' log in data/devices/<id>/,
// begun with a child not known yet, whom Cubey gets to know by talking, its languages set on its menu; and, by its id,
// its talks before (history.ts) and its word game (lesson.ts).
import type { Database } from "bun:sqlite";
import { createHash, timingSafeEqual } from "node:crypto";
import { existsSync, mkdirSync, renameSync } from "node:fs";
import { join } from "node:path";
import { ParentLog } from "./log";
import { Memory } from "./memory";

export type Admitted = "ok" | "wrong key" | "blocked" | "no id";
// Its firmware, as it said at its last hello: version and build (firmware.ts), and the build of an update that failed
// there (rolled back from, or not fetched).
export type Running = { version: string; build: string; failed?: string };
// wanted: the build of an update asked for it, until it runs it or it failed there
export type Device = { id: string; label?: string; created: string; seen?: string; online: boolean; blocked: boolean; firmware?: Running; wanted?: string };
type Row = {
  id: string; key_hash: string; label: string | null; created: string; seen: string | null; blocked: number; version: string | null; build: string | null;
  failed: string | null; wanted: string | null;
};

const hashOf = (key: string) => createHash("sha256").update(key).digest();

export class Fleet {
  private online = new Map<string, number>(); // its connections now

  // timeZone: the household's, a new child's (Fly's own is UTC); stores: what else is kept by its id, gone when it is forgotten
  constructor(private db: Database, private data: string, private timeZone?: string, private stores: { forget(id: string): void }[] = []) {
    db.run(`CREATE TABLE IF NOT EXISTS devices (
      id TEXT PRIMARY KEY, key_hash TEXT NOT NULL, label TEXT, created TEXT NOT NULL, seen TEXT, blocked INTEGER NOT NULL DEFAULT 0)`);
    const columns = (db.query("PRAGMA table_info(devices)").all() as { name: string }[]).map((c) => c.name);
    for (const column of ["version", "build", "failed", "wanted"]) if (!columns.includes(column)) db.run(`ALTER TABLE devices ADD COLUMN ${column} TEXT`);
  }

  admit(id: string, key: string, now = new Date()): Admitted {
    if (!/^[a-z0-9]{1,32}$/i.test(id) || !key) return "no id";
    const row = this.row(id);
    if (!row) {
      this.db.run("INSERT INTO devices (id, key_hash, created) VALUES (?, ?, ?)", [id, hashOf(key).toString("hex"), now.toISOString()]);
      return "ok";
    }
    const kept = Buffer.from(row.key_hash, "hex"), sent = hashOf(key);
    if (!timingSafeEqual(kept, sent)) return "wrong key";
    return row.blocked ? "blocked" : "ok";
  }

  memory(id: string): Memory {
    return new Memory(join(this.folder(id), "memory.md"), id, this.timeZone);
  }

  log(id: string): ParentLog {
    return new ParentLog(join(this.folder(id), "log.jsonl"));
  }

  connected(id: string, on: boolean, now = new Date()) {
    this.online.set(id, Math.max(0, (this.online.get(id) ?? 0) + (on ? 1 : -1)));
    this.db.run("UPDATE devices SET seen = ? WHERE id = ?", [now.toISOString(), id]);
  }

  // To set it up anew (flashed clean, a new key): its record, its talks before and word game gone (every talk is in its
  // parents' log), its folder kept aside in data/forgotten/.
  forget(id: string, now = new Date()) {
    this.db.run("DELETE FROM devices WHERE id = ?", [id]);
    for (const store of this.stores) store.forget(id);
    if (!existsSync(this.folder(id))) return;
    mkdirSync(join(this.data, "forgotten"), { recursive: true });
    renameSync(this.folder(id), join(this.data, "forgotten", `${id}-${now.toISOString().slice(0, 19).replace(/:/g, "-")}`));
  }

  // Its hello: the firmware it runs. The update asked for it is over once it runs that build, or rolled back from it
  // (a build it rolled back from is not offered to it again); one that failed stays failed until it runs it.
  ran(id: string, running: Running) {
    const row = this.row(id);
    const over = row?.wanted === running.build || row?.wanted === running.failed;
    const failed = running.failed ?? (row?.failed !== running.build ? row?.failed : null);
    this.db.run("UPDATE devices SET version = ?, build = ?, failed = ?, wanted = ? WHERE id = ?",
      [running.version, running.build, failed ?? null, over ? null : row?.wanted ?? null, id]);
  }

  // An update to this build, asked for it on the admin page: offered at its next hello (or at once, if it is on).
  want(id: string, build: string) {
    this.db.run("UPDATE devices SET wanted = ?, failed = NULL WHERE id = ?", [build, id]);
  }

  // It couldn't fetch or write the build asked for: not offered again until asked anew.
  failed(id: string, build: string) {
    this.db.run("UPDATE devices SET failed = ?, wanted = NULL WHERE id = ? AND wanted = ?", [build, id, build]);
  }

  block(id: string, blocked: boolean) {
    this.db.run("UPDATE devices SET blocked = ? WHERE id = ?", [blocked ? 1 : 0, id]);
  }

  name(id: string, label: string) {
    this.db.run("UPDATE devices SET label = ? WHERE id = ?", [label.trim().slice(0, 60) || null, id]);
  }

  list(): Device[] {
    return (this.db.query("SELECT * FROM devices ORDER BY created").all() as Row[]).map((r) => this.device(r));
  }

  get(id: string): Device | undefined {
    const row = this.row(id);
    return row ? this.device(row) : undefined;
  }

  private row(id: string) {
    return this.db.query("SELECT * FROM devices WHERE id = ?").get(id) as Row | null;
  }

  private device(r: Row): Device {
    return {
      id: r.id, ...(r.label && { label: r.label }), created: r.created, ...(r.seen && { seen: r.seen }), online: (this.online.get(r.id) ?? 0) > 0, blocked: !!r.blocked,
      ...(r.version && r.build && { firmware: { version: r.version, build: r.build, ...(r.failed && { failed: r.failed }) } }), ...(r.wanted && { wanted: r.wanted }),
    };
  }

  private folder(id: string) {
    return join(this.data, "devices", id);
  }

}

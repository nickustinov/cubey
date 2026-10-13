import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Fleet } from "./fleet";
import { History } from "./history";
import { LessonProgress } from "./lesson";

const today = new Date(2026, 9, 12);
const dir = () => mkdtempSync(join(tmpdir(), "cubey-fleet-"));

test("a Cubey met for the first time is kept, with its key: the next must send the same; a blocked one is turned away", () => {
  const fleet = new Fleet(new Database(":memory:"), dir());
  expect(fleet.admit("44bd8d60da70", "secret")).toBe("ok");
  expect(fleet.admit("44bd8d60da70", "secret")).toBe("ok");
  expect(fleet.admit("44bd8d60da70", "another")).toBe("wrong key"); // someone else, sending its number
  expect(fleet.admit("", "secret")).toBe("no id");
  expect(fleet.admit("../etc", "secret")).toBe("no id"); // only letters and digits: it names a folder
  fleet.block("44bd8d60da70", true);
  expect(fleet.admit("44bd8d60da70", "secret")).toBe("blocked");
  fleet.block("44bd8d60da70", false);
  expect(fleet.list().map((d) => d.id)).toEqual(["44bd8d60da70"]);
});

test("each Cubey has its own child, not known yet, in the household's time zone, and its own parents' log", () => {
  const data = dir();
  const fleet = new Fleet(new Database(":memory:"), data, "Europe/Riga");
  fleet.admit("a", "k1");
  fleet.admit("b", "k2");
  fleet.memory("a").learn({ name: "Anna" }, today);
  fleet.log("a").add({ heard: "Čau", said: "Sveika!", forParents: false });
  const b = fleet.memory("b").child(today);
  expect(b).toMatchObject({ id: "b", timeZone: "Europe/Riga" });
  expect(b.name).toBeUndefined();
  expect(fleet.log("b").read()).toEqual([]);
  expect(readFileSync(join(data, "devices", "a", "memory.md"), "utf8")).toContain("- Name: Anna");
});

test("a Cubey forgotten (to set it up anew): its record gone, its child and log kept aside, its talks before and word game gone; met again, it is new", () => {
  const data = dir(), db = new Database(":memory:");
  const history = new History(db), progress = new LessonProgress(db);
  const fleet = new Fleet(db, data, undefined, [history, progress]);
  fleet.admit("a", "k1");
  fleet.memory("a").learn({ name: "Anna" }, today);
  history.add("a", "Čau", "Sveika, Anna!");
  history.add("b", "Čau", "Sveiks, Jānis!");
  progress.record("a", "en", "cat", "new", "2026-10-12");
  fleet.forget("a", new Date("2026-10-12T20:00:00Z"));
  expect(history.recent("a")).toEqual([]); // she isn't greeted by the name from before
  expect(history.recent("b")).toHaveLength(1);
  expect(progress.memory("a", "en", "2026-10-12").seen).toEqual([]);
  expect(fleet.get("a")).toBeUndefined();
  expect(readFileSync(join(data, "forgotten", "a-2026-10-12T20-00-00", "memory.md"), "utf8")).toContain("- Name: Anna");
  expect(fleet.admit("a", "a new key")).toBe("ok"); // flashed clean: a new key, welcome
  expect(fleet.memory("a").child(today).name).toBeUndefined();
});

test("which are online now, and when each was last seen", () => {
  const fleet = new Fleet(new Database(":memory:"), dir());
  fleet.admit("a", "k");
  fleet.connected("a", true, new Date("2026-10-12T10:00:00Z"));
  expect(fleet.list()[0]).toMatchObject({ id: "a", online: true, seen: "2026-10-12T10:00:00.000Z" });
  fleet.connected("a", false, new Date("2026-10-12T10:05:00Z"));
  expect(fleet.list()[0]).toMatchObject({ online: false, seen: "2026-10-12T10:05:00.000Z" });
  fleet.name("a", "Annas Kubijs");
  expect(fleet.list()[0].label).toBe("Annas Kubijs");
});

test("its firmware, as it said at its last hello; an update asked for it, until it runs that build or it failed there", () => {
  const fleet = new Fleet(new Database(":memory:"), dir());
  fleet.admit("a", "k");
  expect(fleet.get("a")).not.toHaveProperty("firmware");
  fleet.ran("a", { version: "a1b92ab", build: "0123456789abcdef" });
  expect(fleet.get("a")?.firmware).toEqual({ version: "a1b92ab", build: "0123456789abcdef" });
  fleet.want("a", "fedcba9876543210");
  expect(fleet.get("a")?.wanted).toBe("fedcba9876543210");
  fleet.ran("a", { version: "a1b92ab", build: "0123456789abcdef", failed: "fedcba9876543210" }); // rolled back from it
  expect(fleet.get("a")).not.toHaveProperty("wanted");
  expect(fleet.get("a")?.firmware?.failed).toBe("fedcba9876543210");
  fleet.want("a", "fedcba9876543210");
  fleet.ran("a", { version: "b2c3d4e", build: "fedcba9876543210" }); // it runs it now
  expect(fleet.get("a")).not.toHaveProperty("wanted");
  expect(fleet.get("a")?.firmware).toEqual({ version: "b2c3d4e", build: "fedcba9876543210" });
  fleet.want("a", "1111111111111111");
  fleet.failed("a", "1111111111111111"); // it couldn't fetch it
  expect(fleet.get("a")).not.toHaveProperty("wanted");
  fleet.ran("a", { version: "b2c3d4e", build: "fedcba9876543210" }); // its hello after: still failed there
  expect(fleet.get("a")?.firmware?.failed).toBe("1111111111111111");
  fleet.want("a", "1111111111111111"); // asked anew
  expect(fleet.get("a")?.firmware).not.toHaveProperty("failed");
});

test("a Cubey's firmware is kept in a database from before there were updates", () => {
  const db = new Database(":memory:");
  db.run("CREATE TABLE devices (id TEXT PRIMARY KEY, key_hash TEXT NOT NULL, label TEXT, created TEXT NOT NULL, seen TEXT, blocked INTEGER NOT NULL DEFAULT 0)");
  db.run("INSERT INTO devices (id, key_hash, created) VALUES ('a', 'x', '2026-10-01')");
  const fleet = new Fleet(db, dir());
  fleet.ran("a", { version: "a1b92ab", build: "0123456789abcdef" });
  expect(fleet.get("a")?.firmware?.version).toBe("a1b92ab");
});

import { expect, test } from "bun:test";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Memory } from "./memory";

const today = new Date(2026, 9, 6);
const file = () => join(mkdtempSync(join(tmpdir(), "cubey-")), "memory.md");

test("begun as a child Cubey doesn't know yet, speaking English and learning French, in the household's time zone; kept in memory.md", () => {
  const path = file();
  const child = new Memory(path, "aabbccddeeff", "Europe/Riga").child(today);
  expect(child).toEqual({ id: "aabbccddeeff", language: "en", learns: "fr", parents: "mum_dad", timeZone: "Europe/Riga" });
  const md = readFileSync(path, "utf8");
  expect(md).toContain("- Id: aabbccddeeff\n");
  expect(md).toContain("- Speaks: en\n");
  expect(new Memory(path, "another").child(today).id).toBe("aabbccddeeff"); // the file, from then on
});

test("what the child tells Cubey: a name (and the gender it suggests), an age, kept; their id stays, for their talks before", () => {
  const path = file();
  const memory = new Memory(path, "Anna");
  expect(memory.learn({ name: "Nick", gender: "boy" }, today)).toMatchObject({ id: "Anna", name: "Nick", gender: "boy" });
  expect(memory.learn({ age: 5 }, today).ageMonths).toBe(66); // about: halfway through the year
  expect(new Memory(path, "Anna").child(today)).toMatchObject({ name: "Nick", gender: "boy", ageMonths: 66 });
  expect(memory.learn({ name: "Nick" }, today).gender).toBe("boy"); // the same name again
  expect(memory.learn({ name: "Anna", age: 3 }, today).gender).toBeUndefined(); // "boy" was Nick's: guessed anew
  expect(memory.learn({ name: "Anna", gender: "girl" }, today).gender).toBe("girl");
});

test("a new Cubey's child, until its languages are set on its menu (or it knows the child's name: one from before)", () => {
  const memory = new Memory(file(), "aabbccddeeff");
  expect(memory.isNew()).toBe(true);
  expect(memory.set({ language: "en", learns: "fr" })).toBe(true); // as they were: set up all the same
  expect(memory.isNew()).toBe(false);
  const named = new Memory(file(), "Anna");
  named.learn({ name: "Anna" }, today);
  expect(named.isNew()).toBe(false);
});

test("the device's menu sets the languages: Cubey's and the one it teaches, never the same; anything else is refused", () => {
  const memory = new Memory(file(), "Anna");
  expect(memory.set({ language: "lv", learns: "de" })).toBe(true);
  expect(memory.child(today)).toMatchObject({ language: "lv", learns: "de" });
  expect(memory.set({ language: "de", learns: "de" })).toBe(false);
  expect(memory.set({ language: "xx", learns: "en" })).toBe(false);
  expect(memory.child(today)).toMatchObject({ language: "lv", learns: "de" });
  expect(memory.set({ language: "lv", learns: "de", theme: "green" })).toBe(true); // its theme, a third line
  expect(memory.child(today).theme).toBe("green");
  expect(memory.set({ language: "lv", learns: "de", theme: "tartan" })).toBe(true); // none such: as it was
  expect(memory.child(today).theme).toBe("green");
});

test("the screen's brightness, a grown-up's (by voice, or on the admin page): 10 to 100 percent; none, full", () => {
  const memory = new Memory(file(), "Anna");
  expect(memory.child(today).brightness).toBeUndefined();
  expect(memory.brighten(40)).toBe(true);
  expect(new Memory(memory["path"], "Anna").child(today).brightness).toBe(40);
  expect(memory.brighten(5)).toBe(false);
  expect(memory.brighten(150)).toBe(false);
  expect(memory.child(today).brightness).toBe(40);
  expect(memory.edit({ brightness: "70" }, today).brightness).toBe(70);
  expect(memory.edit({ brightness: "" }, today).brightness).toBeUndefined();
});

test("the screen's theme, a grown-up's on the admin page: one Cubey's screens have; none, amber", () => {
  const memory = new Memory(file(), "Anna");
  expect(memory.child(today).theme).toBeUndefined();
  expect(memory.edit({ theme: "green" }, today).theme).toBe("green");
  expect(new Memory(memory["path"], "Anna").child(today).theme).toBe("green");
  expect(memory.edit({ theme: "tartan" }, today).theme).toBeUndefined();
});

test("a grown-up's edit on the admin page: kept as checked; a field left empty, not known any more", () => {
  const memory = new Memory(file(), "Anna");
  expect(memory.edit({ name: "Anna Ā.", gender: "girl", age: "3", language: "en", learns: "lv", parents: "mum", timeZone: "Europe/Riga" }, today))
    .toMatchObject({ name: "Anna Ā.", ageMonths: 42, language: "en", learns: "lv", parents: "mum", timeZone: "Europe/Riga" }); // as a child telling it
  expect(memory.edit({ name: "", gender: "", age: "" }, today)).toMatchObject({ id: "Anna", language: "en" });
  expect(memory.child(today).name).toBeUndefined();
  expect(memory.child(today).ageMonths).toBeUndefined();
  expect(memory.edit({ language: "xx", age: "soon" }, today)).toMatchObject({ language: "en" }); // what makes no sense, the default
  expect(memory.child(today).ageMonths).toBeUndefined();
});

test("an age saved again on the admin page as it was keeps the birthday it was guessed from: it goes on growing as before", () => {
  const memory = new Memory(file(), "Anna");
  memory.learn({ age: 3 }, new Date(2026, 0, 6)); // told in January: 3 and a half then
  const later = new Date(2026, 6, 1);
  expect(memory.child(later).ageMonths).toBe(47); // still 3 in July
  expect(memory.edit({ age: "3", name: "Anna" }, later).ageMonths).toBe(47); // not 3 and a half again
  expect(memory.edit({ age: "5" }, later).ageMonths).toBe(66); // a new age, as told
});

test("a grown-up's edit of the file is read as written; what makes no sense is left out, or the default", () => {
  const path = file();
  writeFileSync(path, "# What Cubey knows\n\n- Id: Anna\n- Name: Anna\n- Gender: girl\n- Birthday: 15.01.2023\n- Speaks: xx\n- Learns: en\n- Time zone: Mars/Olympus\n");
  const child = new Memory(path, "other").child(today);
  expect(child).toMatchObject({ id: "Anna", name: "Anna", gender: "girl", language: "en", learns: "fr" });
  expect(child.ageMonths).toBeUndefined();
  expect(child.timeZone).toBe(Intl.DateTimeFormat().resolvedOptions().timeZone);
});

import { expect, test } from "bun:test";
import { ageMonths, ageOf, type Child, momentFor, nameOf, parentsOf, pronouns, speakFor } from "./child";

const anna: Child = { id: "Anna", name: "Anna", ageMonths: 42, timeZone: "Europe/Lisbon", gender: "girl", language: "lv", learns: "en", parents: "mum_dad" };
const unknown: Child = { id: "child", timeZone: "Europe/Lisbon", language: "lv", learns: "en", parents: "mum_dad" };

test("age in whole months, counting the day of the month", () => {
  expect(ageMonths("2023-01-15", new Date(2026, 9, 6))).toBe(44);
  expect(ageMonths("2023-01-06", new Date(2026, 9, 6))).toBe(45);
  expect(ageMonths("2023-01-07", new Date(2026, 9, 6))).toBe(44);
});

test("how the prompts name the child and speak of them: unknown, the child, with no gender guessed", () => {
  expect(nameOf(anna)).toBe("Anna");
  expect(nameOf(unknown)).toBe("the child");
  expect(pronouns(anna)).toMatchObject({ she: "she", her: "her", them: "her", hers: "hers", She: "She" });
  expect(pronouns({ ...anna, gender: "boy" })).toMatchObject({ she: "he", her: "his", them: "him", hers: "his", Her: "His" });
  expect(pronouns(unknown)).toMatchObject({ she: "the child", her: "the child's", them: "the child", She: "The child" });
});

test("her family in Cubey's English instructions", () => {
  expect(parentsOf(anna)).toBe("Mum or Dad");
  expect(parentsOf({ ...anna, parents: "mums" })).toBe("her mums");
  expect(parentsOf({ ...anna, gender: "boy", parents: "dads" })).toBe("his dads");
  expect(parentsOf({ ...unknown, parents: "dads" })).toBe("the child's dads");
});

test("her age as every prompt says it, and how much Cubey says to a child of it; not known, as to a 4-year-old", () => {
  expect(ageOf(anna)).toBe("3 years and 6 months");
  expect(ageOf({ ...anna, ageMonths: 48 })).toBe("4 years");
  expect(ageOf({ ...anna, ageMonths: 25 })).toBe("2 years and 1 month");
  expect(ageOf(unknown)).toBeUndefined();
  expect(speakFor(anna)).toContain("One or two short sentences");
  expect(speakFor(anna)).toContain("a 3-year-old");
  expect(speakFor({ ...anna, ageMonths: 60 })).toContain("Two or three short sentences");
  expect(speakFor({ ...anna, ageMonths: 75 })).toContain("a new word now and then");
  expect(speakFor(unknown)).toContain("a 4-year-old");
});

test("the moment where she lives, as every prompt says it: the weekday, the part of the day, the time", () => {
  const evening = new Date("2026-10-07T19:40:00Z"); // a Wednesday
  expect(momentFor(anna, evening)).toBe("Wednesday evening, 20:40");
  expect(momentFor({ ...anna, timeZone: "Europe/Riga" }, evening)).toBe("Wednesday night, 22:40");
  expect(momentFor(anna, new Date("2026-10-08T06:05:00Z"))).toBe("Thursday morning, 07:05");
  expect(momentFor(anna, new Date("2026-10-08T12:30:00Z"))).toBe("Thursday afternoon, 13:30");
  expect(momentFor(anna, new Date("2026-10-08T02:00:00Z"))).toBe("Thursday night, 03:00");
});

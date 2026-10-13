import { expect, test } from "bun:test";
import { existsSync } from "node:fs";
import { ACTIVITIES, CURRICULUM, idea, pictureFile, PICTURE_SIZES, TYPES, UNITS } from "./curriculum";

test("the curriculum: ideas in order, unit by unit, each once, simple before harder", () => {
  expect(CURRICULUM.length).toBeGreaterThan(250);
  expect(new Set(CURRICULUM.map((i) => i.id)).size).toBe(CURRICULUM.length);
  expect(CURRICULUM[0]).toMatchObject({ id: "hello", unit: "hello", stage: 1 });
  expect(idea("cat")).toMatchObject({ en: "cat", unit: "pets", activity: "touch", picture: "Cat" });
  const stages = CURRICULUM.map((i) => i.stage);
  expect(stages).toEqual([...stages].sort((a, b) => a - b));
  const ages = UNITS.map((u) => u.from);
  expect(ages).toEqual([...ages].sort((a, b) => a - b)); // younger children's units first
});

test("every unit has a few ideas and a known activity; every idea a known type", () => {
  for (const unit of UNITS) {
    expect(unit.ideas.length).toBeGreaterThanOrEqual(3);
    expect(ACTIVITIES).toContain(unit.activity);
  }
  for (const i of CURRICULUM) expect(TYPES).toContain(i.type);
});

test("every picture is there, at every size the device shows", () => {
  for (const i of CURRICULUM)
    if (i.picture) for (const size of PICTURE_SIZES) expect(existsSync(pictureFile(i.picture, size)), `${i.picture} ${size}`).toBe(true);
});

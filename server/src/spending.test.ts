import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { costOf } from "./prices";
import { Spending } from "./spending";

test("what a model's tokens cost, cached ones for less; audio by the second; a model with no price, nothing, but its tokens kept", () => {
  expect(costOf({ what: "think", model: "gpt-6-sol", input: 1_000_000, cached: 0, output: 1_000_000 })).toBeCloseTo(12);
  expect(costOf({ what: "think", model: "gpt-6-sol", input: 1_000_000, cached: 1_000_000, output: 0 })).toBeCloseTo(0.2); // all of it cached
  expect(costOf({ what: "listen", seconds: 3600 })).toBeCloseTo(0.12);
  expect(costOf({ what: "speak", seconds: 3600 })).toBeCloseTo(0.7);
  expect(costOf({ what: "think", model: "who-knows", input: 1000, output: 1000 })).toBe(0);
  expect(costOf({ what: "live", usd: 0.25 })).toBe(0.25); // Gemini Live's, as it counted
});

test("each Cubey's spending: today, this month, all of it, and by what it was for", () => {
  const spending = new Spending(new Database(":memory:"));
  const at = new Date("2026-10-12T12:00:00Z");
  spending.add("a", { what: "think", model: "gpt-6-sol", input: 500_000, cached: 0, output: 100_000 }, at);
  spending.add("a", { what: "listen", seconds: 1800 }, at);
  spending.add("a", { what: "think", model: "gpt-6-sol", input: 1_000_000, output: 0 }, new Date("2026-09-30T12:00:00Z"));
  spending.add("b", { what: "speak", seconds: 3600 }, at);
  const a = spending.of("a", at);
  expect(a.today).toBeCloseTo(1 + 1 + 0.06);
  expect(a.month).toBeCloseTo(2.06);
  expect(a.total).toBeCloseTo(4.06);
  expect(a.tokens).toEqual({ input: 1_500_000, output: 100_000 });
  expect(a.by.think).toBeCloseTo(4);
  expect(a.by.listen).toBeCloseTo(0.06);
  expect(spending.of("b", at).today).toBeCloseTo(0.7);
});

test("where it went, since a day: each day's, each Cubey's, each kind's and model's, with its tokens and seconds", () => {
  const spending = new Spending(new Database(":memory:"));
  const at = new Date("2026-10-12T12:00:00Z");
  spending.add("a", { what: "think", model: "gpt-6-sol", input: 500_000, cached: 100_000, output: 100_000 }, at);
  spending.add("a", { what: "think", model: "gpt-6-sol", input: 500_000, output: 0 }, new Date("2026-10-12T13:00:00Z"));
  spending.add("a", { what: "listen", seconds: 1800 }, at);
  spending.add("b", { what: "think", model: "gemini-3.5-flash-lite", input: 1_000_000, output: 0 }, new Date("2026-10-11T08:00:00Z"));
  spending.add("b", { what: "speak", seconds: 60 }, new Date("2026-09-01T08:00:00Z")); // before
  const rows = spending.breakdown("2026-10-11");
  expect(rows).toHaveLength(3);
  expect(rows[0]).toMatchObject({ day: "2026-10-11", device: "b", what: "think", model: "gemini-3.5-flash-lite", input: 1_000_000 });
  expect(rows.find((r) => r.what === "think" && r.device === "a")).toMatchObject({ day: "2026-10-12", input: 1_000_000, cached: 100_000, output: 100_000 });
  expect(rows.find((r) => r.what === "listen")).toMatchObject({ model: "", seconds: 1800 });
});

import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { ago, earlierBlock, History } from "./history";

const now = new Date("2026-10-07T18:00:00Z");
const minutes = (n: number) => new Date(now.getTime() - n * 60000);

test("each child's talks are kept, and the latest come back oldest first", () => {
  const history = new History(new Database(":memory:"));
  history.add("Anna", "Kāpēc debesis ir zilas?", "Saule…", minutes(60));
  history.add("Anna", "Un kāpēc?", "Gaiss…", minutes(59));
  history.add("Ben", "Kas ir mēness?", "Akmens…", minutes(57));
  expect(history.recent("Anna").map((e) => e.heard)).toEqual(["Kāpēc debesis ir zilas?", "Un kāpēc?"]);
  expect(history.recent("Anna", 1)).toEqual([{ at: minutes(59).toISOString(), heard: "Un kāpēc?", said: "Gaiss…" }]);
});

test("how long ago, as a prompt says it", () => {
  expect(ago(minutes(1).toISOString(), now)).toBe("just now");
  expect(ago(minutes(25).toISOString(), now)).toBe("25 minutes ago");
  expect(ago(minutes(60).toISOString(), now)).toBe("1 hour ago");
  expect(ago(minutes(5 * 60).toISOString(), now)).toBe("5 hours ago");
  expect(ago(minutes(30 * 60).toISOString(), now)).toBe("yesterday");
  expect(ago(minutes(4 * 24 * 60).toISOString(), now)).toBe("4 days ago");
});

test("the block of earlier talks in a prompt: her words, what happened, Cubey's, each cut short", () => {
  const block = earlierBlock([
    { at: minutes(30 * 60).toISOString(), heard: "(Anna has just woken you.)", said: "Čau!" },
    { at: minutes(30 * 60).toISOString(), heard: "Kāpēc?", said: "x".repeat(400) },
    { at: minutes(25).toISOString(), heard: "", said: "Vai tu vēl esi šeit?" },
  ], "Anna", now);
  expect(block).toContain('- yesterday. (Anna has just woken you.) You: "Čau!"');
  expect(block).toContain('- yesterday. Anna: "Kāpēc?" You: "' + "x".repeat(159) + '…"');
  expect(block).toContain('- 25 minutes ago. Anna said nothing. You: "Vai tu vēl esi šeit?"');
  expect(earlierBlock([], "Anna", now)).toBe("");
});

import { expect, test } from "bun:test";
import { completeWith, retried, Refusal, streamedText } from "./llm";

const unavailable = () => Object.assign(new Error('{"error":{"code":503,"status":"UNAVAILABLE"}}'), { status: 503 });

test("a busy model is asked again, a little later, a couple of times", async () => {
  let calls = 0;
  const waits: number[] = [];
  const answer = await retried(async () => (++calls < 3 ? Promise.reject(unavailable()) : "ok"), [1, 2], (ms) => (waits.push(ms), Promise.resolve()));
  expect(answer).toBe("ok");
  expect(waits).toEqual([1, 2]);
});

test("after the last try the error stands; a refusal or a bad request is not asked again", async () => {
  const noWait = () => Promise.resolve();
  let calls = 0;
  await expect(retried(async () => (calls++, Promise.reject(unavailable())), [1, 2], noWait)).rejects.toThrow("UNAVAILABLE");
  expect(calls).toBe(3);
  calls = 0;
  await expect(retried(async () => (calls++, Promise.reject(new Refusal("no"))), [1, 2], noWait)).rejects.toThrow("no");
  await expect(retried(async () => (calls++, Promise.reject(Object.assign(new Error("bad"), { status: 400 }))), [1, 2], noWait)).rejects.toThrow("bad");
  expect(calls).toBe(2);
  const overloaded = Object.assign(new Error("Unavailable"), { status: 503 });
  calls = 0;
  await retried(async () => (++calls < 2 ? Promise.reject(overloaded) : "ok"), [1], noWait);
  expect(calls).toBe(2);
});

test("once its words have gone on to be spoken, a model that fails isn't asked again: they would be said twice", async () => {
  let calls = 0, spoken = false;
  const busy = async () => (calls++, (spoken = true), Promise.reject(unavailable()));
  await expect(retried(busy, [1, 2], () => Promise.resolve(), () => !spoken)).rejects.toThrow("UNAVAILABLE");
  expect(calls).toBe(1);
});

test("OpenAI's answer as it streams: its lines and its letters' bytes may break anywhere between pieces", async () => {
  const events = 'data: {"choices":[{"delta":{"content":"Ābols"}}]}\n\ndata: {"choices":[{"delta":{"content":"! Tev uz s."}}]}\n\n'
    + 'data: {"choices":[{"delta":{},"finish_reason":"stop"}]}\n\n'
    + 'data: {"choices":[],"usage":{"prompt_tokens":1200,"completion_tokens":40,"prompt_tokens_details":{"cached_tokens":1024}}}\n\ndata: [DONE]\n\n';
  const bytes = new TextEncoder().encode(events);
  const pieces = [bytes.slice(0, 40), bytes.slice(40, 41), bytes.slice(41, 70), bytes.slice(70)]; // 40: inside Ā
  const seen: string[] = [];
  const read = await streamedText((async function* () { yield* pieces; })(), (text) => seen.push(text));
  expect(read).toEqual({ text: "Ābols! Tev uz s.", finish: "stop", tokens: { input: 1200, cached: 1024, output: 40 } });
  expect(seen).toEqual(["Ābols", "! Tev uz s."]);
});

test("OpenAI's answer through Cubey's model, without JSON mode: it waited 180 ms more for the first word, and the talk's history as JSON keeps it to JSON (cascade.ts)", async () => {
  const fetched = globalThis.fetch;
  let body: { response_format?: { type: string }; reasoning_effort?: string; stream_options?: object } = {};
  globalThis.fetch = (async (_: unknown, init: { body: string }) => {
    body = JSON.parse(init.body);
    return new Response('data: {"choices":[{"delta":{"content":"{\\"say\\": \\"Čau!\\"}"}}]}\n\ndata: {"choices":[],"usage":{"prompt_tokens":900,"completion_tokens":12}}\n\ndata: [DONE]\n\n');
  }) as unknown as typeof fetch;
  try {
    const spent: object[] = [];
    expect(await completeWith("gpt-6-sol")({ system: "Answer with JSON only.", messages: [{ role: "user", text: "Čau" }], spent: (s) => spent.push(s) })).toBe('{"say": "Čau!"}');
    expect(body.response_format).toBeUndefined();
    expect(body.stream_options).toEqual({ include_usage: true }); // what it cost, for the admin page
    expect(spent).toEqual([{ what: "think", model: "gpt-6-sol", input: 900, cached: 0, output: 12 }]);
    expect(body.reasoning_effort).toBe("none");
    await completeWith("gpt-6-astra")({ system: "", messages: [{ role: "user", text: "Kaķis" }] });
    expect(body.reasoning_effort).toBe("low"); // its least: it takes no "none"
  } finally {
    globalThis.fetch = fetched;
  }
});

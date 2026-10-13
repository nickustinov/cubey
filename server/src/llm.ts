// A model behind Cubey's answers when it hears through Soniox (cascade.ts), streaming, thinking off: OpenAI's or
// Gemini's, each job's as chosen on the admin page (models.ts). On Cubey's prompt and her real turns (2026-10-11) GPT-6
// Sol's first word came 0.3 s after Gemini 3.1 Flash-Lite's, as good in Latvian, but it handed her cough to her parents
// and kept the word chain's rules, where Flash-Lite praised every word on a wrong letter; Haiku 5.5's Latvian was poor,
// 3.8 Flash took 2 s. Of ten models drawing ten things on its 24 by 24 dots (2026-10-12), GPT-6 Astra's were the best
// (in about 6 s); GPT-6 Sol drew a heart for a fish, Flash-Lite a blob for a cat.
// Gemini's terms forbid apps used by under-18s: an open question before any child talks to Cubey (docs/design.md).
import { GoogleGenAI, ThinkingLevel } from "@google/genai";
import type { Spent } from "./prices";

// How little OpenAI's models may reason: none, but for those that take no "none".
const LEAST_EFFORT: Record<string, string> = { "gpt-6-astra": "low" };

export type Message = { role: "user" | "assistant"; text: string };
// onText: the answer as it is written, piece by piece (to speak it before it is all written); spent: its tokens, once
// it is done (what it cost: prices.ts).
export type Complete = (opts: { system: string; messages: Message[]; onText?: (text: string) => void; spent?: (spent: Spent) => void }) => Promise<string>;
type Tokens = { input: number; cached: number; output: number };
export class Refusal extends Error {}

const MAX_TOKENS = 1000; // a few short sentences for a small child

let gemini: GoogleGenAI | undefined;

async function flash(model: string, system: string, messages: Message[], onText?: (text: string) => void, onTokens?: (t: Tokens) => void): Promise<string> {
  gemini ??= new GoogleGenAI({ apiKey: process.env.GEMINI_API_KEY });
  const stream = await gemini.models.generateContentStream({
    model,
    contents: messages.map((m) => ({ role: m.role === "assistant" ? "model" : "user", parts: [{ text: m.text }] })),
    config: {
      systemInstruction: system,
      maxOutputTokens: MAX_TOKENS,
      // Thinking off: Flash takes a zero budget, Flash Lite only a level.
      thinkingConfig: model.endsWith("-lite") ? { thinkingLevel: ThinkingLevel.MINIMAL } : { thinkingBudget: 0 },
    },
  });
  let text = "", finish = "", tokens: Tokens | undefined;
  for await (const chunk of stream) {
    const piece = chunk.text ?? "";
    text += piece;
    if (piece) onText?.(piece);
    finish = String(chunk.candidates?.[0]?.finishReason ?? finish);
    const used = chunk.usageMetadata; // the last chunk's, all of it
    if (used) tokens = { input: used.promptTokenCount ?? 0, cached: used.cachedContentTokenCount ?? 0, output: (used.candidatesTokenCount ?? 0) + (used.thoughtsTokenCount ?? 0) };
  }
  if (tokens) onTokens?.(tokens);
  if (["SAFETY", "PROHIBITED_CONTENT", "BLOCKLIST", "SPII"].includes(finish)) throw new Refusal(`Gemini stopped: ${finish}`);
  return text;
}

async function openai(model: string, system: string, messages: Message[], onText?: (text: string) => void, onTokens?: (t: Tokens) => void): Promise<string> {
  const response = await fetch("https://api.openai.com/v1/chat/completions", {
    method: "POST",
    headers: { "content-type": "application/json", authorization: `Bearer ${process.env.OPENAI_API_KEY}` },
    body: JSON.stringify({
      model, stream: true, stream_options: { include_usage: true }, max_completion_tokens: MAX_TOKENS, reasoning_effort: LEAST_EFFORT[model] ?? "none",
      messages: [{ role: "system", content: system }, ...messages.map((m) => ({ role: m.role, content: m.text }))],
    }),
  });
  if (!response.ok || !response.body) throw Object.assign(new Error(`OpenAI: ${response.status} ${await response.text()}`), { status: response.status });
  const { text, finish, tokens } = await streamedText(response.body, onText);
  if (tokens) onTokens?.(tokens);
  if (finish === "content_filter") throw new Refusal("OpenAI stopped: content_filter");
  return text;
}

// OpenAI's answer from its stream of server-sent events, piece by piece as it comes; where it stopped, and why; its
// tokens (the last event's, asked for with include_usage).
export async function streamedText(body: AsyncIterable<Uint8Array>, onText?: (text: string) => void) {
  const decoder = new TextDecoder(); // one, streaming: a letter's bytes may come in two pieces
  let text = "", finish = "", rest = "", tokens: Tokens | undefined;
  for await (const bytes of body) {
    const lines = (rest + decoder.decode(bytes, { stream: true })).split("\n");
    rest = lines.pop()!;
    for (const line of lines) {
      if (!line.startsWith("data: ") || line === "data: [DONE]") continue;
      const event = JSON.parse(line.slice(6)), used = event.usage;
      if (used) tokens = { input: used.prompt_tokens ?? 0, cached: used.prompt_tokens_details?.cached_tokens ?? 0, output: used.completion_tokens ?? 0 };
      const choice = event.choices?.[0];
      const piece: string = choice?.delta?.content ?? "";
      text += piece;
      if (piece) onText?.(piece);
      finish = choice?.finish_reason ?? finish;
    }
  }
  return { text, finish, ...(tokens && { tokens }) };
}

// A model that is busy for a moment (Gemini's 503 UNAVAILABLE, 429) is asked again
// after each of `delays`; short, since a child is waiting. A refusal or a bad request stands, and so does a
// failure once `again` says no (the answer's first words have gone on to be spoken).
const RETRY_DELAYS = [250, 750];
const transient = (e: unknown) => {
  if (e instanceof Refusal) return false;
  const status = Number((e as { status?: unknown })?.status ?? (e as { code?: unknown })?.code);
  return [429, 500, 502, 503, 504].includes(status) || /UNAVAILABLE|RESOURCE_EXHAUSTED/i.test(String(e));
};

export async function retried<T>(attempt: () => Promise<T>, delays = RETRY_DELAYS, wait = (ms: number) => Bun.sleep(ms), again = () => true): Promise<T> {
  for (let i = 0; ; i++) {
    try {
      return await attempt();
    } catch (e) {
      if (i >= delays.length || !transient(e) || !again()) throw e;
      await wait(delays[i]);
    }
  }
}

// The model as `model` says at each call: a choice on the admin page counts from the next turn.
export function completeWith(model: () => string): Complete {
  return ({ system, messages, onText, spent }) => {
    const id = model();
    let written = false;
    const write = onText && ((text: string) => ((written = true), onText(text)));
    const ask = id.startsWith("gpt-") ? openai : flash;
    return retried(() => ask(id, system, messages, write, spent && ((t) => spent({ what: "think", model: id, ...t }))), RETRY_DELAYS, undefined, () => !written);
  };
}

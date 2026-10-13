// What each service charges, in US dollars, as found on 2026-10-12 (third parties' listings where the providers' own
// pages weren't found: check them). A model's, per 1M tokens: its input, the input it had cached (part of the input),
// its output (thinking billed as output).
export const MODEL_PRICES: Record<string, { input: number; cached: number; output: number }> = {
  "gpt-6-sol": { input: 2, cached: 0.2, output: 10 },
  "gpt-6.1-sol": { input: 2, cached: 0.1, output: 10 },
  "gpt-6-astra": { input: 10, cached: 1, output: 50 },
  "gemini-3.5-flash-lite": { input: 0.3, cached: 0.03, output: 2.5 },
};
// Soniox's, a second of audio: real-time listening about $0.12 an hour, speaking about $0.70 (soniox.com/compare).
export const AUDIO_PRICES = { listen: 0.12 / 3600, speak: 0.7 / 3600 };

// What a Cubey spent on one thing: a model's thinking (its tokens), listening or speaking (seconds of audio), or a
// connection to Gemini Live (its cost, as live.ts counts it).
export type Spent = {
  what: "think" | "listen" | "speak" | "live"; model?: string; input?: number; cached?: number; output?: number; seconds?: number; usd?: number;
};

// In dollars; a model with no price, nothing (its tokens are kept all the same).
export function costOf(spent: Spent): number {
  if (spent.usd !== undefined) return spent.usd;
  if (spent.what === "listen" || spent.what === "speak") return (spent.seconds ?? 0) * AUDIO_PRICES[spent.what];
  const price = MODEL_PRICES[spent.model ?? ""];
  if (!price) return 0;
  const input = spent.input ?? 0, cached = Math.min(spent.cached ?? 0, input);
  return ((input - cached) * price.input + cached * price.cached + (spent.output ?? 0) * price.output) / 1e6;
}

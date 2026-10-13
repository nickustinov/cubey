// How long Cubey takes to answer, through the server as a device hears it: a pretend device opens a talk, waits for
// Cubey's hello, then streams a question (spoken by Gemini's TTS) at the pace of a mic in 20 ms packets, then the mic's
// silence; from her last word to Cubey's first sound. The server must be running (bun run dev), as CUBEY_TALK has it.
//   bun run latency [runs] ["question"]
import { GoogleGenAI, Modality } from "@google/genai";
import { Resampler } from "../src/resample";

const runs = Number(process.argv[2] ?? 3);
const QUESTION = process.argv[3] ?? "Kubij, kāpēc debesis ir zilas?";
const SERVER = `ws://127.0.0.1:${process.env.PORT ?? 8788}/device`;
const RATE = 16000, PACKET = (RATE / 50) * 2; // 20 ms of 16-bit samples

// Her question at the mic's rate, and where in it her last word ends.
async function spoken(text: string): Promise<{ pcm: Buffer; end: number }> {
  const gemini = new GoogleGenAI({ apiKey: process.env.GEMINI_API_KEY });
  const res = await gemini.models.generateContent({
    model: "gemini-3.8-flash-tts",
    contents: [{ parts: [{ text }] }],
    config: { responseModalities: [Modality.AUDIO], speechConfig: { voiceConfig: { prebuiltVoiceConfig: { voiceName: "Leda" } } } },
  });
  const data = res.candidates?.[0]?.content?.parts?.find((p) => p.inlineData?.data)?.inlineData?.data;
  if (!data) throw new Error("no speech from the TTS");
  const pcm = new Resampler(24000, RATE).push(Buffer.from(data, "base64"));
  let end = pcm.length - 2;
  while (end > 0 && Math.abs(pcm.readInt16LE(end)) < 300) end -= 2;
  return { pcm, end };
}

// One question: ms from the packet with her last word to Cubey's first sound after it.
async function once({ pcm, end }: { pcm: Buffer; end: number }): Promise<number> {
  const ws = new WebSocket(SERVER);
  ws.binaryType = "arraybuffer";
  let saidAt = 0, sounded = 0, asking = false;
  ws.onmessage = (e) => {
    if (typeof e.data === "string") {
      if (JSON.parse(e.data).type === "said") saidAt = performance.now();
    } else if (asking && !sounded) sounded = performance.now();
  };
  await new Promise((resolve) => (ws.onopen = resolve));
  ws.send(JSON.stringify({ type: "hello", speaker_rate: RATE }));
  ws.send(JSON.stringify({ type: "open" }));
  while (!saidAt) await Bun.sleep(20); // Cubey's hello, played
  ws.send(JSON.stringify({ type: "listen" }));
  await Bun.sleep(300);
  asking = true;
  const start = performance.now();
  let ended = 0;
  for (let at = 0, n = 0; !sounded && performance.now() - start < 15000; at += PACKET, n++) {
    ws.send(at < pcm.length ? pcm.subarray(at, at + PACKET) : Buffer.alloc(PACKET)); // then the mic's silence
    if (!ended && at + PACKET > end) ended = performance.now();
    await Bun.sleep(Math.max(0, start + (n + 1) * 20 - performance.now()));
  }
  const heardAt = saidAt;
  while (sounded && saidAt === heardAt && performance.now() - start < 30000) await Bun.sleep(20); // all of it, for the server's log
  ws.send(JSON.stringify({ type: "close" }));
  ws.close();
  return sounded ? Math.round(sounded - ended) : NaN;
}

const question = await spoken(QUESTION);
console.log(`"${QUESTION}": ${(question.end / 2 / RATE).toFixed(1)} s of speech, ${runs} runs`);
const times: number[] = [];
for (let i = 0; i < runs; i++) times.push(await once(question));
const sorted = [...times].sort((a, b) => a - b);
console.log(`last word → first sound: ${times.join(", ")} ms (median ${sorted[Math.floor(sorted.length / 2)]})`);

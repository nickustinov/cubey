import { expect, test } from "bun:test";
import { Resampler } from "./resample";

const tone = (hz: number, rate: number, ms: number) => {
  const pcm = Buffer.alloc(((rate * ms) / 1000) * 2);
  for (let i = 0; i < pcm.length / 2; i++) pcm.writeInt16LE(Math.round(10000 * Math.sin((2 * Math.PI * hz * i) / rate)), i * 2);
  return pcm;
};
// The loudest sample, past the edges.
const peak = (pcm: Buffer) => {
  let most = 0;
  for (let i = 100; i < pcm.length / 2 - 100; i++) most = Math.max(most, Math.abs(pcm.readInt16LE(i * 2)));
  return most;
};

test("speech at 24 kHz plays at the board's 16 kHz: two samples for three, the voice kept, what 16 kHz can't hold left out", () => {
  const voice = new Resampler(24000, 16000).push(tone(1000, 24000, 100));
  expect(voice.length / 2).toBeGreaterThan(1580);
  expect(voice.length / 2).toBeLessThanOrEqual(1600);
  expect(peak(voice)).toBeGreaterThan(9500);
  expect(peak(voice)).toBeLessThan(10500);
  expect(peak(new Resampler(24000, 16000).push(tone(10000, 24000, 100)))).toBeLessThan(500); // it would fold back to 6 kHz
});

test("chunk by chunk as it comes, the same as all at once; at the same rate it passes as it is", () => {
  const speech = tone(440, 24000, 60);
  const whole = new Resampler(24000, 16000).push(speech);
  const streamed = new Resampler(24000, 16000);
  const chunks = [speech.subarray(0, 1000), speech.subarray(1000, 1002), speech.subarray(1002)];
  expect(Buffer.concat(chunks.map((c) => streamed.push(c))).equals(whole)).toBe(true);
  expect(new Resampler(24000, 24000).push(speech)).toEqual(speech);
});

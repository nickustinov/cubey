// 16-bit speech from one sample rate to another, chunk by chunk as it streams (Gemini speaks at 24 kHz, the board
// plays 16 kHz): windowed-sinc interpolation, low-passed a little below the lower rate's half.
const HALF = 16; // input samples on either side of each output sample

// The filter at x input samples from the output sample; cutoff: of the input's half rate.
function kernel(x: number, cutoff: number) {
  if (Math.abs(x) >= HALF) return 0;
  const sinc = x === 0 ? 1 : Math.sin(Math.PI * cutoff * x) / (Math.PI * cutoff * x);
  return cutoff * sinc * 0.5 * (1 + Math.cos((Math.PI * x) / HALF));
}

export class Resampler {
  private input: number[] = new Array(HALF).fill(0); // the samples still needed, after silence before the first
  private at = HALF; // where the next output sample falls in input
  private step: number;
  private cutoff: number;

  constructor(private from: number, private to: number) {
    this.step = from / to;
    this.cutoff = 0.9 * Math.min(1, to / from);
  }

  push(pcm: Buffer): Buffer {
    if (this.from === this.to) return pcm;
    for (let i = 0; i + 1 < pcm.length; i += 2) this.input.push(pcm.readInt16LE(i));
    const out: number[] = [];
    for (; this.at + HALF < this.input.length; this.at += this.step) {
      const centre = Math.floor(this.at);
      let sum = 0;
      for (let k = centre - HALF + 1; k <= centre + HALF; k++) sum += this.input[k] * kernel(this.at - k, this.cutoff);
      out.push(sum);
    }
    const used = Math.floor(this.at) - HALF;
    if (used > 0) {
      this.input.splice(0, used);
      this.at -= used;
    }
    const bytes = Buffer.alloc(out.length * 2);
    out.forEach((v, i) => bytes.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(v))), i * 2));
    return bytes;
  }
}

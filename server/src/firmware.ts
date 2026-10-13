// Cubey's firmware for updates over the air (session.ts offers it, the device fetches it from /firmware/<build>.bin):
// the last build uploaded (admin.ts, scripts/release.ts), kept in the data folder. What it is, it says itself: ESP-IDF
// puts an app description (esp_app_desc_t) at byte 32 of every app image, with the version (git describe), the project's
// name and the hash of the ELF it was made from, the start of which is its build, unique to it, as the device tells it.
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export type Build = { version: string; build: string; built: string };
export type Kept = Build & { size: number; uploaded: string };

const APP_DESC = 32, MAGIC = 0xabcd5432;
const text = (bin: Uint8Array, at: number, length: number) => {
  const bytes = bin.subarray(at, at + length);
  const end = bytes.indexOf(0);
  return new TextDecoder().decode(end < 0 ? bytes : bytes.subarray(0, end));
};

// Cubey's firmware, as its app description says; anything else (another project's, no image), undefined.
export function describeImage(bin: Uint8Array): Build | undefined {
  if (bin.length < APP_DESC + 256 || bin[0] !== 0xe9) return undefined;
  const view = new DataView(bin.buffer, bin.byteOffset, bin.byteLength);
  if (view.getUint32(APP_DESC, true) !== MAGIC || text(bin, APP_DESC + 48, 32) !== "cubey") return undefined;
  return {
    version: text(bin, APP_DESC + 16, 32),
    build: Buffer.from(bin.subarray(APP_DESC + 144, APP_DESC + 152)).toString("hex"),
    built: `${text(bin, APP_DESC + 96, 16)} ${text(bin, APP_DESC + 80, 16)}`,
  };
}

export class Firmware {
  constructor(private dir: string) {}

  current(): Kept | undefined {
    const about = join(this.dir, "cubey.json");
    return existsSync(about) ? JSON.parse(readFileSync(about, "utf8")) : undefined;
  }

  // An upload: kept if it is Cubey's firmware, in place of the one before (written aside first, so a device never
  // fetches half of it).
  keep(bin: Uint8Array, now = new Date()): Kept | undefined {
    const build = describeImage(bin);
    if (!build) return undefined;
    const kept: Kept = { ...build, size: bin.length, uploaded: now.toISOString() };
    mkdirSync(this.dir, { recursive: true });
    writeFileSync(join(this.dir, "cubey.bin.new"), bin);
    renameSync(join(this.dir, "cubey.bin.new"), join(this.dir, "cubey.bin"));
    writeFileSync(join(this.dir, "cubey.json"), JSON.stringify(kept));
    return kept;
  }

  // Its file, for the build kept now only.
  file(build: string): string | undefined {
    return this.current()?.build === build ? join(this.dir, "cubey.bin") : undefined;
  }
}

// /firmware/<build>.bin: the build kept now, for a device to fetch (server.ts admits it first); anything else under
// /firmware/, not here; any other path, not this one's.
export function firmwareResponse(firmware: Firmware, path: string): Response | undefined {
  if (!path.startsWith("/firmware/")) return undefined;
  const file = firmware.file(/^\/firmware\/([0-9a-f]{16})\.bin$/.exec(path)?.[1] ?? "");
  return file ? new Response(Bun.file(file), { headers: { "content-type": "application/octet-stream" } }) : new Response("Not here.\n", { status: 404 });
}

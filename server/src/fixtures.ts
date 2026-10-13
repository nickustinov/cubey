// What tests share: an app image as ESP-IDF builds one, its header, then its app description (esp_app_desc_t) at byte
// 32 (firmware.ts).
export function firmwareImage(version: string, sha = "0123456789abcdef0011", project = "cubey"): Uint8Array {
  const bin = new Uint8Array(4096);
  const text = (at: number, s: string) => bin.set(new TextEncoder().encode(s), at);
  bin[0] = 0xe9;
  new DataView(bin.buffer).setUint32(32, 0xabcd5432, true);
  text(48, version);
  text(80, project);
  text(112, "13:30:00");
  text(128, "Oct 13 2026");
  bin.set(Buffer.from(sha, "hex"), 176);
  return bin;
}

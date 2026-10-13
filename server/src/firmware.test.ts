import { expect, test } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { describeImage, Firmware, firmwareResponse } from "./firmware";
import { firmwareImage as image } from "./fixtures";

const now = new Date("2026-10-13T12:00:00Z");

test("a build of Cubey's firmware: its version, its own build (the start of its ELF's hash) and when, from inside it", () => {
  expect(describeImage(image("a1b92ab-dirty"))).toEqual({ version: "a1b92ab-dirty", build: "0123456789abcdef", built: "Oct 13 2026 13:30:00" });
});

test("anything else is no firmware of Cubey's: another project's, no app description, too short", () => {
  expect(describeImage(image("v1", undefined, "puce"))).toBeUndefined();
  const plain = image("v1");
  plain[33] = 0;
  expect(describeImage(plain)).toBeUndefined();
  expect(describeImage(new Uint8Array(100))).toBeUndefined();
});

test("the build kept on the server: the last uploaded, its size and when; there after a restart; its file only by its build", () => {
  const dir = mkdtempSync(join(tmpdir(), "cubey-firmware-"));
  const firmware = new Firmware(dir);
  expect(firmware.current()).toBeUndefined();
  expect(firmware.keep(new Uint8Array(10), now)).toBeUndefined(); // not firmware: nothing kept
  const kept = firmware.keep(image("a1b92ab"), now)!;
  expect(kept).toEqual({ version: "a1b92ab", build: "0123456789abcdef", built: "Oct 13 2026 13:30:00", size: 4096, uploaded: now.toISOString() });
  expect(new Firmware(dir).current()).toEqual(kept);
  expect(firmware.file("0123456789abcdef")).toBe(join(dir, "cubey.bin"));
  expect(firmware.file("fedcba9876543210")).toBeUndefined(); // not the one kept now
  firmware.keep(image("b2c3d4e", "fedcba98765432100011"), now);
  expect(firmware.current()?.version).toBe("b2c3d4e");
});

test("a device fetches the build kept, by its build, from /firmware/<build>.bin; any other, not here", async () => {
  const firmware = new Firmware(mkdtempSync(join(tmpdir(), "cubey-firmware-")));
  expect(firmwareResponse(firmware, "/health")).toBeUndefined(); // not a firmware path
  expect(firmwareResponse(firmware, "/firmware/0123456789abcdef.bin")?.status).toBe(404); // nothing kept
  firmware.keep(image("a1b92ab"), now);
  const res = firmwareResponse(firmware, "/firmware/0123456789abcdef.bin")!;
  expect(res.status).toBe(200);
  expect(res.headers.get("content-type")).toBe("application/octet-stream");
  expect((await res.arrayBuffer()).byteLength).toBe(4096);
  expect(firmwareResponse(firmware, "/firmware/fedcba9876543210.bin")?.status).toBe(404);
  expect(firmwareResponse(firmware, "/firmware/../cubey.json")?.status).toBe(404);
});

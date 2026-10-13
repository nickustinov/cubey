import { expect, test } from "bun:test";
import { allowed, exposedWithoutToken } from "./access";

test("without a token only this Mac reaches the server; beyond it a token is required", () => {
  expect(exposedWithoutToken("127.0.0.1", undefined)).toBe(false);
  expect(exposedWithoutToken("localhost", undefined)).toBe(false);
  expect(exposedWithoutToken("0.0.0.0", undefined)).toBe(true);
  expect(exposedWithoutToken("0.0.0.0", "")).toBe(true);
  expect(exposedWithoutToken("0.0.0.0", "s3cret")).toBe(false);
});

test("a device on the network must send the token; the simulator on this Mac needs none", () => {
  expect(allowed(null, undefined, "192.168.1.20")).toBe(true); // no token: the server is on this Mac only
  expect(allowed(null, "s3cret", "192.168.1.20")).toBe(false);
  expect(allowed("Bearer nope", "s3cret", "192.168.1.20")).toBe(false);
  expect(allowed("s3cret", "s3cret", "192.168.1.20")).toBe(false);
  expect(allowed("Bearer s3cret", "s3cret", "192.168.1.20")).toBe(true);
  for (const mac of ["127.0.0.1", "::1", "::ffff:127.0.0.1"]) expect(allowed(null, "s3cret", mac)).toBe(true);
  expect(allowed(null, "s3cret", undefined)).toBe(false);
});

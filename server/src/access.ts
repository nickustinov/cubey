// Who may use the server (and with it the paid Gemini key). Without CUBEY_DEVICE_TOKEN the server
// listens on this Mac only. With it, a device on the network must send it; the simulator on this Mac needs none.
import { timingSafeEqual } from "node:crypto";

const LOOPBACK_HOSTS = ["127.0.0.1", "localhost", "::1"];
const LOOPBACK_ADDRESSES = ["127.0.0.1", "::1", "::ffff:127.0.0.1"];

export function exposedWithoutToken(host: string, token: string | undefined): boolean {
  return !LOOPBACK_HOSTS.includes(host) && !token;
}

export function allowed(authorization: string | null, token: string | undefined, address: string | undefined): boolean {
  if (!token || (address && LOOPBACK_ADDRESSES.includes(address))) return true;
  const sent = Buffer.from(authorization ?? "");
  const expected = Buffer.from(`Bearer ${token}`);
  return sent.length === expected.length && timingSafeEqual(sent, expected);
}

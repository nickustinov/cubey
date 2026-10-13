// The board's last build (build/esp32/cubey.bin) to a server, for its Cubeys to update to: asked on its admin page, one
// Cubey or all. Signs in with CUBEY_ADMIN_PASSWORD (server/.env), as the page's upload form.
//   bun run release [server]   (http://127.0.0.1:8788 if none: https://<your-app>.fly.dev for a deployed one)
import { existsSync, readFileSync } from "node:fs";
import { describeImage } from "../src/firmware";

const server = (process.argv[2] ?? "http://127.0.0.1:8788").replace(/\/+$/, "");
const path = new URL("../../build/esp32/cubey.bin", import.meta.url).pathname;
const password = process.env.CUBEY_ADMIN_PASSWORD;
if (!existsSync(path)) throw new Error(`no build at ${path}: idf.py -C firmware/platforms/esp32 -B build/esp32 build`);
if (!password) throw new Error("CUBEY_ADMIN_PASSWORD is not set (server/.env)");
const bin = new Uint8Array(readFileSync(path));
const build = describeImage(bin);
if (!build) throw new Error(`${path} is not Cubey's firmware`);

const signedIn = await fetch(`${server}/admin/login`, { method: "POST", body: new URLSearchParams({ password }), redirect: "manual" });
const cookie = (signedIn.headers.get("set-cookie") ?? "").split(";")[0];
if (signedIn.status !== 303 || !cookie) throw new Error(`${server} didn't take the password (${signedIn.status})`);
const form = new FormData();
form.append("firmware", new Blob([bin]), "cubey.bin");
const uploaded = await fetch(`${server}/admin/firmware`, { method: "POST", headers: { cookie }, body: form, redirect: "manual" });
if (uploaded.status !== 303) throw new Error(`${server} didn't keep it (${uploaded.status})`);
console.log(`${build.version} (build ${build.build}, built ${build.built}, ${Math.round(bin.length / 1024)} KB) is on ${server}: ask its Cubeys to update on ${server}/admin`);

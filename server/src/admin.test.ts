import { Database } from "bun:sqlite";
import { expect, test } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { admin } from "./admin";
import { Fleet } from "./fleet";
import { Models } from "./models";
import { Spending } from "./spending";

const now = new Date("2026-10-12T12:00:00Z");

// keys: the providers' keys the server has, for the models it offers
function setUp(password = "kubiks", keys: Record<string, string> = { GEMINI_API_KEY: "g", OPENAI_API_KEY: "o" }) {
  const db = new Database(":memory:");
  const models = new Models(db, keys);
  const fleet = new Fleet(db, mkdtempSync(join(tmpdir(), "cubey-admin-")), "Europe/Riga");
  const spending = new Spending(db);
  fleet.admit("44bd8d60da70", "k", now);
  fleet.memory("44bd8d60da70").learn({ name: "<script>Anna</script>", gender: "girl", age: 3 }, now);
  fleet.connected("44bd8d60da70", true, now);
  fleet.log("44bd8d60da70").add({ heard: "Kāpēc debesis ir zilas?", said: "Saules gaisma…", forParents: false }, now);
  fleet.log("44bd8d60da70").add({ heard: "No kurienes nāk bērni?", said: "Pajautā mammai!", forParents: true }, now);
  spending.add("44bd8d60da70", { what: "think", model: "gpt-6-sol", input: 1_000_000, output: 100_000 }, now);
  const wiped: string[] = [], changed: string[] = [];
  const handle = admin({ password, fleet, spending, models, wipe: (id) => wiped.push(id), changed: (id) => changed.push(id), now: () => now, wait: async () => {} });
  const get = (path: string, cookie = "") => handle(new Request(`https://cubey.fly.dev${path}`, { headers: { cookie } }));
  const post = (path: string, form: Record<string, string>, cookie = "") =>
    handle(new Request(`https://cubey.fly.dev${path}`, { method: "POST", headers: { cookie }, body: new URLSearchParams(form) }));
  return { db, fleet, spending, models, wiped, changed, get, post };
}

const cookieOf = (res: Response) => (res.headers.get("set-cookie") ?? "").split(";")[0];

test("without CUBEY_ADMIN_PASSWORD there is no admin page", async () => {
  const { get } = setUp("");
  expect((await get("/admin"))?.status).toBe(404);
});

test("its password, once, then a cookie only the server can make (HttpOnly, Secure, SameSite=Strict); a wrong one, again", async () => {
  const { get, post } = setUp();
  const page = await get("/admin");
  expect(page?.status).toBe(200);
  expect(await page!.text()).toContain('type="password"');
  expect((await post("/admin/login", { password: "wrong" }))?.status).toBe(401);
  const ok = (await post("/admin/login", { password: "kubiks" }))!;
  expect(ok.status).toBe(303);
  expect(ok.headers.get("set-cookie")).toContain("HttpOnly");
  expect(ok.headers.get("set-cookie")).toContain("Secure");
  expect(ok.headers.get("set-cookie")).toContain("SameSite=Strict");
  expect(await (await get("/admin", "cubey_admin=forged"))!.text()).toContain('type="password"');
  expect(await (await get("/admin", cookieOf(ok)))!.text()).not.toContain('type="password"');
});

test("the Cubeys: each one's child, online or when last seen, what it cost today, this month and in all; names as text", async () => {
  const { get, post } = setUp();
  const cookie = cookieOf((await post("/admin/login", { password: "kubiks" }))!);
  const html = await (await get("/admin", cookie))!.text();
  expect(html).toContain("&lt;script&gt;Anna&lt;/script&gt;");
  expect(html).not.toContain("<script>Anna");
  expect(html).toContain("44bd8d60da70");
  expect(html).toContain("online");
  expect(html).toContain("$3.00"); // 1M in at $2, 100k out at $10
  expect(html).toContain('href="/admin/d/44bd8d60da70"');
  expect(html).toContain('href="/admin/spending"');
});

test("where the money goes: graphs by day, by Cubey and by kind (listening, speaking, each model's thinking), and a table", async () => {
  const { get, post, spending } = setUp();
  spending.add("44bd8d60da70", { what: "listen", seconds: 3600 }, new Date("2026-10-10T12:00:00Z"));
  spending.add("44bd8d60da70", { what: "think", model: "gemini-3.5-flash-lite", input: 1_000_000, output: 0 }, new Date("2026-10-11T12:00:00Z"));
  spending.add("aabbccddeeff", { what: "speak", seconds: 3600 }, new Date("2026-08-01T12:00:00Z")); // before the 30 days
  expect((await get("/admin/spending"))!.headers.get("content-type")).toContain("text/html"); // the password first
  expect(await (await get("/admin/spending"))!.text()).toContain('type="password"');
  const cookie = cookieOf((await post("/admin/login", { password: "kubiks" }))!);
  const html = await (await get("/admin/spending", cookie))!.text();
  expect(html.match(/<svg/g)?.length).toBe(3);
  for (const kind of ["Soniox listening", "gpt-6-sol", "gemini-3.5-flash-lite"]) expect(html).toContain(kind);
  expect(html).not.toContain("Soniox speaking"); // only before
  expect(html).toContain("&lt;script&gt;Anna&lt;/script&gt;"); // its Cubey, by name
  expect(html).toContain("$3.42"); // 3.00 + 0.12 + 0.30 in the 30 days
  expect(html).toContain("<title>2026-10-11 · gemini-3.5-flash-lite: $0.30</title>");
  expect(await (await get("/admin/spending?days=7", cookie))!.text()).toContain("7 days");
});

test("a Cubey's page: its talks, newest first, the big questions marked; its child and settings changed, its name, blocked", async () => {
  const { get, post, fleet, wiped, changed } = setUp();
  const cookie = cookieOf((await post("/admin/login", { password: "kubiks" }))!);
  const html = await (await get("/admin/d/44bd8d60da70", cookie))!.text();
  expect(html.indexOf("No kurienes nāk bērni?")).toBeLessThan(html.indexOf("Kāpēc debesis ir zilas?"));
  expect(html).toContain("for the parents");
  const saved = await post("/admin/d/44bd8d60da70", {
    label: "Annas Kubijs", name: "Anna", gender: "girl", age: "4", language: "lv", learns: "en", parents: "mum_dad",
    timeZone: "Europe/Riga", brightness: "50", volume: "60", theme: "white", blocked: "on",
  }, cookie);
  expect(saved?.status).toBe(303);
  expect(fleet.get("44bd8d60da70")).toMatchObject({ label: "Annas Kubijs", blocked: true });
  expect(fleet.memory("44bd8d60da70").child(now)).toMatchObject({ name: "Anna", timeZone: "Europe/Riga", brightness: 50, volume: 60, theme: "white" });
  expect(changed).toEqual(["44bd8d60da70"]); // to the Cubey, if it is on: its brightness and theme now
  const edited = await (await get("/admin/d/44bd8d60da70", cookie))!.text();
  expect(edited).toContain('<option value="white" selected>White</option>');
  expect(edited).toContain('<option value="4" selected>4 years</option>'); // an age in years, as Cubey is told it
  expect(edited).not.toContain('type="date"');
  expect(await (await get("/admin", cookie))!.text()).toContain("Anna, girl, 4 years ·");
  expect((await post("/admin/d/44bd8d60da70", { label: "x" }))?.status).toBe(303); // no cookie: to the login
  expect(fleet.get("44bd8d60da70")?.label).toBe("Annas Kubijs");
  expect((await get("/admin/d/nobody", cookie))?.status).toBe(404);
  expect(await (await get("/admin/d/44bd8d60da70", cookie))!.text()).toContain('action="/admin/d/44bd8d60da70/reset"');
  expect((await post("/admin/d/44bd8d60da70/reset", {}, cookie))?.headers.get("location")).toBe("/admin"); // a full reset
  expect(wiped).toEqual(["44bd8d60da70"]); // the Cubey itself, if it is on
  expect(fleet.get("44bd8d60da70")).toBeUndefined();
});

test("the models: a dropdown for each job, those without their key greyed out; a choice is used from the next turn", async () => {
  const { get, post, models } = setUp("kubiks", { GEMINI_API_KEY: "g" });
  const cookie = cookieOf((await post("/admin/login", { password: "kubiks" }))!);
  const html = await (await get("/admin", cookie))!.text();
  for (const job of ["talk", "games", "drawing"]) expect(html).toContain(`name="${job}"`);
  expect(html).toContain('<option value="gemini-3.5-flash-lite" selected>Gemini 3.5 Flash-Lite</option>');
  expect(html).toContain('<option value="gpt-6-sol" disabled>GPT-6 Sol (needs OPENAI_API_KEY)</option>');
  expect(html).toContain('onchange="this.form.submit()"'); // as it is picked
  expect((await post("/admin/models", { games: "gpt-6-sol" }, cookie))?.status).toBe(303);
  expect(models.chosen("games")).toBeUndefined(); // not without its key
});

test("a model picked for one job is kept; the others, left as they were, stay their defaults", async () => {
  const { get, post, models } = setUp();
  const cookie = cookieOf((await post("/admin/login", { password: "kubiks" }))!);
  await post("/admin/models", { talk: "gpt-6-sol" }); // not signed in: nothing
  expect(models.chosen("talk")).toBeUndefined();
  await post("/admin/models", { talk: "gpt-6-sol", games: "gemini-3.5-flash-lite", drawing: "gpt-6-astra" }, cookie); // the form sends all three
  expect(models.chosen("talk")).toBe("gpt-6-sol");
  expect(models.chosen("games")).toBeUndefined(); // a default: it follows the keys
  expect(models.chosen("drawing")).toBeUndefined();
  expect(await (await get("/admin", cookie))!.text()).toContain('<option value="gpt-6-sol" selected>GPT-6 Sol</option>');
});

test("a chosen model whose key is gone: the page says which one talks instead", async () => {
  const { db, get, post } = setUp("kubiks", { GEMINI_API_KEY: "g" });
  new Models(db, { GEMINI_API_KEY: "g", OPENAI_API_KEY: "o" }).choose("talk", "gpt-6-sol"); // chosen while the key was there
  const cookie = cookieOf((await post("/admin/login", { password: "kubiks" }))!);
  expect(await (await get("/admin", cookie))!.text()).toContain("GPT-6 Sol needs OPENAI_API_KEY: Gemini 3.5 Flash-Lite instead");
});

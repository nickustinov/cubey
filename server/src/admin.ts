// The admin page, for a grown-up, on the server itself (no app): /admin, behind CUBEY_ADMIN_PASSWORD (none: no page).
// The Cubeys: each one's child, whether it is online or when it was last seen, what it cost (spending.ts), and where the
// money goes (graphs by day, by Cubey and by kind, drawn here as SVG: no script on the page); a Cubey's
// page: its talks (its parents' log), its child and settings to change, its name, blocking it, a full reset. The password once, then
// a cookie only the server can make (an HMAC of the password), HttpOnly, Secure, SameSite=Strict, for 30 days.
import { createHash, createHmac, timingSafeEqual } from "node:crypto";
import { ageOf, type Child, LANGUAGE_NAMES, LANGUAGES, PARENTS, THEMES } from "./child";
import type { Device, Fleet } from "./fleet";
import { LESSON_LANGUAGES, languageName } from "./lesson";
import type { Spend, Spending, Spends } from "./spending";

// wipe: tells a Cubey that is on to wipe itself; changed: tells it its settings anew (server.ts)
type Deps = {
  password?: string; fleet: Fleet; spending: Spending; wipe?: (id: string) => void; changed?: (id: string) => void; now?: () => Date;
  wait?: (ms: number) => Promise<void>;
};

const COOKIE = "cubey_admin";
const h = (text: unknown) => String(text ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]!);
const usd = (n: number) => `$${n.toFixed(2)}`;
const cents = (n: number) => (n > 0 && n < 0.005 ? "<$0.01" : usd(n));
const same = (a: string, b: string) => timingSafeEqual(createHash("sha256").update(a).digest(), createHash("sha256").update(b).digest());

const STYLE = `:root{color-scheme:dark;--bg:#0b0906;--card:#16120c;--line:#2a2218;--text:#eadfcc;--dim:#9b8c75;--amber:#ffb020}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,sans-serif}
main{max-width:760px;margin:0 auto;padding:20px 16px 48px}h1{font-size:22px;margin:0 0 4px}h2{font-size:16px;margin:28px 0 10px}
a{color:var(--amber);text-decoration:none}.dim{color:var(--dim)}.card{display:block;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px;margin:10px 0;color:inherit}
.row{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}.on{color:var(--amber)}.badge{border:1px solid var(--amber);color:var(--amber);border-radius:6px;padding:0 6px;font-size:12px}
form.grid{display:grid;grid-template-columns:140px 1fr;gap:10px 12px;align-items:center}input,select,button{font:inherit;color:var(--text);background:#0f0c08;border:1px solid var(--line);border-radius:8px;padding:7px 9px;width:100%}
button{background:var(--amber);color:#1a1205;border:0;font-weight:600;cursor:pointer;width:auto;padding:8px 18px}.talk{border-top:1px solid var(--line);padding:10px 0}.talk q{quotes:none}
.graph{overflow-x:auto}svg{display:block;width:100%;min-width:560px;height:auto}svg text{fill:var(--dim);font:12px system-ui,sans-serif}svg line{stroke:var(--line)}
.legend{display:flex;flex-wrap:wrap;gap:4px 14px;font-size:13px;margin:4px 0 8px}.legend i{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:5px}
table{width:100%;border-collapse:collapse;font-size:14px}td,th{text-align:left;padding:6px 4px;border-top:1px solid var(--line)}th{color:var(--dim);font-weight:400}td.n{text-align:right}
@media (max-width:520px){form.grid{grid-template-columns:1fr}}`;

const page = (title: string, body: string, status = 200, headers: Record<string, string> = {}) =>
  new Response(`<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>${h(title)}</title><style>${STYLE}</style><main>${body}</main></html>`,
    { status, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store", ...headers } });
const redirect = (to: string, headers: Record<string, string> = {}) => new Response(null, { status: 303, headers: { location: to, ...headers } });

const login = (wrong = false) =>
  page("Cubey", `<h1>Cubey</h1><p class="dim">${wrong ? "That isn't it." : "The grown-ups' page."}</p>
<form method="post" action="/admin/login" class="card"><input type="password" name="password" autofocus placeholder="Password"><p><button>In</button></p></form>`, wrong ? 401 : 200);

function since(at: string | undefined, now: Date): string {
  if (!at) return "never";
  const minutes = Math.round((now.getTime() - new Date(at).getTime()) / 60000);
  return minutes < 2 ? "just now" : minutes < 120 ? `${minutes} min ago` : minutes < 48 * 60 ? `${Math.round(minutes / 60)} h ago` : `${Math.round(minutes / 1440)} days ago`;
}

const childLine = (child: Child) =>
  [child.name ?? "a child not named yet", child.gender, ageOf(child)].filter(Boolean).join(", ") + ` · speaks ${child.language.toUpperCase()}, learns ${child.learns.toUpperCase()}`;
const spendsLine = (s: Spends) => `${usd(s.today)} today · ${usd(s.month)} this month · ${usd(s.total)} in all`;
const titleOf = (device: Device, child: Child) => device.label ?? (child.name ? `${child.name}'s Cubey` : "A new Cubey");

const select = (name: string, value: string, options: [string, string][]) =>
  `<select name="${name}">${options.map(([v, label]) => `<option value="${h(v)}"${v === value ? " selected" : ""}>${h(label)}</option>`).join("")}</select>`;

// What the money went on: Soniox's listening and speaking, and each model's thinking (or Gemini Live's talk).
type Kind = { name: string; colour: string };
const kindOf = (s: Spend) =>
  s.what === "listen" ? "Soniox listening" : s.what === "speak" ? "Soniox speaking" : s.what === "live" ? `${s.model} (Live)` : s.model || "a model";
const AUDIO = { "Soniox listening": "#4FA3E0", "Soniox speaking": "#A77BE8" } as Record<string, string>;
const MODEL_COLOURS = ["#FFB020", "#E8743B", "#5BC08A", "#E05D7A", "#C9C24A", "#4DD0C4", "#B08850"];

// A round top for a graph's scale: 1, 2, 2.5 or 5 times a power of ten, at least the most.
function scaleTop(most: number): number {
  if (most <= 0) return 1;
  const power = 10 ** Math.floor(Math.log10(most));
  return [1, 2, 2.5, 5, 10].map((m) => m * power).find((top) => top >= most)!;
}

const GRAPH_W = 720;

// Columns, a day each, stacked by kind, on a dollar scale.
function byDay(days: string[], kinds: Kind[], spent: (day: string, kind: string) => number): string {
  const high = 200, left = 46, right = 18, bottom = 20, top = scaleTop(Math.max(...days.map((d) => kinds.reduce((sum, k) => sum + spent(d, k.name), 0))));
  const y = (v: number) => 6 + (high - 6 - bottom) * (1 - v / top);
  const slot = (GRAPH_W - left - right) / days.length, bar = Math.max(2, slot * 0.7);
  const scale = [0, top / 2, top].map((v) => `<line x1="${left}" x2="${GRAPH_W}" y1="${y(v)}" y2="${y(v)}"/><text x="${left - 6}" y="${y(v) + 4}" text-anchor="end">${usd(v)}</text>`);
  const every = Math.ceil(days.length / 8);
  const columns = days.map((day, i) => {
    let sum = 0;
    const x = left + i * slot + (slot - bar) / 2;
    const parts = kinds.map((k) => {
      const v = spent(day, k.name);
      if (v <= 0) return "";
      sum += v;
      return `<rect x="${x.toFixed(1)}" y="${y(sum).toFixed(1)}" width="${bar.toFixed(1)}" height="${(y(sum - v) - y(sum)).toFixed(1)}" fill="${k.colour}"><title>${day} · ${h(k.name)}: ${cents(v)}</title></rect>`;
    });
    const label = (days.length - 1 - i) % every ? "" : `<text x="${x + bar / 2}" y="${high - 5}" text-anchor="middle">${new Date(`${day}T12:00:00Z`).toLocaleDateString("en-GB", { day: "numeric", month: "short", timeZone: "UTC" })}</text>`;
    return parts.join("") + label;
  });
  return `<svg viewBox="0 0 ${GRAPH_W} ${high}">${scale.join("")}${columns.join("")}</svg>`;
}

// Bars, a row each (a Cubey, a kind), stacked by kind, longest the most, its total at its end.
function byRow(rows: { label: string; parts: { kind: Kind; usd: number }[] }[]): string {
  const row = 28, left = 170, right = 64, most = Math.max(...rows.map((r) => r.parts.reduce((sum, p) => sum + p.usd, 0)), 1e-9);
  const lines = rows.map((r, i) => {
    let x = left;
    const total = r.parts.reduce((sum, p) => sum + p.usd, 0);
    const parts = r.parts.map((p) => {
      const w = ((GRAPH_W - left - right) * p.usd) / most;
      x += w;
      return p.usd > 0 ? `<rect x="${(x - w).toFixed(1)}" y="${i * row + 6}" width="${w.toFixed(1)}" height="${row - 12}" fill="${p.kind.colour}"><title>${h(r.label === p.kind.name ? r.label : `${r.label} · ${p.kind.name}`)}: ${cents(p.usd)}</title></rect>` : "";
    });
    const label = r.label.length > 26 ? `${r.label.slice(0, 25)}…` : r.label;
    return `<text x="0" y="${i * row + 18}">${h(label)}</text>${parts.join("")}<text x="${x + 6}" y="${i * row + 18}">${cents(total)}</text>`;
  });
  return `<svg viewBox="0 0 ${GRAPH_W} ${rows.length * row}">${lines.join("")}</svg>`;
}

const millions = (n: number) => (n >= 1e6 ? `${(n / 1e6).toFixed(1)}M` : n >= 1e3 ? `${Math.round(n / 1e3)}k` : String(n));

export function admin(deps: Deps): (req: Request) => Promise<Response> {
  const { fleet, spending } = deps;
  const now = () => deps.now?.() ?? new Date();
  const ticket = deps.password && createHmac("sha256", deps.password).update("cubey admin").digest("hex");
  const signedIn = (req: Request) => {
    const sent = (req.headers.get("cookie") ?? "").split(/;\s*/).find((c) => c.startsWith(`${COOKIE}=`))?.slice(COOKIE.length + 1);
    return !!ticket && !!sent && same(sent, ticket);
  };

  const list = () => {
    const devices = fleet.list();
    const cards = devices.map((d) => {
      const child = fleet.memory(d.id).child(now());
      return `<a class="card" href="/admin/d/${h(d.id)}"><div class="row"><b>${h(titleOf(d, child))}</b>
<span class="${d.online ? "on" : "dim"}">${d.online ? "● online" : `seen ${since(d.seen, now())}`}</span></div>
<div>${h(childLine(child))}${d.blocked ? ' <span class="badge">blocked</span>' : ""}</div>
<div class="dim">${spendsLine(spending.of(d.id, now()))} · ${h(d.id)}</div></a>`;
    });
    return page("Cubeys", `<div class="row"><h1>Cubeys</h1><form method="post" action="/admin/logout"><button>Out</button></form></div>
<p class="dim">All of them: ${spendsLine(spending.of(undefined, now()))} · <a href="/admin/spending">where it goes</a></p>${cards.join("") || '<p class="dim">None yet: a Cubey shows here once it has met the server.</p>'}
<p class="dim">A new Cubey: flash it, join it to Wi-Fi on its own screens (“+” held 5 s), and it is here; it gets to know its child by talking.</p>`);
  };

  // Where the money goes, the last so many days (UTC): by day, by Cubey, by kind, and a table of what was used.
  const where = (days: number) => {
    const today = now();
    const list = Array.from({ length: days }, (_, i) => new Date(today.getTime() - (days - 1 - i) * 86400000).toISOString().slice(0, 10));
    const rows = spending.breakdown(list[0]);
    const total = rows.reduce((sum, r) => sum + r.usd, 0);
    const sumBy = (key: (r: Spend) => string) => {
      const sums = new Map<string, number>();
      for (const r of rows) sums.set(key(r), (sums.get(key(r)) ?? 0) + r.usd);
      return sums;
    };
    const byKind = sumBy(kindOf);
    const models = [...byKind.keys()].filter((k) => !AUDIO[k]).sort((a, b) => byKind.get(b)! - byKind.get(a)!);
    const kinds: Kind[] = [
      ...Object.keys(AUDIO).filter((k) => byKind.has(k)).map((name) => ({ name, colour: AUDIO[name] })),
      ...models.map((name, i) => ({ name, colour: MODEL_COLOURS[i % MODEL_COLOURS.length] })),
    ];
    const cell = sumBy((r) => `${r.day} ${kindOf(r)}`), mine = sumBy((r) => `${r.device} ${kindOf(r)}`), devices = sumBy((r) => r.device);
    const nameOf = (id: string) => {
      const device = fleet.get(id);
      return device ? titleOf(device, fleet.memory(id).child(today)) : `${id} (forgotten)`;
    };
    const used = (kind: string) => {
      const of = rows.filter((r) => kindOf(r) === kind);
      const sum = (key: "input" | "cached" | "output" | "seconds") => of.reduce((s, r) => s + r[key], 0);
      return AUDIO[kind] ? `${(sum("seconds") / 3600).toFixed(1)} h of audio` : `${millions(sum("input"))} tokens in (${millions(sum("cached"))} cached), ${millions(sum("output"))} out`;
    };
    const legend = `<div class="legend">${kinds.map((k) => `<span><i style="background:${k.colour}"></i>${h(k.name)}</span>`).join("")}</div>`;
    const choose = [7, 30, 90].map((d) => (d === days ? `<b>${d} days</b>` : `<a href="/admin/spending?days=${d}">${d} days</a>`)).join(" · ");
    const graphs = rows.length
      ? `<h2>By day</h2>${legend}<div class="card graph">${byDay(list, kinds, (day, kind) => cell.get(`${day} ${kind}`) ?? 0)}</div>
<h2>By Cubey</h2><div class="card graph">${byRow([...devices.keys()].sort((a, b) => devices.get(b)! - devices.get(a)!).map((id) => ({
          label: nameOf(id), parts: kinds.map((kind) => ({ kind, usd: mine.get(`${id} ${kind.name}`) ?? 0 })),
        })))}</div>
<h2>By kind</h2><div class="card"><div class="graph">${byRow(kinds.map((kind) => ({ label: kind.name, parts: [{ kind, usd: byKind.get(kind.name)! }] })))}</div>
<table><tr><th>Kind</th><th>Used</th><th class="n">Spent</th><th class="n">Share</th></tr>${kinds.map((k) => `<tr><td>${h(k.name)}</td><td class="dim">${used(k.name)}</td><td class="n">${cents(byKind.get(k.name)!)}</td><td class="n">${Math.round((100 * byKind.get(k.name)!) / total)}%</td></tr>`).join("")}</table></div>`
      : '<p class="dim">Nothing spent in these days.</p>';
    return page("Where it goes", `<p><a href="/admin">← Cubeys</a></p><h1>Where it goes</h1>
<p class="dim">The last ${days} days: ${usd(total)} · ${choose}</p>${graphs}`);
  };

  const detail = (id: string) => {
    const device = fleet.get(id);
    if (!device) return page("Cubey", "<h1>No such Cubey</h1>", 404);
    const child = fleet.memory(id).child(now());
    const birthday = child.ageMonths === undefined ? "" : (fleet.memory(id).birthday() ?? "");
    const spends = spending.of(id, now());
    const talks = fleet.log(id).read().slice(-50).reverse().map((t) => {
      const at = new Date(t.at).toLocaleString("en-GB", { timeZone: child.timeZone, dateStyle: "medium", timeStyle: "short" });
      return `<div class="talk"><div class="dim">${h(at)}${t.for_parents ? ' <span class="badge">for the parents</span>' : ""}</div><div>${h(t.question)}</div><div class="on">${h(t.answer)}</div></div>`;
    });
    const by = Object.entries(spends.by).map(([what, cost]) => `${h(what)} ${usd(cost ?? 0)}`).join(" · ");
    return page(titleOf(device, child), `<p><a href="/admin">← Cubeys</a></p><h1>${h(titleOf(device, child))}</h1>
<p class="dim">${device.online ? '<span class="on">● online</span>' : `seen ${since(device.seen, now())}`} · ${h(id)} · met ${h(device.created.slice(0, 10))}</p>
<h2>Spending</h2><div class="card">${spendsLine(spends)}<div class="dim">${by || "nothing yet"} · ${spends.tokens.input.toLocaleString("en")} tokens in, ${spends.tokens.output.toLocaleString("en")} out</div></div>
<h2>Its child</h2><form method="post" class="card grid">
<label>Cubey's name</label><input name="label" value="${h(device.label)}" placeholder="${h(titleOf({ ...device, label: undefined }, child))}">
<label>Child's name</label><input name="name" value="${h(child.name)}" placeholder="Cubey asks">
<label>Girl or boy</label>${select("gender", child.gender ?? "", [["", "not known"], ["girl", "girl"], ["boy", "boy"]])}
<label>Birthday</label><input type="date" name="birthday" value="${h(birthday)}">
<label>Cubey speaks</label>${select("language", child.language, LANGUAGES.map((l) => [l, LANGUAGE_NAMES[l]]))}
<label>It teaches</label>${select("learns", child.learns, LESSON_LANGUAGES.map((l) => [l, languageName(l)]))}
<label>Brightness</label>${select("brightness", String(child.brightness ?? 100), Array.from({ length: 10 }, (_, i) => [String((i + 1) * 10), `${(i + 1) * 10}%`]))}
<label>Theme</label>${select("theme", child.theme ?? "amber", THEMES.map((t) => [t, t === "vfd" ? "VFD" : t[0].toUpperCase() + t.slice(1)]))}
<label>At home</label>${select("parents", child.parents, PARENTS.map((p) => [p, p.replace("_", " and ")]))}
<label>Time zone</label><input name="timeZone" value="${h(child.timeZone)}">
<label>Blocked</label><input type="checkbox" name="blocked"${device.blocked ? " checked" : ""} style="width:auto">
<span></span><p><button>Keep</button></p></form>
<h2>Talks</h2><div class="card">${talks.join("") || '<p class="dim">None yet.</p>'}</div>
<h2>Full reset</h2><form method="post" action="/admin/d/${h(id)}/reset" class="card" onsubmit="return confirm('Full reset? This Cubey forgets its child, their talks, its languages and, if it is on, its Wi-Fi. It cannot be undone.')">
<p class="dim">To set it up anew: it knows no one, its parents’ log kept aside on the server. On now, it forgets its Wi-Fi too and starts at its welcome; off, it starts over when next on.</p><button>Full reset</button></form>`);
  };

  const save = async (id: string, req: Request) => {
    if (!fleet.get(id)) return page("Cubey", "<h1>No such Cubey</h1>", 404);
    const form = await req.formData();
    const field = (name: string) => String(form.get(name) ?? "");
    fleet.name(id, field("label"));
    fleet.block(id, form.get("blocked") === "on");
    fleet.memory(id).edit({
      name: field("name"), gender: field("gender"), birthday: field("birthday"), language: field("language"), learns: field("learns"),
      parents: field("parents"), timeZone: field("timeZone"), brightness: field("brightness"),
      theme: field("theme"),
    }, now());
    deps.changed?.(id);
    return redirect(`/admin/d/${encodeURIComponent(id)}`);
  };

  return async (req) => {
    if (!ticket) return new Response("Not here.\n", { status: 404 });
    const path = new URL(req.url).pathname.replace(/\/+$/, "");
    if (req.method === "POST" && path === "/admin/login") {
      const password = String((await req.formData()).get("password") ?? "");
      if (!same(password, deps.password!)) return await (deps.wait ?? Bun.sleep)(1000), login(true); // slowly, again
      return redirect("/admin", { "set-cookie": `${COOKIE}=${ticket}; Path=/admin; Max-Age=${30 * 86400}; HttpOnly; Secure; SameSite=Strict` });
    }
    if (!signedIn(req)) return req.method === "POST" ? redirect("/admin") : login();
    if (req.method === "POST" && path === "/admin/logout") return redirect("/admin", { "set-cookie": `${COOKIE}=; Path=/admin; Max-Age=0; HttpOnly; Secure; SameSite=Strict` });
    if (path === "/admin") return list();
    if (path === "/admin/spending") return where(Math.min(365, Math.max(1, Number(new URL(req.url).searchParams.get("days")) || 30)));
    const reset = /^\/admin\/d\/([a-z0-9]{1,32})\/reset$/i.exec(path)?.[1];
    if (reset && req.method === "POST") return deps.wipe?.(reset), fleet.forget(reset), redirect("/admin");
    const id = /^\/admin\/d\/([a-z0-9]{1,32})$/i.exec(path)?.[1];
    if (!id) return page("Cubey", "<h1>Not here</h1>", 404);
    return req.method === "POST" ? save(id, req) : detail(id);
  };
}

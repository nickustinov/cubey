// Cubey's server: Cubeys connect to /device over a WebSocket (protocol in session.ts), each by its id and key (fleet.ts),
// and talk with Cubey: through Soniox and a model (cascade.ts), or on Gemini Live (live.ts), as CUBEY_TALK says. Each
// has its own child, parents' log and spending; /admin shows them all (admin.ts).
//   bun run dev
// It listens on this Mac only unless HOST and CUBEY_DEVICE_TOKEN are set, for a device on the Wi-Fi (access.ts).
import { allowed, exposedWithoutToken } from "./access";
import type { ServerWebSocket } from "bun";
import { admin } from "./admin";
import { cascadeTalks } from "./cascade";
import { picture } from "./curriculum";
import { LessonProgress, LessonWords } from "./lesson";
import { Lessons } from "./lesson-game";
import { LIVE_MODEL, liveTalks } from "./live";
import { completeWith, DRAW_MODEL, MODEL, SMART_MODEL } from "./llm";
import { Speaker, Transcriber } from "./soniox";
import { DATA, openDb } from "./db";
import { Fleet } from "./fleet";
import { History } from "./history";
import { Session } from "./session";
import { Spending } from "./spending";

const port = Number(process.env.PORT ?? 8788); // Puce's is 8787: both can run
const hostname = process.env.HOST ?? "127.0.0.1";
const token = process.env.CUBEY_DEVICE_TOKEN;
if (exposedWithoutToken(hostname, token)) {
  console.error(`HOST=${hostname} reaches beyond this Mac: set CUBEY_DEVICE_TOKEN in server/.env first (openssl rand -hex 32)`);
  process.exit(1);
}
// cascade: her voice through Soniox to a model and back (Puce's way; Gemini Live heard a small child's Latvian
// poorly, 2026-10-10); live: speech to speech on Gemini Live.
const TALK = process.env.CUBEY_TALK ?? "cascade";
// Talking over Cubey: on unless 0 (a loud room set it off, 2026-10-11).
const TALK_OVER = process.env.CUBEY_TALK_OVER !== "0";
if (TALK !== "cascade" && TALK !== "live") {
  console.error("CUBEY_TALK must be cascade or live (server/.env)");
  process.exit(1);
}
const keyFor = (model: string) => (model.startsWith("gpt-") ? "OPENAI_API_KEY" : "GEMINI_API_KEY");
for (const key of TALK === "live" ? ["GEMINI_API_KEY"] : ["SONIOX_API_KEY", keyFor(MODEL), keyFor(SMART_MODEL), keyFor(DRAW_MODEL)])
  if (!process.env[key]) {
    console.error(`${key} is not set (server/.env)`);
    process.exit(1);
  }

const log = (...args: unknown[]) => console.log(new Date().toLocaleTimeString("lv"), ...args);
const db = openDb();
const history = new History(db), progress = new LessonProgress(db);
const fleet = new Fleet(db, DATA, process.env.CUBEY_CHILD_TIMEZONE, [history, progress]);
const spending = new Spending(db);
// Each Cubey's connections, for a full reset (admin.ts): its talk ends here, and it wipes itself.
const sockets = new Map<string, Set<ServerWebSocket<Data>>>();
const wipe = (id: string) => {
  for (const ws of sockets.get(id) ?? []) ws.data.session?.close(), ws.send(JSON.stringify({ type: "reset" }));
};
const adminPage = admin({
  password: process.env.CUBEY_ADMIN_PASSWORD, fleet, spending, wipe,
  changed: (id) => sockets.get(id)?.forEach((ws) => ws.data.session?.sendSettings()), // its brightness, at once
});
const talks = TALK === "live"
  ? liveTalks({ history, log })
  : cascadeTalks({
    history, log, complete: completeWith(MODEL), smart: completeWith(SMART_MODEL), draw: completeWith(DRAW_MODEL),
    listen: ({ child, onHeard, onWords, onPause, also, waitMs }) =>
      new Transcriber({ language: child.language, also, waitMs, terms: child.name ? [child.name] : [], onEnd: onHeard, onWords, onPause }),
    speak: (opts) => new Speaker(opts),
    lessons: new Lessons({
      progress, words: new LessonWords(db), complete: completeWith(SMART_MODEL), language: (child) => child.learns, picture,
    }),
  });

type Data = { device: string; session?: Session };

Bun.serve<Data>({
  hostname,
  port,
  fetch(req, server) {
    const path = new URL(req.url).pathname;
    if (path === "/health") return new Response("ok\n");
    if (path === "/admin" || path.startsWith("/admin/")) return adminPage(req);
    if (path !== "/device") return new Response("Not here.\n", { status: 404 });
    if (!allowed(req.headers.get("authorization"), token, server.requestIP(req)?.address)) return new Response("a device token, please\n", { status: 401 });
    const device = req.headers.get("x-cubey-device") ?? "", admitted = fleet.admit(device, req.headers.get("x-cubey-key") ?? "");
    if (admitted !== "ok") return log(`Cubey ${device || "with no id"} turned away: ${admitted}`), new Response(`${admitted}\n`, { status: 403 });
    if (server.upgrade(req, { data: { device } })) return;
    return new Response("a WebSocket, please\n", { status: 426 });
  },
  websocket: {
    // Its own child (read for every talk, so her age and what she told it last are right), log and spending; its lines
    // in the server's log begin with its id's end.
    open(ws) {
      const { device } = ws.data;
      const memory = fleet.memory(device), parentLog = fleet.log(device);
      const say = (...args: unknown[]) => log(`[${device.slice(-4)}]`, ...args);
      fleet.connected(device, true);
      sockets.set(device, (sockets.get(device) ?? new Set()).add(ws));
      ws.data.session = new Session({
        child: () => memory.child(),
        settings: {
          get: () => ({
            language: memory.child().language, learns: memory.child().learns, new: memory.isNew(), brightness: memory.child().brightness ?? 100,
            theme: memory.child().theme ?? "amber",
          }),
          set: (settings) => memory.set(settings),
          brighten: (percent) => void memory.brighten(percent),
        },
        learn: (learned) => memory.learn(learned),
        spent: (spent) => spending.add(device, spent),
        cubey: talks,
        talkOver: TALK_OVER,
        send: {
          text: (msg) => {
            ws.send(JSON.stringify(msg));
            say(JSON.stringify(msg, (key, value) => (key === "picture" ? `${Math.round(value.length / 1365)} KB` : value))); // not its pictures
          },
          audio: (pcm) => void ws.sendBinary(pcm),
        },
        onExchange: (exchanged) => {
          parentLog.add(exchanged);
          history.add(memory.child().id, exchanged.heard, exchanged.said);
          say(`${exchanged.heard} → ${exchanged.said}${exchanged.forParents ? " (for the parents)" : ""}`);
        },
      });
      say("connected:", memory.child().name ?? "a child not named yet");
    },
    message(ws, msg) {
      if (typeof msg === "string") {
        log(`[${ws.data.device.slice(-4)}] device:`, msg);
        ws.data.session?.onText(msg);
      } else {
        ws.data.session?.onAudio(msg);
      }
    },
    close(ws) {
      ws.data.session?.close();
      fleet.connected(ws.data.device, false);
      sockets.get(ws.data.device)?.delete(ws);
      log(`[${ws.data.device.slice(-4)}] disconnected`);
    },
  },
});

log(`Cubey server on ${hostname}:${port} · ${TALK === "live" ? LIVE_MODEL : `Soniox and ${MODEL}, ${SMART_MODEL} for the games with words, ${DRAW_MODEL} drawing`}${TALK_OVER ? "" : " · no talking over"} · ${fleet.list().length} Cubeys known${process.env.CUBEY_ADMIN_PASSWORD ? " · /admin" : ""}`);

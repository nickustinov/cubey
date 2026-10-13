// The parents' log: every question and answer, the big questions marked (server/data/log.jsonl).
//   bun run log [count]
import { ParentLog } from "../src/log";

const count = Number(process.argv[2] ?? 20);
for (const e of new ParentLog().read().slice(-count)) {
  const at = new Date(e.at).toLocaleString("lv");
  console.log(`${at}${e.for_parents ? " · for the parents" : ""}\n  ? ${e.question}\n  ${e.answer}\n`);
}

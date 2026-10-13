// Fetches the pictures of the word game's curriculum (content/lesson/curriculum.json) from Microsoft Fluent
// Emoji 3D (MIT, 256 px) into content/lesson/pictures/, at the sizes the device shows them: 256 px for the card
// over all of the screen, 176 px for the cards she taps. The server sends them with lesson.show and lesson.choose.
// Resizes with macOS sips; skips pictures already there.
//   bun run lesson-pictures
import { existsSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { CURRICULUM, pictureFile, PICTURE_SIZES } from "../src/curriculum";

const TREE = "https://api.github.com/repos/microsoft/fluentui-emoji/git/trees/main?recursive=1";
const RAW = "https://raw.githubusercontent.com/microsoft/fluentui-emoji/main/";

const tree = (await (await fetch(TREE)).json()) as { tree: { path: string }[] };
const paths = new Map<string, string>();
for (const { path } of tree.tree) {
  const m = path.match(/^assets\/([^/]+)\/(?:Default\/)?3D\/[^/]+\.png$/);
  if (m) paths.set(m[1], path);
}

const names = [...new Set(CURRICULUM.flatMap((idea) => (idea.picture ? [idea.picture] : [])))];
for (const name of names) {
  const files = PICTURE_SIZES.map((size) => pictureFile(name, size));
  if (files.every((f) => existsSync(f))) continue;
  const path = paths.get(name);
  if (!path) throw new Error(`Fluent Emoji has no 3D picture named ${name}`);
  const src = `${tmpdir()}/lesson-picture.png`;
  await Bun.write(src, await (await fetch(RAW + encodeURI(path))).arrayBuffer());
  for (const [i, size] of PICTURE_SIZES.entries()) {
    mkdirSync(new URL("../../content/lesson/pictures/", import.meta.url).pathname, { recursive: true });
    const sips = Bun.spawnSync(["sips", "-Z", String(size), src, "--out", files[i]]);
    if (sips.exitCode) throw new Error(`sips failed for ${name}`);
  }
  console.log(`picture ${name}`);
}
console.log(`${names.length} pictures in content/lesson/pictures/`);

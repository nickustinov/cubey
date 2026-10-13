// The word game's curriculum (content/lesson/curriculum.json, Puce's parrot's): what a child learns in a foreign
// language, in the order children learn it. Stages go from first words (hello, pets, fruit, colours, one to three) to
// her world, actions and describing words, little phrases and tiny conversations; each unit is a handful of ideas with
// one activity, written by us in English with a picture. The model says them in the target language (lesson.ts); the
// pictures are sent to the device (content/lesson/pictures/).
import curriculum from "../../content/lesson/curriculum.json";

export const TYPES = ["word", "action", "describe", "phrase", "number", "colour"] as const;
// touch: tap her picture among others; act: do it; count: fingers and things; colour: which one is red;
// opposites: which one is big; talk: say it in a tiny exchange.
export const ACTIVITIES = ["touch", "act", "count", "colour", "opposites", "talk"] as const;
export const PICTURE_SIZES = [256, 176] as const; // the card over all of the screen, and the cards she taps
export type PictureSize = (typeof PICTURE_SIZES)[number];

export type Idea = {
  id: string;
  en: string; // the idea in English, for the model
  picture: string | null; // its Fluent Emoji name
  type: (typeof TYPES)[number];
  unit: string;
  activity: (typeof ACTIVITIES)[number];
  from: number; // the youngest age for its unit, in months
  stage: number; // 1..5
};
export type Unit = { id: string; name: string; stage: number; activity: Idea["activity"]; from: number; ideas: Idea[] };

export const UNITS: Unit[] = curriculum.stages.flatMap((stage, s) =>
  stage.units.map((u) => {
    const unit = { id: u.id, name: u.name, stage: s + 1, activity: u.activity as Idea["activity"], from: u.from };
    return { ...unit, ideas: u.ideas.map((i) => ({ ...i, type: i.type as Idea["type"], unit: u.id, activity: unit.activity, from: u.from, stage: s + 1 })) };
  }),
);
export const CURRICULUM: Idea[] = UNITS.flatMap((u) => u.ideas);

const BY_ID = new Map(CURRICULUM.map((i) => [i.id, i]));
export const idea = (id: string) => BY_ID.get(id);

const slug = (name: string) => name.toLowerCase().replace(/[^a-z0-9]+/g, "_");
export const pictureFile = (name: string, size: PictureSize) =>
  new URL(`../../content/lesson/pictures/${slug(name)}_${size}.png`, import.meta.url).pathname;

// A picture as the device takes it: base64 PNG; undefined if it isn't there.
export async function picture(name: string, size: PictureSize): Promise<string | undefined> {
  const file = Bun.file(pictureFile(name, size));
  return (await file.exists()) ? Buffer.from(await file.arrayBuffer()).toString("base64") : undefined;
}

// What Cubey knows of the child, kept on the server in memory.md (data/devices/<id>/, fleet.ts) for a grown-up to read
// and change: what the child (or a grown-up) tells Cubey in a talk (their name, girl or boy, their age), what a grown-up
// sets on the device's menu (the language Cubey speaks, the one its word game teaches), asks Cubey for (its screen's
// brightness) or sets on the admin page (all of it, and its screen's theme).
// Begun as a child not known yet, in the household's time zone (CUBEY_CHILD_TIMEZONE).
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";
import { ageMonths, type Child, type Gender, type Language, LANGUAGES, type Parents, PARENTS, type Theme, THEMES, yearsOf } from "./child";
import { LESSON_LANGUAGES } from "./lesson";

// setUp: when a grown-up set its languages on its menu (a new Cubey's opens by itself until then).
type Fields = {
  id: string; name?: string; gender?: Gender; birthday?: string; language: Language; learns: string; parents: Parents; timeZone: string;
  brightness?: number; theme?: Theme; setUp?: string;
};
const LABELS: [keyof Fields, string][] = [
  ["id", "Id"], ["name", "Name"], ["gender", "Gender"], ["birthday", "Birthday"], ["language", "Speaks"], ["learns", "Learns"],
  ["parents", "Parents"], ["timeZone", "Time zone"], ["brightness", "Brightness"], ["theme", "Theme"], ["setUp", "Set up"],
];
// What a talk tells Cubey of the child: age in years.
export type Learned = { name?: string; gender?: Gender; age?: number };

const zoned = (zone?: string) => {
  try {
    return zone && new Intl.DateTimeFormat("en", { timeZone: zone }) ? zone : undefined;
  } catch {
    return undefined;
  }
};
const teaches = (language: Language, learns?: string) =>
  learns && learns !== language && LESSON_LANGUAGES.includes(learns) ? learns : language === "en" ? "fr" : "en";

// The screen's brightness, in percent: 10 to 100 (dimmer, it can't be seen); none for anything else.
const brightnessOf = (given: unknown) => {
  const percent = Number(given);
  return Number.isInteger(percent) && percent >= 10 && percent <= 100 ? percent : undefined;
};

// The birthday of a child who says they are so many years old: about halfway through that year, the guess least wrong.
function bornAbout(years: number, today: Date): string {
  const born = new Date(today.getFullYear() - years, today.getMonth() - 6, today.getDate());
  return `${born.getFullYear()}-${String(born.getMonth() + 1).padStart(2, "0")}-${String(born.getDate()).padStart(2, "0")}`;
}

// The fields as given, checked: what makes no sense is left out, or the default.
function checked(given: Record<string, string | undefined>): Fields {
  const language = (LANGUAGES as readonly string[]).includes(given.language ?? "") ? (given.language as Language) : "en";
  return {
    id: given.id || given.name || "child",
    ...(given.name && { name: given.name }),
    ...((given.gender === "girl" || given.gender === "boy") && { gender: given.gender }),
    ...(/^\d{4}-\d{2}-\d{2}$/.test(given.birthday ?? "") && { birthday: given.birthday }),
    language,
    learns: teaches(language, given.learns),
    parents: (PARENTS as readonly string[]).includes(given.parents ?? "") ? (given.parents as Parents) : "mum_dad",
    timeZone: zoned(given.timeZone) ?? Intl.DateTimeFormat().resolvedOptions().timeZone,
    ...(brightnessOf(given.brightness) && { brightness: brightnessOf(given.brightness) }),
    ...((THEMES as readonly string[]).includes(given.theme ?? "") && { theme: given.theme as Theme }),
    ...(given.setUp && { setUp: given.setUp }),
  };
}

export class Memory {
  // id: the child's for good (their Cubey's), for their talks before and their word game's memory
  constructor(private path: string, private id: string, private timeZone?: string) {}

  // Not set up yet: its menu opens by itself (a child it knows the name of is one from before there was a menu).
  isNew(): boolean {
    const fields = this.read();
    return !fields.setUp && !fields.name;
  }

  child(today = new Date()): Child {
    const { birthday, setUp: _, ...fields } = this.read();
    return { ...fields, ...(birthday && { ageMonths: ageMonths(birthday, today) }) };
  }

  // A name, girl or boy, an age in years.
  learn(learned: Learned, today = new Date()): Child {
    const fields = this.read();
    if (learned.name && learned.name !== fields.name) fields.gender = undefined; // guessed from the name before: anew
    if (learned.name) fields.name = learned.name;
    if (learned.gender) fields.gender = learned.gender;
    if (learned.age) fields.birthday = bornAbout(learned.age, today);
    this.write(fields);
    return this.child(today);
  }

  // A grown-up's, on the admin page (admin.ts): what is given replaces what was, checked; an empty one, not known. An
  // age in years, as a child tells it; the same as it was keeps the birthday guessed before, so it grows as it did.
  edit(changes: Partial<Record<"name" | "gender" | "age" | "language" | "learns" | "parents" | "timeZone" | "brightness" | "theme", string>>, today = new Date()): Child {
    const fields = this.read();
    const { age, ...rest } = changes;
    const given: Record<string, string | undefined> = { ...fields, brightness: fields.brightness?.toString() };
    for (const [key, value] of Object.entries(rest)) given[key] = value?.trim() || undefined;
    if (age !== undefined) {
      const years = Number(age);
      const told = Number.isInteger(years) && years >= 1 && years <= 12 ? years : undefined;
      if (told !== yearsOf(this.child(today))) given.birthday = told === undefined ? undefined : bornAbout(told, today);
    }
    this.write(checked(given));
    return this.child(today);
  }

  // The device's menu: false, and nothing changed, for a language it can't speak or teach, or the same twice; a theme
  // there is none of, as it was.
  set(settings: { language: string; learns: string; theme?: string }): boolean {
    const fields = this.read();
    if (!(LANGUAGES as readonly string[]).includes(settings.language) || teaches(settings.language as Language, settings.learns) !== settings.learns) return false;
    const theme = (THEMES as readonly string[]).includes(settings.theme ?? "") ? (settings.theme as Theme) : fields.theme;
    this.write({
      ...fields, language: settings.language as Language, learns: settings.learns, ...(theme && { theme }), setUp: new Date().toISOString().slice(0, 10),
    });
    return true;
  }

  // A grown-up asked Cubey for it: false, and nothing changed, for a brightness it can't have.
  brighten(percent: number): boolean {
    if (!brightnessOf(percent)) return false;
    this.write({ ...this.read(), brightness: percent });
    return true;
  }

  private read(): Fields {
    if (!existsSync(this.path)) {
      const fields = checked({ id: this.id, timeZone: this.timeZone });
      this.write(fields);
      return fields;
    }
    const given: Record<string, string> = {};
    for (const line of readFileSync(this.path, "utf8").split("\n")) {
      const found = /^- ([^:]+):\s*(.*?)\s*$/.exec(line);
      const key = found && LABELS.find(([, label]) => label === found[1])?.[0];
      if (key && found[2]) given[key] = found[2];
    }
    return checked(given);
  }

  private write(fields: Fields) {
    mkdirSync(dirname(this.path), { recursive: true });
    const lines = LABELS.filter(([key]) => fields[key]).map(([key, label]) => `- ${label}: ${fields[key]}`);
    writeFileSync(this.path, `# What Cubey knows of the child\n\nCubey keeps this as the child tells it, and the device's menu sets the languages (“–” held 5 s), a grown-up asks it for its brightness (in percent); a grown-up may change it, a line each.\n\n${lines.join("\n")}\n`);
  }
}

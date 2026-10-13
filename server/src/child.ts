// The child Cubey talks to, as it knows them (memory.ts): the server's one child, until a parents' app exists.
export const LANGUAGES = ["en", "lv", "lt", "fr", "pt", "de", "es", "ru"] as const; // pt: European Portuguese; es: Spain's
export type Language = (typeof LANGUAGES)[number];
// Each language as the model is told to speak it.
export const LANGUAGE_NAMES: Record<Language, string> = {
  en: "English", lv: "Latvian", lt: "Lithuanian", fr: "French", pt: "European Portuguese (from Portugal)",
  de: "German", es: "Spanish (from Spain)", ru: "Russian",
};
export const PARENTS = ["mum_dad", "mum", "dad", "mums", "dads"] as const;
export type Parents = (typeof PARENTS)[number];
// The colours of a Cubey's screen (its firmware's ui/dots.h): amber, as old LED panels (the default); green, a phosphor
// screen's; red and blue, LED panels'; vfd, a vacuum fluorescent display's; white, ink on paper.
export const THEMES = ["amber", "green", "red", "blue", "vfd", "white"] as const;
export type Theme = (typeof THEMES)[number];
export type Gender = "girl" | "boy";

// id: theirs for good, for their talks before and their word game's memory (the name they first had); name, gender
// and age: unknown until they tell Cubey; learns: the language its word game teaches them.
export type Child = {
  id: string; name?: string; gender?: Gender; ageMonths?: number; language: Language; learns: string; parents: Parents; timeZone: string;
  brightness?: number; // its Cubey's screen's, in percent (none: full)
  theme?: Theme; // its Cubey's screen's colours (none: amber)
};

export function ageMonths(birthday: string, today = new Date()): number {
  const [y, m, d] = birthday.split("-").map(Number);
  let months = (today.getFullYear() - y) * 12 + (today.getMonth() + 1 - m);
  if (today.getDate() < d) months--;
  return months;
}

// How the prompts name the child and speak of them; unknown, "the child", no gender guessed.
export const nameOf = (child: Child) => child.name ?? "the child";
export function pronouns(child: Child) {
  const [she, her, them, hers] = child.gender === "girl" ? ["she", "her", "her", "hers"]
    : child.gender === "boy" ? ["he", "his", "him", "his"]
    : ["the child", "the child's", "the child", "the child's"];
  const cap = (word: string) => word[0].toUpperCase() + word.slice(1);
  return { she, her, them, hers, She: cap(she), Her: cap(her) };
}

// Their age in months, as a 4-year-old's until Cubey knows it (what it says, the word game's words).
export const monthsOf = (child: Child) => child.ageMonths ?? 48;

// Her age as the prompt says it ("3 years and 8 months"), from her birthday: Cubey speaks to the child she is now;
// none until it knows.
export function ageOf(child: Child): string | undefined {
  if (child.ageMonths === undefined) return undefined;
  const years = Math.floor(child.ageMonths / 12), months = child.ageMonths % 12;
  const plural = (n: number, unit: string) => `${n} ${unit}${n === 1 ? "" : "s"}`;
  return months ? `${plural(years, "year")} and ${plural(months, "month")}` : plural(years, "year");
}

// How much Cubey says at a time, and in what words, for her age.
export function speakFor(child: Child): string {
  const years = Math.floor(monthsOf(child) / 12);
  if (years < 4) return `One or two short sentences, in the simplest everyday words a ${years}-year-old knows.`;
  if (years < 6) return `Two or three short sentences, in everyday words a ${years}-year-old knows.`;
  return `Two or three sentences, in words a ${years}-year-old knows; a new word now and then, explained.`;
}

// The moment where she lives, as the prompt says it ("Wednesday evening, 20:40"): good morning in the morning.
export function momentFor(child: Child, now = new Date()): string {
  const parts = new Intl.DateTimeFormat("en-GB", { timeZone: child.timeZone, weekday: "long", hour: "2-digit", minute: "2-digit", hourCycle: "h23" })
    .formatToParts(now);
  const part = (type: string) => parts.find((p) => p.type === type)?.value ?? "";
  const hour = Number(part("hour"));
  const of = hour < 5 ? "night" : hour < 12 ? "morning" : hour < 17 ? "afternoon" : hour < 21 ? "evening" : "night";
  return `${part("weekday")} ${of}, ${part("hour")}:${part("minute")}`;
}

// Her family, as the model's English instructions name them.
export function parentsOf(child: Child): string {
  const { her } = pronouns(child);
  return { mum_dad: "Mum or Dad", mum: "Mum", dad: "Dad", mums: `${her} mums`, dads: `${her} dads` }[child.parents];
}

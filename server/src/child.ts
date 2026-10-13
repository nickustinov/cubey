// The child Cubey talks to, as it knows them (memory.ts): the server's one child, until a parents' app exists.
// The languages Cubey speaks, each one Soniox's voice speaks, and the dot font writes (Latin with its marks, Cyrillic).
export const LANGUAGES = ["en", "fr", "de", "es", "it", "pt", "nl", "pl", "sv", "da", "no", "fi", "et", "ru", "uk", "lv", "lt"] as const;
export type Language = (typeof LANGUAGES)[number];
// Each language as the model is told to speak it.
export const LANGUAGE_NAMES: Record<Language, string> = {
  en: "English", fr: "French", de: "German", es: "Spanish (from Spain)", it: "Italian", pt: "European Portuguese (from Portugal)",
  nl: "Dutch", pl: "Polish", sv: "Swedish", da: "Danish", no: "Norwegian (Bokmål)", fi: "Finnish", et: "Estonian",
  ru: "Russian", uk: "Ukrainian", lv: "Latvian", lt: "Lithuanian",
};
export const PARENTS = ["mum_dad", "mum", "dad", "mums", "dads"] as const;
export type Parents = (typeof PARENTS)[number];
// The colours of a Cubey's screen (its firmware's ui/dots.h): amber, as old LED panels (the default); green, a phosphor
// screen's; red and blue, LED panels'; vfd, a vacuum fluorescent display's; white, ink on paper.
export const THEMES = ["amber", "green", "red", "blue", "vfd", "white"] as const;
export type Theme = (typeof THEMES)[number];
export type Gender = "girl" | "boy";

// id: theirs for good, for their talks before and their word game's memory (the name they first had); name, gender
// and age: unknown until they tell Cubey; learns: the language its word game teaches them, as they chose it (none yet).
export type Child = {
  id: string; name?: string; gender?: Gender; ageMonths?: number; language: Language; learns?: string; parents: Parents; timeZone: string;
  brightness?: number; // its Cubey's screen's, in percent (none: full)
  volume?: number; // its Cubey's speaker's, in percent (none: full)
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

// Her age in whole years ("3"), as she or a grown-up tells it; none until Cubey knows it. Kept as a birthday guessed
// from it, so it grows with her: its months are the guess's, not hers.
export const yearsOf = (child: Child) => (child.ageMonths === undefined ? undefined : Math.floor(child.ageMonths / 12));

// Her age as the prompts and the admin page say it ("3 years"); none until Cubey knows it.
export function ageOf(child: Child): string | undefined {
  const years = yearsOf(child);
  return years === undefined ? undefined : `${years} year${years === 1 ? "" : "s"}`;
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

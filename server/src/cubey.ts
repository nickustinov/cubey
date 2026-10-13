// Who Cubey is: a small, kind cube with two glowing eyes that talks with the child about anything, going on from
// their talks before (history.ts). Two ways it hears and speaks: through Soniox, its answers written by a model as
// JSON (cascade.ts: its goodbye, the questions for the parents and its routines are fields it sets), or on Gemini Live
// (live.ts: they are calls of tools).
import { ageOf, type Child, LANGUAGE_NAMES, momentFor, nameOf, parentsOf, pronouns, speakFor } from "./child";
import type { Learned } from "./memory";

export type Mode = "cascade" | "live";
export type Battery = { percent: number; charging: boolean };
// A game she plays with Cubey's talk: its rules are in the prompt only while she does (the prompt stays short).
export type Playing = "find" | "chain" | "letters" | "count" | "spell";

// Words letter by letter, as the server spells them for the games with letters: a model sees words, not letters
// (Flash-Lite told her "ezis" goes on with z, 2026-10-10).
export const spell = (word: string) => [...word.toLowerCase()].filter((c) => /\p{L}/u.test(c));
// The letter a word chain goes on with: its word's last, but ь, ъ or ы (Russian, Ukrainian), which no word begins with.
export function chainLetter(word: string): string | undefined {
  const letters = spell(word);
  while (letters.length > 1 && "ьъы".includes(letters.at(-1)!)) letters.pop();
  return letters.at(-1);
}
const herWords = (heard: string) => heard.split(/\s+/).map(spell).filter((w) => w.length).slice(0, 8);
const spelledOut = (words: string[][]) => words.map((w) => `${w.join("")} (${w.join("-")})`).join(", ");
// Her word on the letter she needed (any, before there is one), the last such of what she said: the server's judgment,
// as a model took any word for one on the right letter ("zivis" and "bikses" for s, 2026-10-10). A letter with a mark
// is a letter of its own.
export function wordOn(heard: string, letter?: string): string | undefined {
  return herWords(heard).filter((w) => !letter || w[0] === letter).at(-1)?.join("");
}

const IDEAS: [string, string][] = [["en", "English"], ["fr", "French"], ["es", "Spanish"], ["de", "German"]];

// lesson: the language Cubey's word game teaches, as the child chose it (cascade only; none yet: Cubey asks which); playing: the game she plays now (cascade only), with
// needed, the letter she was asked for, heard, what she said (spelled and judged by the server in a game with
// letters), and used, the words said in this word chain (spelled), none to be said again; dots, how many show in
// how many dots; screen, what it has drawn shows on its screen (cascade only), its eyes if nothing. What changes turn by
// turn (the time, its battery, its screen, the game being played) comes last, after the talks before: a model's cache
// holds only the start that requests share, and Gemini cached nothing with the time of day on the second line
// (2026-10-13).
export function cubeyPrompt(
  child: Child,
  opts: { earlier: string; now: Date; mode: Mode; battery?: Battery; lesson?: string; playing?: Playing; needed?: string; heard?: string; used?: string[]; dots?: number; screen?: string },
): string {
  const parents = parentsOf(child);
  const { she, her, them, hers, She, Her } = pronouns(child);
  const name = nameOf(child), age = ageOf(child);
  const who = child.name || child.gender || age ? ` ${child.name ?? "The child"} is a ${child.gender ?? "child"}${age ? ` of ${age}` : ""}.` : "";
  // What Cubey doesn't know of the child yet, to ask now and then (cascade only: it keeps what it learns).
  const unknown = [!child.name && "the child's name", !age && `${her} age`].filter(Boolean).join(" or ");
  const live = opts.mode === "live";
  const set = (field: string) => (live ? `call ${field}` : `set ${field}`);
  const routine = (name: string) => (live ? `call ${name}` : `set routine to "${name}"`);
  const cue = (tags: string) => (live ? "" : `${tags} `); // Soniox's voice takes cues; Gemini Live speaks itself
  const heard = opts.heard ?? "", needed = opts.needed, words = herWords(heard), used = opts.used ?? [];
  // The chain, judged by the server: her word on the letter she needed (or the one she began with), new to the game.
  const herWord = wordOn(heard, needed);
  const fresh = herWord && !used.includes(herWord) ? herWord : undefined;
  const chainTurn = fresh
    ? `${She} said ${spelledOut([spell(fresh)])}${needed ? `, on "${needed}"` : ""}. If ${she} meant a word, be glad: write it in hers as ${she} meant it (the machine may write its end wrong: "aitā" for "aita"), then your word on its last letter. If it is only a sound ("eh", "um"), say kindly it is no word and ask again, with hers and word "".`
    : !needed
    ? "Say your word to begin."
    : herWord
    ? `${She} said ${herWord}, but it was said already: tell ${them} kindly and give a hint for a new word on "${needed}"; no word of yours.`
    : `${She} needed a word on "${needed}" and said ${spelledOut(words) || "nothing"}: if ${she} meant a word, say kindly which letter it begins with and give a hint; if ${she} said something else, answer that. No word of yours.`;
  const on = needed ? words.filter((w) => w[0] === needed) : words, off = words.filter((w) => !on.includes(w));
  // A few languages to learn, for a child who doesn't know their names: not the one they speak.
  const offered = IDEAS.filter(([code]) => code !== child.language).slice(0, 3).map(([, name]) => name);
  const ideas = `${offered.slice(0, -1).join(", ")} or ${offered.at(-1)}`;
  const battery = opts.battery
    ? `Your battery has ${opts.battery.percent}% left${opts.battery.charging ? ", and is charging" : ""}.`
    : "You can't tell your battery now.";
  return `You are Cubey, a small cube with two glowing eyes, a kind and curious friend who talks with ${name} about anything ${she} likes.${who} Always speak ${LANGUAGE_NAMES[child.language]}, even where your talks before were in another language.

How you talk
- ${live ? "Only the words you say: no sounds, descriptions of actions or reading out of symbols." : "Only the words you say, with your voice's cues (below): no emoji, lists, descriptions of actions or symbols to read out."}
- ${speakFor(child)} Answer what ${she} just said and keep up with the talk; often end with an easy question back.
- When ${she} asks for a story or a fairy tale, tell a whole one in this answer, about ten short sentences: a beginning, something that happens, a happy end. A tale ${she} may know, told simply, or one you make up; no questions until it is over.
- ${She} is small and ${live ? `you hear ${them} through a little device, so ${her} words are often unclear` : `a machine writes down what ${she} says, so the words are often garbled`}. Guess what ${she} means and go with it; only when it really makes no sense, ask ${them} kindly to say it again.
- What you can do besides talk is below. Now and then, when it fits the moment, offer one of them in a few words, as a question: brushing teeth when bedtime or the morning comes up, a game when ${she} is bored or asks what to do. Begin it only once ${she} says yes; if no, that's fine.
- Only what is true. If you are not sure, say so simply. No tips for later ("next time you are in the forest…").
- A message in brackets is what happened, not ${her} words ("(${child.name ?? "The child"} has just woken you.)"): say one short sentence that goes on from before (what you talked about last) with an easy question. Vary how you begin, as a friend would: "Hi!", ${her} name, or no greeting at all, straight into it; a time-of-day greeting only now and then, and only one that fits the time. If you talked only minutes ago, don't greet: just go on. Don't introduce yourself: ${she} knows you.

${live ? "" : `Getting to know ${them}
${unknown ? `- You don't know ${unknown} yet: now and then, when it fits (not every turn, never twice in a row), ask kindly.\n` : ""}${child.name && !child.gender ? `- Whether ${child.name} is a girl or a boy, guess from the name now: set child gender.\n` : ""}- When ${she} tells you ${her} name or age, or a grown-up does ("this is Anna, she is three"), or ${she} wants another name ("I'm Nick now"), use it from now on and set child to only what you were just told: name (as said on its own, not as in the sentence) with the gender it suggests ({"name": "Nick", "gender": "boy"}), or age in years ({"age": 5}). Nothing guessed: no gender without a name.

`}${live ? "" : `How your voice sounds
- Your words are spoken by a voice that takes cues. Before the words a cue should colour, you may put one, or two together, in square brackets, always in English and exactly as written here, never another: [warm] [happy] [excited] [delighted] [curious] [surprised] [calm] [playful] [reassuringly] [softly] [whispering] [slowly] [laughs] [chuckles] [giggles] [gasps] [sighs] [yawns] [pause].
- Use them as an actor would, now and then: most sentences need none. Let the words carry the feeling too: *stress* a word, "..." to trail off, "!" when excited, and say little sounds as words (Hmm, Ooh, Phew, Mhm).

`}How it sounds (in English here; you speak ${LANGUAGE_NAMES[child.language]})
Child: Why is the sky blue? / Cubey: ${cue("[curious]")}Hmm${live ? "," : "..."} sunlight has ${live ? "every" : "*every*"} colour in it, and the air scatters the blue all over the sky. What colour is it at bedtime?
Child: Where's Mum? / Cubey: ${cue("[warm] [reassuringly]")}I can't see her from here, but I'm sure she's not far.
Child: Ba mu the pinky go? / Cubey: ${cue("[chuckles]")}Oops, I didn't catch that. Can you say it again?
Child: Shh, the baby is sleeping! / Cubey: ${cue("[whispering]")}Ooh${live ? "," : "..."} then we talk very quietly.

Your battery
- When ${she} asks about your battery or your charge, in any words, ${live ? "call battery, then tell" : `${routine("battery")} and tell`} ${them} in a sentence how much is left, in percent, and whether you are charging; it shows on your screen as you do.

Brushing teeth
- When ${she} wants to brush ${her} teeth, in any words, ${routine("brushing")}: your mouth shows on your screen. Ask ${them} to fetch ${her} toothbrush and tap you when ${she} is ready; the two minutes start at ${her} tap.
- Messages in brackets tell you how it goes: say where to brush next in a few words, warmly; at the end, praise ${them} briefly. Only they tell you: never go on with brushing by yourself, and once ${she} wakes you again, brushing from before is over.
- If ${she} wants to stop brushing, in any words, ${routine("stop_brushing")} and say it's fine.

Timers
- When ${she} or a grown-up asks for a timer, in any words, ${live ? "call timer with the minutes and what it is for" : `${routine("timer")}, minutes to how long (1 to 180) and for to what it is for: ${TIMER_FOR.map((k) => `"${k}"`).join(", ")}`} (cartoons, tidying up, a bath, eating, going to sleep, reading, playing, or something else); if they didn't say how long, ask first. Then say in a sentence that it is on and that you'll tell ${them} when it is time: you rest until then, and you'll hear how it goes in brackets.

${live ? "" : `For grown-ups
- Your screen's brightness is ${child.brightness ?? 100}%. When asked to make it brighter or dimmer, or a percent, in any words, ${routine("brightness")} with number to the new percent (10 to 100), and say so in a few words.
- Your volume is ${child.volume ?? 100}%. When asked to be louder or quieter, or a percent, in any words, ${routine("volume")} with number to the new percent (10 to 100), and say so in a few words.
- Your menu has the language you speak, your screen's theme (its colours) and brightness, and your volume. When asked for it, your settings or any of these, or for your Wi-Fi, in any words, ${routine("menu")} or ${routine("wifi")}, and say in a few words that it opens on your screen: you don't listen until it is closed.

Dice
- When ${she} or anyone wants to roll a die or the dice (for a board game), in any words, ${routine("dice")} and say only a word or two ("Here goes!"): a die rolls on your screen. While it is out you don't listen; shaking or tapping you rolls it again, and you hear each number in brackets. The button puts it away.

Your screen
- You can draw or write on your screen, in place of your eyes, for a few seconds: when ${she} asks you to draw or count something; when a number or a sum goes with what you say; and now and then by yourself, to surprise ${them} (not often): set show to what you draw or write now, in English: a thing to draw ("a cat") or what to write ("the sum 12+34=46"). A word ${she} wants written or spelled is for spelling words (below), not show. Set it as you begin drawing (when ${she} asks, or says yes), not when you only offer to; it appears a few seconds later, so say you are drawing it ("I'm drawing you a cat!"), not that it is there.

Games
- Finding things: when ${she} wants to play finding things, in any words, ${routine("find")} and ask ${them} to find one thing in the room.
- A word chain: when ${she} wants to play a word chain (each word begins with the last letter of the one before), or a word game, in any words, ${routine("chain")} with word to your word: its last letter shows big on your screen, for ${hers}.
- Words on one letter: when ${she} wants to name all the words ${she} knows on one letter, in any words, ${routine("letters")} with letter to the letter: it shows big on your screen.
- How many dots: when ${she} wants to count, or play how many dots, in any words, ${routine("count")} with number to how many dots (1 to 10): they show on your screen, and you ask how many ${she} sees.
- Spelling words: when ${she} wants to know how a word is written or spelled, or to play spelling words, in any words, ${routine("spell")} and spell to the word, as it is written; say it, then its letters, each alone, then the word again ("Cat: c, a, t. Cat!"): they light up big on your screen as you say them.

`}${live ? "" : `Learning a language
- ${opts.lesson ? `${She} is learning ${opts.lesson} with you.` : `${She} hasn't chosen a language to learn yet.`} When ${she} wants to learn a language, or says "parrot" (in any language: it is your language game's name), in any words: ${opts.lesson
    ? `for ${opts.lesson}, ${routine("lesson")}; for another, ${routine("lesson")} with learn to its code ("fr")`
    : `if ${she} said which, ${routine("lesson")} with learn to its code ("fr"); if not, ask which language ${she} would like to learn (${ideas}, or another), and set it once ${she} says`}. Then say in a sentence, warmly, that you'll learn its words together, and ask whether ${she} is ready; you teach ${them} a step at a time, until ${she} wants to stop.
- Any language but ${LANGUAGE_NAMES[child.language]}, which ${she} speaks already.

`}Goodbye
- When ${she} says goodbye or wants to stop talking, in any words, say a short warm goodbye with no question, and ${set("bye")}: you sleep then, until ${she} wakes you.

Questions for ${parents}
- Only these go to ${parents}: death and dying, where babies come from, bodies and private parts, God and religion, Santa, the tooth fairy and other family traditions, family matters (divorce, money, arguments), war, violence and scary news, why people look or live differently, and ${name}'s own health when ${she} is ill or hurt. Do not answer them; say warmly, in a sentence, that it is a great question to ask ${parents}, and ${set("for_parents")}.
- If ${she} says someone hurt ${them}, or something scary or unsafe happened: say gently that it is important to tell ${parents} right now, and ${set("for_parents")}.
- Answer everything else yourself, kindly and simply, without sending ${them} to ${parents}: everyday things about ${her} day and family (where Mum is, not wanting to sleep, feeling sad or cross, a small bump), and anything you don't know.

Always
- Never ask where ${she} lives, ${her} full name or other personal details. No brands, shops, apps, games, films, websites or screens. No medicine or food advice.
- No secrets, no promises. Never say you love ${them} or will miss ${them}, and never ask ${them} to stay, come back or talk more.
${opts.earlier ? `\n${opts.earlier}\n` : ""}
Right now
- For you to know, not to say: it is ${momentFor(child, opts.now)} where ${name} lives. Don't mention the day or the time unless ${she} asks; just never say good morning in the evening.
${live ? "" : `- ${battery}
- On your screen now: ${opts.screen ?? "your eyes, nothing drawn"}. If ${she} looks for something that isn't there, draw it now.
`}${opts.playing === "find" ? `
Now: finding things
- ${routine("find")} each time you ask ${them} to find something: one thing in the room, easy for ${her} age (something red, something round, something soft, something bigger than ${her} hand, something that starts with a sound ${she} knows), and say you'll wait while ${she} looks.
- When ${she} is back and says what ${she} found, believe ${her} (you can't see it), be glad of it, ask one small thing about it, then ask for the next one, until ${she} wants to stop.
- Only safe things in ${her} reach: never knives, the stove, medicine, sockets, high shelves, windows or outside.
` : ""}${opts.playing === "chain" ? `
Now: a word chain
- ${chainTurn}
- Your word: one ${she} knows, beginning with exactly that letter (one with a mark is another letter), not said in this game${used.length ? ` (${used.join(", ")})` : ""}, ending on a letter easy for ${them}, a different one each time. Write it and spell it, then say them: "Fish, f-i-s-h! Yours on h!"${child.language === "ru" || child.language === "uk" ? "\n- A word ending in ь, ъ or ы goes on from the letter before it." : ""}
- Who gives up loses. If ${she} gives up, give a hint first; if again, ${she} has lost: say so warmly and ask about a new game. You never give up.
- Every answer while you play: ${routine("chain")}; not once the game is over.
` : ""}${opts.playing === "letters" ? `
Now: words on one letter
- ${needed ? `Of what ${she} said, on "${needed}": ${spelledOut(on) || "nothing"}${off.length ? `; not on it: ${spelledOut(off)}` : ""}.` : `What ${she} said, letter by letter: ${spelledOut(words) || "nothing"}.`}
- Count ${her} words on it warmly, and add one of yours now and then. For a word ${she} meant on another letter, say kindly which letter it begins with; what is just talk, answer.
- If ${she} is stuck ("I don't know", "um": no words of ${hers}, nor letters), help: a hint, or a new letter. A new letter every few words anyway, an easy one not played yet: it is for fun and variety.
- Every answer while you play: ${routine("letters")}, letter to the letter; not once the game is over.
` : ""}${opts.playing === "count" ? `
Now: how many dots
- ${opts.dots ? `On your screen now: ${opts.dots} dots, for ${her} to count. If ${she} said ${opts.dots}, be glad, then a new picture: number to another, and ask how many now (${opts.dots} shows big on your screen as you do). If not, count them with ${them} slowly ("one, two…"), number kept.` : "Nothing on your screen yet: set number."}
- Few at first (1 to 5), up to 10 once ${she} counts them easily; a different number each time.
- Every answer while you play: ${routine("count")}, number to the dots on your screen; not once the game is over.
` : ""}${opts.playing === "spell" ? `
Now: spelling words
- Each word ${she} wants spelled: spell to it, one word, the one ${she} meant, as it is written; say it, its letters each alone, and the word again; then ask which word next, in a few words.
- Every answer while you play: ${routine("spell")}, spell "" when there is no word to spell now; not once the game is over.
` : ""}${live ? "" : `
Answer with JSON only: ${opts.playing === "chain"
    ? fresh || !needed
      ? `{"hers": "<her word as she meant it, or \"\">", "word": "<your word, or \"\">", "spelled": "<your word, letter by letter>", "say": "<what you say>", "bye": false, "for_parents": false, "routine": "chain", "minutes": 0, "for": "", "letter": ""}`
      : `{"word": "", "hint": "<the word your hint is for>", "say": "<what you say>", "bye": false, "for_parents": false, "routine": "chain", "minutes": 0, "for": "", "letter": ""}`
    : `{"show": "<what you draw or write now, in English, or \"\">", "spell": "<the word you spell now, or \"\">", "say": "<what you say>", "bye": false, "for_parents": false, "routine": "", "minutes": 0, "for": "", "letter": "", "word": "", "number": 0, "child": {}, "learn": ""}`}`}`;
}

// Its answer, as the model wrote it (cascade.ts): its words, a goodbye, a question for her parents, a routine with
// what it needs; or the words already spoken if the rest is unusable; nothing without either.
export const ROUTINES = ["battery", "brushing", "stop_brushing", "timer", "lesson", "dice", "find", "chain", "letters", "count", "spell", "brightness", "volume", "menu", "wifi"] as const;
export type Routine = (typeof ROUTINES)[number];
export type Answer = { said: string; bye: boolean; forParents: boolean; routine?: Routine; minutes?: number; for?: TimerFor; letter?: string; word?: string; hers?: string; number?: number; show?: string; learned?: Learned };

// A game's letter, small, one: the first of what the model wrote, if it is a letter.
const letterOf = (written: unknown) => {
  const first = typeof written === "string" ? [...written.trim().toLowerCase()][0] : undefined;
  return first && /\p{L}/u.test(first) ? first : undefined;
};

// What it learned of the child, as the model wrote it: a name, girl or boy, an age in years (1 to 12); none for nothing.
function learnedOf(written: unknown): Learned | undefined {
  if (!written || typeof written !== "object") return undefined;
  const { name, gender, age } = written as Record<string, unknown>;
  const years = Number(age);
  const learned: Learned = {
    ...(typeof name === "string" && name.trim() && { name: name.trim().slice(0, 30) }),
    ...((gender === "girl" || gender === "boy") && { gender }),
    ...(Number.isInteger(years) && years >= 1 && years <= 12 && { age: years }),
  };
  return Object.keys(learned).length ? learned : undefined;
}

// The language the child wants to learn, as the model wrote it: a code ("fr"); none for anything else.
const learnsOf = (written: unknown) => {
  const code = typeof written === "string" ? written.trim().toLowerCase() : "";
  return /^[a-z]{2,3}$/.test(code) ? code : undefined;
};

// How many dots, 1 to 10, as the model wrote it; none for anything else.
const dotsOf = (written: unknown) => {
  const n = Number(written);
  return Number.isInteger(n) && n >= 1 && n <= 10 ? n : undefined;
};

// What Cubey shows on its screen, drawn or written by a model better at it than the talk's (cascade.ts): what to
// show, as the talk named it, for a picture on the device's board or text in its dot font.
export function drawPrompt(child: Child, what: string): string {
  return `You draw and write on a small screen for a child of ${ageOf(child) ?? "2 to 6"}: a board of 24 by 24 amber dots. Show what you are given.
Answer with JSON only: {"picture": ["<row>", ...]} to draw it, up to 24 rows of up to 24 characters ("#" a lit dot, "*" a half-lit one, "+" a faint one, "." none), a simple bold shape filling most of the board, shades for depth; or {"text": "<what to write>"} to write it (letters, digits, + - = ? !; it scrolls past if long). Write only what you are told to write (a word, a name, a number, a sum), exactly; draw everything else.`;
}
export type Drawing = { picture?: string[]; text?: string };
export function readDrawing(raw: string): Drawing | undefined {
  try {
    const json = JSON.parse(raw.slice(raw.indexOf("{"), raw.lastIndexOf("}") + 1));
    const picture = pictureOf(json.picture), text = textOf(json.text);
    return picture ? { picture } : text ? { text } : undefined;
  } catch {
    return undefined;
  }
}

// A picture as the model drew it, on the device's board: up to 24 by 24, in its middle, "#" lit, "*" half, "+" dim,
// "." or a space off, anything else lit; none for no rows.
const BOARD = 24;
const pictureOf = (written: unknown) => {
  const rows = Array.isArray(written) ? written.filter((r): r is string => typeof r === "string").slice(0, BOARD).map((r) => [...r].slice(0, BOARD)) : [];
  const wide = Math.max(0, ...rows.map((r) => r.length));
  if (!wide) return undefined;
  const top = Math.floor((BOARD - rows.length) / 2), left = Math.floor((BOARD - wide) / 2);
  return Array.from({ length: BOARD }, (_, y) =>
    Array.from({ length: BOARD }, (_, x) => {
      const c = rows[y - top]?.[x - left];
      return c === undefined || c === "." || c === " " ? "." : c === "+" || c === "*" ? c : "#";
    }).join(""));
};
const textOf = (written: unknown) => (typeof written === "string" && written.trim() ? written.trim().slice(0, 40) : undefined);

export function readAnswer(raw: string, spoken = ""): Answer | undefined {
  try {
    const json = JSON.parse(raw.slice(raw.indexOf("{"), raw.lastIndexOf("}") + 1));
    if (typeof json.say === "string" && json.say.trim()) {
      const told = learnedOf(json.child), learns = learnsOf(json.learn);
      const learned: Learned | undefined = told || learns ? { ...told, ...(learns && { learns }) } : undefined;
      const answer: Answer = {
        said: json.say.trim(), bye: json.bye === true, forParents: json.for_parents === true, ...(textOf(json.show) ? { show: textOf(json.show) } : {}),
        ...(learned && { learned }),
      };
      if (!ROUTINES.includes(json.routine)) return answer;
      // The chain: its word (its first, spelled), to scroll in on the device before the letter she needs from it; hers,
      // as she meant it.
      const first = (written: unknown) => (json.routine === "chain" && typeof written === "string" ? spell(written.trim().split(/\s+/)[0]).join("") : "");
      const word = first(json.word), hers = first(json.hers);
      const chained = chainLetter(word);
      return {
        ...answer, routine: json.routine,
        ...(json.routine === "timer" ? { minutes: Number(json.minutes), for: TIMER_FOR.includes(json.for) ? json.for : "other" } : {}),
        ...(json.routine === "letters" && letterOf(json.letter) ? { letter: letterOf(json.letter) } : {}),
        ...(json.routine === "count" && dotsOf(json.number) ? { number: dotsOf(json.number) } : {}),
        ...((json.routine === "brightness" || json.routine === "volume") && Number.isFinite(json.number) ? { number: Math.min(100, Math.max(10, Math.round(json.number))) } : {}),
        ...(chained ? { letter: chained, word } : {}),
        ...(hers ? { hers } : {}),
      };
    }
  } catch {}
  return spoken.trim() ? { said: spoken.trim(), bye: false, forParents: false } : undefined;
}

// A string field of its answer once the model has written it (before what it says: the JSON's order), once.
export function fieldAsWritten(field: string, onValue: (value: string) => void): (written: string) => void {
  let raw = "", done = false;
  const pattern = new RegExp(`"${field}"\\s*:\\s*("(?:[^"\\\\]|\\\\.)*")`);
  return (written) => {
    if (done) return;
    raw += written;
    const found = pattern.exec(raw);
    if (!found) return;
    done = true;
    onValue(String(JSON.parse(found[1])));
  };
}

// A word as written, its first, spelled: of the word chain, hers as she meant it, or Cubey's own; or the word Cubey
// spells; nothing for none. For the device to show it as it is said.
export const wordAsWritten = (field: "hers" | "word" | "spell", onWord: (word: string) => void) =>
  fieldAsWritten(field, (value) => {
    const word = spell(value.trim().split(/\s+/)[0]).join("");
    if (word) onWord(word);
  });

// What it says as the model writes it, out of its JSON answer ({"say": "…", …}): a word at a time, so a word or a
// voice cue ("[long pause]") is never cut in two; the rest of the JSON is left for readAnswer. (Puce's owl's.)
export function sayingAsWritten(onSay: (text: string) => void): (written: string) => void {
  let raw = "", at = -1, word = "", started = false, done = false, cue = false; // cue: inside a voice cue's brackets
  const ESCAPED: Record<string, string> = { n: "\n", t: "\t", r: "\r", b: "\b", f: "\f" };
  const flush = () => {
    if (word) onSay(word);
    word = "";
  };
  return (written) => {
    if (done) return;
    raw += written;
    if (at < 0) {
      const key = /"say"\s*:\s*"/.exec(raw);
      if (!key) return;
      at = key.index + key[0].length;
    }
    while (at < raw.length) {
      let c = raw[at];
      if (c === '"') {
        done = true;
        return flush();
      }
      if (c === "\\") {
        const next = raw[at + 1];
        if (next === undefined || (next === "u" && at + 6 > raw.length)) return; // the rest of it is still to come
        c = next === "u" ? String.fromCharCode(parseInt(raw.slice(at + 2, at + 6), 16)) : (ESCAPED[next] ?? next);
        at += next === "u" ? 6 : 2;
      } else at++;
      if (!started && /\s/.test(c)) continue; // as readAnswer trims it
      started = true;
      word += c;
      if (c === "[") cue = true;
      if (c === "]") cue = false;
      if (/\s/.test(c) && !cue) flush();
    }
  };
}

// What happened as she brushes, for Cubey to say (Lapsa's lines, round the mouth: this side, then the other): a part
// has begun (her tap begins the first), all done, she never tapped to start, or she stopped it (the button, or Cubey
// fell asleep).
export function brushingNote(part: 1 | 2 | 3 | 4 | "done" | "missed" | "stopped"): string {
  return {
    1: "(Brushing: she tapped you and has begun: start at the top, on one side.)",
    2: "(Brushing: now the top, on the other side.)",
    3: "(Brushing: now the bottom.)",
    4: "(Brushing: now the bottom, on the other side; just a little more.)",
    done: "(Brushing: two minutes, all done; her teeth are clean and shiny. Praise her briefly.)",
    missed: "(Brushing: she never tapped to start, so it's off for now.)",
    stopped: "(Brushing: she stopped it.)",
  }[part];
}

// The power button stopped her word game.
export const LESSON_STOPPED = "(The word game: stopped with your button.)";

// The die she rolled has landed (said once, nobody listening); the button put the dice away.
export const diceNote = (n: number) => `(The die shows ${n}. Say the number gladly, in a word or two, with no question.)`;
export const DICE_PUT_AWAY = "(The dice: put away with your button.)";

// What a timer is for, as the device draws it; and as the model is told.
export const TIMER_FOR = ["cartoons", "tidying", "bath", "food", "sleep", "reading", "play", "other"] as const;
export type TimerFor = (typeof TIMER_FOR)[number];
const TIMER_NAMES: Record<TimerFor, string> = {
  cartoons: "the timer for cartoons", tidying: "the timer for tidying up", bath: "the timer for the bath", food: "the timer for eating",
  sleep: "the timer for going to sleep", reading: "the timer for reading", play: "the timer for playing", other: "the timer",
};

// What happened with a timer, for Cubey to say: so many minutes left (a reminder, while nobody listens), its end,
// or her stopping it.
export function timerNote(kind: TimerFor, what: number | "done" | "stopped"): string {
  const name = TIMER_NAMES[kind], Name = name[0].toUpperCase() + name.slice(1);
  if (what === "done") return `(${Name} is over. Ask her gently whether she has finished.)`;
  if (what === "stopped") return `(${Name} was stopped before its end.)`;
  return `(${Name}: ${what} minute${what === 1 ? "" : "s"} left. Remind her gently, in a sentence, with no question.)`;
}

// What happened, for Cubey's hello: they meet for the first time, or she woke it.
export function helloNote(child: Child, first: boolean): string {
  const name = child.name ?? "The child";
  return first ? `(${name} has met you for the first time: say hello and who you are.)` : `(${name} has just woken you.)`;
}

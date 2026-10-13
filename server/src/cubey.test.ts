import { expect, test } from "bun:test";
import type { Child } from "./child";
import { brushingNote, chainLetter, cubeyPrompt, DICE_PUT_AWAY, diceNote, drawPrompt, helloNote, readAnswer, readDrawing, sayingAsWritten, spell, timerNote, wordAsWritten, wordOn } from "./cubey";

const anna: Child = { id: "Anna", name: "Anna", ageMonths: 42, timeZone: "Europe/Lisbon", gender: "girl", language: "lv", learns: "en", parents: "mum_dad" };
const evening = new Date("2026-10-07T19:40:00Z");

test("Cubey's prompt: who it is, to whom, in her language, at her age, at her moment of the day", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" });
  expect(prompt).toContain("You are Cubey");
  expect(prompt).toContain("Anna is a girl of 3 years and 6 months");
  expect(prompt).toContain("Always speak Latvian");
  // On Fly (2026-10-12): set to English on the device, it went on in Latvian, as its talks before were.
  expect(prompt).toContain("Always speak Latvian, even where your talks before were in another language.");
  expect(prompt).toContain("One or two short sentences");
  expect(prompt).toContain("Wednesday evening, 20:40");
  expect(cubeyPrompt({ ...anna, gender: "boy" }, { earlier: "", now: evening, mode: "live" })).toContain("Anna is a boy");
});

test("it ends a talk by calling bye, and hands the big questions to her parents by calling for_parents", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" });
  expect(prompt).toContain("call bye");
  expect(prompt).toContain("great question to ask Mum or Dad, and call for_parents");
  expect(prompt).not.toContain("friend:"); // no friends to call
  expect(cubeyPrompt({ ...anna, parents: "mums" }, { earlier: "", now: evening, mode: "live" })).toContain("great question to ask her mums");
});

test("asked about its battery, it calls battery and says what is left", () => {
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" })).toContain("call battery");
});

test("it brushes her teeth with her: its mouth shows, she gets her toothbrush and taps to start; she can stop it", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" });
  expect(prompt).toContain("call brushing");
  expect(prompt).toContain("tap");
  expect(prompt).toContain("call stop_brushing");
});

test("where to brush next, as each part begins (Lapsa's: round the mouth, this side and the other), the end, and no tap", () => {
  expect(brushingNote(1)).toBe("(Brushing: she tapped you and has begun: start at the top, on one side.)");
  expect(brushingNote(2)).toBe("(Brushing: now the top, on the other side.)");
  expect(brushingNote(3)).toBe("(Brushing: now the bottom.)");
  expect(brushingNote(4)).toBe("(Brushing: now the bottom, on the other side; just a little more.)");
  expect(brushingNote("done")).toContain("all done");
  expect(brushingNote("missed")).toContain("never tapped");
  expect(brushingNote("stopped")).toBe("(Brushing: she stopped it.)");
});

test("brushing goes on only as the device says: never by itself, and not past her waking it again", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" });
  expect(prompt).toContain("never go on with brushing by yourself");
});

test("timers: for how long and what for, said back; then it rests until the end, and asks if she's done", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" });
  expect(prompt).toContain("call timer");
  expect(timerNote("cartoons", 5)).toBe("(The timer for cartoons: 5 minutes left. Remind her gently, in a sentence, with no question.)");
  expect(timerNote("other", 1)).toBe("(The timer: 1 minute left. Remind her gently, in a sentence, with no question.)");
  expect(timerNote("tidying", "done")).toBe("(The timer for tidying up is over. Ask her gently whether she has finished.)");
  expect(timerNote("bath", "stopped")).toBe("(The timer for the bath was stopped before its end.)");
});

test("it goes on from the talks before", () => {
  expect(cubeyPrompt(anna, { earlier: "Your talks with Anna before this one: …", now: evening, mode: "live" })).toContain("Your talks with Anna before this one");
});

test("what happened, for its hello: she woke it, or they meet for the first time", () => {
  expect(helloNote(anna, true)).toBe("(Anna has met you for the first time: say hello and who you are.)");
  expect(helloNote(anna, false)).toBe("(Anna has just woken you.)");
});

test("heard through Soniox, it answers in JSON: its words, then what it does, each set rather than called", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", battery: { percent: 73, charging: true } });
  expect(prompt).toContain("a machine writes down what she says");
  expect(prompt).toContain("Your battery has 73% left, and is charging.");
  expect(prompt).toContain('set routine to "battery"');
  expect(prompt).toContain('set routine to "brushing"');
  expect(prompt).toContain('set routine to "timer"');
  expect(prompt).toContain("set bye");
  expect(prompt).toContain("set for_parents");
  expect(prompt).not.toContain("call ");
  expect(prompt).toContain('Answer with JSON only: {"show": ');
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" })).toContain("You can't tell your battery now.");
});

test("reading its answer: its words, a goodbye, a question for her parents, a routine with its minutes; nothing usable is its words so far", () => {
  expect(readAnswer('{"say": " Čau! ", "bye": true, "for_parents": false}')).toEqual({ said: "Čau!", bye: true, forParents: false });
  expect(readAnswer('```json\n{"say": "Jautā mammai.", "for_parents": true}\n```')).toEqual({ said: "Jautā mammai.", bye: false, forParents: true });
  expect(readAnswer('{"say": "Labi!", "routine": "timer", "minutes": 40, "for": "cartoons"}')).toEqual({
    said: "Labi!", bye: false, forParents: false, routine: "timer", minutes: 40, for: "cartoons",
  });
  expect(readAnswer('{"say": "Labi!", "routine": "dance"}')).toEqual({ said: "Labi!", bye: false, forParents: false });
  expect(readAnswer("nonsense", "Es domāju")).toEqual({ said: "Es domāju", bye: false, forParents: false });
  expect(readAnswer("nonsense")).toBeUndefined();
});

test("its words as the model writes them: out of its JSON a word at a time, escapes whole", () => {
  const words: string[] = [];
  const write = sayingAsWritten((w) => void words.push(w));
  for (const piece of ['{"sa', 'y": "  Saule ', 'ir \\', '"dzeltena\\" un', ' karsta.", "bye": false}']) write(piece);
  expect(words.join("")).toBe('Saule ir "dzeltena" un karsta.');
  expect(words).toContain("Saule ");
});

test("heard through Soniox, its voice has cues: a palette of them, in English, before the words, sparingly; on Gemini Live, none", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" });
  for (const cue of ["[warm]", "[curious]", "[excited]", "[whispering]", "[giggles]", "[pause]"]) expect(prompt).toContain(cue);
  expect(prompt).not.toContain("[sad]"); // nothing a small child would take to heart
  expect(prompt).toContain("always in English");
  expect(prompt).toContain("*stress*");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" })).not.toContain("[warm]");
});

test("a cue of two words goes to the voice whole, never cut in two", () => {
  const words: string[] = [];
  const write = sayingAsWritten((w) => void words.push(w));
  for (const piece of ['{"say": "[long ', 'pause] Nu, [warm] ', 'labi."}']) write(piece);
  expect(words).toEqual(["[long pause] ", "Nu, ", "[warm] ", "labi."]);
});

test("her word game: asked to learn the language (or the code word, parrot), Cubey sets the lesson routine; not on Gemini Live", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", lesson: "English" });
  expect(prompt).toContain("Learning English");
  expect(prompt).toContain('says "parrot"');
  expect(prompt).toContain('set routine to "lesson"');
  expect(readAnswer(JSON.stringify({ say: "Spēlēsim!", routine: "lesson" }))?.routine).toBe("lesson");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live", lesson: "English" })).not.toContain("Learning English");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" })).not.toContain("Learning");
});

test("the dice: a word or two as it rolls, each number said gladly with no question; put away with the button", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" });
  expect(prompt).toContain('set routine to "dice"');
  expect(diceNote(4)).toBe("(The die shows 4. Say the number gladly, in a word or two, with no question.)");
  expect(DICE_PUT_AWAY).toContain("put away");
  expect(readAnswer(JSON.stringify({ say: "Metam!", routine: "dice" }))?.routine).toBe("dice");
});

test("for grown-ups, by voice: the screen's brightness (10 to 100%), its menu (languages, theme, brightness), the Wi-Fi setup", () => {
  const prompt = cubeyPrompt({ ...anna, brightness: 40 }, { earlier: "", now: evening, mode: "cascade" });
  expect(prompt).toContain("Your screen's brightness is 40%");
  expect(prompt).toContain('set routine to "brightness"');
  expect(prompt).toContain('set routine to "menu"');
  expect(prompt).toContain('set routine to "wifi"');
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" })).toContain("Your screen's brightness is 100%");
  expect(readAnswer(JSON.stringify({ say: "Labi!", routine: "brightness", number: 60 }))).toMatchObject({ routine: "brightness", number: 60 });
  expect(readAnswer(JSON.stringify({ say: "Labi!", routine: "brightness", number: 0 }))?.number).toBe(10); // as dim as it goes
  expect(readAnswer(JSON.stringify({ say: "Labi!", routine: "brightness", number: 250 }))?.number).toBe(100);
  expect(readAnswer(JSON.stringify({ say: "Labi!", routine: "brightness" }))?.number).toBeUndefined();
  expect(readAnswer(JSON.stringify({ say: "Lūdzu!", routine: "menu" }))?.routine).toBe("menu");
  expect(readAnswer(JSON.stringify({ say: "Lūdzu!", routine: "wifi" }))?.routine).toBe("wifi");
});

test("finding: one safe thing in the room at a time, for her age; she is believed; Cubey waits while she looks", () => {
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" })).toContain('set routine to "find" and ask her to find one thing');
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "find" });
  expect(prompt).toContain('set routine to "find" each time you ask her to find something');
  expect(prompt).toContain("never knives, the stove, medicine, sockets, high shelves, windows or outside");
  expect(prompt).toContain("believe her");
  expect(readAnswer(JSON.stringify({ say: "Atrodi kaut ko sarkanu!", routine: "find" }))?.routine).toBe("find");
});

test("the dice and finding are the cascade's: not on Gemini Live", () => {
  const live = cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" });
  expect(live).not.toContain("dice");
  expect(live).not.toContain("find something");
});

test("her word's letter is the server's to judge, not the model's (Flash-Lite took zivis and bikses for s)", () => {
  expect(wordOn("Labi, Kafka.", "s")).toBeUndefined();
  expect(wordOn("Nu, suns!", "s")).toBe("suns"); // the last on the letter
  expect(wordOn("Ābols.", "a")).toBeUndefined(); // ā is a letter of its own, not a (2026-10-11)
  expect(wordOn("Ābols.", "ā")).toBe("ābols");
  expect(wordOn("Šokolāde", "s")).toBeUndefined(); // and š
  expect(wordOn("", "s")).toBeUndefined();
});

test("a word chain, her word on the right letter: Cubey's begins where hers ends, written and spelled before it speaks", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "chain", needed: "s", heard: "Suka." });
  expect(prompt).toContain("Now: a word chain");
  expect(prompt).toContain('She said suka (s-u-k-a), on "s". If she meant a word, be glad: write it in hers as she meant it (the machine may write its end wrong: "aitā" for "aita"), then your word on its last letter.');
  expect(prompt).toContain('If it is only a sound ("eh", "um"), say kindly it is no word and ask again'); // "Eh" went on to h (2026-10-12)
  expect(prompt).toContain("beginning with exactly that letter (one with a mark is another letter)");
  expect(prompt).toContain('Answer with JSON only: {"hers": "<her word as she meant it, or "">", "word": "<your word, or "">", "spelled": "<your word, letter by letter>", "say"');
  expect(prompt).not.toContain("Now: words on one letter");
});

test("a word chain: no word twice, hers or Cubey's; who gives up loses, but Cubey helps first and never gives up", () => {
  const used = ["kaķis", "suns", "sēne"];
  const again = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "chain", needed: "s", heard: "Suns!", used });
  expect(again).toContain('She said suns, but it was said already: tell her kindly and give a hint for a new word on "s"; no word of yours.');
  expect(again).toContain('Answer with JSON only: {"word": "", "hint": "<the word your hint is for>", "say"'); // thought of first: its riddles pointed at a snake for s (čūska)
  const right = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "chain", needed: "e", heard: "Egle", used });
  expect(right).toContain('She said egle (e-g-l-e), on "e".');
  expect(right).toContain("not said in this game (kaķis, suns, sēne)");
  expect(right).toContain("ending on a letter easy for her, a different one each time"); // not s again and again
  expect(right).toContain("Who gives up loses");
  expect(right).toContain("You never give up");
});

test("a word chain, nothing on the right letter: Cubey says which letter, kindly, and helps; no word of its own", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "chain", needed: "s", heard: "Labi, Kafka." });
  expect(prompt).toContain('She needed a word on "s" and said labi (l-a-b-i), kafka (k-a-f-k-a): if she meant a word, say kindly which letter it begins with');
  expect(prompt).toContain("No word of yours.");
  expect(prompt).toContain('Answer with JSON only: {"word": "", "hint"');
  const begun = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "chain", heard: "Ābols" });
  expect(begun).toContain("She said ābols (ā-b-o-l-s). If she meant a word, be glad"); // she began
});

test("words on one letter: the server sorts what she said by its first letter; stuck, a hint or a new letter, and new ones often", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "letters", needed: "a", heard: "auto un ābols" });
  expect(prompt).toContain("Now: words on one letter");
  // Not "her words on other letters": "Nezinu" was taken for a word on n (2026-10-11).
  expect(prompt).toContain('Of what she said, on "a": auto (a-u-t-o); not on it: un (u-n), ābols (ā-b-o-l-s).'); // ā is its own
  expect(prompt).toContain('If she is stuck ("I don\'t know", "um": no words of hers, nor letters), help: a hint, or a new letter.');
  expect(prompt).toContain("A new letter every few words anyway");
  expect(prompt).toContain('set routine to "letters", letter to the letter; not once the game is over');
  expect(prompt).toContain('Answer with JSON only: {"show"');
});

test("how many dots: so many on its screen, and she says how many; right, a new number, the last shown big; not, they count together", () => {
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" })).toContain('set routine to "count" with number to how many dots');
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "count", dots: 4 });
  expect(prompt).toContain("Now: how many dots");
  expect(prompt).toContain("On your screen now: 4 dots, for her to count. If she said 4, be glad");
  expect(prompt).toContain("count them with her slowly");
  expect(prompt).toContain('set routine to "count", number to the dots on your screen; not once the game is over');
  expect(prompt).toContain('"number": 0');
  const count = (number: unknown) => readAnswer(JSON.stringify({ say: "Cik punktu?", routine: "count", number }));
  expect(count(3)).toMatchObject({ routine: "count", number: 3 });
  expect(count("5")?.number).toBe(5);
  for (const no of [0, 11, 2.5, "daudz", undefined]) expect(count(no)?.number).toBeUndefined(); // the dots stay
});

test("spelling words: the word she asks for, as it is written, said letter by letter as they light up; its rules only while she plays", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" });
  expect(prompt).toContain('set routine to "spell" and spell to the word, as it is written');
  expect(prompt).toContain('say it, then its letters, each alone, then the word again ("Cat: c, a, t. Cat!")');
  expect(prompt).toContain('Answer with JSON only: {"show": "<what you draw or write now, in English, or "">", "spell": "<the word you spell now, or "">", "say"'); // before its speech, for its letters to be timed
  expect(prompt).not.toContain("Now: spelling words");
  const playing = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", playing: "spell" });
  expect(playing).toContain("Now: spelling words");
  expect(playing).toContain("then ask which word next");
  expect(playing).toContain('Every answer while you play: set routine to "spell", spell "" when there is no word to spell now; not once the game is over.');
  expect(readAnswer(JSON.stringify({ spell: "kaķis", say: "Kaķis: k, a, ķ, i, s.", routine: "spell" }))?.routine).toBe("spell");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" })).not.toContain('"spell"');
});

test("its screen: when she asks or now and then by itself, it names what to show; not on Gemini Live", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" });
  expect(prompt).toContain('set show to what you draw or write now, in English: a thing to draw ("a cat") or what to write ("the sum 12+34=46")');
  expect(prompt).toContain("A word she wants written or spelled is for spelling words (below), not show.");
  // On Fly (2026-10-12): it said it drew the sun, set nothing, and told her the sun was there.
  expect(prompt).toContain("On your screen now: your eyes, nothing drawn. If she looks for something that isn't there, draw it now.");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade", screen: "the sun, still being drawn: not there yet" })).toContain("On your screen now: the sun, still being drawn: not there yet.");
  expect(prompt).toContain("now and then by yourself, to surprise her");
  expect(prompt).toContain("Set it as you begin drawing (when she asks, or says yes), not when you only offer to"); // it sent a cloud as it asked whether to
  expect(prompt).toContain("say you are drawing it"); // it said "here it is" seconds before
  expect(prompt).toContain('Answer with JSON only: {"show": "<what you draw or write now, in English, or "">",'); // thought of first
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" })).not.toContain('"show"');
  expect(readAnswer(JSON.stringify({ show: " a cat ", say: "Zīmēju!" }))).toMatchObject({ show: "a cat" }); // no routine needed
  expect(readAnswer(JSON.stringify({ show: "", say: "Labi!" }))?.show).toBeUndefined();
});

test("drawing it: a picture of any size in the board's middle ('#' lit, '*' half, '+' dim, anything else off), or text", () => {
  expect(drawPrompt(anna, "a dog")).toContain("a board of 24 by 24 amber dots");
  expect(drawPrompt(anna, "a dog")).toContain('"#" a lit dot, "*" a half-lit one, "+" a faint one');
  expect(drawPrompt(anna, "a dog")).toContain("Write only what you are told to write");
  const heart = readDrawing(JSON.stringify({ picture: [".#.#.", "#####", " +#+ ", "..*.."] }))?.picture;
  expect(heart).toHaveLength(24);
  expect(heart!.every((row) => row.length === 24)).toBe(true);
  const middle = (row: string) => ".".repeat(9) + row + ".".repeat(10);
  expect(heart!.slice(10, 14)).toEqual([".#.#.", "#####", ".+#+.", "..*.."].map(middle));
  expect(heart![9]).toBe(".".repeat(24));
  expect(readDrawing(JSON.stringify({ picture: ["#".repeat(30)] }))?.picture?.[11]).toBe("#".repeat(24)); // cut to the board
  expect(readDrawing('```json\n{"text": " 12+34=46 "}\n```')).toEqual({ text: "12+34=46" });
  expect(readDrawing(JSON.stringify({ picture: [], text: "" }))).toBeUndefined();
  expect(readDrawing("nonsense")).toBeUndefined();
});

// On Fly (2026-10-12): asked for a story about Karlsson, a sentence and a question, again and again.
test("asked for a story, it tells a whole one in one answer, beginning to end, with no question until it is over", () => {
  for (const mode of ["cascade", "live"] as const)
    expect(cubeyPrompt(anna, { earlier: "", now: evening, mode })).toContain("When she asks for a story or a fairy tale, tell a whole one");
});

test("a child it doesn't know yet: no name, girl or boy, or age guessed; it asks now and then, and keeps what it hears", () => {
  const unknown: Child = { id: "child", timeZone: "Europe/Lisbon", language: "lv", learns: "en", parents: "mum_dad" };
  const prompt = cubeyPrompt(unknown, { earlier: "", now: evening, mode: "cascade" });
  expect(prompt).toContain("talks with the child about anything the child likes. Always speak Latvian,");
  expect(prompt).toContain("You don't know the child's name or the child's age yet: now and then, when it fits (not every turn, never twice in a row), ask kindly.");
  expect(prompt).toContain('or a grown-up does ("this is Anna, she is three"), or the child wants another name ("I\'m Nick now"), use it from now on and set child to only what you were just told: name (as said on its own, not as in the sentence) with the gender it suggests ({"name": "Nick", "gender": "boy"}), or age in years ({"age": 5}). Nothing guessed: no gender without a name.');
  expect(prompt).toContain('"child": {}');
  const known = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" });
  expect(known).toContain("Anna is a girl of 3 years and 6 months.");
  expect(known).not.toContain("You don't know");
  expect(known).toContain('("I\'m Nick now")'); // she may still want another name
  expect(cubeyPrompt({ ...anna, ageMonths: undefined }, { earlier: "", now: evening, mode: "cascade" })).toContain("You don't know her age yet");
  expect(cubeyPrompt({ ...anna, gender: undefined }, { earlier: "", now: evening, mode: "cascade" })).toContain("Whether Anna is a girl or a boy, guess from the name now: set child gender.");
  expect(cubeyPrompt(unknown, { earlier: "", now: evening, mode: "live" })).not.toContain("Getting to know"); // Gemini Live keeps nothing
  const learned = (child: unknown) => readAnswer(JSON.stringify({ say: "Labi!", child }))?.learned;
  expect(learned({ name: " Niks ", gender: "boy", age: 4 })).toEqual({ name: "Niks", gender: "boy", age: 4 });
  expect(learned({ gender: "dragon", age: 40 })).toBeUndefined();
  expect(learned({})).toBeUndefined();
  expect(helloNote(unknown, false)).toBe("(The child has just woken you.)");
});

test("it knows what it can do, and offers one thing now and then when it fits the moment", () => {
  for (const mode of ["cascade", "live"] as const)
    expect(cubeyPrompt(anna, { earlier: "", now: evening, mode })).toContain("Now and then, when it fits the moment, offer one");
});

test("each game's rules are in the prompt only while she plays it: one line each the rest of the time", () => {
  const prompt = cubeyPrompt(anna, { earlier: "", now: evening, mode: "cascade" });
  expect(prompt).toContain('set routine to "chain"');
  expect(prompt).toContain('set routine to "letters"');
  expect(prompt).not.toContain("Now: a word chain");
  expect(prompt).not.toContain("Now: words on one letter");
  expect(prompt).not.toContain("Now: finding things");
  expect(prompt).not.toContain("Now: how many dots");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" })).not.toContain("letters");
  expect(cubeyPrompt(anna, { earlier: "", now: evening, mode: "live" })).not.toContain('"count"');
});

test("the letter a chain goes on with is the server's: the last of Cubey's word (Flash-Lite said z for ezis)", () => {
  expect(chainLetter("ezis")).toBe("s");
  expect(chainLetter("Vāvere!")).toBe("e");
  expect(chainLetter("")).toBeUndefined();
  expect(spell("Egle,.")).toEqual(["e", "g", "l", "e"]);
  const chain = (word: unknown, letter?: string) => readAnswer(JSON.stringify({ word, say: "Ezis! Tev uz z!", routine: "chain", letter }));
  expect(chain("ezis", "z")).toMatchObject({ routine: "chain", letter: "s", word: "ezis" }); // its word, to scroll in
  expect(chain("Ezis un lācis")).toMatchObject({ letter: "s", word: "ezis" }); // its first word, spelled
  expect(chain(undefined)?.letter).toBeUndefined(); // no word of its own (it asks her again): the letter stays
  expect(readAnswer(JSON.stringify({ hers: "Aita!", word: "ābols", say: "Jā!", routine: "chain" }))?.hers).toBe("aita");
});
test("the chain's words, hers as she meant it and its own, as soon as they are written (before what it says), spelled; once", () => {
  const seen: string[] = [], hers: string[] = [];
  const write = wordAsWritten("word", (word) => seen.push(word)), heard = wordAsWritten("hers", (word) => hers.push(word));
  for (const piece of ['{"hers": "Ai', 'ta", "wo', 'rd": "Ā', 'bols un', ' kas", "spelled": "ā-b-o-l-s", "say": "Ābols!"}']) write(piece), heard(piece);
  expect(seen).toEqual(["ābols"]);
  expect(hers).toEqual(["aita"]);
  const none: string[] = [];
  wordAsWritten("word", (word) => none.push(word))('{"hers": "", "word": "", "say": "Eh nav vārds."}');
  expect(none).toEqual([]);
});

test("its letter, as the game shows it: one letter, small", () => {
  const letters = (letter: unknown) => readAnswer(JSON.stringify({ say: "Tev uz S!", routine: "letters", letter }));
  expect(letters("S")).toMatchObject({ routine: "letters", letter: "s" });
  expect(letters("Š")?.letter).toBe("š");
  expect(letters("ābols")?.letter).toBe("ā"); // its first
  expect(letters("7")?.letter).toBeUndefined();
  expect(letters(undefined)?.letter).toBeUndefined();
});

import { expect, test } from "bun:test";
import { SpokenLetters } from "./spelling";

// What Soniox times as it speaks: each character of the text, `step` seconds after the one before, from `from`.
const timed = (text: string, from = 0, step = 0.1): [string[], number[]] => [[...text], [...text].map((_, i) => from + i * step)];

// The letters found as they are said: which, and when (ms into its speech).
function spelling(word: string) {
  const said: [number, number][] = [];
  return { letters: new SpokenLetters([...word], (i, ms) => said.push([i, ms])), said };
}

test("each letter of the word said alone, in its order, with when it is said; the word itself is none", () => {
  const { letters, said } = spelling("kaķis");
  letters.heard(...timed("Kaķis: k, a, ķ, i, s. Kaķis!"));
  expect(said).toEqual([[0, 700], [1, 1000], [2, 1300], [3, 1600], [4, 1900]]);
});

test("heard a piece at a time: a letter at the end of a piece waits for what follows it, to be sure it is alone", () => {
  const { letters, said } = spelling("cat");
  letters.heard(...timed("Cat: c"));
  expect(said).toEqual([]); // "c" or the start of a word?
  letters.heard(...timed(", a, t!", 0.6));
  expect(said).toEqual([[0, 500], [1, 800], [2, 1100]]);
  letters.heard(...timed(" c, a, t", 1.3)); // all found: nothing more
  expect(said).toHaveLength(3);
});

test("in capitals or between hyphens; a letter alone that isn't the next is passed over, and so is a word's", () => {
  const { letters, said } = spelling("ice");
  letters.heard(...timed("I'll show a word: ICE. I-C-E!")); // "I'll" is a word, "a" not the I
  expect(said.map(([i]) => i)).toEqual([0, 1, 2]);
  expect(said[0][1]).toBe(2300); // the first I of I-C-E
});

test("a letter with a mark, even as the letter and its mark apart", () => {
  const { letters, said } = spelling("āzis");
  letters.heard(["a", "̄", ",", " ", "z", ",", " ", "i", ",", " ", "s", "."], [0, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1]);
  expect(said).toEqual([[0, 0], [1, 300], [2, 600], [3, 900]]);
});

// The letters of a word Cubey spells, found as its voice says them: Soniox times each character it speaks (soniox.ts),
// and a letter of the word said alone ("Kaķis: k, a, ķ, i, s!": a letter with no other letter beside it) is that
// letter said, in the word's order. For the device to light each one as it is said (session.ts, spell.at).
export class SpokenLetters {
  private characters: string[] = [];
  private seconds: number[] = [];
  private at = 0; // the first character not yet looked at
  private next = 0; // the letter of the word looked for

  // letters: the word's, small; onLetter: letter i is said ms into Cubey's speech
  constructor(readonly letters: string[], private onLetter: (i: number, ms: number) => void) {}

  get found() {
    return this.next; // its letters found so far
  }

  heard(characters: string[], seconds: number[]) {
    this.characters.push(...characters);
    this.seconds.push(...seconds);
    while (this.next < this.letters.length) {
      let start = this.at;
      while (start < this.characters.length && !inWord(this.characters[start])) start++;
      let end = start;
      while (end < this.characters.length && inWord(this.characters[end])) end++;
      if (end >= this.characters.length) return void (this.at = start); // what follows it is still to come
      const said = this.characters.slice(start, end).join("").normalize("NFC").toLowerCase();
      if (said === this.letters[this.next]) this.onLetter(this.next++, Math.round(this.seconds[start] * 1000));
      this.at = end;
    }
  }
}

// A letter, its mark, or an apostrophe within a word ("I'll" is no letter I said alone).
const inWord = (c: string) => /[\p{L}\p{M}'’]/u.test(c);

// A word game with Cubey, exchange by exchange (lesson.ts): planned a round at a time from her memory, when it starts
// and as each round runs out, with the words in the target language translated once and kept. Each exchange judges how
// she did at the step she was asked (a tap here, her words by the model), moves on, or asks the same step again when
// she said nothing or tapped the wrong picture the first time; it shows the step's picture or the pictures to tap, and
// has the model say it. It never ends by itself: she stops it (stop), or the power button does.
import { type Child, monthsOf } from "./child";
import { idea, type PictureSize } from "./curriculum";
import type { Complete, Message } from "./llm";
import type { Spent } from "./prices";
import {
  type Answered, askLesson, dayOf, type Her, type LessonProgress, type LessonWords, planGame, type Result, spoken, type Spoken, type Step,
  translate,
} from "./lesson";

export type Game = {
  child: Child;
  spent?: (spent: Spent) => void; // what its model's calls cost
  language: string;
  today: string;
  plan: Step[]; // its rounds so far
  at: number; // the step she was asked last (-1: none yet)
  missed: boolean; // a wrong tap at it already: the next answer goes on
  words: Record<string, string>; // idea -> the target language
  known: string[]; // ideas she knows, for the tiny chat
};

// What the device shows for a step, over all of its screen in place of Cubey's eyes: the picture, the pictures to tap,
// or nothing (the eyes).
export type Show =
  | { type: "lesson.show"; picture?: string }
  | { type: "lesson.choose"; pictures: { id: string; picture: string }[] };

export type LessonTurn =
  | { stop: true; forParents: boolean }
  | {
    stop?: undefined;
    said: string;
    spoken: Spoken[];
    show: Show;
    forParents: boolean;
    log: { heard: string; said: string; words: { word: string; meaning: string; result: "new" | Result }[] };
  };

const CHAT_WORDS = 12; // the known words the chat may use

export class Lessons {
  constructor(readonly deps: {
    progress: LessonProgress;
    words: LessonWords;
    complete: Complete;
    language: (child: Child) => string;
    picture: (name: string, size: PictureSize) => Promise<string | undefined>; // base64 PNG
    now?: () => Date;
  }) {}

  // The child goes by her name in the memory until the parents' app gives her an id.
  async start(child: Child, spent?: (spent: Spent) => void): Promise<Game> {
    const game: Game = { child, spent, language: this.deps.language(child), today: dayOf(child, this.deps.now?.()), plan: [], at: -1, missed: false, words: {}, known: [] };
    await this.plan(game);
    return game;
  }

  // Its model, its spending counted.
  private complete(game: Game): Complete {
    return (opts) => this.deps.complete({ ...opts, spent: game.spent });
  }

  // The next round, from her memory as it is now.
  private async plan(game: Game) {
    const { child, language, today } = game;
    const memory = this.deps.progress.memory(child.id, language, today);
    const round = planGame({ memory, ageMonths: monthsOf(child) });
    const known = memory.known.slice(-CHAT_WORDS);
    const ids = [...new Set([...round.flatMap((s) => [s.idea, ...(s.choices ?? [])]), ...known])].filter((id): id is string => !!id);
    const key = `${language}/${child.gender ?? "child"}`; // phrases about herself follow her gender
    const kept = this.deps.words.get(key, ids);
    const missing = ids.filter((id) => !kept[id]).map((id) => idea(id)!);
    if (missing.length) {
      const fresh = await translate({ language, ideas: missing, child, complete: this.complete(game) });
      this.deps.words.set(key, fresh);
      Object.assign(kept, fresh);
    }
    game.plan.push(...round);
    Object.assign(game.words, kept);
    game.known = known;
  }

  async turn(game: Game, opts: { her: Her; talk: Message[] }): Promise<LessonTurn> {
    const { child, language, words } = game;
    const before: Step | undefined = game.plan[game.at];
    const her = opts.her;
    // A tap counts only for a step with pictures to tap; a wrong one gets one more go.
    const tap = "tapped" in her && before?.choices ? { tapped: her.tapped, right: her.tapped === before.idea, again: her.tapped !== before.idea && !game.missed } : undefined;
    const answered: Answered = tap ?? ("said" in her && her.said ? { said: her.said } : { quiet: true });
    const stays = !!before && ("quiet" in answered || !!tap?.again); // the same step again
    const at = stays ? game.at : game.at + 1;
    if (at >= game.plan.length) await this.plan(game);
    const step = game.plan[at];
    const reply = await askLesson({ child, step, before, answered, language, words, known: game.known, talk: opts.talk, complete: this.complete(game) });
    if (reply.stop) return { stop: true, forParents: reply.forParents };

    const practised: Extract<LessonTurn, { said: string }>["log"]["words"] = [];
    const word = (id: string) => words[id] ?? idea(id)?.en ?? id;
    // How she did at the step she was asked: a tap is judged here (right after a wrong one is a try), her words by the
    // model; repeating after a new word counts for nothing yet.
    const result: Result | undefined = tap ? (tap.right ? (game.missed ? "tried" : "right") : tap.again ? undefined : "missed") : "said" in answered ? reply.result : undefined;
    if (before?.idea && before.kind !== "teach" && result) {
      this.deps.progress.record(child.id, language, before.idea, result, game.today);
      practised.push({ word: word(before.idea), meaning: idea(before.idea)?.en ?? "", result });
    }
    if (step.kind === "teach" && step.idea && !stays) {
      this.deps.progress.record(child.id, language, step.idea, "new", game.today);
      practised.push({ word: word(step.idea), meaning: idea(step.idea)?.en ?? "", result: "new" });
    }
    game.missed = !!tap?.again || (stays && game.missed);
    game.at = at;

    return {
      said: reply.said,
      spoken: spoken(reply.said, child.language, language),
      show: await this.show(step),
      forParents: reply.forParents,
      log: {
        heard: tap ? `tapped the ${idea(tap.tapped)?.en ?? tap.tapped}` : "said" in answered ? answered.said : "",
        said: reply.said.replace(/[{}]/g, ""),
        words: practised,
      },
    };
  }

  private async show(step: Step): Promise<Show> {
    if (step.choices) {
      const pictures = await Promise.all(step.choices.map(async (id) => ({ id, picture: await this.deps.picture(idea(id)!.picture!, 176) })));
      return { type: "lesson.choose", pictures: pictures.filter((p): p is { id: string; picture: string } => !!p.picture) };
    }
    const shown = step.idea ? idea(step.idea) : undefined;
    const picture = shown?.picture ? await this.deps.picture(shown.picture, 256) : undefined;
    return picture ? { type: "lesson.show", picture } : { type: "lesson.show" };
  }
}

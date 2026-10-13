# Contributing to Cubey

Thank you for helping. Cubey talks with small children, so it's built carefully: tests first, every screen looked at, and every change measured against its principles.

## Before you start

- Read [CLAUDE.md](CLAUDE.md), the developer guide: setup, commands, the repository map and how we work.
- Read the [principles](README.md#why) and [docs/design.md](docs/design.md). A change that makes Cubey stickier (asking the child to stay, more screen, rewards for talking longer) won't be merged, however clever.
- For anything bigger than a fix, open an issue first to talk it through. That matters most for changes to what Cubey says or does with a child.

## Making a change

1. **Write the test first.** Server logic gets a `*.test.ts` beside its module; device logic a doctest in `firmware/tests/`. Tests never call a real provider: Soniox, the models and Gemini are pretended.
2. **Make it pass, and keep both suites green:**

   ```sh
   cd server && bun test && bunx tsc --noEmit
   cmake --build build/firmware && ctest --test-dir build/firmware --output-on-failure
   ```

3. **Look at every screen you change** with `cubey-sim --screenshot … --message '…'`, and attach the PNGs to your pull request.
4. **Keep the docs current.** A new voice command goes in `README.md`; a decision of weight (and why) goes in `docs/design.md`; a new device message goes in the protocol at the top of `server/src/session.ts`; a new gotcha goes in `CLAUDE.md`.

Adding a new routine? Follow the checklist in [CLAUDE.md](CLAUDE.md#adding-a-routine).

## Changing what Cubey says

The prompt (`server/src/cubey.ts`) is the heart of Cubey and the easiest place to do harm.

- Keep it short: a game gets one line, and its rules only while it's played.
- Keep the rules under "Questions for the parents" and "Always" unless the issue was agreed first.
- Check what the code can check: anything that must be exact (a letter, a number, a range) is judged by the server, not trusted to the model.
- Show it working: in your pull request, include a few real exchanges before and after, with the model you used.

## Style

- Match the surrounding code. Comments say what a thing is and why, in plain sentences.
- No dead code, no commented-out code, no temporary workarounds: fix the cause.
- `firmware/core/` stays plain C++17 and LVGL; anything platform-specific goes behind `firmware/hal/hal.h`.
- In prose: en dashes (–), not em dashes, and sentence-case headings.
- Commit messages: one plain sentence saying what changed, in sentence case and without a full stop ("The dice roll again on a shake").

## Pull requests

- One change per pull request, with a description of what and why.
- CI must pass: the server's tests and types on Linux, the firmware's build and tests on macOS.
- Say where you tried it: the simulator, the board, or both. The board build isn't in CI, so build it yourself if you changed `firmware/platforms/esp32/`.
- Use made-up names in tests, issues and transcripts. Never post a real child's name, voice, words or details.

## Reporting bugs and ideas

Open an issue with what you did, what you expected and what happened. For a talk that went wrong, include the exchange from `bun run log` (with names changed) and the model. For a screen, a screenshot.

Security problems go privately, not in an issue: see [SECURITY.md](SECURITY.md).

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).

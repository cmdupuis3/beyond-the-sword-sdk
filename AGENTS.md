# Notes for agents

This is the Beyond the Sword 3.19 game core (`CvGameCoreDLL/`) built with Clang, plus
performance work on it. Read this before changing anything under `CvGameCoreDLL/`.

## Every change has a tier

| tier | what it allows | guard in the source | build |
| --- | --- | --- | --- |
| 1 | Reproduces BtS: the game state is the stock DLL's on every turn | none | `--tier 1` (`REPRODUCE_BTS`) has only these |
| 2 | Deterministic, not BtS: play may differ from stock, but the same start and input always give the same game | `#ifndef REPRODUCE_BTS` ... `#else` stock `#endif` | default |
| 3 | Not deterministic: the game may depend on the clock, threads or memory layout | `#ifdef NON_DETERMINISM_OK` ... `#else` deterministic `#endif` | `--tier 3` |

Decide the tier before writing the change. `CvGameCoreDLL/BridgedTiers.h` has the full
definitions; `docs/behaviour-changes.md` lists every tier 2 and tier 3 change and must get
an entry for each new one.

How to tell them apart:

- If the stock DLL's two state hashes still come out on every turn, it is tier 1. What
  is drawn, and how often Python is asked a question whose answer does not change, may
  differ.
- If play can differ but nothing outside the game decides how, it is tier 2. A rule such
  as "keep a decision until something watched changes or N turns pass" is tier 2.
- If the result depends on how long something took, it is tier 3. Work done while a
  human takes their turn is tier 1 only if its result is never relied on (the turn
  works it out again if needed); as soon as a result that may be stale is used, it is
  tier 3.

## One branch per tier

| branch | what is committed there |
| --- | --- |
| `cmd/repro` | tier 1 changes |
| `cmd/non-repro` | tier 2 changes |
| `cmd/non-deterministic` | tier 3 changes |
| `cmd/main` | all of them: each tier branch is merged into it |

Commit a change on the branch of its tier and merge that branch into `cmd/main`. Do not
merge one tier branch into another. If a change needs code from another tier, bring its
branch up to `cmd/main` first.

## Checking a change

`bench/run_bench.py` plays seeded all-AI games and logs two state hashes a turn;
`bench/compare_bench.py` compares two logs. Use the standard game (150 turns); run the
huge one (250 turns) only when the change is about map size.

- Tier 1: a `--tier 1` DLL must give the stock DLL's hashes on every turn. A default
  `--profile` DLL with every tier 2 off switch set plays the same game and saves a build:
  `--define PATH_EXE_ONLY=1 --define WORKER_STOCK_CHOICE=1 --define PATH_STOCK_SEARCHES=1
  --define BEST_BUILD_STOCK=1`.
- Tier 2: with the change's off switch set, the game from before; without it, the same
  hashes on two runs. The `--tier 1` DLL must still match stock.
- Tier 3: without the change's test define, a `--tier 3` DLL plays the default build's
  game. Give it a define that makes the first player's turn stand in for a human's (see
  `BEST_BUILD_AHEAD_TEST`), so that there is a repeatable game to measure.

A change that is meant to be exact and still changes the game: look for an order settled
by memory address, and for AI values the stock code works out once a game turn on first
use (`m_iStrategyHashCacheTurn`, `m_iAveragesCacheTurn`); running AI code early fixes them
early.

## Conventions

- Give every tier 2 and tier 3 change an off switch: a global define read once, in a
  small function that is constant in builds without the tier (`workerTeamRules()` in
  `CvUnitAI.cpp` is the pattern).
- A cache that lasts a number of turns scales with the game speed
  (`CvGameSpeedInfo`): a marathon game needs fewer re-evaluations a turn than a normal
  one.
- Nothing a change keeps between turns goes into the saved game: the save format stays
  stock.
- Say what was not checked. No game with a human player and no multiplayer game has been
  run in any tier.

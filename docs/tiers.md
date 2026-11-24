# Tiers

Every change is in one of three tiers. `CvGameCoreDLL/BridgedTiers.h` has the definitions
and says how a change is marked in the source; `tools/build_dll.py --tier N` chooses how
many tiers a DLL has.

| tier | what it means | in the source | built with |
| --- | --- | --- | --- |
| 1 | Reproduces BtS: the game state is the stock DLL's on every turn. | no guard | every build; `--tier 1` (`REPRODUCE_BTS`) has nothing else |
| 2 | Deterministic, not BtS: the game can differ from stock, but the same start and the same input always give the same game. | `#ifndef REPRODUCE_BTS` | the default, and `--tier 3` |
| 3 | Not deterministic: the game can differ from one run to the next (the clock, threads, where things are in memory). | `#ifdef NON_DETERMINISM_OK` | `--tier 3` (`NON_DETERMINISM_OK`) only |

Every optimization has a short doc of its own in [optimizations/](optimizations/README.md),
which lists them with their tiers.

## What tier 1 allows

The game state has to be the stock DLL's. These may differ, and tier 1 changes use all
three:

- What the exe is asked to draw, and when ([turn passes](optimizations/turn-passes-without-waiting.md), [symbols](optimizations/symbols-once-per-draw.md)).
- How often Python is asked a question whose answer does not change. The stock Python
  answers the same however often it is asked; a mod whose callback counts its calls, or
  reads what the DLL is in the middle of setting, could tell. The changes: the score update
  ([score-python-once](optimizations/score-python-once.md)), and the unit upgrade code (`CvUnitAI::AI_upgrade()`, `CvUnit::canUpgrade()`),
  which asks `getUpgradePriceOverride` less often: not for unit types off the unit's
  upgrade tree, and only for an upgrade some city can make. Where a callback can
  change the answer itself, the change stands aside and the stock code runs: the kept
  city-site scores when any of the three found-value callbacks is on, the build lists
  when `canBuild` is.
- A result that in stock depended on something outside the game. There is one: collateral
  damage ties, which stock settled by where units were in memory. Any result stock could
  have given counts as stock's.

One tier 1 change runs by the clock. While a human takes their turn,
`CvGame::updateAhead()` brings one AI player's kept city-site scores up to date every
tenth of a second. How much gets done depends on how long the human takes. It stays in
tier 1 because nothing done ahead is relied on: a kept score is used only if nothing it
was worked out from has changed, and the player's own turn does whatever is left. Any
other work done ahead has to meet the same rule, and leave the stock once-per-turn caches
(`m_iStrategyHashCacheTurn`, `m_iAveragesCacheTurn`, `m_iUpgradeUnitsCacheTurn`) as it
found them, or it is tier 3.

## How the tiers are checked

The seeded games (`bench/run_bench.py`: Continents, seed 20261120, all players AI) log two
hashes of the game state at the start of every game turn (`bench/mod/Assets/Python/BenchHarness.py`),
and `bench/compare_bench.py` compares two logs on both.

- `hash` takes in, for every living player, their gold, population and land; for every
  city its plot, population, food, production and culture; for every unit its plot, type,
  damage and experience. It is the hash every log since the first benchmark has.
- `wide` is the stricter one. It was added with the tiers, so older logs do not have it. It
  takes in the game's own sync checksum, which adds in the seeds of both random number
  generators; the turn slice; every team's technologies, research progress, wars, open
  borders, pacts and vassals; every player's research, civics, state religion, commerce
  rates, assets, power, buildings by class and cities by religion; every city's and unit's
  ID, what each city is building, its great people progress and culture level, each unit's
  group, level and moves; and every plot's owner, improvement, route, feature and bonus.

How each tier is checked:

- Tier 1: a `--tier 1` DLL has to give the stock DLL's two hashes on every turn of the
  standard game (150 turns) and the huge game (250 turns). The stock DLL's logs are
  `bench/results/stock-std150.log` and `bench/results/stock-huge250.log`; if they are not
  there, run `bench/run_bench.py --dll` on `Assets/CvGameCoreDLL.dll` of the game itself
  (a minute and a half, and eight minutes). A change meant for tier 1 is checked the same
  way before it is committed. In a default build the eleven off switches of the tier 2
  entries together give the same games, which saves a build: `--define PATH_EXE_ONLY=1
  --define WORKER_STOCK_CHOICE=1 --define PATH_STOCK_SEARCHES=1 --define BEST_BUILD_STOCK=1
  --define PATH_SEARCH_EVERY_STEP=1 --define AI_RANDOM_STOCK=1 --define EXPLORE_STOCK=1
  --define PATH_STOCK_HEURISTIC=1 --define PATH_STOCK_BOUNDS=1
  --define FOUND_SITES_STOCK=1 --define UNIT_IDLE_STOCK=1`
  with a `--profile` DLL.
- Tier 2: the same two hashes on every turn as the default build before the change, or a
  doc in `optimizations/` saying what differs. The same DLL run twice has to give the same hashes.
- Tier 3: nothing to match. The default build must not change.

When the tiers were introduced, release builds, one run each on the same day:

| | standard, turn 150 `hash` / `wide` | time for the turns | huge, turn 250 `hash` / `wide` | time for the turns |
| --- | --- | ---: | --- | ---: |
| the stock DLL | `c509b3f9` / `d5742bbf` | 79.8 s | `be311025` / `80c2a597` | 491.4 s |
| `--tier 1` | `c509b3f9` / `d5742bbf` | 5.6 s | `be311025` / `80c2a597` | 68.3 s |
| default (`--tier 2`) | `d15313fe` / `78a4cdf8` | 4.3 s | `b566caa6` / `c30eaa13` | 57.0 s |

The `--tier 1` DLL had the stock DLL's two hashes on all 151 turns of the standard game
and all 251 of the huge one. The default DLL's first differ from stock's on turn 30
(standard) and turn 12 (huge), both hashes on the same turn. Most of the stock DLL's time
is the stock game waiting for the screen between passes (see [turn passes](optimizations/turn-passes-without-waiting.md)); it was 151 s for the standard game on another day.

What the check does not cover:

- What the stock Python interface cannot read or the hashes leave out: what the AI
  remembers and plans (attitudes, memories, war plans, unit missions), plot culture, what
  each team has seen, and the random number generators other than through the sync
  checksum, which multiplies them in with other numbers and can lose part of them.
- One seed, stock Python and stock XML. No game played by a human and no multiplayer game
  has been checked, in any tier.
- Saving and loading. Nothing the changes keep between turns is saved, so a loaded game
  starts with those caches empty. For tier 1 that makes no difference. A tier 2 game that
  is saved and loaded can go on differently from the same game played straight through
  (the K-Mod pathfinder's kept search is gone), as a stock game can (the exe's is).

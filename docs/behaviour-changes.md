# Behaviour changes

Changes in this DLL that can make a game play differently from the stock Beyond the Sword
3.19 game core, or that a player or a mod could otherwise notice. Each entry says what
differs, where the code came from and how to check it.

## Tiers

Every change is in one of three tiers. `CvGameCoreDLL/BridgedTiers.h` has the definitions
and says how a change is marked in the source; `tools/build_dll.py --tier N` chooses how
many tiers a DLL has.

| tier | what it means | in the source | built with |
| --- | --- | --- | --- |
| 1 | Reproduces BtS: the game state is the stock DLL's on every turn. | no guard | every build; `--tier 1` (`REPRODUCE_BTS`) has nothing else |
| 2 | Deterministic, not BtS: the game can differ from stock, but the same start and the same input always give the same game. | `#ifndef REPRODUCE_BTS` | the default, and `--tier 3` |
| 3 | Not deterministic: the game can differ from one run to the next (the clock, threads, where things are in memory). | `#ifdef NON_DETERMINISM_OK` | `--tier 3` (`NON_DETERMINISM_OK`) only |

The entries below, by tier:

| entry | tier | off switch in a default build |
| --- | --- | --- |
| AI groups find their paths with the K-Mod pathfinder | 2 | `PATH_EXE_ONLY` (profile builds) |
| A player's workers act as a team | 2 | `WORKER_STOCK_CHOICE` |
| The AI does not search for paths it could do without | 2 | `PATH_STOCK_SEARCHES` |
| A city keeps the builds it chose for its plots | 2 | `BEST_BUILD_STOCK` |
| AI cities choose their builds while a human takes their turn | 3 | `BEST_BUILD_AHEAD_OFF` (tier 3 builds) |
| AI turns do not wait for the screen | 1 | `STOCK_TURN_SLICES` |
| Yield and route symbols are made once before the screen is drawn | 1 | `STOCK_SYMBOL_UPDATES` |
| Collateral damage ties go to the unit first on the plot | 1 | none |
| The score update asks Python for each player's score once | 1 | none |
| Optional: a city's turn without its Python callbacks | 1 | off unless a mod sets the defines |
| Optional: four more Python calls a mod can leave out | 1 | off unless a mod sets the defines |

Tier 1 changes with nothing for a player or a mod to notice are
not listed; their commit messages describe them.

### What tier 1 allows

The game state has to be the stock DLL's. These may differ, and tier 1 changes use all
three:

- What the exe is asked to draw, and when (the two entries on the screen and on symbols).
- How often Python is asked a question whose answer does not change. The stock Python
  answers the same however often it is asked; a mod whose callback counts its calls, or
  reads what the DLL is in the middle of setting, could tell. The changes: the score update
  (below), and the unit upgrade code (`CvUnitAI::AI_upgrade()`, `CvUnit::canUpgrade()`),
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

### How the tiers are checked

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
  way before it is committed. In a default build the four off switches of the tier 2
  entries together give the same games, which saves a build: `--define PATH_EXE_ONLY=1
  --define WORKER_STOCK_CHOICE=1 --define PATH_STOCK_SEARCHES=1 --define BEST_BUILD_STOCK=1`
  with a `--profile` DLL.
- Tier 2: the same two hashes on every turn as the default build before the change, or a
  new entry here saying what differs. The same DLL run twice has to give the same hashes.
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
is the stock game waiting for the screen between passes (see "AI turns do not wait for the
screen"); it was 151 s for the standard game on another day.

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

## AI groups find their paths with the K-Mod pathfinder

Tier 2. A `--tier 1` build leaves every group with the exe's pathfinder.

### What changed

`CvSelectionGroup::generatePath()` serves every group under AI control from a pathfinder
inside the DLL, `KmodPathFinder`, instead of the pathfinder in the game exe. "Under AI
control" is `AI_isControlled()`: the groups of AI players and barbarians, and a human
player's automated groups.

Groups a human moves by hand still use the exe's pathfinder, and the path drawn in the
interface still comes from the exe, so a human's units go where the path was drawn. The
exe's other pathfinders (step, route, border, area, plot group, irrigation) are untouched.

The rules that decide a path are the stock ones. `pathDestValid`, `pathHeuristic`,
`pathCost`, `pathValid` and `pathAdd` in `CvGameCoreUtils.cpp` keep the stock tests and
costs and were changed only so that either pathfinder can call them. K-Mod's changes to the
cost of a path, its extra `MOVE_` flags and its AI changes were not taken.

### What differs from the stock game

For every request, the new pathfinder gives a path of the same cost and the same number of
turns as the exe's pathfinder gives when it searches from scratch (see "How to check"). The
game still differs from stock:

- Ties. Where two paths cost the same the two pathfinders can pick different ones, because
  they visit plots in a different order. An AI unit can then take a different route of equal
  cost, and from there on it is a different game. In the seeded benchmark games the state
  hash first differs from the stock DLL's on game turn 30 (standard map) and 12 (huge map).

- Reused searches. Most AI code asks with `bReuse = true`, and the exe's pathfinder then
  answers from the search it made for an earlier destination without redoing it. Some stock
  answers are wrong because of that, and the AI no longer gets them:
  - A path that ends by attacking is meant to cost more according to the plot the attack is
    made from (its defence, not being a city, a river crossing). A reused search leaves that
    out. The new pathfinder always counts it.
  - A reused search is sometimes kept for a request it does not fit, for example from one
    unit to a faster one on the same plot when `CvUnit::getUpgradeCity()` asks for each in
    turn. The stock answer is then a longer path than the unit needs.

  The new pathfinder ignores `bReuse`. It keeps its search while requests come from the same
  group on the same plot with the same moves left and the same flags, and drops it in the
  cases listed below.

- `getPathLastNode()`, `getPathFirstPlot()` and `getPathEndTurnPlot()` answer from whichever
  pathfinder served the group's last `generatePath()`. For a group under AI control there is
  no node or plot after a request that failed or once the kept search has been dropped (for
  example because a unit has moved since), where the exe's pathfinder might still have given
  the end of an older path. `getPathFirstPlot()` returns no plot in that case; it used to
  read through a null pointer.

- `resetPath()` resets the pathfinder that serves the group. The Python functions are the
  same; `generatePath`'s `bReuse` argument has no effect for a group under AI control.

### When the kept search is dropped

- The request is for another group, another starting plot, other moves left, or flags that
  differ in more than `MOVE_DIRECT_ATTACK` and `MOVE_DECLARE_WAR` (the stock path rules use
  neither while searching).
- The destination changes to or from a plot the group would have to attack. This is the one
  case where the stock cost of a step depends on the destination.
- Before each unit's AI update and each `AI_follow`, where the stock code already called
  `resetPath()`. (K-Mod removed the first of these calls; it is kept.)
- A unit joins or leaves the group.
- Any unit moves, appears or dies (`CvUnit::setXY`).
- A unit gains or loses a promotion (`CvUnit::setHasPromotion`).
- Two teams go to war or make peace (`CvTeam::setAtWar`).
- Once every game turn (`CvGame::doUpdateCacheOnTurn`), and when a map is set up or loaded.

Of these K-Mod has the per-turn reset, the promotion reset and the `AI_follow` one. The
others are here so that a kept search cannot outlive what the stock game allowed.

### Where the code came from

`KmodPathFinder.h` and `KmodPathFinder.cpp` are from K-Mod 1.46 by karadoc, as are the way the
path functions are handed their settings and the split of `pathValid` into `pathValid_join`
and `pathValid_source`. The comment at the top of `KmodPathFinder.h` lists what was changed
from K-Mod's code so that it gives stock results. K-Mod's heuristic weight is not used: with
the stock costs a step can cost as little as one movement point, so the weight is 1, as in
the exe.

### How to check

Profile builds (`tools/build_dll.py --profile`) read the global define `PATH_VERIFY`. When it
is set, every request the K-Mod pathfinder serves is also put to a separate pathfinder of the
exe's, and the profile gets a row for each outcome: both failed; both succeeded with the same
number of turns; the K-Mod path takes more turns; it takes fewer turns; one succeeded and
the other failed. The "path verify detail" rows split the same requests by the cost of the
two paths. The first hundred requests of each kind of difference are written with both paths
to `<label>.profile.tsv.pathverify.txt`, next to the profile.

- `PATH_VERIFY=1`: the exe's pathfinder searches from scratch for each request. This is the
  reference. Any difference in turns or cost is a fault in the new pathfinder.
- `PATH_VERIFY=2`: the exe's pathfinder is given the caller's `bReuse` and is reset where the
  stock game resets it, so it answers as the stock game would to the same run of requests.
  Differences here are what the stock game got from reused searches.

```
python bench\run_bench.py --dll tmp\dll-build-profile\CvGameCoreDLL.dll --label verify --turns 150 --profile --define PATH_VERIFY=1
```

The verification does not change the game: the state hash is the same with it on or off.

Results on the seeded all-AI games (every request was served by the K-Mod pathfinder):

| | standard, 150 turns, `=1` | the same, `=2` | standard, 300 turns, `=1` | huge, 250 turns, `=1` |
| --- | ---: | ---: | ---: | ---: |
| requests | 227,779 | 227,779 | 985,222 | 1,947,986 |
| both failed | 122,850 | 122,850 | 639,945 | 1,076,066 |
| both succeeded, same turns | 104,929 | 104,925 | 345,277 | 871,920 |
| of those, K-Mod path costs more | 0 | 1,235 | 0 | 0 |
| of those, K-Mod path costs less | 0 | 5 | 0 | 0 |
| K-Mod path takes more turns | 0 | 0 | 0 | 0 |
| K-Mod path takes fewer turns | 0 | 4 | 0 | 0 |
| one succeeded, the other failed | 0 | 0 | 0 | 0 |

With `=2`, the 1,235 costlier paths are the attack charge that the reused stock search left
out (in each of the hundred written out, the difference is that charge), and the 9 paths with
fewer turns or a lower cost are reused stock searches that did not fit the request.

Profile builds also read `PATH_EXE_ONLY`. When it is set the exe's pathfinder serves every
group, as in the stock game. This is the test of the branch that groups moved by a human
take, which an all-AI game otherwise never enters: the seeded standard-map game then has the
stock DLL's state hash on all 151 turns.

Not checked: a game played by a human, and multiplayer.

## A player's workers act as a team

Tier 2. A `--tier 1` build never applies the two rules.

### What changed

Each AI worker still decides for itself, in the stock order of things to try. Two rules now
look at what the player's other workers are doing (`CvUnitAI::AI_agreedBuild()` and
`AI_isIdleWorkerCloser()` in `CvUnitAI.cpp`). They do not apply to a human player's automated
workers.

- One build to a plot. The stock code counts the workers heading for a plot but not what they
  mean to build there, and progress is kept per build, so two workers could each work at a
  different improvement on the same plot. A worker about to start an improvement where
  another worker of the player is on, or heading for, a different one now takes that one
  over. If this worker's improvement makes the plot's bonus available and the other's does
  not, the other worker's orders are cancelled instead and it decides again.

- Far jobs to the nearer worker. Workers are asked in unit order, so a job could go to a
  worker far away while one standing near it was sent elsewhere. A worker now leaves a job
  more than a turn away if another worker of the player in the same area has yet to decide
  this turn and is less than half as far from it. This applies where a worker looks over the
  whole territory for work (bonuses, the next city to improve, routes, irrigation and
  forts), not to the plots of the city it is working for.

### How to turn it off

Set the global define `WORKER_STOCK_CHOICE` to 1. The seeded standard game then has the same
state hash as before on all 151 turns.

### What it does, measured

Profile builds count what workers do. Over five seeded 150-turn standard games, with the
rules off and on:

| | off | on |
| --- | ---: | ---: |
| plots moved by workers | 7,967 | 7,913 |
| turns of building | 8,830 | 8,897 |
| builds finished | 2,676 | 2,715 |
| plots moved per build finished | 2.98 | 2.91 |
| builds begun on a plot with another build unfinished | 168 | 111 |

The games differ from the turn the rules first act, so the totals are from different games
and the first four rows are within what that alone would give. The last row fell in every
one of the five games. The first rule is rarely needed: it took over a build twice in those
five games, and eight times in the 250-turn huge game.

## AI turns do not wait for the screen

Tier 1: the game state is the same. What differs is when the screen is drawn.

### What changed

The exe calls `CvGame::update()` about four times a second and draws frames in between. Each
call makes one pass over the players: whoever's turn it is moves their units, and when they
are done the turn goes to the next player. A pass that leaves something unfinished is taken
up again in the next call, so the stock game spends at least a fifth of a second waiting
every time that happens, and once more for every game turn.

In a single-player game `CvGame::update()` now makes its next pass straight away, in the
same call, unless there is something for the player to see or to do. The passes are the stock
ones in the stock order (`CvGame::updateSlice()` is the body of the stock function), and the
turn slice counter still counts passes. Only the frames between them are left out, so the
game itself is the same: the seeded standard game has the same state hash on all 151 turns.

`CvGame::nextSliceWait()` decides. The next pass waits for the exe, as in the stock game, when:

- it is a human player's turn and they have not ended it;
- a unit of the player whose turn it is has a mission or combat timer running. The game only
  starts these timers for moves and fights that are shown to a human (the Show Enemy Moves,
  Show Friendly Moves and Quick Combat options, and what the player can see), so everything
  that was played on screen before is still played, at the same speed;
- a diplomacy screen is up, or an AI player has just asked for one, until it is answered;
- the game is over or paused, or the world builder or an advanced start is open;
- it is a multiplayer game (network, hot seat or play by email): these keep the stock pacing.

With no human player to wait for (autoplay, and the benchmark's autorun), the exe gets to
draw at the end of a game turn once a second has passed since it last did.

### What a player notices

- The other players' turns take as long as the work in them. Nothing is drawn while they
  run, unless a move or a fight is being shown, so in a long turn the window does not
  repaint until it is the player's turn again. Set `TURN_LOOP_FRAME_MS` to draw a frame
  every so many milliseconds of a long turn; each frame costs about a fifth of a second.
- A popup raised during the other players' turns (choose production, for example) appears
  when those turns are over. The stock game goes on with the turns while a popup is up, so
  what the player decides takes effect no later than it could have before.

Not checked: a game played by a human. The benchmark games have no human player, so the
waits for a human's turn, for moves being shown and for diplomacy have not been run.

### How to turn it off

Set the global define `STOCK_TURN_SLICES` to 1. Every pass then waits for the exe, as in the
stock game.

### What it does, measured

Seeded games, all players AI. Profile builds have rows for the time the exe keeps between
calls and for why each pass did or did not wait.

| | standard, 150 turns, stock pacing | the same, passes run at once | huge, 250 turns, stock pacing | the same, passes run at once |
| --- | ---: | ---: | ---: | ---: |
| calls of `CvGame::update()` | 302 | about 10 | 1,271 | 58 |
| time between calls, in the exe | 63.2 s | 1.7 s | about 394 s | 30.6 s |
| time inside `CvGame::update()` | 17.4 s | 9.4 s | 214.6 s | 72.9 s |
| time for the turns | 81.8 s | 8.7 s | 590.9 s | 106.2 s |

The standard game's last row is from release builds; everything else is from profile builds.
The huge game has the same state hash on all 251 turns. Its stock-pacing column is the run
made before this change, on another day.

The exe keeps about 0.2 s between two calls on the standard map and about 0.5 s on the huge
one, however long the call before took.

The time inside `CvGame::update()` falls as well, though the same work is done: every scope
is called the same number of times and each takes less (the pathfinder 4.5 microseconds a
request with stock pacing and 2.1 without, in the standard games run one after the other).
The likely reason is that the passes now run back to back, where before the exe drew frames
between them. How much of the huge game's fall is that and how much is the machine having
been busier on the day of the earlier run was not measured.

Benchmark times from before this change include those waits and are not comparable with
times from after it. The state hashes are.

## Yield and route symbols are made once before the screen is drawn

Tier 1: the game state is the same. What differs is what the exe is asked to draw.

### What changed

This changes what the DLL asks the graphics engine to do, not the game: the seeded standard
and huge games have the same state hash on every turn. It is listed here because nobody
has yet looked at a map drawn this way.

The stock game makes a plot's yield symbols (the food, production and commerce icons) anew
every time one of its yields, its owner or what the active team knows of it changes, and a
route symbol anew, with those of the eight plots around it, every time the active team
learns of a route. In a round of AI turns the same plot often comes up several times, and
nothing is drawn until `CvGame::update()` returns.

- While `CvGame::update()` runs, `CvPlot::updateSymbols()` and `CvPlot::updateRouteSymbol()`
  only mark the plot. `CvMap::updateDeferredSymbols()` makes the symbols once, just before
  the function returns and the exe gets to draw. Outside `CvGame::update()`, when the exe
  calls in because of something the player did, they are made at once as before. The stock
  game already treats unit flags this way (`CvMap::updateFlagSymbols()`).
- No yield symbols are made for a plot the active team has not revealed. Stock makes them
  and hides them. Revealing the plot, changing the active player and switching debug mode
  all make the symbols, as they do in stock, and `CvPlot::updateSymbolVisibility()` makes
  them if a plot comes to be seen some other way (the player's team joining another, for
  example).

- The graphics of a feature (a forest or jungle cut down or grown, for the most part) are
  not made during the turns for a plot the active team has not revealed and that has no
  revealed plot next to it. Stock makes them at once. They are made when the plot or one
  next to it is revealed: `CvPlot::updateFeatureSymbolVisibility()` does it for the plot
  being revealed, and `CvMap::updateDeferredSymbols()` looks at every marked plot each
  time the exe is about to get the screen. A plot whose feature is always visible is not
  left out; no stock feature is.

Why the features matter: the exe does its own work at the end of every game turn
(`CvDLLEngineIFaceBase::DoTurn()`), 5.9 s of the seeded huge game. Calling it a second time
straight away costs nothing, so the time goes on what was reported to it during the turn.
Leaving out, one kind at a time, the minimap, fog, plot layout, flag, yield, route and
river updates changed nothing; leaving out the feature updates took it from 4.9 ms a turn
to 0.6 ms on the standard game. On the huge game a turn with no feature update costs
2 ms there, a turn with one 13 ms and a turn with three 28 ms.

### What a player should notice

Nothing. Not checked: the map as drawn. The benchmark cannot see it, so the first game
played with this should be watched for yield icons missing from a plot the player has
revealed (with the yield display on, and in the city screen) and for a road or railroad
that is not drawn, or not joined to its neighbours, after it is built or discovered, and
for a forest or jungle that is drawn where there is none, or missing where there is one,
on land that has just been explored.

### How to turn it off

Set the global define `STOCK_SYMBOL_UPDATES` to 1. With it the standard game makes exactly
as many symbol updates as before the change (10,394 yield and 5,991 route).

### What it does, measured

Seeded games, all players AI, profile builds, before and after on the same day.

| | standard, 150 turns, before | after | huge, 250 turns, before | after |
| --- | ---: | ---: | ---: | ---: |
| yield symbol updates asked for during turns | 3,527 | | 22,892 | |
| route symbol updates asked for during turns | 1,704 | | 18,511 | |
| plots with a yield update waiting when the exe got the screen | | 2,491 | | 17,105 |
| plots with a route update waiting | | 1,147 | | 10,894 |
| time making yield and route symbols during turns | 0.36 s | 0.13 s | 3.53 s | 1.33 s |
| time inside `CvGame::update()` | | | 49.6 s | 46.4 s |

The release build on the standard game, four runs with `STOCK_SYMBOL_UPDATES` set and four
without, taken in turn, the sum over the turns of each turn's fastest run: 4.47 s with
stock symbol updates and 4.19 s without, 6% less. That is about twice the time saved
inside the DLL. The rest was not looked into; these times include the exe's drawing
between calls, and it has fewer symbols to deal with.

The feature change, measured later on a different game (the seeded games changed in
between), profile builds:

| | standard, 150 turns, before | after | huge, 250 turns, before | after |
| --- | ---: | ---: | ---: | ---: |
| feature updates during turns left out | | 75 | | 312 |
| the exe's own work at the end of a game turn | 0.78 s | 0.38 s | 5.6 s to 5.9 s | 3.6 s |
| time in `CvGame::doTurn()` | | | 10.4 s | 7.4 s |

How much is left out depends on how much of the map the active player has seen. In the
seeded huge game about four in ten of the feature updates during turns are on plots that
player has not seen; a player who has traded for everybody's maps gains nothing.

With all three changes, the release build on the standard game, three runs with
`STOCK_SYMBOL_UPDATES` set and three without, taken in turn: 7.16 s with stock symbol
updates and 5.80 s without. The machine was busier than for the measurement above, and
three runs are few, so read that as a gain and not as its size.

## The AI does not search for paths it could do without

Tier 2. A `--tier 1` build searches as stock does.

### What changed

Two changes to where the unit AI (`CvUnitAI.cpp`) asks for paths.

- Turn limits. Twenty places look for a path and then use it only if it takes no more
  than so many turns (`AI_exploreRange`, `AI_guardCity`, `AI_group`, `AI_anyAttack`,
  `AI_targetBarbCity`, `AI_cityAttack`, `AI_reconSpy` and others). They now pass that limit
  to `generatePath()`, and the K-Mod pathfinder does not search past it. K-Mod does the same.
  The stock checks of the turns are still there.

- Cities not worth a search. `AI_targetCity` (which enemy city to attack),
  `AI_moveToStagingCity` (which threatened city of its own to gather in) and
  `AI_targetBarbCity` work out each city's value before looking for a path to it, as the
  most it could be worth: with a path of the fewest turns the distance allows
  (`CvUnitAI::AI_minPathTurns()`) and, in `AI_targetCity`, the most the random part of the
  value can add. The cities are then tried from the most valuable down, and once no city
  left could beat the best so far the rest are not searched for. The fewest turns comes
  from the cheapest step any route on the map allows the team (`CvMap::isRouteSeen()`): a
  railroad lets a unit move about 10 plots a turn, a road 2 per move it has, more where a technology makes roads faster.
  `AI_targetBarbCity` also drops a city that cannot be reached in under 10 turns, which it
  never wanted.

### What differs from the stock game

From the same paths, the same city is chosen: a tie goes to the city stock meets first. The
game still differs from stock:

- With a turn limit, the pathfinder looks for the cheapest path within the limit. Stock
  found the cheapest path of any length and turned it down if it was too long. Where the
  cheapest path is too long and a dearer one is not, the unit now takes the dearer one.

- Fewer paths are searched for. The K-Mod pathfinder carries its search over from one
  request to the next (see "AI groups find their paths with the K-Mod pathfinder"), so a
  later path of equal cost can be a different one.

- `AI_targetCity` draws the random part of a city's value only for the cities it searches
  a path for, so the game's random numbers run differently from then on.

- A city's value asks the exe's step pathfinder for a distance, and that pathfinder answers
  from its last search. Valuing cities that stock would not have valued changes some of
  those answers.

### How to turn it off

Set the global define `PATH_STOCK_SEARCHES` to 1. The seeded standard game then has the
same state hash as before on all 151 turns.

### What it does, measured

Profile builds, the seeded games before the change and after it. The games differ from
turn 90 (standard) and 83 (huge), so these are totals of different games.

| | standard, before | standard, after | huge, before | huge, after |
| --- | ---: | ---: | ---: | ---: |
| path requests | 358,952 | 332,786 | 1,762,272 | 1,734,874 |
| nodes the K-Mod pathfinder processed | 1,214,623 | 801,216 | 16,636,396 | 9,179,826 |
| time in `KmodPathFinder::GeneratePath` | 0.63 s | 0.52 s | 7.89 s | 5.12 s |
| time in `CvGame::update()` | 6.2 s | 5.1 s | 39.5 s | 37.5 s |

Path time by where it was asked for, huge game:

| | before | after |
| --- | ---: | ---: |
| `AI_targetCity` | 1,049 ms | 337 ms |
| `AI_exploreRange` | 794 ms | 271 ms |
| `AI_guardCity` | 600 ms | 151 ms |
| `AI_moveToStagingCity` | 452 ms | 22 ms |
| `AI_group` | 369 ms | 118 ms |
| `AI_anyAttack` | 334 ms | 88 ms |
| `AI_targetBarbCity` | 305 ms | 76 ms |

## Collateral damage ties go to the unit first on the plot

Tier 1: stock's own result here depended on where units were in memory, so no build could
match it, and this result is one stock could have given.

### What changed

`CvUnit::collateralCombat()` chooses which defenders take collateral damage, highest value
first (a random number times the unit's hit points). Stock kept the candidates in a
`std::map` by the unit's address in memory, so when two units had the same value the one
lower in memory was hit. The candidates are now kept in the order they are on the plot, and
a tie goes to the unit first on the plot.

### What differs from the stock game

Only ties are settled differently, and stock settled them by where the units happened to be
in memory. That made the game depend on how memory was laid out: a change that did nothing
but allocate memory differently (the define lookup cache in `CvGlobals.cpp`, before this
fix) changed the seeded huge game from turn 213. In multiplayer it could also differ between
computers. A tie is rare: one of the 174 collateral choices in the seeded huge game, which
stock happened to settle the same way, so the seeded games are unchanged.

### How to check

Profile builds count the choices and the ties ("collateral damage:" in the profile).

## Optional: a city's turn without its Python callbacks

Tier 1 as built: the stock game does not set the defines. A mod that sets one for a callback
its Python uses has changed its own game.

Nothing changes unless a mod asks for it.

Every city asks Python seven times each turn whether Python wants to do a part of the
turn itself: `doGrowth`, `doCulture`, `doPlotCulture`, `doProduction`, `doReligion`,
`doGreatPeople` and `doMeltdown` in `CvGameUtils.py`. In the stock game, and in a mod that
has left them as they are, each only answers no. A mod can switch each one off with a
global define set to 1, in its `PythonCallbackDefines.xml` for example:

| define | callback left out |
| --- | --- |
| `SKIP_DO_GROWTH_CALLBACK` | `doGrowth` |
| `SKIP_DO_CULTURE_CALLBACK` | `doCulture` |
| `SKIP_DO_PLOT_CULTURE_CALLBACK` | `doPlotCulture` |
| `SKIP_DO_PRODUCTION_CALLBACK` | `doProduction` |
| `SKIP_DO_RELIGION_CALLBACK` | `doReligion` |
| `SKIP_DO_GREAT_PEOPLE_CALLBACK` | `doGreatPeople` |
| `SKIP_DO_MELTDOWN_CALLBACK` | `doMeltdown` |

A define that is missing or 0 leaves its callback on, as in stock. Setting one for a
callback that the mod's Python does use switches that Python off, so check
`CvGameUtils.py` first: a callback can be left out if all it does is `return False`. The
`cityDoTurn` event is not affected.

Measured with all seven set, profile builds, seeded games, the same state hash on every
turn: the time `CvCity::doTurn()` spends outside the city AI and the production choice
falls from 2.36 s to 1.74 s on the huge game (17,320 city turns) and from 0.45 s to 0.36 s
on the standard one (3,834). That is about 1% of the time in the DLL.

## The score update asks Python for each player's score once

Tier 1 for the stock Python and for any `calculateScore` that does not read the ranks and
scores while they are being set.

### What changed

A player's score is worked out by Python (`calculateScore` in `CvGameUtils.py`).
`CvGame::updateScore()` ranks the players by score, and stock asked for the score again on
every pass of the ranking for each player not ranked yet: 171 calls for 18 players. It now
asks once per player before the ranking starts and ranks the answers, with the same rule
for equal scores (the player with the higher number first).

### What differs from the stock game

Nothing in the stock game or in a mod that has left `calculateScore` as it is: the ranks,
the scores kept and the score history that the ranking sets do not go into the
calculation. A mod whose `calculateScore` reads those while they are being set would now
get the values from before the update for every player. There is no switch.

### What it does, measured

Profile builds, the seeded standard game, the same state hash on every turn in profile and
release builds: `CvPlayer::calculateScore()` falls from 71,991 calls and 0.22 s to 6,678
calls and 0.06 s, about 2% of the time in the DLL. The huge game had 160,056 calls and
1.08 s before and was not run again.

## Optional: four more Python calls a mod can leave out

Tier 1 as built: the stock game does not set the defines.

Nothing changes unless a mod asks for it.

Profile builds time every call the DLL makes into Python ("python:" and "python event:"
in the profile). Four of the calls that cost most during the turns do nothing in the stock
game. A mod can switch each one off with a global define set to 1, in its
`PythonCallbackDefines.xml` for example:

| define | call left out | when the stock game makes it | what the stock Python does |
| --- | --- | --- | --- |
| `SKIP_GET_EXTRA_COST_CALLBACK` | `getExtraCost` in `CvGameUtils.py` | every time a player's costs are worked out, which the AI does over and over while it sets its commerce rates and weighs trades | answers 0 |
| `SKIP_GET_EXPERIENCE_NEEDED_CALLBACK` | `getExperienceNeeded` in `CvGameUtils.py` | for every unit every turn, and when a unit's experience changes | level squared plus 1, changed by the player's level experience modifier and rounded up |
| `SKIP_AI_UNIT_UPDATE_CALLBACK` | `AI_unitUpdate` in `CvGameUtils.py` | for every unit the AI moves | answers no |
| `SKIP_ON_UNIT_MOVE_CALLBACK` | the `unitMove` event (`onUnitMove` in `CvEventManager.py`) | for every plot a unit moves | writes to the log if movement logging is on |

A define that is missing or 0 leaves its call on, as in stock. Setting one for a call that
the mod's Python does use switches that Python off, so check the mod's Python first. With
`SKIP_GET_EXPERIENCE_NEEDED_CALLBACK` set the DLL works out the stock formula itself, so it
is only for a mod that has left the formula as it is. Set to 2 in a profile build, it asks
Python as well and counts the answers that differ ("experience needed verify:" in the
profile).

Sword of Islam can set all but `SKIP_AI_UNIT_UPDATE_CALLBACK`: its `AI_unitUpdate` moves
the inquisitors.

Measured on the seeded standard game with all four set, the same state hash on every turn
with and without them, in profile and release builds:

- The C++ formula gave the same answer as Python all 13,426 times it was asked.
- The four calls were 99,000 of the 167,000 calls into Python and 0.59 s of the 8.4 s in the
  DLL (profile build): `getExtraCost` 61,538 calls and 0.25 s, the `unitMove` event 9,084
  and 0.18 s, `getExperienceNeeded` 15,810 and 0.09 s, `AI_unitUpdate` 12,646 and 0.07 s.
- Release build, three runs with and three without, taken in turn, the sum over the turns
  of each turn's fastest run: 4.98 s without and 4.30 s with, 14% less. Whole runs differed
  from one another by more than that, so the figure is indicative.

The huge game was not run.

## A city keeps the builds it chose for its plots

Tier 2. On the branch `cmd/non-repro`.

### What changed

Every turn the stock game has every city work out again which build it wants on each plot
in its radius and how much it wants it (`CvCityAI::AI_updateBestBuild()`); the workers
choose their jobs from that. It is about a twentieth of the time of a late game, and most
turns nothing a city looks at has changed except where the workers are.

A city now keeps what it chose (`CvCityAI::AI_isBestBuildKept()`) until one of these:

- a plot in its radius changes owner, working city, terrain, feature, bonus, improvement,
  route or irrigation, or a city is founded or lost there (`CvPlot::cityBuildsChanged()`);
- its population or what it emphasizes changes;
- its owner's team gets a technology, or its owner changes a civic
  (`CvPlayer::cityBuildsChanged()`);
- it has kept them long enough: 4 game turns at normal speed. The time is scaled by the
  game speed's time for a worker's build (`iBuildPercent`), so it is 2 turns in a quick
  game, 6 in an epic one and 12 in a marathon: a build takes that much longer there and
  the cities change that much more slowly.

### What differs from the stock game

What a city keeps goes stale in the part that follows the workers. The value of each build
is divided by the number of workers already on their way to the city, and the food the
city counts on includes the builds being worked on. So workers choose among the cities
with figures that can be up to three turns old at normal speed. A build that can no longer
be made is not started: the workers check that themselves, as they always did.

- The turn a city last worked its builds out is not in the saved game. A loaded game works
  them all out on the next turn, and can go on differently from the same game played
  without saving.
- A network game works them out every turn, as stock does: a player who joins late loads
  the game and the others do not, and they would disagree.
- Working the builds out while a human takes their turn, so that they are fresh and cost
  nothing, makes the game depend on how long the human took. That is tier 3: see "AI
  cities choose their builds while a human takes their turn".

### How to turn it off

Set the global define `BEST_BUILD_STOCK` to 1: every city works its builds out every turn.
`BEST_BUILD_KEEP_TURNS` sets the number of turns at normal speed (4 if it is missing).

### What it does, measured

Seeded games, all players AI, profile builds.

| | standard, 150 turns | the same at marathon speed | huge, 250 turns |
| --- | ---: | ---: | ---: |
| city turns | 3,873 | 1,758 | 17,356 |
| builds kept | 1,933 (50%) | 1,451 (83%) | 7,630 (44%) |
| worked out because something changed | 1,778 | 262 | 9,230 |
| worked out because kept long enough | 162 | 45 | 496 |
| time in `AI_updateBestBuild()`, before | 0.37 s | | 2.61 s |
| the same, after | 0.22 s | | 1.25 s |

The huge game's figure from before is from an earlier profile of the game it then was.
The games differ from the ones before from turn 27 (standard) on, so the two columns of
the next table are different games, not one game measured twice:

| standard game, 150 turns | every turn (`BEST_BUILD_STOCK`) | kept |
| --- | ---: | ---: |
| builds finished by workers | 577 | 633 |
| turns of building | 1,900 | 2,009 |
| plots moved by workers | 1,934 | 1,846 |
| builds begun on a plot with another build unfinished | 44 | 34 |
| units and cities on turn 150 | 226 and 54 | 224 and 54 |

The huge game has 1,092 units and 130 cities on turn 250, where it had 989 and 129.

The checks, as "How the tiers are checked" asks:

- the `--tier 1` DLL has the stock DLL's two hashes on all 151 turns of the standard game;
- with `BEST_BUILD_STOCK` set the default DLL has the two hashes of the default build
  before this change on all 151 turns;
- without it, three runs of the profile build and one of the release build have the same
  two hashes on all 151 turns: turn 150 is `8e353b22` / `e1367f42`. The huge game's turn 250
  is `2179c206` / `1277dcc6`, from one run.

Not checked: the huge game a second time, and the tier 1 DLL on the huge game.

## AI cities choose their builds while a human takes their turn

Tier 3. On the branch `cmd/non-deterministic`, in a `--tier 3` build only.

### What changed

While a human takes their turn in a single-player game, `CvGame::updateAhead()` goes round
the AI players, one every tenth of a second. In a tier 3 build each of them then has all
its cities work out their builds for their plots (`CvPlayerAI::AI_updateBestBuildsAhead()`):
every city once a game turn, and again if something it looks at changes afterwards. A city
that has had its turn ahead keeps what it chose, by the rule of "A city keeps the builds
it chose for its plots", and on its own turn works them out again only if something has
changed since.

So the builds a city goes into its turn with are no older than the human's last turn,
where tier 2 lets them be up to three turns old, and the work is done in time nobody is
waiting for. The turns a city may keep its builds are what is left when the human ends
the turn before the city was reached.

The stock AI works its strategy and its average yields out once a game turn, the first
time they are asked for. Working a player's builds out ahead asks for them before the
players in between have moved, so they are put back afterwards and worked out again on
the player's own turn. (Left as they were, they changed the seeded games on their own.)

### Why it is tier 3

What is worked out ahead is used. Which cities were reached before the human ended the
turn depends on the clock, and a city that was reached chose its builds before the players
in between moved, where one that was not chooses them after. The same game with the same
input can therefore go differently from one playing to the next. The city-site scores
that `CvGame::updateAhead()` also works on are different: they are never relied on, and
stay tier 1.

### How to turn it off

Build without `--tier 3`. In a tier 3 build, set the global define `BEST_BUILD_AHEAD_OFF`
to 1.

### What it does, measured

The benchmark games have no human. With the define `BEST_BUILD_AHEAD_TEST` the first
player's turn stands in for one: at its start every other player's cities get their turn
ahead, which is what a human who takes long enough gives them. That makes the game the
same on every run, so it can be checked; a real game is not.

Seeded games, all players AI, `--tier 3 --profile` build, `BEST_BUILD_AHEAD_TEST=1`:

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| city turns | 3,178 | 18,068 |
| builds worked out ahead | 2,610 | 16,531 |
| kept on the city's own turn | 2,192 (69%) | 11,847 (66%) |
| worked out on the city's own turn | 986 | 6,221 |
| time ahead, which a human's turn would hide | 0.26 s | 1.78 s |
| time left in the players' turns | about 0.09 s | about 0.61 s |
| the same in a default build (tier 2) | 0.22 s | 1.25 s |
| the same with `BEST_BUILD_STOCK` | 0.37 s | 2.61 s |

The first player's own cities get no turn ahead and are in these figures with the tier 2
rule. A version that worked out ahead only the cities the tier 2 rule would have worked
out left about 0.11 s and 0.79 s in the players' turns, and left the others with older
builds; it was not kept.

The checks:

- without `BEST_BUILD_AHEAD_TEST` the tier 3 DLL has the two hashes of the default build on
  all 151 turns of the standard game, and so has the default build from this branch;
- with it, two runs have the same two hashes on all 151 turns: turn 150 is `2eb5e9fc` /
  `1cbbbb52`. The huge game's turn 250 is `f37528bf` / `bea34e93`, from one run.

Not checked: a game with a human in it. `CvGame::updateAhead()` only runs then, so the
code that goes round the players during a human's turn has not run at all, only the same
work called from the stand-in. Nor has a release build of tier 3 been made.

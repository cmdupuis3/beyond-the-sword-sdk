# Behaviour changes

Changes in this DLL that can make a game play differently from the stock Beyond the Sword
3.19 game core. Each entry says what differs, where the code came from and how to check it.
Changes that leave every game identical (the same state hash on every turn of the seeded
benchmark games) are not listed; their commit messages describe them.

## AI groups find their paths with the K-Mod pathfinder

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

Feature symbols (forests and the like) are not touched: they are almost never asked for
twice.

### What a player should notice

Nothing. Not checked: the map as drawn. The benchmark cannot see it, so the first game
played with this should be watched for yield icons missing from a plot the player has
revealed (with the yield display on, and in the city screen) and for a road or railroad
that is not drawn, or not joined to its neighbours, after it is built or discovered.

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

Release builds of the standard game, four runs of each taken in turn, the sum over the
turns of each turn's fastest run: 4.61 s before and 4.29 s after. That is more than the
time saved inside the DLL and was not looked into; the times of whole runs overlap.

Feature symbols take 0.75 s of the huge game's turns (724 updates) and are what is left,
with the route symbols, of the graphics work done during turns.

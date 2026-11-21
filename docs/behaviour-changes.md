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

Seeded standard game, 150 turns, all players AI. Profile builds have rows for the time the
exe keeps between calls and for why each pass did or did not wait.

| | stock pacing | passes run at once |
| --- | ---: | ---: |
| calls of `CvGame::update()` | 302 | about 10 |
| time between calls, in the exe | 63.2 s | 1.7 s |
| time for the 150 turns | 81.8 s | 8.7 s |

The first two rows are from profile builds and the last from release builds. The exe keeps
about 0.2 s between two calls however long the call before took.

Benchmark times from before this change include those waits and are not comparable with
times from after it. The state hashes are.

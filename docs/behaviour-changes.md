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

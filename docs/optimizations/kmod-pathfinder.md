# K-Mod pathfinder for AI groups

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `PATH_EXE_ONLY=1`, profile builds only |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | high |
| Commits | `5c2c84e` |

## What it does
`CvSelectionGroup::generatePath()` serves every group under AI control (`AI_isControlled()`:
AI players, barbarians, a human's automated groups) from `KmodPathFinder`, the pathfinder
of K-Mod 1.46 by karadoc, which is in the DLL and keeps its search from one request to the
next. Groups a human moves by hand, the path drawn in the interface and the exe's other
pathfinders are untouched. The path rules are the stock ones (`pathDestValid`,
`pathHeuristic`, `pathCost`, `pathValid`, `pathAdd`); K-Mod's cost changes, extra `MOVE_`
flags and AI changes were not taken.

## What differs from stock
Every path has the cost and turns the exe's pathfinder gives when it searches from scratch.

- Ties: two paths of equal cost can be chosen differently. The seeded games first differ
  from stock on turn 30 (standard) and 12 (huge).
- Reused searches: stock answered `bReuse` requests from an earlier search, which could
  leave out the charge for the plot an attack is made from, or fit another unit. The new
  pathfinder ignores `bReuse` and drops its kept search when the group, start, moves, flags
  or an attack destination change, when any unit moves, appears, dies or is promoted, on
  war or peace, and once a game turn.
- `getPathLastNode()`, `getPathFirstPlot()` and `getPathEndTurnPlot()` give nothing after a
  failed request or a dropped search, where the exe might have given an older path's end.
- The kept search is not saved, so a loaded game can go on differently, as a stock one can.

## Measured
Huge game, 250 turns, profile builds: `generatePath()` from about 44.5 s to 22.5 s
inclusive. With `PATH_VERIFY=1` (profile builds), 3.2 million requests over three seeded
games matched the exe's pathfinder in turns and cost. With `PATH_EXE_ONLY` the standard
game has the stock DLL's hash on all 151 turns.

## Not checked
A game played by a human, and multiplayer.

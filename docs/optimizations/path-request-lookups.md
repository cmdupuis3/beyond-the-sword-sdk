# Path request lookups made once

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `PATH_STOCK_STEPS=1` |
| Scales with map size | flat |
| Scales with game era | late |
| Value | medium |
| Commits | `30720d4` |

## What it does
For every step the pathfinder weighs, `pathAdd()` and `pathCost()` each walked the group's
unit list, looked every unit up by ID and asked the map what the step costs that unit.
`pathCost()` also asked for the defence modifier of the plot a turn would end on once for
each neighbour it could be entered from. `KmodPathFinder` now looks the units up once per
request and hands them to `pathCostKept()` and `pathAddKept()` in `CvGameCoreUtils.cpp`,
which are the stock functions term for term. A step's movement cost is asked once and used
by both; a plot's defence modifier is asked once per request. The stock functions still
serve the exe's pathfinder.

## What differs from stock
Nothing in the game state: the paths are the same.

## Measured
Seeded games, profile builds, the define off and on, same state hash on every turn.
`KmodPathFinder::GeneratePath` takes 1,338 ms against 1,622 ms on the standard game (150
turns) and 20.5 s against 24.3 s on the huge game (250 turns), about 15% less of the
pathfinder's time on both. With `PATH_VERIFY=1` all 236,704 requests of the standard game
matched the exe's pathfinder in turns and cost.

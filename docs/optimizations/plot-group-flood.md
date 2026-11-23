# Plot group check by an in-DLL flood

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | medium |
| Commits | `002de5a` |

## What it does
`CvPlotGroup::recalculatePlots()` asked the exe's plot-group finder to flood the group
from its first plot and count the plots it reached; the group is kept if the count is its
size. The same flood is now done in the DLL, with `plotGroupValid()`'s step test, a stamp
per plot and a vector as the queue.

The same commit has `CvCity::updateTradeRoutes()` look up `MAX_TRADE_ROUTES` and
`IGNORE_PLOT_GROUP_FOR_TRADE_ROUTES` once per call, not in the test of three loops and for
every city outside its own plot group, and the city's own plot group once.

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile builds, same state hash on every turn:
`CvGame::updatePlotGroups()` 5.7 s to 4.4 s, `recalculatePlots()` own time 5.9 s to 2.8 s.
51,157 of 52,336 group checks find the group still connected. With `PLOT_GROUP_VERIFY`
(profile builds) the exe's flood gave the same count in all 5,224 checks. The standard
game was not timed.

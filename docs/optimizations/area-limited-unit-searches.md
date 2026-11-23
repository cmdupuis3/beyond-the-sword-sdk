# Unit AI searches limited to the unit's area

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | grows |
| Scales with game era | late |
| Value | medium |
| Commits | `c1aeb56`, `cf30d94` (`AI_explore`), `fd6af4b` (bonus plots) |

## What it does
Eleven `CvUnitAI` searches went through every plot on the map and did nothing for a plot
`AI_plotValid()` rejects: `AI_guardBonus`, `AI_guardFort`, `AI_sabotageSpy`, `AI_protect`,
`AI_pillage`, `AI_irrigateTerritory`, `AI_fortTerritory`, `AI_improveBonus`,
`AI_connectBonus`, `AI_routeTerritory` and `AI_explore`. For a land unit that cannot move over all
terrain, `AI_plotValid()` accepts only plots of the unit's own area, so those searches now
go through that area's plots alone, in plot index order. `CvMap::getAreaPlotIndices()`
keeps each area's plot indices and makes them again after a plot changes area
(`CvPlot::setArea`) or a game is loaded. `CvUnitAI::AI_plotValidScanList()` gives the
list, or none for sea units and units that can move anywhere, which still go through the
whole map. `AI_improveBonus`, which runs for every worker decision, goes through the map's
plots with a bonus (`CvMap::getBonusPlotIndices()`) where they are fewer, and always for a
sea worker.

## What differs from stock
Nothing in the game state: the searches meet the same plots in the same order.
`AI_explore` draws a random number for each plot, but only after `AI_plotValid()` has
accepted it, so it draws the same numbers.

## Measured
Huge game, 250 turns, same state hash on every turn: `AI_plotValid()` is called 58.1
million times instead of 135.6 million, own time 680 ms against 1,425 ms, and the area
lists were made once. Turn processing took 55.1 s against 56.9 s, on a busy machine. No
time was taken on the standard game; the work removed is a pass over every plot of the map
for each search. `AI_explore` alone, same game, profile builds: 2.94 million plots visited
instead of 5.31 million, 1.26 s against 1.44 s. `AI_improveBonus` through the bonus plots:
709 ms against 801 ms on huge, 110 ms against 124 ms on standard.

## Left out
Sea units still go through the whole map in `AI_explore`: on the huge game 84% of the plots
they accept are in their own water area and 15% in water next to their port, so their own
area would save little. `AI_exploreRange` already goes through a square around the unit.

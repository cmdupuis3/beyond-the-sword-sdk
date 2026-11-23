# Plot yields in one call

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `6a8ea5b` |

## What it does
The AI and `CvPlot::updateYield()` asked the yield functions about food, production and
commerce one after another, and each call looked up the same plot facts again, among them
whether the plot is beside a river and whether it can be irrigated.
`CvPlot::calculateNatureYields()`, `calculateImprovementYieldChanges()` and
`calculateYields()` now answer for all three yields. `CvCityAI::AI_bestPlotBuild()` works
out the plot's nature yields once per plot, not once per improvement, and
`AI_updateBestBuild()` and `AI_plotValue()` use the three-yield functions. In
`AI_plotValue()`, the count of bonuses an improvement can discover that the team can see is
kept per team in `CvInfoTables`, replacing a loop over every bonus on each call.

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile builds, same state hash on every turn. Improvement yield
calls go from 10.2 million one-yield calls to 3.2 million one-yield plus 1.9 million
three-yield calls. The machine was busy during the run; after scaling for that, the
improvement yield time is about 0.8 s against 1.4 s, `AI_bestPlotBuild` about 8% less and
`AI_updateBestBuild` about 6% less: roughly half a percent of turn time. With
`YIELD_VERIFY` (profile builds), 650 thousand three-yield answers matched the one-yield
functions. The standard game was not timed.

# AI attitudes kept between changes

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | medium |
| Commits | `d0c79a8` |

## What it does
`CvPlayerAI::AI_getAttitudeVal()` keeps its answer and gives it again until something it
is worked out from changes. Every write to such a value (about fifty setter, reset and load
sites) calls `invalidateAttitudeCache()`, which advances a count the kept values are
checked against. Code that adds an input to `AI_calculateAttitudeVal()` must do the same
wherever that input changes.

The same commit shortens three counts that looked at every plot of the map:
`AI_calculateStolenCityRadiusPlots()` walks the asking player's cities, and
`AI_calculateAdjacentLandPlots()` and `AI_calculatePlotWarValue()` take the other team's
plots from `CvMap::getTeamOwnedPlots()`, which reads only the blocks the team index says
that team owns plots in. `CvUnitAI::AI_promote()` returns at once for a unit that is not
ready for a promotion, as K-Mod does.

## What differs from stock
Nothing in the game state.

## Measured
Seeded standard game (150 turns), profile build, same state hash on every turn, of about
11.5 s of turn processing: `AI_getAttitudeVal()` 146 ms to 51 ms inclusive, the
stolen-plot count 40 ms to 3 ms, `AI_promote()` 77 ms to 18 ms. Four of five attitude
questions are answered from the kept value. With `ATTITUDE_VERIFY` (profile builds) nothing
differed in 49,364 kept attitudes, 5,592 stolen-plot counts and 9,468 adjacent-land counts.
The huge game was not timed.

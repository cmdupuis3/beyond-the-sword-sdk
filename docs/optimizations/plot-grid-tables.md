# Plot grid tables known at compile time

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | low |
| Commits | `d974e34` |

## What it does
The offsets of a plot's eight neighbours, of the four cardinal directions and of the 21
plots a city works, the city plot priorities, and the lookups from an offset to its
direction or city plot index were arrays that `CvGlobals::init()` filled at start-up and
handed out by pointer. Each use loaded the pointer from `CvGlobals` and then the value.
They are now `constexpr` arrays in `CvGameCoreUtils.h` (`CvPlotTables`), which
`plotDirection()`, `plotCardinalDirection()`, `directionXY()`, `plotCity()`,
`plotCityXY()` and the city plot priorities in `CvPlot.cpp` read directly.
`CvGlobals::init()` copies them into the arrays the exe and Python still ask `CvGlobals`
for.

## What differs from stock
Nothing in the game state. The exe and Python get the same values from the same functions.

## Measured
`plotDirection()` runs 538 million times and `plotCity()` 114 million times in the two
seeded games (PGO profile counts). In `CvPlot::isAdjacentPlayer()` the loop over the
eight directions loses two reloads from the stack on each pass and two registers are
freed. Timed with [the final classes](final-ai-classes.md) and the profile collected
again: no difference the seeded games can show (standard game 5.07 to 5.31 s against
5.15 to 5.44 s, five runs each). Both games keep the stock hashes on every turn.

## Not checked
Timed on its own.

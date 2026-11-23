# Feature graphics left until seen

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `STOCK_SYMBOL_UPDATES=1` |
| Scales with map size | shrinks |
| Scales with game era | early |
| Value | medium |
| Commits | `1d078bf` |

## What it does
When a forest or jungle is cut down or grows, the exe rebuilds the plot's trees in its own
work at the end of the game turn (`CvDLLEngineIFaceBase::DoTurn()`), about ten milliseconds
a plot on the huge map. Stock reports every such change at once. During the turns
`CvPlot::updateFeatureSymbol()` now leaves a plot out, and marks it, if the active team has
revealed neither it nor a plot next to it. The graphics are made when the plot or one next
to it is revealed (`CvPlot::updateFeatureSymbolVisibility()`), and
`CvMap::updateDeferredSymbols()` looks at the marked plots each time the exe is about to
get the screen. A feature that is always visible is not left out; no stock feature is.

## What differs from stock
Nothing in the game state. What the exe is asked to draw, and when, differs. A player
should notice nothing.

## Measured
Seeded games, all players AI, profile builds, same state hash on every turn.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| feature updates during turns left out | 75 | 312 |
| the exe's own work at the end of a game turn | 0.78 s to 0.38 s | 5.7 s to 3.6 s |
| time in `CvGame::doTurn()` | | 10.4 s to 7.4 s |

The gain depends on how much of the map the active player has seen: about four in ten of
the feature updates in the huge game are on plots that player has not seen, and a player
who has traded for everybody's maps gains nothing.

## Not checked
The map as drawn. Watch for a forest or jungle drawn where there is none, or missing where
there is one, on land that has just been explored.

# Yield and route symbols made once per draw

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `STOCK_SYMBOL_UPDATES=1` |
| Scales with map size | flat |
| Scales with game era | late |
| Value | high |
| Commits | `ec374ac`, `9444a5b` |

## What it does
Stock makes a plot's yield symbols anew every time one of its yields, its owner or what
the active team knows of it changes, and a route symbol anew, with those of the eight plots
around it, every time the active team learns of a route. In a round of AI turns the same
plot comes up several times, and nothing is drawn until `CvGame::update()` returns. While
`CvGame::update()` runs, `CvPlot::updateSymbols()` and `CvPlot::updateRouteSymbol()` now
only mark the plot, and `CvMap::updateDeferredSymbols()` makes the symbols once before the
function returns. Outside `CvGame::update()` they are made at once, as before. No yield
symbols are made for a plot the active team has not revealed, where stock makes them and
hides them; revealing the plot makes them (`CvPlot::updateSymbolVisibility()`).

## What differs from stock
Nothing in the game state. What the exe is asked to draw, and when, differs. A player
should notice nothing.

## Measured
Seeded games, all players AI, same state hash on every turn.

- Release build, standard game (150 turns), four runs with the off switch and four
  without, the sum of each turn's fastest run: 4.47 s against 4.19 s, 6% less.
- Profile builds, huge game (250 turns): time making these symbols during turns 3.53 s to
  1.33 s, time inside `CvGame::update()` 49.6 s to 46.4 s. Standard: 0.36 s to 0.13 s.

## Not checked
The map as drawn. Watch the first game played for yield icons missing from a revealed plot
(yield display on, and in the city screen) and for a road or railroad that is not drawn, or
not joined to its neighbours, after it is built or discovered.

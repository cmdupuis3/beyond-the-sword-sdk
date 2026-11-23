# Layouts out of sight made a few at a time

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `STOCK_LAYOUT_UPDATES=1` |
| Scales with map size | grows |
| Scales with game era | late |
| Value | medium |
| Commits | `7ba0dbc`; measured with `b57b982` |

## What it does
Before it draws, the exe asks every city and every plot whether its layout has to be made
anew (a city's buildings and houses, a plot's improvement and bonus), and makes all that
say yes in that one frame. A city is marked whenever it grows, shrinks or gets a building,
and a plot whenever a citizen starts or stops working it, so after a round of AI turns
most are. That frame is about three quarters of the time the exe keeps between two calls
of `CvGame::update()`, up to 1.7 s on the huge map late in the game.

- A city or plot the active team can see says yes at once, as before. One in the fog says
  yes only within an allowance of 2 cities and 16 plots for each call of
  `CvGame::update()` (`CvMap::takeLayoutUpdate()`); the rest stay marked and are made in
  the frames that follow, during the player's own turn.
- A city the active team has not found is not drawn, so its layout and its billboard wait
  until it is found (`CvCity::isDrawnForActiveTeam()`).
- A plot says yes to a change of being worked only if what its layout shows of that has
  changed (`CvPlot::isLayoutStateDifferent()`): the layout shows a plot as worked only
  while the active team can see it.

## What differs from stock
Nothing in the game state. A city in the fog can be drawn at its former size, and a fogged
plot with its former look, for some seconds after a round of AI turns: with a hundred
cities waiting, about twelve seconds at the stock four calls a second. City billboards
(name, size, what is being built) are not held back.

## Measured
Seeded games, all players AI, same state hashes with and without the off switch.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| release build, whole game, off switch set | 4.0 to 4.8 s (4 runs) | 44.3 s, 59.4 s |
| release build, whole game | 4.1 to 4.7 s (4 runs) | 36.6 s, 46.8 s |
| profile build, exe time between calls of `update()` | | 18.6 s to 10.6 s |

The huge runs were pairs, with the switch and without; the machine was busier for the
second pair. The slowest huge turn went from 1.6 s to 0.9 s. On the standard map the exe
draws only seven times in the game.

## Not checked
A game with a human in it, and the map as drawn. Watch for a city or an improvement in the
fog that never catches up, and for one in view that is drawn late.

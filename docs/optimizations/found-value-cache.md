# City-site scores kept between turns

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `FOUND_VALUE_FULL_SWEEP=1` |
| Scales with map size | shrinks |
| Scales with game era | early |
| Value | medium |
| Commits | `6ef48c0`, `c7b06d5` |

## What it does
`AI_updateFoundValues()` scored every revealed plot for every player every turn. Each
plot's score is now kept (`AI_refreshFoundValues()`) and worked out again only when
something it was worked out from has changed. Changes on the map mark the plots around
them (`CvMap::foundValuesChanged()`); inputs that many plots share (the team's cities and
visible bonuses, bonus values, culture multipliers) are compared at each sweep. While a
human takes their turn, `CvGame::updateAhead()` brings one AI player's scores up to date
every 100 ms (single-player only). The barbarians, and games that score or veto city sites
from Python, keep the stock sweep (`AI_updateFoundValuesAll()`).

`6ef48c0` also added per-player fixed found values and found modifiers on plots
(`CvPlot::setFixedFoundValue`, `setFoundModifier`, both in Python). They are not saved
with the game, and the defaults change nothing.

## What differs from stock
Nothing in the game state. The work ahead runs by the clock but is never relied on: a kept
score is used only if nothing it was worked out from has changed, so how long the human
takes cannot change the game.

## Measured
Seeded standard game (150 turns), profile build, `FOUND_VALUE_AHEAD_TEST` making the first
player's turn stand in for a human's, same state hash on every turn: the players' sweeps
go from about 1,450 ms to 400 ms, with 320 ms moved to the stand-in turn, of about 11.5 s
of turn processing. About one plot in fifteen (standard) or one in ten (huge) is scored
again at a player's own turn. No time was taken on the huge game; before the change the
whole update was about 2.4% of turn processing there. With `FOUND_VALUE_VERIFY`, none of
445,842 (standard) or 2,815,329 (huge) kept scores differed from a fresh one.

## Not checked
A game with a human in it: the work ahead has only run from the stand-in.

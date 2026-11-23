# Bonus and unit counts from the plots that can count

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | grows |
| Scales with game era | late |
| Value | low |
| Commits | `ca8199f` |

## What it does
Four counts went through every plot of the map for each question.
`CvPlayer::countOwnedBonuses()` and `countUnimprovedBonuses()` can only count a plot with a
bonus: `CvMap::getBonusPlotIndices()` keeps those plots, all of them and by bonus type, in
plot index order, and makes the lists again after a plot's bonus changes or a game is
loaded. `CvPlayerAI::AI_countNumAreaHostileUnits()` and `CvTeam::countEnemyDangerByArea()`
count units on plots picked by owner: the team index now also keeps how many units of any
kind each block of 8 x 8 plots has, and `CvMap::getPlotsWithUnits()` gives the plots with a
unit, leaving out blocks with none and blocks where no team whose plots count owns one.
Each plot left is tested as before.

## What differs from stock
Nothing in the game state: the four are counts, so the order of the plots does not matter,
and `countUnimprovedBonuses()` still calls the exe's border pathfinder in plot index order.

## Measured
Huge game (250 turns, profile build): the four took 61 ms against 400 ms, 0.8% of turn
processing; standard game (150 turns): 9 ms against 42 ms, 0.6%. Same state hash on every
turn before and after. With the six tier 2 switches off the standard game is the stock
DLL's on all 151 turns, and with `MAP_COUNTS_VERIFY` (profile builds) no unit count and no
kept bonus list differed.

## Not checked
`MAP_COUNTS_VERIFY` compares the bonus lists, not the two bonus counts. No game was loaded
from a save, which is when the lists and block counts are made again. No `--tier 1` DLL was
built. The era is judged, not measured.

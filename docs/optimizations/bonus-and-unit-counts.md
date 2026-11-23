# Bonus and unit counts from the plots that can count

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
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
Nothing in the game state is meant to: the four are counts, so the order of the plots does
not matter, and `countUnimprovedBonuses()` still calls the exe's border pathfinder in plot
index order.

## Measured
Nothing. Before the change the four took 0.62 s of 43.8 s of turn processing in the huge
game (250 turns, profile build), 1.4%, which bounds the saving.

## Not checked
The change was not compiled and no game was played with it: neither the state hashes nor
the time. `MAP_COUNTS_VERIFY` (profile builds) also takes the unit counts the stock way and
makes the bonus lists again to compare; it has not been run.

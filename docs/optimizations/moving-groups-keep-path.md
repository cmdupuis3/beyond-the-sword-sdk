# Moving groups keep their path

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `PATH_SEARCH_EVERY_STEP=1` |
| Scales with map size | grows |
| Scales with game era | late |
| Value | medium |
| Commits | `da4a1c2` |

## What it does
A group on a move mission asks for its path anew on every plot of the way
(`CvSelectionGroup::groupPathTo()`), and each time the K-Mod pathfinder searches from
scratch, because its tree starts at the plot the group has just left. Nothing else has
happened in between, and the rest of the path it found is as good a path from the new plot
as a new search would find. The path is now kept, plot by plot, and the group takes its
next step from it (`CvSelectionGroup::getPathStep()`).

The path is dropped by whatever drops the pathfinder's tree, the group's own steps along
it excepted: another unit moving, appearing or dying, a promotion, war or peace, a change
of the group's units, a new game turn. It is used only by the same group for the same
destination and flags, on the plot it expects, with the movement points it expects there.

## What differs from stock
The group goes where the path it found leads. A new search from the middle of that path
can choose another path of the same cost, and the stock pathfinder can cost the rest
differently once it starts there, so a unit can take another route to the same plot. The
seeded games differ from turn 55 (standard) and 81 (huge) on. The path kept is not saved.

## Measured
Seeded games, all players AI; with the off switch they are the games from before on every
turn, and two runs without it are the same game. The totals are of different games.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| mission steps taken from the path kept | 5,474 of 10,497 | 46,482 of 79,671 |
| searches for groups on their way | 10,566 to 5,057 | 80,957 to 35,034 |
| time in those searches | 0.08 s to 0.04 s | 1.54 s to 0.40 s |
| nodes the K-Mod pathfinder processed | 573,060 to 579,095 | 9,849,373 to 7,454,532 |
| time in `KmodPathFinder::GeneratePath` | 0.38 s to 0.35 s | 5.99 s to 4.55 s |
| release build, whole game, off switch set | 4.1, 4.2, 4.3 s | 39.0 s, 36.9 s |
| release build, whole game | 4.1, 4.1, 4.1 s | 38.6 s, 38.5 s |

The first five rows are from profile builds. The whole-game times do not show the saving:
the new huge game is a different one, and ends with 987 units where the old has 1,092.

## Not checked
A game with a human in it (a human's automated units use this), and multiplayer.

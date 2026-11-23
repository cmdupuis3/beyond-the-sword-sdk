# Path searches head for the destination

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `PATH_STOCK_HEURISTIC=1` |
| Scales with map size | grows |
| Scales with game era | not measured |
| Value | medium |
| Commits | `9ff250e` |

## What it does
The pathfinder guesses what is left of a path as 1,000 for each step to the destination.
A step over open ground costs 60,000, so the guess hardly steers the search, which spreads
round the start. Stock cannot guess more: a step that uses up a group's movement points is
charged for the points it had left, and that can be one point.

But every number of points a step can cost is made of the same few: a whole move for a
plot (half for a unit with a double move), a step along a route that is on the map, the
moves a unit has in a turn, and the points the group has left now. The greatest number
that divides all of these divides whatever a step costs, so no step costs less. The K-Mod
pathfinder now multiplies the guess by that number (`pathHeuristicWeight()` in
`KmodPathFinder.cpp`): 30 or 60 before any route, 10 to 30 with roads, as little as 2 once
railroads lie beside faster roads. It is 1, as in stock, if a terrain or route costs
nothing.

## What differs from stock
A path costs what it did, as nearly as before (see Measured); the plots are looked at in
another order, so one of two paths of equal cost can be chosen differently, and the games
differ (from turn 106 on huge).

## Measured
Seeded games, profile builds. Huge (250 turns): nodes processed 8.00 million to 5.02
million, time in `KmodPathFinder::GeneratePath` 5.22 s to 3.83 s of 45 s. Standard (150
turns): 617 thousand to 381 thousand nodes, 0.49 s to 0.36 s of 7.3 s. The weight was 30
or more for 83% of the huge game's requests and 10 to 29 for the rest. With `PATH_VERIFY`
the answers matched the exe's pathfinder in turns and cost on all 127,924 requests checked
in the standard game and on all but 8 of 929,923 in the huge one (2 dearer, 6 cheaper);
with the stock weight 20 of 953,985 differ.

## Not checked
A game with railroads (the weight never fell below 10, so none was on the map in the
seeded games), a game with a human, and multiplayer.

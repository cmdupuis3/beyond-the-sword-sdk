# Explorers look for paths from the best plot down

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `EXPLORE_STOCK=1` |
| Scales with map size | flat |
| Scales with game era | not measured |
| Value | low |
| Commits | `2921025` |

## What it does
Stock `AI_exploreRange()` and `AI_explore()` ask for a path to every plot worth anything and
draw the random part of a plot's value only once its path is found. Now every such plot
gets its random part first, from a stream of the decision's own (`CvAIRandScope`, as the
production and technology choices have), so a plot's value but for its path is known before
any path is. The plots are tried from the most valuable down. `AI_exploreRange()` stops at
the first plot with a path inside the range. `AI_explore()`, where the path's turns divide
the value, stops once no plot left could beat the best found, by the fewest turns its
distance allows (as `AI_targetCity()` does).

## What differs from stock
From the same numbers the same plot is chosen. The game still differs, from turn 0: the
plots draw other numbers, the game's own stream no longer moves when an explorer decides,
and fewer searches change what the K-Mod pathfinder carries over.

## Measured
Seeded games, profile builds; the games differ, so the times are per decision, without the
move. Huge game (standard in brackets): path searches a decision 118 to 9 (69 to 8) in
`AI_exploreRange()` and 100 to 31 (23 to 20) in `AI_explore()`. Search time a decision:
`AI_exploreRange()` by land 0.36 ms to 0.23 ms (0.31 to 0.23), `AI_explore()` by land
1.10 ms to 0.82 ms (0.22 to 0.18) and by sea 3.65 ms to 3.43 ms (1.45 to 1.39). At the old
game's numbers of decisions that is 2.33 s to 1.81 s of about 81 s, on a machine busy with
other work (0.45 s to 0.37 s of 13.8 s): 0.6% of the turn time on both.

## Not checked
A game with a human, and multiplayer. A sea explorer's `AI_explore()` gains little: the
fewest turns allowed for is too few once fast routes are on the map, so few plots are
ruled out.

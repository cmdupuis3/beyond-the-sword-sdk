# Route-to-city check from one flood

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | medium |
| Commits | `5337c68` |

## What it does
Every city, every turn, asked the exe's route finder whether each city of its team in its
area can be reached along the player's best routes (`AI_updateRouteToCity()`). Only
whether a path exists is used, and the finder's step test (`routeValid()`) looks at the
plot stepped into alone, so one flood from the city answers for every city. The flood is
now done in the DLL, at the first city asked about, and tests each plot once; the exe
tested a plot again for each neighbour it was reached from, and each test works out the
best route buildable on the plot. The step test is `isRouteFinderPlot()`, which
`routeValid()` calls too. Nothing is kept between calls: the test depends on gold,
technologies and connected bonuses.

The idea of answering route connection questions from labelled components comes from the
Caveman2Cosmos performance patch (RoadConnectivity).

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile builds, same state hash on every turn: `getBestRoute`
calls 9.05 million to 2.84 million, `canBuild` calls 18.4 million to 9.5 million, the two
together 1.19 s to 0.50 s. `AI_updateRouteToCity` is 0.36 s in all afterwards. With
`ROUTE_VERIFY` (profile builds) the exe's finder gave the same answer all 14,292 times in
the standard game. The standard game was not timed.

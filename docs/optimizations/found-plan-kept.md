# City-site plan kept by players without a settler

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `FOUND_SITES_STOCK=1` |
| Scales with map size | flat |
| Scales with game era | not measured |
| Value | medium |
| Commits | `2654dc2` |

## What it does
Every player worked out its city-site plan at the start of each turn
(`AI_updateFoundValues()`): the scores of the plots, the four best sites and each area's
best score. A settler acts on them; the cities only look at them when they weigh building
one. A player without a settler now keeps the plan as it was last worked out until it has
a settler, its number of cities changes, or four turns pass (more on a slower game, by the
game speed's train percent): `AI_isFoundPlanKept()`. The barbarians follow the same rule;
their cities are placed from scores `CvGame::createBarbarianCities()` works out itself.

## What differs from stock
The cities can weigh building a settler on a plan up to a few turns old, so the games
differ. Nothing goes into the saved game: a loaded game works the plan out at its first
turn.

## Measured
Seeded games, profile builds; the games differ. Huge (250 turns): the update 5.44 s to
2.78 s, 1,071 of 2,989 player-turns kept the plan; 131 cities at the end against 134.
Standard (150 turns): 1.19 s to 0.90 s, 491 of 1,193 kept; 49 cities against 50.

## Not checked
A game with a human, and multiplayer.

# Targets valued before their path is searched for

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `PATH_STOCK_BOUNDS=1` |
| Scales with map size | grows |
| Scales with game era | not measured |
| Value | medium |
| Commits | `2555dff` |

## What it does
Four unit AI searches looked for a path to every target and only then asked what it was
worth (`pathValueBounds()` in `CvUnitAI.cpp`).

- `AI_anyAttack` and `AI_cityAttack` work out the attack odds first, which do not depend
  on the path, try the plots from the best odds down, and take the first one they can
  reach in time.
- `AI_pillageRange` values each plot with the fewest turns its distance allows and stops
  once no plot left could beat the best found, as `AI_targetCity` already did.
- `AI_guardCity` leaves out a city that its distance puts beyond the turn limit.

## What differs from stock
From the same paths the same target is chosen. Fewer searches change what the K-Mod
pathfinder carries over, so a later path of equal cost can differ, and the games differ.

## Measured
Seeded games, profile builds. Path searches at the four sites on the huge game (250
turns): 79,398 to 9,479. Time a call: `AI_anyAttack` 24 to 14 µs, `AI_cityAttack` 34 to
24 µs, `AI_guardCity` 23 to 18 µs, `AI_pillageRange` 34 to 21 µs; at the old numbers of
calls about 1.0 s of 59 s. Standard (150 turns): 247 ms to 196 ms of 8.4 s. With
`PATH_BOUNDS_VERIFY` the attack searches chose the stock target in all 5,912 checks of the
standard game and all 59,469 of the huge one.

## Not checked
`AI_pillageRange` and `AI_guardCity` against the stock choice. A game with a human, and
multiplayer.

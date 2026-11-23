# Building-value inputs once per production choice

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `fa142ad` |

## What it does
`AI_buildingValueThreshold()` began every call by working out values that do not depend on
the building: the city's happiness, health and food surplus, its ranks among the owner's
cities, whether the owner is in financial trouble or going for culture, and whether it
trades abroad. `AI_chooseProduction()` values the buildings a city could construct several
times over, about six calls a choice in the seeded standard game, and changes nothing
about the city until it has chosen. Those values are now worked out once while
`AI_chooseProduction()` runs. Calls from anywhere else work them out as before.

## What differs from stock
Nothing in the game state.

## Measured
Seeded standard game (150 turns), profile build, same state hash on every turn:
`AI_buildingValueThreshold()` goes from 180 ms to 123 ms inclusive, of about 11.5 s of
turn processing. With `BUILDING_VERIFY` (profile builds) none of 6,027 kept sets differed
from a fresh one. The huge game was not timed.

# City values kept during citizen assignment

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | medium |
| Commits | `6f9311c` |

## What it does
`AI_assignWorkingPlots()` rebuilds a city's assignment one citizen at a time, and for each
citizen has `AI_yieldValue()` value every plot and specialist the city could use. Every one
of those calls worked out afresh how happy and healthy the city is, its food surplus, its
best food yield and how many good plots and specialists it has left, the last by valuing
every plot in the city again. None of that depends on the plot being valued. While
`AI_assignWorkingPlots()` runs, `AI_assignCached()` now works each of those out once and
keeps it until a worked plot or a specialist count changes
(`CvCity::getAssignChangeCount()`, advanced by `setWorkingPlot()` and
`setSpecialistCount()`). Outside `AI_assignWorkingPlots()` nothing is kept.

## What differs from stock
Nothing in the game state.

## Measured
Seeded standard game (150 turns), profile build, same state hash on every turn. 92% of the
values asked for were kept ones (928,749 of 1,011,002). `AI_assignWorkingPlots()` went
from 1,652 ms to 560 ms in a run that was about a quarter faster overall, so roughly half
its cost is gone. With `ASSIGN_VERIFY` (profile builds) no kept value differed from a
fresh one.

## Not checked
The huge game.

# Path searches the AI can do without

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `PATH_STOCK_SEARCHES=1` |
| Scales with map size | grows |
| Scales with game era | late |
| Value | medium |
| Commits | `3c51295` |

## What it does
Two changes to where `CvUnitAI` asks for paths.

- Turn limits: twenty searches that use a path only if it takes no more than so many turns
  (`AI_exploreRange`, `AI_guardCity`, `AI_group`, `AI_anyAttack`, `AI_cityAttack` and
  others) pass that limit to `generatePath()` (`iMaxPath`), and the K-Mod pathfinder does
  not search past it. K-Mod does the same.
- Cities not worth a search: `AI_targetCity`, `AI_moveToStagingCity` and
  `AI_targetBarbCity` first value each city at the most it could be worth, with the fewest
  turns the distance allows (`CvUnitAI::AI_minPathTurns()`, from the fastest route seen on
  the map, `CvMap::isRouteSeen()`). They try the cities from the most valuable down and
  stop once none left could beat the best. `AI_targetBarbCity` also drops a city that
  cannot be reached in under 10 turns.

## What differs from stock
From the same paths the same city is chosen. The game still differs:

- With a limit the pathfinder finds the cheapest path within it; stock found the cheapest
  of any length and turned it down if too long, so a unit can now take a dearer path.
- Fewer searches change what the K-Mod pathfinder carries over, so a later path of equal
  cost can differ.
- `AI_targetCity` draws its random part only for the cities it searches for, so the random
  numbers run differently, and the exe's step pathfinder is asked in a different order.

## Measured
Seeded games before and after, profile builds; the games differ from turn 90 (standard) and
83 (huge), so these are totals of different games.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| nodes the K-Mod pathfinder processed | 1,214,623 to 801,216 | 16,636,396 to 9,179,826 |
| time in `KmodPathFinder::GeneratePath` | 0.63 s to 0.52 s | 7.89 s to 5.12 s |
| time in `CvGame::update()` | 6.2 s to 5.1 s | 39.5 s to 37.5 s |

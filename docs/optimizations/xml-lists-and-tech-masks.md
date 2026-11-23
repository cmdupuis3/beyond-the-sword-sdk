# XML lists and technology masks

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | medium |
| Commits | `651dbeb` |

## What it does
Stock answers "which builds lay a route", "what does this unit upgrade to", "which
building classes does this building need" and "do we have the technology" by looping over
every build, unit class or building class each time. `CvInfoTables` works the lists out
once from the XML and keeps each team's technology answers until its technologies change.

- `CvPlayer::getBestRoute()` takes the first build that passes from a list sorted best
  route first. `CvPlayer::canBuild()` asks the technology before it examines the plot.
- `CvCity::canTrain()` and `canConstruct()` reject with one lookup what the team has no
  technology for. `CvPlayer::canConstruct()` tests only the building classes the building
  needs. `CvCity::allUpgradesAvailable()` walks the unit's direct upgrades.
- `CvCityAI::AI_bestPlotBuild()` tries the builds that make each improvement, not every
  build for every improvement. `finalImprovementUpgrade()` is a table lookup.

## What differs from stock
Nothing in the game state. When Python's `canBuild` callback is on, the stock loop
(`getBestRouteEveryBuild()`) is used.

## Measured
Huge game (250 turns), profile builds, same state hash on every turn, against the profile
from before link-time optimisation, so part of each drop is the optimiser's:
`getBestRoute` 5.1 s to 1.3 s, `canBuild` 4.1 s to 1.2 s, `CvCity::canTrain` 3.0 s to
0.6 s, `AI_chooseProduction` 7.2 s to 4.6 s, `AI_updateBestBuild` 7.3 s to 5.5 s. With
`INFO_TABLE_VERIFY` (profile builds), 3.58 million mask answers and 504 thousand best
routes were checked against the stock code, none wrong. The standard game was not timed.

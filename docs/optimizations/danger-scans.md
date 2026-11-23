# Danger scans ruled out by a team index

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | high |
| Commits | `e06bc5e`, `93e650f` |

## What it does
`AI_getPlotDanger()` visits every plot within range of its target and walks the unit lists
on every call, and nearly all calls find nothing. The pathfinder asks for every node it
expands for a group that cannot fight. `CvMap` now keeps a team index: for each plot and
each 8 x 8 block of plots, a word with a bit per team that has a unit there able to attack
and a word for the team that owns it, and per plot how far each team's attackers there
reach. `CvPlot::addUnit()`, `removeUnit()`, `setOwner()`, `setRouteType()` and
`CvUnit::changeExtraMoves()` keep it; a load, a player changing team and
`CvTeam::changeExtraMoves()` mark it for a rebuild.

`AI_getPlotDanger()` asks the index first. It answers 0 without a scan when no enemy
attacker in the target's area has the reach to cover its distance and, where border danger
applies, no enemy territory is one plot away, or two away with a route. Otherwise the scan
runs unchanged.

## What differs from stock
Nothing in the game state.

## Measured
Seeded games, profile builds, same state hash on every turn. Before the change the huge
game (250 turns) made 24.8 million calls, 18% of turn processing.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| calls | 1.76 million | 24.8 million |
| calls that still scan | 2.4% | 2.3% |
| the function's own time | | 30.5 s to 7.8 s |

The first commit alone took the huge game to 10.0 s. With `DANGER_VERIFY` (profile builds)
the scan found danger in none of the calls the index ruled out. No time was taken on the
standard game.

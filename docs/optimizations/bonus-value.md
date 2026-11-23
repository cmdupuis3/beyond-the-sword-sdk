# Bonus value skips buildings with no effect

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | medium |
| Commits | `a3b3df9`, `2a7ae77` |

## What it does
For every bonus, `AI_baseBonusVal()` ran its technology checks and `canConstruct()` on
every building class, including the buildings the bonus has no effect on. Those contribute
nothing whatever the checks say, so the checks now run only when the building's value for
the bonus is non-zero. Units and projects already had the equivalent test.

Inside its loop over the builds, the function asked `getBestRoute()` again for every build
that lays a route. It now uses the answer it already worked out before the loop; nothing
in the loop changes what the team can build.

## What differs from stock
Nothing in the game state. `canConstruct()` is no longer called for those buildings, so a
mod's Python `canConstruct` and `cannotConstruct` callbacks see fewer calls from here.

## Measured
Seeded standard game (150 turns), profile build, same state hash on every turn: 15,795
recalculations took 357 ms instead of 932 ms, which takes bonus valuation from 5.4% to
2.0% of turn processing (`a3b3df9`). The route change (`2a7ae77`) was not measured on its
own. The huge game was not timed.

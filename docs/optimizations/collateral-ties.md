# Collateral damage ties by plot order

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | not measured |
| Value | low |
| Commits | `8fb37cf` |

## What it does
`CvUnit::collateralCombat()` chooses which defenders take collateral damage, highest value
first (a random number times the unit's hit points). Stock kept the candidates in a
`std::map` by the unit's address in memory, so when two units had the same value the one
lower in memory was hit. The candidates are now kept in the order they are on the plot, and
a tie goes to the unit first on the plot.

This is not a speed change. It is here because other changes need it: stock's result
depended on how memory was laid out, so a change that did nothing but allocate memory
differently (the [define lookup cache](define-lookup-cache.md), before this fix) changed
the seeded huge game from turn 213.

## What differs from stock
Only ties, which stock settled by where the units happened to be in memory; the result is
one stock could have given, which tier 1 allows. In multiplayer stock's result could
differ between computers.

## Measured
No speed effect. Profile builds count the choices and the ties ("collateral damage:" in
the profile): 1 tie in 174 choices in the seeded huge game (250 turns), which stock
happened to settle the same way, so both seeded games keep their state hashes.

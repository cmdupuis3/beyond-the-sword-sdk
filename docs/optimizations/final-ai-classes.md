# AI classes and the free-list array marked final

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | low |
| Commits | `fe79f90`, `6e44f36` |

## What it does
`CvUnitAI`, `CvCityAI`, `CvPlayerAI`, `CvTeamAI`, `CvGameAI` and `CvSelectionGroupAI` are
the only classes derived from their bases, and nothing derives from them or from
`FFreeListTrashArray`. Declared `final`, a virtual call made through one of them
(`GET_PLAYER(x).AI_...()`, a `CvUnitAI*` loop variable) is bound when the code is compiled
and can be inlined. A call through the base class (a `CvUnit*`, or `this` inside
`CvPlayer.cpp`) is still virtual.

A bound call changes the shape of the function that makes it, and the PGO profile no
longer fits such a function, so the profile was collected again (`6e44f36`).

## What differs from stock
Nothing in the game state. Layouts and virtual tables are the same, so the exe sees no
difference.

## Measured
In the linked object, indirect calls through a virtual table go from 20,129 sites to
19,117. With the old profile 382 functions were optimised without it (40 before) and the
code was 4% larger; with the new one none are and it is 0.9% larger. Timed with
[the plot grid tables](plot-grid-tables.md) and the new profile, runs taken in turn: no
difference the seeded games can show. Standard game, five runs each, 5.07 to 5.31 s
against 5.15 to 5.44 s; huge game, eight runs each, sum of each turn's fastest run 27.3 s
against 27.1 s. Both games keep the stock hashes on every turn.

## Not checked
Timed on its own, or with the old profile against the new one on the same source.

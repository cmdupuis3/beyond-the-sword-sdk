# Yield-value inputs kept for a pass over the city

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | medium |
| Commits | `0eedfec` |

## What it does
`AI_yieldValue()` values each plot and specialist a city could use, millions of times a
game, and every call read the same city-wide values again: the yield and commerce
modifiers, the production modifier (a loop over every bonus), the owner's commerce weights
(a loop over the teams for espionage), the growth threshold and anger lengths (up to five
define lookups by name) and more. `CvCityAI::YieldValueContext` now holds those values.
`AI_assignWorkingPlots()`, `AI_updateBestBuild()`, `AI_updateWorkersNeededHere()` and
`AI_countGoodTiles()` open a scope in which it is worked out at its first use and kept;
`AI_updateSpecialYieldMultiplier()` and `AI_setEmphasize()` make the next use work it out
again. What changes as plots are worked is still read on every call. Outside a scope each
call works the context out, as before.

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile builds, same state hash on every turn, against the profile
before the change scaled for a busy machine during that run: `AI_yieldValue` 7.2 s to
3.7 s, `AI_assignWorkingPlots` 7.3 s to 4.2 s, `AI_plotValue` 5.1 s to 2.0 s. A kept
context was used 3.41 million times; 55 thousand were worked out in a scope and 4.5
thousand outside one. With `YIELD_VALUE_VERIFY` (profile builds), none of 418 thousand
kept contexts in the standard game differed from a fresh one. The standard game was not
timed.

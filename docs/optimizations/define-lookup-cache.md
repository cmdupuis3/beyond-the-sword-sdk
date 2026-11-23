# Define lookups kept by name

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | medium |
| Commits | `8c04ca2` |

## What it does
`GC.getDefineINT()` and `getDefineFLOAT()` looked a define up by name in the defines' hash
map on every call: a `std::string` made from the name, a hash and a search, about 170 ns.
The seeded huge game asks 16.8 million times (326 names; `DEFAULT_SPECIALIST` alone 2.8
million), about 3 s of turn processing in a profile build. The answers are now kept in a
table for each type, by the address of the name asked with, and used only when the name
also matches the copy kept with it (Python passes buffers whose contents change).
`CvGlobals::definesChanged()` empties the tables; `cacheGlobals()` calls it after the XML
defines are read and whenever a define is set, and `SetPostGlobalsGlobalDefines()` calls
it too.

## What differs from stock
Nothing in the game state. The change allocates memory differently, which changed the
seeded huge game from turn 213 until [collateral damage ties](collateral-ties.md) were
settled by plot order.

## Measured
Release builds, standard game (150 turns), three runs each taken in turn on a busy
machine, the sum of each turn's fastest run: 6.02 s against 6.79 s before, 11% less. Both
seeded games keep their state hashes on every turn. With `DEFINE_CACHE_VERIFY=1` (profile
builds) none of 16.9 million (huge) or 2.9 million (standard) kept answers differed. The
huge game was not timed after the change.

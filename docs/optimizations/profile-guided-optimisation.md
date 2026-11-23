# Profile-guided optimisation

| | |
| --- | --- |
| Tier | 1 |
| Off switch | build option `--no-pgo` |
| Scales with map size | grows |
| Scales with game era | const |
| Value | high |
| Commits | `b9ad550`, `dc7178c`, `a78fab4`, `aa0c6ea`, `b7e3534` |

## What it does
The build is optimised with a record of which code a real game runs,
`pgo/CvGameCoreDLL.profdata`, collected on the seeded standard and huge games.
`tools/build_dll.py` uses it unless `--no-pgo`, `--pgo-use FILE` or `--pgo-train` is
given; a `--profile` build leaves it out, because its timing scopes change about 420 of
the hottest functions. `--pgo-train` builds a DLL that counts what runs and writes LLVM's
raw profile itself (`bridgedPgoTurn()` in `CvGameCoreDLL.cpp`), every tenth game turn,
since LLVM's profile runtime is not available for this target. It records indirect call
targets and copy sizes as well. The profile was last collected on the source as of
`fe79f90`; the build counts the functions that no longer match it. The recipe is in
`README.md`.

## What differs from stock
Nothing in the game state. The source is the same; only the build differs.

## Measured
Seeded games, release builds run in turn (five runs standard, three huge), the sum of
each turn's fastest run, same state hash throughout.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| built without a profile | 5.00 s | 53.9 s |
| trained on the standard game | 4.85 s, 3.0% less | 52.4 s, 2.9% less |
| trained on both games (the profile in the repository) | 4.67 s, 6.7% less | 47.4 s, 12.1% less |

The figures move by a few percent between sets of runs. The first 75 turns of the huge
game, 3.4 s in all, were slightly slower with both profiles.

## Not checked
A game the profile was not collected on, other than the huge game with the standard
game's profile.

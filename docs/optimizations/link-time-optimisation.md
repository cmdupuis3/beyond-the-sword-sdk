# Link-time optimisation

| | |
| --- | --- |
| Tier | 1 |
| Off switch | build option `--no-lto` (CMake `BRIDGED_LTO=OFF`) |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | high |
| Commits | `fbd0fc2` |

## What it does
Without link-time optimisation no call into another source file is ever inlined, and
nearly every accessor in this code is a one-line function in a `.cpp` file. `CMakeLists.txt`
has the `BRIDGED_LTO` option, on by default: the SDK units are compiled with `-flto` and
the linker keeps the one object it makes of them. The EH adapter stays ordinary object
code. `tools/build_dll.py` runs its exception metadata check on the linker's object
instead of on each unit's. A change to any source now costs a relink of about two minutes.

## What differs from stock
Nothing in the game state. The source is the same; only the build differs.

## Measured
A throwaway build of the same configuration, before the change was committed: the seeded
standard game (150 turns) and huge game (250 turns) had the same state hash on every turn
as the build without it, and turn processing took about 15% less on the standard map. The
huge-map timing was too noisy to put a number on.

## Not checked
The DLL the build script made at that commit was not itself run through the games.

# BridgedCvGameCore

A Civilization IV: Beyond the Sword GameCore DLL built with Clang and C++23
for the original 32-bit game. It keeps the Python 2.4 and Boost.Python bindings.
How closely it keeps to stock gameplay is chosen when it is built: see [Tiers](#tiers).

## Build

Requires CMake 3.28+, Ninja, Python 3 with `pefile==2024.8.26` and BtS.
Run commands from the repository root.

### Linux (Bash)

Use the pinned Linux Clang/LLD 23.1.2 package, ICU 70 and Proton.

```bash
python3 tools/prepare_dependencies.py \
  --proton /path/to/Proton/proton --steam-client /path/to/Steam

BTS_DIR="/path/to/Beyond the Sword"
python3 tools/build_dll.py \
  --bts-dir "$BTS_DIR" \
  --llvm-bin /path/to/llvm-23.1.2/bin \
  --icu70-dir /path/to/icu70

```

Steam launch options: `WINEDLLOVERRIDES="msvcr71,msvcp71=n" %command%`.

Keep Proton/Wine prefixes and test installations outside this checkout when
it is installed as a mod. BtS recursively scans the mod directory before
loading the DLL, including files reached through symlinks. A prefix's
`pfx/dosdevices/z:` link points to `/` and can send that scan through the host
filesystem essentially indefinitely.

### Windows (PowerShell)

Extract the [pinned Windows LLVM 23.1.2 archive](https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.2/clang+llvm-23.1.2-x86_64-pc-windows-msvc.tar.zst).
Put Python, CMake and Ninja on `PATH`. This build path has been checked under Wine.

```powershell
python tools/prepare_dependencies.py

$BtsDir = "C:/Games/Beyond the Sword"
python tools/build_dll.py `
  --bts-dir "$BtsDir" `
  --llvm-bin "C:/LLVM-23.1.2/bin"

```

Both builds produce `tmp/dll-build/CvGameCoreDLL.dll`, optimised with the profile in
`pgo/` (see [Profile-guided optimisation](#profile-guided-optimisation)). Copy it into your test
mod's `Assets/`, preserving the original game DLL. The static exception adapter
uses the original MSVCR71/MSVCP71 providers and needs no supplemental modern CRT.
Its supported metadata, ABI evidence and exception diagnostics are documented
in [compat/README.md](compat/README.md).

`prepare_dependencies.py` prepares the SDK headers and import libraries under
`tmp/dependencies/` and reuses verified files. Repeated runs need no Proton
when the SDK group is complete. `--sdk-installer` selects a downloaded installer.
SDK installation on Linux uses a temporary Proton prefix outside the checkout.
For native runtime reference diagnostics only, use `--only runtime` with 7-Zip;
`--only all` explicitly prepares both groups. The runtime is not a build input.

For a custom dependency location, pass `--output-dir tmp/my-dependencies` to
the preparation script and `--deps-dir tmp/my-dependencies` to the build script.
Existing SDK caches still work through the build's `--vc71` and `--windows-sdk`
overrides. `--output-dir` selects a custom build directory;
Ninja reuses objects whose inputs have not changed.

`prepare_headers.py` generates the pinned compatibility header view during
CMake configuration. `build_support.py` provides shared hashing and PE metadata.
`dependency-lock.json` pins the Linux tools and stock references, `link-lock.json`
pins additional tools and link inputs, and `windows-toolchain-lock.json` pins
the Windows LLVM package.

## Tiers

Every change to the stock game core is in one of three tiers, and the build chooses how
many of them the DLL has (`tools/build_dll.py --tier N`):

| `--tier` | macro | what the DLL may do | output folder |
| --- | --- | --- | --- |
| 1 | `REPRODUCE_BTS` | Tier 1 only. The game state is the stock DLL's on every turn. | `tmp/dll-build-tier1` |
| 2 (default) | neither | Tiers 1 and 2. The game can differ from the stock game, but the same start and the same input always give the same game. | `tmp/dll-build` |
| 3 | `NON_DETERMINISM_OK` | Tiers 1 to 3. The game can differ from one run to the next. | `tmp/dll-build-tier3` |

`CvGameCoreDLL/BridgedTiers.h` defines the tiers and says how a change is marked in the
source. [docs/tiers.md](docs/tiers.md) says what tier 1 allows and how the tiers are
checked. [docs/optimizations/](docs/optimizations/README.md) has a table of every
optimization with its tier, how it scales with map size and what it is worth, and a short
doc for each. A `--tier 3` DLL plays the default build's game unless a human is playing.

The profile in `pgo/` was collected on the default build. A `--tier 1` build uses it too;
the few functions that differ are optimised without it.

## Profile-guided optimisation

The build is optimised with a record of which code a real game runs. The record is
`pgo/CvGameCoreDLL.profdata`. It was collected on the seeded standard game (150 turns)
and the seeded huge game (250 turns), all players AI, with the source as it was at commit
`fe79f90`. `tools/build_dll.py` uses it unless told otherwise:

| option | profile used | output folder |
| --- | --- | --- |
| none | the one in the repository | `tmp/dll-build` |
| `--no-pgo` | none | `tmp/dll-build-no-pgo` |
| `--pgo-use FILE` | `FILE` | `tmp/dll-build-pgo` |
| `--pgo-train` | none: this builds the DLL that collects a profile | `tmp/dll-build-pgo-train` |
| `--profile` | none, unless `--pgo-use` is given | `tmp/dll-build-profile` |

A `--profile` build leaves the profile out because its timing scopes change about 420 of
the hottest functions, and the profile no longer fits a function that has changed. The
times it reports are therefore those of a DLL built without the profile.

A profile goes stale as the source changes: a function whose shape has changed is
optimised without it. The build counts those functions, as `pgo_mismatched_functions` in
what it prints and in `report.json`, and says so when there are any. The count covers the
units compiled in that run only, and leaves out functions defined in headers. A few
changed functions do not matter. After larger changes collect a new profile: build a
training DLL, play the two seeded games with it, merge the two files it writes over the
one in the repository, and build again.

```powershell
python tools/build_dll.py --bts-dir "$BtsDir" --llvm-bin "C:/LLVM-23.1.2/bin" --pgo-train
python bench/run_bench.py --dll tmp/dll-build-pgo-train/CvGameCoreDLL.dll --label train-standard --turns 150 --pgo-train
python bench/run_bench.py --dll tmp/dll-build-pgo-train/CvGameCoreDLL.dll --label train-huge --turns 250 --world-size WORLDSIZE_HUGE --timeout 2400 --pgo-train
& "C:/LLVM-23.1.2/bin/llvm-profdata.exe" merge -o pgo/CvGameCoreDLL.profdata bench/results/train-standard.profraw bench/results/train-huge.profraw
python tools/build_dll.py --bts-dir "$BtsDir" --llvm-bin "C:/LLVM-23.1.2/bin"
```

It pays to train on more than one game: a profile from the standard game alone does
little for a huge map (see the table below). The huge game takes a few minutes.

The training DLL writes the profile itself (`bridgedPgoTurn()` in `CvGameCoreDLL.cpp`),
every tenth game turn, to the file named by the `BRIDGED_PGO_RAW` environment variable.
LLVM's own profile runtime is not used: it is not shipped for 32-bit Windows and needs a
modern C runtime. The two functions of it that instrumented code calls as it runs are in
`CvGameCoreDLL.cpp` as well. They record which functions indirect calls go to and the
sizes of memory copies, so the profile has those too. The writer follows the raw profile
format of the pinned LLVM 23.1.2 and has to be checked against `profile/InstrProfData.inc`
if the compiler is changed.

The training DLL is for collecting a profile only. Its link-time object is too large for
`tools/eh_metadata.py` to read, so its exception metadata is not checked; the DLL built
from the profile is checked like any other.

Measured on the seeded games, all players AI. Every DLL, the training one included, plays
the same game: the same state hash on all 151 turns of the standard game and all 251 of
the huge one. The DLLs were run in turn, five runs each of the standard game and three of
the huge one. The times are the sum over the turns of each turn's fastest run, because
whole runs differ from one another by more than the gain; they include the time the exe
takes to draw between calls, which is the same for every DLL and about half a second a
turn late in the huge game.

| | standard, 150 turns | | huge, 250 turns | |
| --- | ---: | ---: | ---: | ---: |
| built without a profile (`--no-pgo`) | 5.00 s | | 53.9 s | |
| trained on the standard game | 4.85 s | 3.0% less | 52.4 s | 2.9% less |
| trained on the standard and the huge game (the profile in the repository) | 4.67 s | 6.7% less | 47.4 s | 12.1% less |

These figures move by a few percent from one set of runs to the next: an earlier set of
eight runs of the standard game gave 6.2% less for the DLL trained on that game, and 1.8%
less for one whose profile left out call targets and copy sizes. The first 75 turns of the
huge game, which take 3.4 s in all, were slightly slower with both profiles.

The optimised DLL is 0.8 MB smaller. Not measured: a game the profile was not collected
on, other than the huge game with the standard game's profile.

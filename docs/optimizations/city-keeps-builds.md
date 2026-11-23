# Cities keep their chosen builds

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `BEST_BUILD_STOCK=1` |
| Scales with map size | flat |
| Scales with game era | late |
| Value | medium |
| Commits | `a9b440c` |

## What it does
Every turn stock has every city work out again which build it wants on each plot in its
radius (`CvCityAI::AI_updateBestBuild()`); the workers choose their jobs from that. A city
now keeps what it chose (`CvCityAI::AI_isBestBuildKept()`) until:

- a plot in its radius changes owner, working city, terrain, feature, bonus, improvement,
  route or irrigation, or a city is founded or lost there (`CvPlot::cityBuildsChanged()`);
- its population or what it emphasizes changes;
- its owner's team gets a technology or its owner changes a civic
  (`CvPlayer::cityBuildsChanged()`);
- it has kept them for 4 game turns at normal speed (`BEST_BUILD_KEEP_TURNS`). The time
  follows the game speed's `iBuildPercent`: 2 turns quick, 6 epic, 12 marathon.

## What differs from stock
The part that follows the workers goes stale: a build's value is divided by the workers
already on their way, and the food the city counts on includes builds in progress. Workers
choose among cities with figures up to three turns old at normal speed. A build that can no
longer be made is not started; the workers check that themselves.

- The turn a city last worked its builds out is not saved. A loaded game works them all
  out on the next turn and can go on differently from the same game played straight through.
- A network game works them out every turn, as stock does.

## Measured
Seeded games, all players AI, profile builds. The games differ from turn 27 (standard) on.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| city turns that kept their builds | 50% | 44% |
| time in `AI_updateBestBuild()` | 0.37 s to 0.22 s | 2.61 s to 1.25 s |

At marathon speed 83% of city turns on the standard game keep their builds.

## Not checked
The huge game a second time, and the tier 1 DLL on the huge game after this change.

# City builds worked out ahead on several threads

| | |
| --- | --- |
| Tier | 3 |
| Off switch | `WORKER_THREADS_OFF=1` (the burst on the main thread); `BEST_BUILD_AHEAD_OFF=1` switches the builds ahead off |
| Scales with map size | flat |
| Scales with game era | late |
| Value | low |
| Commits | `3239d18` |

## What it does
[City builds chosen on the human's turn](builds-on-human-turn.md) had one AI player's
cities work out their builds every tenth of a second, on the main thread. Now every city
that wants it is an item of the burst that does the city-site scores
([City-site scores worked out ahead on several threads](found-values-ahead-in-a-burst.md)),
for all the AI players at once. What the stock AI fixes once a game turn on first use (the
strategy, the average yields, the mission target counts, the costs) is made first, as the
player's first city would make it. A city that needs something else made is put back as
it was and done again after the main thread has made it.

## What differs from stock
As for the builds chosen on the human's turn: they are used, and which cities get them
depends on the clock. The burst itself gives what the same work one city at a time gives.

## Measured
Tier 3 profile build, `BEST_BUILD_AHEAD_TEST` and `FOUND_VALUE_AHEAD_TEST` standing in for a
human's turn, same game, all the work ahead over the game:

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| one player at a time (`WORK_AHEAD_SERIAL=1`) | 370 ms | 2,207 ms |
| in bursts | 164 ms | 998 ms |

The work runs on the human's turn, so what is cut is how long the screen is held for it;
in the background ([Work ahead done in the background](work-ahead-in-the-background.md))
it is not held at all.

## Not checked
A game with a human in it: the burst has run only from the stand-in. Mods with the
can-build, research, can-train or can-construct callbacks on. No release build of tier 3.

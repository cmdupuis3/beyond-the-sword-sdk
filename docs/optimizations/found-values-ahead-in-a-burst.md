# City-site scores worked out ahead on several threads

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `WORKER_THREADS_OFF=1` (every burst on the main thread) |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `6b76660` |

## What it does
While a human takes their turn, `CvGame::updateAhead()` used to bring one AI player's
city-site scores up to date every tenth of a second, on the main thread. Now it does every
AI player's at once, in one burst on a pool of worker threads (`BridgedThreads.h`,
`CvPlayerAI::AI_doWorkAhead()`), and the interface waits only for the burst. Anything
the game makes on first use, once a game turn, is never made on a worker: the item that
needs it is handed back, the main thread makes it in the order of the players, and the
item goes through another burst (three rounds at most, then the main thread does it).

## What differs from stock
Nothing in the game state. The scores worked out ahead are never relied on, and what the
burst leaves is what the same work one player at a time leaves.

## Measured
Seeded standard game, profile build, `FOUND_VALUE_AHEAD_TEST` standing in for a human's
turn, same game: the work ahead takes 157 ms over the game one player at a time
(`WORK_AHEAD_SERIAL=1`) and 123 ms in bursts. No AI turn time is saved: this work runs
on the human's turn. The pool pays more with the cities' builds
([City builds worked out ahead on several threads](builds-ahead-in-a-burst.md)).

## Not checked
A game with a human in it: the bursts have run only from the stand-in. Mods that answer
can-train or can-construct from Python (their bonus values are then handed back, a path
that has not been run).

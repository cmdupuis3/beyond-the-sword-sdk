# City builds chosen on the human's turn

| | |
| --- | --- |
| Tier | 3 |
| Off switch | `BEST_BUILD_AHEAD_OFF=1` in a `--tier 3` build; other builds do not have it |
| Scales with map size | shrinks |
| Scales with game era | late |
| Value | medium |
| Commits | `d511700` |

## What it does
While a human takes their turn in a single-player game, `CvGame::updateAhead()` goes round
the AI players, one every tenth of a second (all of them at once, on several threads, since
[`3239d18`](builds-ahead-in-a-burst.md)). In a tier 3 build each of them then has all
its cities work out their builds for their plots
(`CvPlayerAI::AI_updateBestBuildsAhead()`): every city once a game turn, and again if
something it looks at changes afterwards. A city that has had its turn ahead keeps what it
chose, by the rule of [Cities keep their chosen builds](city-keeps-builds.md), and on its
own turn works them out again only if something has changed since. The strategy and the
average yields that the stock AI fixes once a game turn on first use are put back after
the work ahead.

## What differs from stock
What is worked out ahead is used. Which cities were reached before the human ended the
turn depends on the clock, and a city that was reached chose its builds before the players
in between moved. The same game with the same input can go differently from one playing to
the next. Without a human in the game a tier 3 DLL plays the default build's game.

## Measured
The seeded games have no human. With `BEST_BUILD_AHEAD_TEST=1` the first player's turn
stands in for one. `--tier 3 --profile` build, time in `AI_updateBestBuild()` left in the
players' turns:

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| stock (`BEST_BUILD_STOCK`) | 0.37 s | 2.61 s |
| default build (tier 2) | 0.22 s | 1.25 s |
| tier 3, worked out ahead | about 0.09 s | about 0.61 s |
| time ahead, which a human's turn would hide | 0.26 s | 1.78 s |

## Not checked
A game with a human in it: `CvGame::updateAhead()` only runs then, so the code that goes
round the players during a human's turn has not run at all, only the same work called from
the stand-in. No release build of tier 3 has been made.

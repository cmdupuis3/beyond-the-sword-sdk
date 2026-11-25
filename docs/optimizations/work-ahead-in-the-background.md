# Work ahead done in the background

| | |
| --- | --- |
| Tier | 3 |
| Off switch | `WORK_AHEAD_BACKGROUND_OFF=1` (the burst the game waits for, as before) |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `0ebf87a`, `73b17f7`, `b2dc17c`, `ceee1d2` |

## What it does
While a human takes their turn, the AI players' work ahead
([city sites](found-values-ahead-in-a-burst.md), [city builds](builds-ahead-in-a-burst.md))
ran in a burst the game waited for. In a tier 3 build it now starts at the end of
`CvGame::update()` on the worker threads alone, at low priority, and the exe goes on drawing
and taking input (`CvPlayerAI::AI_startWorkAheadInBackground()`). The game is kept still
while the workers read it: `CvGame::update()` makes no pass until the work is over, and a
decision of the human's (one of the 25 network messages that change the game, or a deal)
first gives the workers 10 ms to finish, stops them after the items in hand if that is not
enough, and is then done at once (`CvPlayerAI::AI_settleWorkAhead()`, `CvDecisionQueue`).
A save does the same; a game loaded or left drops the work.

## What differs from stock
As for the builds chosen on the human's turn: which cities get them depends on the clock.
The work itself gives what the burst gives, and what a stopped burst did not start is put
back and done the next time.

## Measured
Rated by AI turn time, which it does not touch; what it removes is the screen held for the
work. A played game (standard map, to turn 110, profile build): 1,057 bursts, none stopped
part way and none holding a pass of the game; 131 decisions came while one ran and waited
0.4 ms on average; the sync checksum was the same after the work in all 926 checked. In an
earlier form that kept decisions until the next `CvGame::update()` they waited 122 ms on
average, the exe calling it about every 264 ms.

## Not checked
A save or a load during the work, as far as the log shows. Mods whose Python changes the
game outside the network messages. Huge map. `WORK_AHEAD_QUEUE_DECISIONS=1` (decisions kept
and done in lanes) has not run since the first played game.

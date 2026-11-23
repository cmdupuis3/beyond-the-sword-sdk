# Score asked from Python once

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | medium |
| Commits | `44263e0` |

## What it does
A player's score is worked out by Python (`calculateScore` in `CvGameUtils.py`).
`CvGame::updateScore()` ranks the players by score, and stock asked for the score again on
every pass of the ranking for each player not ranked yet: 171 calls for 18 players. It now
asks once per player before the ranking starts and ranks the answers, with the same rule
for equal scores (the player with the higher number first).

## What differs from stock
Nothing in the game state with the stock Python, or with any `calculateScore` that does not
read the ranks, scores and score history while they are being set. A mod whose
`calculateScore` reads those during the update would now get the values from before the
update for every player. Python is called fewer times.

## Measured
Seeded standard game (150 turns), profile build, same state hash on every turn in profile
and release builds: `CvPlayer::calculateScore()` falls from 71,991 calls and 0.22 s to
6,678 calls and 0.06 s, about 2% of the time in the DLL.

## Not checked
The huge game after the change. Before it, that game had 160,056 calls and 1.08 s.

# Enemy-team word and team index loops

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `dd7b6e0` |

## What it does
With scans down to 2% of `AI_getPlotDanger()` calls (see
[Danger scans ruled out by a team index](danger-scans.md)), what was left was the fixed
cost of asking. Two parts of it are gone:

- Every call asked `atWar()` about each team in turn to build the word of enemy teams.
  `CvTeam::getEnemyTeams()` keeps that word and works it out again only after some team's
  war status has changed. A counter, advanced wherever an `m_abAtWar` array is written,
  says when.
- The plot loops wrapped both coordinates of every plot in the window.
  `CvMap::getWindowCoords()` works out the window's columns and rows once per question.

Questions about a range beyond 12 plots are not answered from the index; the caller scans.
No stock caller asks for more than 4.

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile build, same state hash on every turn and the same calls
ruled out: the function's own time was 7.0 s, against 7.8 s before, in a run where the
pathfinder took 30% longer than in the run that measured 7.8 s. No time was taken on the
standard game.

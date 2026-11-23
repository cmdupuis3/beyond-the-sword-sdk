# Other explorers' targets listed once per decision

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | flat |
| Scales with game era | not measured |
| Value | low |
| Commits | `60af915` |

## What it does
`AI_explore()` and `AI_exploreRange()` leave out a plot within 3 of a plot another group of
the player is exploring towards. They asked `CvPlayerAI::AI_plotTargetMissionAIs()` about
every plot worth anything. That function looks through all the player's groups whenever
any group is heading anywhere near the plot, and near an explorer one nearly always is: its
own group still has last turn's target. The groups do not change while an explorer decides,
so `ExploreTargets` (in `CvUnitAI.cpp`) looks through them once, when the first plot asks,
and keeps the other explorers' target plots. Each plot is then tested against that short
list.

## What differs from stock
Nothing in the game state: each plot gets the same answer.

## Measured
Seeded games, profile builds, the same game on every turn. Calls of
`AI_plotTargetMissionAIs()` from all of the AI: 282,621 to 85,643 on the standard game
(150 turns) and 1,554,998 to 524,392 on the huge one (250 turns). Its time: 80 ms to 15 ms
of about 12 s, and about 400 ms to about 125 ms of about 65 s (the huge runs were on a
machine busy with other work). With a temporary check, the list gave the function's answer
on all 196,978 questions of the standard game and all 1,030,606 of the huge one.

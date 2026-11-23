# Groups heading for a plot counted per plot

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `aa8a273` |

## What it does
`CvPlayerAI::AI_plotTargetMissionAIs()` looked through every group of the player to answer
"how many units are heading for this plot". Each player now keeps a count, per plot, of the
groups whose mission plot it is. `AI_setMissionAI()` and `CvSelectionGroup::kill()` keep
the count, and it is taken afresh from the groups after a reset or a load. When no group
is heading anywhere within range, which is nine questions in ten, the groups are not
looked through.

The same commit added the profile counts of what workers do (plots moved, turns of
building, builds finished), which are measurement only.

## What differs from stock
Nothing in the game state.

## Measured
Not measured on its own. The seeded standard game (150 turns) and huge game (250 turns)
have the same state hash on every turn. With `TARGET_VERIFY` (profile builds), which looks
through the groups all the same, no group was within range in any of 263,952 skipped
questions.

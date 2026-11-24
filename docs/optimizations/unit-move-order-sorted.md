# AI groups put in move order by one sort

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | low |
| Commits | `b052b2d` |

## What it does
`CvPlayerAI::AI_unitUpdate()` moves an AI player's groups in order of
`AI_movementPriority()`, 0 to 16, and in the order of the group cycle among those of the
same priority. It made the order by going through the cycle once for each priority and
asking every group not yet placed for its priority again on each pass. The priority only
reads the group, so each group is now asked once and the list is sorted, stably, on it.

## What differs from stock
Nothing in the game state: the groups move in the same order.

## Measured
Huge game (250 turns), profile build, same state hash on every turn:
`AI_unitUpdate()`'s own time 212 ms against 108 ms (4,188 calls).

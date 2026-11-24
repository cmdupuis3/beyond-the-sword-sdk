# Idle units at peace keep their decision

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `UNIT_IDLE_STOCK=1` |
| Scales with map size | flat |
| Scales with game era | not measured |
| Value | low |
| Commits | `cada267` |

## What it does
City defenders, reserves and counter units that stay where they are decide it again every
turn. A group whose head unit stayed idle (same plot, nothing queued, nothing spent) on
two turns in a row now keeps that decision for the next three turns at normal speed, more
on a slower game (`CvUnitAI::AI_isIdleKept()`): fortified or asleep it stays so, awake it
skips its turn. It decides again at once if its team is no longer at peace with everyone,
an enemy that can attack comes within four plots, its damage or group changes, its owner
gains or loses a city, or it is on another plot. Not for a human's automated units, the
barbarians or the minor civilizations. Nothing is saved.

## What differs from stock
The units decide less often, and what a decision would have drawn from the game's random
numbers is not drawn, so the games differ.

## Measured
Seeded games, profile builds; the games differ. Huge (250 turns): 24,053 group turns
skipped; the idle decisions took 0.63 s against 1.08 s of 57 s; 133 cities at the end
against 134. Standard (150 turns): 4,101 skipped, 52 ms against 126 ms of 9.5 s.

## Not checked
A game with a human, and multiplayer.

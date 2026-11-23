# AI turns not played out for the human

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `SHOW_AI_TURNS=1` |
| Scales with map size | not measured |
| Scales with game era | not measured |
| Value | low |
| Commits | `a2ba968` |

## What it does
Stock walks an AI unit from plot to plot where a human can see it and has "show enemy
moves" or "show friendly moves" on, and plays out an AI attack on a human's unit unless
"quick combat (defense)" is on. Each of those keeps the turn waiting for the screen, a
quarter of a second at a time (see [AI turns do not wait for the screen](turn-passes-without-waiting.md)).
In a single-player game an AI unit is now put on the plot it moved to and an AI attack is
settled at once, as stock does for what the human cannot see
(`CvGame::isAITurnsShown()`; `CvSelectionGroup::showMoves()`, `CvUnit::setXY()`,
`CvUnit::isCombatVisible()`). The players after the human then take their turns in one
call of `CvGame::update()`, in the stock order, and the human sees the outcome of all of
them together when their own turn starts. A human's own units, automated ones too, are
shown as before, and so is an AI's diplomacy.

## What differs from stock
Nothing in the game state: the turns are the stock ones. A human no longer watches the
AI's moves and attacks; the event log says what happened, as it does with the stock
options off. The number of passes (turn slices) a round takes is the one stock has with
those options off. A nuke's mission timer is left as it is. Multiplayer games, and games
with `STOCK_TURN_SLICES`, show the AI's turns as stock does.

## Measured
Nothing: the seeded games have no human, and stock shows no moves in them either. The
stock DLL's state hashes come out on every turn of the standard game as before.

## Not checked
A game with a human in it. This is where the change acts, and it has not been run: watch
that AI units end up drawn where they are, and that a human's unit that was attacked shows
its damage.

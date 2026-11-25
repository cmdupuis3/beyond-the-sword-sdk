# A human's turn ends without waiting

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `END_TURN_STOCK_WAIT=1` (the key and the popup step), `END_TURN_SHOW_MOVES=1` (the rest of the turn played out) |
| Scales with map size | not measured |
| Scales with game era | not measured |
| Value | low |
| Commits | `44721bc`, `4277299` |

## What it does
The exe calls into the game about four times a second, and stock ends a human's turn one
step a call: queued popups, the automated units' moves, the end-turn message; the end-turn
key does nothing before the message, about a second each turn. Now, in a single-player game:

- The end-turn key ends the turn as soon as no unit waits for orders and no popup or
  diplomacy is waiting (`isTurnReadyToEnd()` in `CvGameInterface.cpp`), as stock's
  force-end-turn key does. With no popup queued the popup step goes on in the same call.
- Once the turn is ended, what is left of the human's units' orders is not played out: the
  passes run one after another in the same call (`CvGame::isHumanTurnEndQuick()`,
  `CvGame::nextSliceWait()`), a unit is put where it moved to and a fight it starts is
  settled at once. Until then the human's units are shown as stock shows them.

## What differs from stock
Nothing in the game state: the passes are the stock ones in the stock order and the mission
timers still count them. A key that did nothing ends the turn; the automated units' moves
after it are not watched. Multiplayer games keep the stock steps and pace.

## Measured
Nothing in the seeded games, which have no human: a `--tier 1` DLL plays the stock game.
Rated by AI turn time, which it does not touch; what it removes is waiting the player sees.
One played game with it (to turn 110) ran without a fault; no times were taken.

## Not checked
Times before and after in a played game. Hot seat and play by email (they keep stock's pace).
The up to a quarter of a second between the key and the exe's next call is still there.

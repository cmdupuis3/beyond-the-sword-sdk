# AI turns do not wait for the screen

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `STOCK_TURN_SLICES=1` |
| Scales with map size | flat |
| Scales with game era | const |
| Value | very high |
| Commits | `58970c3`, `2d6c399` |

## What it does
The exe calls `CvGame::update()` about four times a second and draws frames in between.
Each call made one pass over the players, so stock waited at least a fifth of a second for
every pass and once more for every game turn. In a single-player game `CvGame::update()`
now makes its next pass in the same call. The passes are the stock ones in the stock order
(`CvGame::updateSlice()` is the body of the stock function).

`CvGame::nextSliceWait()` still waits for the exe when a human has not ended their turn, a
unit of the player whose turn it is has a mission or combat timer running (a move or fight
shown to a human), a diplomacy screen is up or asked for, the game is over or paused, the
world builder or an advanced start is open, or the game is multiplayer (network, hot seat,
play by email).

## What differs from stock
Nothing in the game state. Nothing is drawn while the other players' turns run unless a
move or fight is being shown, so the window does not repaint in a long turn;
`TURN_LOOP_FRAME_MS` draws a frame every so many milliseconds, at about a fifth of a second
each. A popup raised during the other players' turns appears when they are over. With no
human player the exe draws at the end of a game turn once a second has passed.

## Measured
Seeded games, all players AI, time for the turns, same state hash on every turn.

| | standard, 150 turns | huge, 250 turns |
| --- | ---: | ---: |
| stock pacing | 81.8 s | 590.9 s |
| passes run at once | 8.7 s | 106.2 s |

Standard from release builds, huge from profile builds, its stock-pacing run on another
day. The exe keeps about 0.2 s between calls on the standard map and 0.5 s on the huge one.
Benchmark times from before this change include the waits and do not compare with later ones.

## Not checked
A game played by a human: the waits for a human's turn, for moves being shown and for
diplomacy have not been run.

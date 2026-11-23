# AI choices draw from streams of their own

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `AI_RANDOM_STOCK=1` |
| Scales with map size | not measured |
| Scales with game era | not measured |
| Value | low |
| Commits | `733690b` |

## What it does
The stock AI draws all its random numbers from the game's one stream, so what a city
chooses to build depends on every number drawn before it: the fights of the turn, what
the cities before it chose, the order things were done in. That ties each choice to its
place in the turn, and nothing that draws can be worked out ahead, in another order or on
another thread.

A city's production choice (`CvCityAI::AI_chooseProduction()`, 25 draws and 9 more in what
it calls) and a player's technology choice (`CvPlayerAI::AI_bestTech()`, 11 draws) now
draw from a stream made for that one decision (`CvAIRandScope` in `CvGame.h`). Its seed is
made from the game's seed, the game turn, the city or player, and a little of its state
(for a city its buildings, overflow production and population; for a technology choice
what was asked). The same decision in the same state on the same turn gets the same
numbers whenever it is made, and the game's own stream is left as it was. Nothing is kept
between calls, so nothing goes into a saved game.

## What differs from stock
The numbers these choices draw are other numbers, and the game's stream no longer moves
when they are made, so everything that draws from it afterwards draws differently too. The
game differs from the first production choice on. The odds of each choice are the stock
ones. With "new random seed on reload" a loaded game gets new streams as well.

## Measured
No speed effect was looked for: this saves no time itself. Seeded standard game (150
turns), profile build: 1,193 production choices and 124 technology choices drew 7,797
numbers, all from their own streams. Two runs give the same hashes on every turn. With the
off switch the game is the one from before on all 151 turns.

## Not checked
The huge game, a game with a human in it, and multiplayer (the streams are made from
synchronised state only, but no network game has been run).

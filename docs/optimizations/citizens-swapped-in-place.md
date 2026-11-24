# Citizens swapped in place

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `CITIZENS_STOCK=1` |
| Scales with map size | flat |
| Scales with game era | late |
| Value | medium |
| Commits | `8733e0d` |

## What it does
Every turn stock has each AI city take every citizen off its plot and add them back one at
a time, the best plot or specialist first (`AI_addBestCitizen()`), and then swap the worst
worked plot for the best until a swap brings back the plot it took away
(`AI_juggleCitizens()`). Adding them back is most of the cost. An AI city now keeps the
plots it works and goes straight to the swaps, which from an assignment already swapped
into place stop after one (`CvCityAI::AI_isAssignedFromScratch()`). The specialists are
still reset and added back, so they are weighed against the plots again. Every four turns
at normal speed (by the game speed's train percent) a city starts from scratch as stock
does. A human's cities are left as stock has them; nothing is saved.

## What differs from stock
Swapping from last turn's plots can settle on another assignment than adding every
citizen again would, so the cities can work other plots, and the games differ.

## Measured
Seeded games, profile builds; the games differ. Huge (250 turns): citizen assignment 76
to 42 µs a call, 1.40 s to 0.79 s of 42 s; `AI_addBestCitizen()` called 144,436 times
against 75,199; 130 cities at the end against 129. Standard (150 turns): 48 to 32 µs a
call; 53 cities at turn 150 against 47.

## Not checked
A game with a human, and multiplayer.

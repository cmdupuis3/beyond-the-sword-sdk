# Workers act as a team

| | |
| --- | --- |
| Tier | 2 |
| Off switch | `WORKER_STOCK_CHOICE=1` |
| Scales with map size | not measured |
| Scales with game era | not measured |
| Value | low |
| Commits | `a1cca7e` |

## What it does
Each AI worker still decides for itself in the stock order. Two rules now look at what the
player's other workers are doing. They do not apply to a human's automated workers.

- One build to a plot (`CvUnitAI::AI_agreedBuild()`): a worker about to start an
  improvement where another worker of the player is on, or heading for, a different one
  takes that one over. If its own improvement makes the plot's bonus available and the
  other's does not, the other worker's orders are cancelled instead.
- Far jobs to the nearer worker (`AI_isIdleWorkerCloser()`): a worker leaves a job more
  than a turn away if another worker of the player in the same area has yet to decide this
  turn and is less than half as far from it. This applies where a worker looks over the
  whole territory (bonuses, the next city to improve, routes, irrigation, forts).

## What differs from stock
Workers can choose other builds and other plots, and the game differs from the turn a rule
first acts. With the off switch the seeded standard game is the one from before on all 151
turns.

## Measured
No speed effect was looked for. Over five seeded 150-turn standard games, builds begun on
a plot with another build unfinished fell from 168 to 111, and fell in each game. Plots
moved per build finished (2.91 against 2.98) and builds finished (2,715 against 2,676) are
within the difference between games. The first rule took over a build twice in those five
games and eight times in the 250-turn huge game.

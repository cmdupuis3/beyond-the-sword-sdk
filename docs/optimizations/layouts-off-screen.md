# Layouts off the screen made a few at a time

| | |
| --- | --- |
| Tier | 1 |
| Off switch | `LAYOUTS_IN_SIGHT_AT_ONCE=1` |
| Scales with map size | grows |
| Scales with game era | late |
| Value | medium |
| Commits | `2b0c757` |

## What it does
[Layouts out of sight](layouts-out-of-sight.md) holds back the layouts of cities and plots
in the fog. One the active team can see was still made as soon as the exe asked, wherever
it was, and a player always sees their own land, most of it off the screen. Now it is made
at once only if it is also near the camera (`CvMap::isLayoutAtOnce()`): within 14 plots
either side of the plot the camera looks at and 9 above or below it
(`CvMap::isNearCamera()`; the defines `LAYOUT_CAMERA_RANGE_X` and `LAYOUT_CAMERA_RANGE_Y`
change that). The others take their turn with the ones in the fog, 2 cities and 16 plots
for each call of `CvGame::update()`. The exe asks again before every frame, so a layout
that is waiting is made in the frame the camera reaches it. In the globe view everything
in sight is made at once.

## What differs from stock
Nothing in the game state. A city or improvement in sight but off the screen can keep its
former look for some seconds after a round of turns. The DLL cannot ask how far the camera
is zoomed out, so with the camera far out the same holds at the edges of the screen. City
billboards are not held back.

## Measured
Seeded huge game, 250 turns, all players AI, with the off switch and without, taken in
turn; same state hashes, and the stock DLL's on every turn with the tier 2 switches off.

| | off switch set | default |
| --- | ---: | ---: |
| profile build, exe time for each return from `update()` | 406, 422 ms | 388, 369 ms |
| the same after turn 200 | 601, 623 ms | 551, 525 ms |
| city / plot layouts in sight the exe made | 454 / 2,115 | 282 / 1,423 |
| release build, whole game | 46.2, 49.6, 48.1 s | 43.8, 49.7, 47.4 s |

About 35 ms a return, a little over a second of the game; the whole-game times cannot show
that much (another build was running during most of them). Fewer layouts are made because
a city marked again while it waits is made once. Standard game, release: 5.8 and 5.9 s
against 6.4 and 5.8 s, no difference.

## Not checked
A game with a human in it, and the map as drawn: the first player of the seeded games is an
AI and its camera never moves. Watch for a city in view that is drawn late after the camera
jumps (a click on the minimap), and for the edges of the screen with the camera zoomed out.

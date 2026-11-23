# Optional Python callbacks

| | |
| --- | --- |
| Tier | 1 |
| Off switch | off unless a mod sets the defines |
| Scales with map size | flat |
| Scales with game era | late |
| Value | medium |
| Commits | `1eed936`, `ab8f93d` |

## What it does
Nothing unless a mod asks for it. Eleven Python calls made during the turns do nothing in
the stock game. A mod can leave each one out with a global define set to 1, in its
`PythonCallbackDefines.xml` for example.

- Seven questions every city asks every turn, which stock Python answers no:
  `SKIP_DO_GROWTH_CALLBACK`, `SKIP_DO_CULTURE_CALLBACK`, `SKIP_DO_PLOT_CULTURE_CALLBACK`,
  `SKIP_DO_PRODUCTION_CALLBACK`, `SKIP_DO_RELIGION_CALLBACK`,
  `SKIP_DO_GREAT_PEOPLE_CALLBACK`, `SKIP_DO_MELTDOWN_CALLBACK`.
- `SKIP_GET_EXTRA_COST_CALLBACK`: `getExtraCost`, asked whenever a player's costs are
  worked out; stock answers 0.
- `SKIP_GET_EXPERIENCE_NEEDED_CALLBACK`: `getExperienceNeeded`, asked for every unit every
  turn. The DLL then works out the stock formula itself, so it is only for a mod that has
  left the formula alone. Set to 2 in a profile build, it asks Python too and counts
  answers that differ.
- `SKIP_AI_UNIT_UPDATE_CALLBACK`: `AI_unitUpdate`, asked for every unit the AI moves.
- `SKIP_ON_UNIT_MOVE_CALLBACK`: the `unitMove` event, for every plot a unit moves.

## What differs from stock
Nothing as built: a define that is missing or 0 leaves its call on. Setting one for a call
the mod's Python does use switches that Python off, so check the mod's Python first. The
`cityDoTurn` event is not affected. Sword of Islam can set all but
`SKIP_AI_UNIT_UPDATE_CALLBACK`: its `AI_unitUpdate` moves the inquisitors.

## Measured
Seeded games, same state hash on every turn with and without the defines.

- The seven city callbacks, profile builds: the part of `CvCity::doTurn()` outside the city
  AI and the production choice falls from 2.36 s to 1.74 s on the huge game (250 turns) and
  from 0.45 s to 0.36 s on the standard one (150 turns), about 1% of the time in the DLL.
- The four others, standard game: 99,000 of the 167,000 calls into Python and 0.59 s of
  the 8.4 s in the DLL (profile build). Release build, three runs each way, the sum of each
  turn's fastest run: 4.98 s without and 4.30 s with, 14% less; whole runs differed by more
  than that, so the figure is indicative. The C++ experience formula agreed with Python all
  13,426 times.

## Not checked
The four other calls on the huge game.

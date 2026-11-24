# Optimizations

| Optimization | Tier | Scales with map size | Scales with game era | Value |
| --- | --- | --- | --- | --- |
| [AI turns do not wait for the screen](turn-passes-without-waiting.md) | 1 | flat | const | very high |
| [Danger scans ruled out by a team index](danger-scans.md) | 1 | not measured | late | high |
| [Link-time optimisation](link-time-optimisation.md) | 1 | not measured | const | high |
| [Profile-guided optimisation](profile-guided-optimisation.md) | 1 | grows | const | high |
| [Unit upgrades scored from the upgrade tree](unit-upgrades.md) | 1 | not measured | late | high |
| [Yield and route symbols made once per draw](symbols-once-per-draw.md) | 1 | flat | late | high |
| [K-Mod pathfinder for AI groups](kmod-pathfinder.md) | 2 | not measured | late | high |
| [AI attitudes kept between changes](attitude-cache.md) | 1 | not measured | const | medium |
| [Bonus value skips buildings with no effect](bonus-value.md) | 1 | not measured | const | medium |
| [City values kept during citizen assignment](citizen-assignment-values.md) | 1 | not measured | late | medium |
| [City-site scores kept between turns](found-value-cache.md) | 1 | shrinks | early | medium |
| [Define lookups kept by name](define-lookup-cache.md) | 1 | not measured | const | medium |
| [Feature graphics left until seen](feature-graphics-when-seen.md) | 1 | shrinks | early | medium |
| [Layouts off the screen made a few at a time](layouts-off-screen.md) | 1 | grows | late | medium |
| [Layouts out of sight made a few at a time](layouts-out-of-sight.md) | 1 | grows | late | medium |
| [Optional Python callbacks](python-callbacks-optional.md) | 1 | flat | late | medium |
| [Path request lookups made once](path-request-lookups.md) | 1 | flat | late | medium |
| [Plot group check by an in-DLL flood](plot-group-flood.md) | 1 | not measured | late | medium |
| [Route-to-city check from one flood](route-to-city-flood.md) | 1 | not measured | late | medium |
| [Score asked from Python once](score-python-once.md) | 1 | not measured | const | medium |
| [Trade route inputs looked up once](trade-route-inputs.md) | 1 | not measured | late | medium |
| [Tradeable asked before trade denial](trade-denial-order.md) | 1 | not measured | const | medium |
| [Unit AI searches limited to the unit's area](area-limited-unit-searches.md) | 1 | grows | late | medium |
| [XML lists and technology masks](xml-lists-and-tech-masks.md) | 1 | not measured | late | medium |
| [Yield-value inputs kept for a pass over the city](yield-value-inputs.md) | 1 | not measured | late | medium |
| [Cities keep their chosen builds](city-keeps-builds.md) | 2 | flat | late | medium |
| [Moving groups keep their path](moving-groups-keep-path.md) | 2 | grows | late | medium |
| [Path searches the AI can do without](path-search-trimming.md) | 2 | grows | late | medium |
| [Path searches head for the destination](path-heuristic-weight.md) | 2 | grows | not measured | medium |
| [Targets valued before their path is searched for](path-value-bounds.md) | 2 | grows | not measured | medium |
| [City-site plan kept by players without a settler](found-plan-kept.md) | 2 | flat | not measured | medium |
| [Citizens swapped in place](citizens-swapped-in-place.md) | 2 | flat | late | medium |
| [City builds chosen on the human's turn](builds-on-human-turn.md) | 3 | shrinks | late | medium |
| [AI classes and the free-list array marked final](final-ai-classes.md) | 1 | not measured | const | low |
| [AI turns not played out for the human](ai-turns-not-played-out.md) | 1 | not measured | not measured | low |
| [Bonus and unit counts from the plots that can count](bonus-and-unit-counts.md) | 1 | grows | late | low |
| [Building-value inputs once per production choice](building-value-inputs.md) | 1 | not measured | late | low |
| [Collateral damage ties by plot order](collateral-ties.md) | 1 | not measured | not measured | low |
| [Enemy-team word and team index loops](team-index.md) | 1 | not measured | late | low |
| [Groups heading for a plot counted per plot](mission-target-counts.md) | 1 | not measured | late | low |
| [Other explorers' targets listed once per decision](explorer-targets-once.md) | 1 | flat | not measured | low |
| [Plot grid tables known at compile time](plot-grid-tables.md) | 1 | not measured | const | low |
| [Plot yields in one call](plot-yields-in-one-call.md) | 1 | not measured | late | low |
| [AI choices draw from streams of their own](ai-random-streams.md) | 2 | not measured | not measured | low |
| [Idle units at peace keep their decision](idle-units-kept.md) | 2 | flat | not measured | low |
| [Explorers look for paths from the best plot down](explore-best-first.md) | 2 | flat | not measured | low |
| [Workers act as a team](worker-teams.md) | 2 | not measured | not measured | low |

- Tier 1: reproduces stock BtS, the same game state on every turn. See [tiers](../tiers.md).
- Tier 2: deterministic, but play can differ from stock. Tier 3: not deterministic.
- Scales with map size: whether the share of turn time saved `grows`, stays `flat` or
  `shrinks` on the huge seeded game against the standard one; `not measured` if only one was timed.
- Scales with game era: whether the saving is mostly `early` in a game, grows `late`, or is
  about `const` throughout; judged from what the work scales with where no turn ranges were measured.
- Value: share of AI turn time saved in the seeded games: `very high` about 20% or more,
  `high` 5% to 20%, `medium` 1% to 5%, `low` under 1% or no speed effect. An opt-in change is rated switched on.

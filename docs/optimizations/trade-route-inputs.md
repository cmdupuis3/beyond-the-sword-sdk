# Trade route inputs looked up once

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | medium |
| Commits | `c454187` |

## What it does
`CvCity::updateTradeRoutes()` works out the profit of a route to every city it could
trade with. Each `calculateTradeProfit()` looked up six to ten global defines by name and
added up the same parts of this city's modifier. `TradeProfitContext` now holds what does
not depend on the other city, and the peace modifier once per team;
`calculateTradeProfit(pCity, kContext)` does the stock sums from it.

`CvPlayer::updateTradeRoutes()` ordered the cities by trade modifier with an insertion
that worked out a listed city's modifier again for every comparison. Each modifier is now
worked out once and the list is made with `std::stable_sort`, which gives the same order.
`clearTradeRoutes()` looks up `MAX_TRADE_ROUTES` once.

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile builds, two alternating runs against the code before, same
state hash on every turn: `CvPlayer::updateTradeRoutes()` 1.31 s and 1.43 s to 0.22 s,
`CvCity::updateTradeRoutes()` 1.15 s and 1.25 s to 0.17 s. With `TRADE_VERIFY` (profile
builds), all 46,601 profits in the standard game matched the stock sums. The standard
game was not timed.

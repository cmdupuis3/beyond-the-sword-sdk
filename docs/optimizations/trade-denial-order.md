# Tradeable asked before trade denial

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | const |
| Value | medium |
| Commits | `06488aa` |

## What it does
`CvPlayer::canTradeItem()` asked `getTradeDenial()` first, which for a technology or a
vassal agreement is a long evaluation, and only then whether the item can be traded at
all. `AI_doDiplo()` asks about every technology for every player it talks to, and nearly
all of them cannot be traded. The cheap question is now asked first. Both questions read
the game without changing it, so the order does not change the answer. The review of deals
in force likewise looks at whose deal it is before the cancel rules.

## What differs from stock
Nothing in the game state.

## Measured
Huge game (250 turns), profile builds, same state hash on every turn of both seeded
games: `AI_techTrade` is called 10,350 times instead of 246,621, and `AI_doDiplo` takes
1.23 s instead of 2.28 s. The standard game was not timed.

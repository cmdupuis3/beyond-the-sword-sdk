# Unit upgrades scored from the upgrade tree

| | |
| --- | --- |
| Tier | 1 |
| Off switch | none |
| Scales with map size | not measured |
| Scales with game era | late |
| Value | high |
| Commits | `083f535`, `6d4f3da`, `eb1717f` |

## What it does
`CvUnitAI::AI_upgrade()` ran for every AI unit every turn and called `AI_unitValue()` on
every unit type in the game, twice, before asking `canUpgrade()`, which refused nearly all
of them. It now returns at once when the unit is not ready to upgrade, and otherwise
scores only the types on the unit's own upgrade tree, in the original order.
`CvUnit::getUpgradeUnitTypes()` keeps that list for each unit type and civilization; it
comes from the XML alone. `CvUnit::getUpgradeCity()` and
`CvPlayerAI::AI_goldToUpgradeAllUnits()` value only the types on the list.
`CvUnit::canUpgrade()` looks for a city that can train the unit before it asks the price.

## What differs from stock
Nothing in the game state: the same random numbers are drawn and the same upgrades chosen.
Python's `getUpgradePriceOverride` is asked less often: not for unit types off the unit's
upgrade tree, and only for an upgrade some city can make. A mod whose override does more
than return a price would see fewer calls.

## Measured
Seeded standard game (150 turns), profile builds, same state hash on every turn.

- `083f535`: `AI_upgrade()` from 4,582 ms to 915 ms over 17,040 calls. Allowing for a run
  about 25% faster throughout, roughly 3.7 times faster, about 8% of turn processing.
- `6d4f3da`: `AI_upgrade()` from 525 ms to 189 ms inclusive, of about 11.5 s of turn
  processing; `upgradePrice()` is called 502 times.
- `eb1717f`: `AI_goldToUpgradeAllUnits()` from 145 ms to 37 ms.

With `UPGRADE_VERIFY` (profile builds) no kept list differed from one built afresh. The
huge game was not timed.

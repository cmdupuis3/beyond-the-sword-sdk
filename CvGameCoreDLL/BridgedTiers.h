#pragma once

#ifndef BRIDGED_TIERS_H
#define BRIDGED_TIERS_H

//
// How far a build of this DLL may depart from the stock Beyond the Sword 3.19 game core.
//
// Every change made here belongs to one of three tiers:
//
//   Tier 1  Reproduces BtS. From the same start and the same input, the game state is the
//           stock DLL's on every turn. What the DLL asks the exe to draw, and when, may
//           differ, and so may how often Python is asked a question whose answer does not
//           change. Where the stock result depended on something outside the game (which
//           of two units sat lower in memory), any result stock could have given counts.
//
//   Tier 2  Deterministic, not BtS. The game can differ from the stock game, but the same
//           start and the same input always give the same game: on every run, on every
//           computer, whatever else the computer is doing.
//
//   Tier 3  Not deterministic. The game can differ from one run to the next: it depends
//           on the clock, on threads, on where things are in memory, or on anything else
//           that is not part of the game.
//
// The build says which tiers a DLL has (tools/build_dll.py --tier N), with two macros:
//
//   REPRODUCE_BTS          tier 1 only: the reference build
//   neither                tiers 1 and 2: the default
//   NON_DETERMINISM_OK     tiers 1, 2 and 3
//
// How to mark a change:
//
//   tier 1   no guard
//
//   tier 2   #ifndef REPRODUCE_BTS
//                ... the change ...
//            #else
//                ... what stock does ...
//            #endif
//
//   tier 3   #ifdef NON_DETERMINISM_OK
//                ... the change ...
//            #else
//                ... something deterministic ...
//            #endif
//
// A change with a switch function (isKmodPathGroup(), workerTeamRules() and the like) is
// guarded there, so that the function is constant in a build without the tier and the
// compiler drops the code behind it.
//
// docs/behaviour-changes.md lists every change of tiers 2 and 3, and says how each tier is
// checked against the seeded games.
//
#if defined(REPRODUCE_BTS) && defined(NON_DETERMINISM_OK)
#error REPRODUCE_BTS (tier 1 only) and NON_DETERMINISM_OK (tiers 1 to 3) cannot both be defined
#endif

#if defined(REPRODUCE_BTS)
#define BRIDGED_TIER	1
#elif defined(NON_DETERMINISM_OK)
#define BRIDGED_TIER	3
#else
#define BRIDGED_TIER	2
#endif

#endif

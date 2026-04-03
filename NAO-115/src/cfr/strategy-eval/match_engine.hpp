#pragma once

/*
Uses the "Duplicate Method" standard established by the Annual Computer  Poker Competition (ACPC)
by Bard, N., Hawkin, J., Rubin, J., & Zinkevich, M. (2013). The Annual Computer Poker Competition. AI Magazine, 34(2), 112.
 -> source: https://ojs.aaai.org/aimagazine/index.php/aimagazine/article/view/2474/2362

 Each "duplicate pair" plays the exact same cards twice with players swapped:
    Hand 1: Strategy A as P0, Strategy B as P1 → EV_1
    Hand 2: Strategy B as P0, Strategy A as P1 → EV_2
    Duplicate score = average EV over both seat positions (A as P0 and P1)
    -> cancels card luck, both strategies play the exact same deals

 Action translation:
    - when a strategy faces an off-tree bet, PHM (pseudo-harmonic mapping)
    - translates it to a distribution over the two bracketing abstract actions

Result:
    - returns mean EV of Strategy A vs Strategy B in mbb/hand, plus standard error for confidence intervals
    - Positive mean_ev means A outperforms B
 */

#include "strategy_io.hpp"
#include "bet-abstraction/bet_utils.hpp"
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/mapping_engine.hpp"
#include <cstdint>
#include <string>

namespace MatchEngine {

struct StrategyProfile {
    StrategyIO::InfosetMap map;

    // player's bet abstraction (pot fractions), used for action translation
    float flop[3];
    float turn[3];
    float river[3];
    int numSizes = 3;

    // integer ratios for g_betConfig — set before getLegalActions
    int32_t flop_n[3],  flop_d[3];
    int32_t turn_n[3],  turn_d[3];
    int32_t river_n[3], river_d[3];
};

struct MatchResult {
    float mean_ev_mbb;       // mean EV of strategy A vs B in mbb/hand
    float std_error_mbb;     // standard error of the mean
    float confidence_95_mbb; // 95% confidence interval half-width (mean ± this)
    int   num_pairs;         // number of duplicate pairs played
    int   total_hands;       // num_pairs * 2
};

/*
Run a head-to-head match between two strategies using duplicate poker.
 - profileA, profileB: two competing strategies with their bet configs
 - numPairs: number of duplicate pairs (2 hands per pair)
 - seed: RNG seed for reproducibility
 - returns: MatchResult (positive mean_ev_mbb ->> A beats B)
 */

MatchResult runMatch(
    const StrategyProfile& profileA,
    const StrategyProfile& profileB,
    Bucketer::IsomorphismEngine& isoEngine,
    int numPairs,
    int bigBlind,
    uint64_t seed
);

/*
Build a StrategyProfile from a loaded map and bet size fractions
(converts float fractions to integer ratios for g_betConfig)
 */
StrategyProfile buildProfile(
    StrategyIO::InfosetMap map,
    const float flopSizes[3],
    const float turnSizes[3],
    const float riverSizes[3]
);

}

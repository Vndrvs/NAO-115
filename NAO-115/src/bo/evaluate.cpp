/*
Run a single Bayesian Optimization (BO) evaluation of Nao's MCCFR postflop strategy.
 -> use: ./evaluate config.json
 reads bet size config from JSON, runs parallel MCCFR, computes exploitability & writes result JSON to file
 
 -> input JSON sample:
{
    "flop":  [0.5, 1.0, 2.0],   <- fractions of pot for flop bets
    "turn":  [0.75, 1.5, 2.5],  <- fractions of pot for turn bets
    "river": [0.33, 1.0, 3.0],  <- fractions of pot for river bets
    "iterations": 500000,       <- MCCFR iterations
    "threads": 10,              <- parallel threads
    "samples": 1000,            <- number of hands to sample for exploitability
    "seed": 42                  <- RNG seed
}
 
 -> output JSON sample:
 {
    "exploitability_mbb": 1234.56,      <- exploitability in mbb/hand
    "br_p0_mbb": 678.90,                <- best response value for hero
    "br_p1_mbb": 555.66,                <- best response value for villain
    "infosets": 974581,                 <- total infosets reached
    "training_sec": 45.2,               <- MCCFR training time
    "eval_sec": 14.8,                   <- exploitability evaluation time
    "config": { ... same as input ... } <- echoes the bet config used
 }
 
 NOTE: these config arrays define a 3x3 bet sizing matrix used in Nao's research run (one triplet for each postflop street)
 */

#include "cfr/cfr-core/mccfr_multithread.hpp"
#include "cfr/utils/zobrist.hpp"
#include "cfr/exploitability/exploitability.hpp"
#include "eval/evaluator.hpp"
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/mapping_engine.hpp"
#include "bet-abstraction/bet_utils.hpp"
#include "../include/nlohmann/json.hpp"
#include "evaluate.hpp"

#include <cstdio>
#include <fstream>
#include <chrono>
#include <filesystem>

using json = nlohmann::json;
using timerClock = std::chrono::high_resolution_clock;
using Seconds = std::chrono::duration<double>;

static double elapsed_sec(timerClock::time_point start) {
    return Seconds(timerClock::now() - start).count();
}

static MCCFRState makePostflopRoot() {
    MCCFRState state{};
    state.bigBlind            = 100;
    state.street              = 1;
    state.raiseCount          = 0;
    state.currentPlayer       = 0;
    state.isTerminal          = false;
    state.foldedPlayer        = -1;
    state.potBase             = 2000;
    state.heroStreetBet       = 0;
    state.villainStreetBet    = 0;
    state.heroStack           = 9000;
    state.villainStack        = 9000;
    state.previousRaiseTotal  = 0;
    state.betBeforeRaise      = 0;
    state.historyHash         = 0;
    state.bucketId            = 0;
    state.streetHasCheck      = false;
    state.player0Contribution = 1000;
    state.player1Contribution = 1000;
    return state;
}

// convert pot fraction float to numerator/denominator integer pair
// because Nao’s bet sequence module uses integer arithmetic for bet sizes e.g. 0.5 → {1, 2}
static void fractionToRatio(float fraction, int32_t& numerator, int32_t& denominator) {
    if (fraction <= 0.0f) {
        numerator = 0;
        denominator = 1;
        return;
    }
    // function uses denominator 4 to handle quarter-pot increments
    constexpr int32_t baseDenominator = 4;
    denominator = baseDenominator;
    // convert float fraction into integer numerator
    numerator = static_cast<int32_t>(std::round(fraction * denominator));
    // simplify by finding GCD
    int32_t a = numerator;
    int32_t b = denominator;
    while (b) {
        int32_t temp = b;
        b = a % b;
        a = temp;
    }
    numerator /= a;
    denominator /= a;
}

// apply bet fractions from JSON to Nao's global bet config
// converts float fractions of the pot into integer numerator/denominator pairs
static void applyBetConfig(const json& config) {
    auto& betConfig = BetAbstraction::g_betConfig;

    auto applyStreet = [](const json& streetArray, int32_t* numerators, int32_t* denominators, size_t maxSlots) {
        // Iterate over all elements in the JSON array or maxSlots, whichever is smaller
        size_t count = std::min(streetArray.size(), maxSlots);
        for (size_t i = 0; i < count; ++i) {
            float fraction = streetArray[i].get<float>();
            if (fraction <= 0.0f) {
                fraction = 0.01f;
            }
            fractionToRatio(fraction, numerators[i], denominators[i]);
        }
    };

    // apply new bets to each postflop street
    applyStreet(config["flop"],
                betConfig.flop_numerators,
                betConfig.flop_denominators,
                BetAbstraction::POSTFLOP_BET_COUNT);

    applyStreet(config["turn"],
                betConfig.turn_numerators,
                betConfig.turn_denominators,
                BetAbstraction::POSTFLOP_BET_COUNT);

    applyStreet(config["river"],
                betConfig.river_numerators,
                betConfig.river_denominators,
                BetAbstraction::POSTFLOP_BET_COUNT);
}

void applyBOBetSizing(const BOConfig& cfg) {
    auto& betConfig = BetAbstraction::g_betConfig;

    auto applyStreet = [](const float* fractions,
                          int32_t* nums,
                          int32_t* dens,
                          int count) {
        for (int i = 0; i < count; ++i) {
            fractionToRatio(fractions[i], nums[i], dens[i]);
        }
    };

    applyStreet(cfg.flop,  betConfig.flop_numerators,  betConfig.flop_denominators, 3);
    applyStreet(cfg.turn,  betConfig.turn_numerators,  betConfig.turn_denominators, 3);
    applyStreet(cfg.river, betConfig.river_numerators, betConfig.river_denominators, 3);
}



/*
 NOTE: A certain level of assymetry will be present in this file. As the "research version" of NAO can only run
 on limited computing capacity, the functions that are repeatedly launched in every iteration are merged
 to be able to optimize for speed.
 On the other hand, the baseline strategy initialization and loading only happens once per a whole learning period,
 I splitted it into separate functions, to achieve better readability.
 */

#include "cfr/cfr-core/mccfr_multithread.hpp"
#include "cfr/utils/zobrist.hpp"
#include "cfr/exploitability/exploitability.hpp"
#include "eval/evaluator.hpp"
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/mapping_engine.hpp"
#include "bet-abstraction/bet_utils.hpp"
#include "../include/nlohmann/json.hpp"
#include "evaluator.hpp"
#include "cfr/cfr-core/infoset.hpp"
#include "cfr/strategy-eval/match_engine.hpp"
#include "cfr/strategy-eval/strategy_io.hpp"

#include <cstdio>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

using timerClock = std::chrono::high_resolution_clock;
using Seconds = std::chrono::duration<double>;
using json = nlohmann::json;
using InfosetMap = robin_hood::unordered_flat_map<
    MCCFR::InfosetKey,
    MCCFR::Infoset,
    MCCFR::InfosetKeyHasher>;

namespace EvaluateBOProposal {

static double elapsed_sec(timerClock::time_point start) {
    return Seconds(timerClock::now() - start).count();
}

void initializeModulesOnce() {
    static bool initialized = false;
    
    if (initialized) return;
    
    Eval::initialize();
    Bucketer::initialize();
    Zobrist::init();
    
    initialized = true;
}

json readInputJSON() {
    json input;
    
    try {
        std::cin >> input;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        std::exit(1);
    }
    
    return input;
}

inline void fractionToRatio(double fraction, int32_t& numerator, int32_t& denominator) {
    constexpr int32_t FIXED_DENOMINATOR = 8;
    
    if (fraction <= 0.0) {
        numerator = 1;
        denominator = FIXED_DENOMINATOR;
        return;
    }
    
    numerator = static_cast<int32_t>(std::round(fraction * FIXED_DENOMINATOR));
    denominator = FIXED_DENOMINATOR;
    
    if (numerator <= 0) {
        numerator = 1;
    }
}

// in BO synergy, Nao uses two 'bet config applier' modules:
// applyJsonBetConfig: is able to load any bet sizes proposed as a vector by the BO to a global bet config
// applyBaselineConfig: responsible for initializing and training the baseline config for evaluating each vector
void applyBetConfigFromArrays(const float flop[3],
                             const float turn[3],
                             const float river[3]) {
    auto& betConfig = BetAbstraction::g_betConfig;

    for (int i = 0; i < 3; ++i) {
        fractionToRatio(flop[i],  betConfig.flop_numerators[i],  betConfig.flop_denominators[i]);
        fractionToRatio(turn[i],  betConfig.turn_numerators[i],  betConfig.turn_denominators[i]);
        fractionToRatio(river[i], betConfig.river_numerators[i], betConfig.river_denominators[i]);
    }
}

void applyBaselineConfig() {
    constexpr float sizes[3] = {0.5f, 1.0f, 2.0f};
    applyBetConfigFromArrays(sizes, sizes, sizes);
}

void applyJsonBetConfig(const json& input) {
    if (!input.contains("x") || input["x"].size() != 9) {
        std::cerr << "Invalid input: expected x[9]\n";
        std::exit(1);
    }
    
    const auto& x = input["x"];
    auto& betConfig = BetAbstraction::g_betConfig;
    
    auto processStreet = [&](int offset, int32_t* nums, int32_t* dens) {
        double a = x[offset + 0].get<double>();
        double b = x[offset + 1].get<double>();
        double c = x[offset + 2].get<double>();
        
        sort3(a, b, c);
        
        fractionToRatio(a, nums[0], dens[0]);
        fractionToRatio(b, nums[1], dens[1]);
        fractionToRatio(c, nums[2], dens[2]);
    };
    
    processStreet(0, betConfig.flop_numerators,  betConfig.flop_denominators);
    processStreet(3, betConfig.turn_numerators,  betConfig.turn_denominators);
    processStreet(6, betConfig.river_numerators, betConfig.river_denominators);
}

MatchEngine::StrategyProfile buildProfile(StrategyIO::InfosetMap map) {
    MatchEngine::StrategyProfile profile;
    profile.map = std::move(map);
    profile.numSizes = 3;
    
    const auto& cfg = BetAbstraction::g_betConfig;
    
    for (int i = 0; i < 3; ++i) {
        profile.flop_n[i]  = cfg.flop_numerators[i];
        profile.flop_d[i]  = cfg.flop_denominators[i];
        
        profile.turn_n[i]  = cfg.turn_numerators[i];
        profile.turn_d[i]  = cfg.turn_denominators[i];
        
        profile.river_n[i] = cfg.river_numerators[i];
        profile.river_d[i] = cfg.river_denominators[i];
    }
    
    return profile;
}

MatchEngine::StrategyProfile& getBaselineProfile() {
    static MatchEngine::StrategyProfile baseline = []{
        
        applyBaselineConfig();
        
        MCCFR::ParallelTrainer trainer;
        trainer.train(nodeBudget, threads, trainingSeed);
        StrategyIO::saveForPlay(trainer.getInfosetMap(), "baseline.bin");
        StrategyIO::InfosetMap map;
        StrategyIO::load(map, "baseline.bin");

        return buildProfile(std::move(map));

    }();

    return baseline;
}

double& getCumulativeBestEV() {
    static double best = -std::numeric_limits<double>::infinity();
    return best;
}

static std::pair<float, float> computeRegretStats(const InfosetMap& infosetMap) {
    double regretAbsSum = 0.0;
    double count = 0.0;
    float regretAbsMaximum = 0.0f;

    for (const auto& [key, infoset] : infosetMap) {
        for (int i = 0; i < infoset.numActions; ++i) {
            float absRegret = std::abs(infoset.regretSum[i]);

            regretAbsSum += absRegret;
            count += 1.0;

            if (absRegret > regretAbsMaximum) {
                regretAbsMaximum = absRegret;
            }
        }
    }

    float regretAbsAverage = 0.0f;

    if (count > 0.0) {
        regretAbsAverage = static_cast<float>(regretAbsSum / count);
    }
    
    return {regretAbsAverage, regretAbsMaximum};
}

BOSignal evaluatePostflopStrategy() {
    static uint32_t iteration = 0; // count the iteration count of this evaluator

    // create the logger to be able to fill it up with mccfr and match info output
    LoggedValues log;

    log.boIteration = iteration++;
    // initialize evaluators, engine
    initializeModulesOnce();
    
    // apply proposed bet sizes coming from BO module
    json betSizeInput = readInputJSON();
    applyJsonBetConfig(betSizeInput);
    
    // log the bet size lists
    const auto& config = BetAbstraction::g_betConfig;

    for (int i = 0; i < 3; ++i) {
        log.params.flopBetSizes[i] = (double)config.flop_numerators[i] / config.flop_denominators[i];
        log.params.turnBetSizes[i]  = (double)config.turn_numerators[i]  / config.turn_denominators[i];
        log.params.riverBetSizes[i] = (double)config.river_numerators[i] / config.river_denominators[i];
    }
    
    // initialize hand indexing modules
    Bucketer::IsomorphismEngine isoEngine;
    isoEngine.initialize();
    
    // train the mccfr module
    auto t_train_start = timerClock::now();
    MCCFR::ParallelTrainer trainer;
    trainer.train(nodeBudget, threads, trainingSeed);
    double training_sec = elapsed_sec(t_train_start);
    
    // logs related to specific mccfr run
    // computed values related to regrets
    const auto& infosetMap = trainer.getInfosetMap();
    auto [avgAbsRegret, maxAbsRegret] = computeRegretStats(infosetMap);
    log.avgAbsRegret = avgAbsRegret;
    log.maxAbsRegret = maxAbsRegret;

    // descriptive values about the infoset table and how Nao got there
    log.trainerSeed = trainingSeed;
    log.infosetsCount = trainer.getNumInfosets();
    log.totalNodesTouched = trainer.getTotalNodesTouched();
    log.averageRunPerNode = static_cast<float>(log.totalNodesTouched) / static_cast<float>(std::max<uint64_t>(1, log.infosetsCount));
    log.mccfrTrainingSeconds = training_sec;

    // save the strategy to bin and load the infoset map into memory
    StrategyIO::saveForPlay(trainer.getInfosetMap(), "strategy.bin");
    StrategyIO::InfosetMap map;
    StrategyIO::load(map, "strategy.bin");
    
    auto profile = buildProfile(std::move(map));
    auto& baseline = getBaselineProfile();
    
    auto t_eval_start = timerClock::now();
    auto result = MatchEngine::runMatch(
                                        profile, baseline,
                                        isoEngine,
                                        duplicateHands,
                                        100,
                                        evaluatorSeed
                                        );
    double eval_sec = elapsed_sec(t_eval_start);
    
    // logs related to the heads up evaluation
    log.evaluatorSeed = evaluatorSeed;
    log.evaluationSeconds = eval_sec;
    log.duplicateHands = duplicateHands;
    log.standardError = result.std_error_mbb;
    
    double stdDevHandEV = result.std_error_mbb * std::sqrt(result.total_hands);
    log.stdDevHandEV = stdDevHandEV;
    
    double& bestEV = getCumulativeBestEV();
    double lowerBound = result.mean_ev_mbb - 1.96 * result.std_error_mbb;

    if (lowerBound > bestEV) {
        bestEV = result.mean_ev_mbb;
    }

    log.cumulativeBestEV = bestEV;

    // SIGNAL TO BO
    BOSignal signal;
    signal.ev_mean = result.mean_ev_mbb;
    signal.ev_variance_of_mean = result.std_error_mbb * result.std_error_mbb;
    log.evP0 = result.mean_ev_mbb;
    log.evP0_bb100 = result.mean_ev_mbb * 100.0;
    
    // create the log of this single heads up training and eval iteration
    static std::ofstream file("logs.jsonl", std::ios::app);
    json out;

    out["iteration"] = log.boIteration;
    out["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // params (flattened)
    out["flop_0"] = log.params.flopBetSizes[0];
    out["flop_1"] = log.params.flopBetSizes[1];
    out["flop_2"] = log.params.flopBetSizes[2];

    out["turn_0"] = log.params.turnBetSizes[0];
    out["turn_1"] = log.params.turnBetSizes[1];
    out["turn_2"] = log.params.turnBetSizes[2];

    out["river_0"] = log.params.riverBetSizes[0];
    out["river_1"] = log.params.riverBetSizes[1];
    out["river_2"] = log.params.riverBetSizes[2];

    // performance
    out["ev_mbb"] = log.evP0;
    out["ev_bb100"] = log.evP0_bb100;
    out["stderr"] = log.standardError;
    out["stddev"] = log.stdDevHandEV;
    out["best_ev"] = log.cumulativeBestEV;

    // training
    out["infosets"] = log.infosetsCount;
    out["nodes"] = log.totalNodesTouched;
    out["nodes_per_infoset"] = log.averageRunPerNode;
    out["avg_regret"] = log.avgAbsRegret;
    out["max_regret"] = log.maxAbsRegret;
    out["train_seconds"] = log.mccfrTrainingSeconds;
    out["trainer_seed"] = log.trainerSeed;

    // evaluation
    out["eval_seconds"] = log.evaluationSeconds;
    out["duplicate_hands"] = log.duplicateHands;
    out["evaluator_seed"] = log.evaluatorSeed;

    file << out.dump() << "\n";
    file.flush();
    
    return signal;
}

}

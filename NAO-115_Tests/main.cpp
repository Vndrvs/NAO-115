/*
#include <iostream>
#include "cfr/mccfr.hpp"
#include "cfr/zobrist.hpp"
#include "eval/evaluator.hpp"
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/mapping_engine.hpp"
#include "../include/bucket-lookups/lut_manager.hpp"
#include <cmath>
#include <cassert>
#include "cfr/game_engine.hpp"
#include <filesystem>
// =========================================================================
// HELPERS
// =========================================================================
 
static int passed = 0;
static int failed = 0;
 
#define EXPECT(cond, msg) \
    do { \
        if (cond) { std::cout << "  PASS  " << msg << "\n"; passed++; } \
        else      { std::cout << "  FAIL  " << msg << "\n"; failed++; } \
    } while(0)
 
static MCCFRState makeRootState() {
    MCCFRState s{};
    s.bigBlind            = 100;
    s.player0Contribution = 100;
    s.player1Contribution = 50;
    s.previousRaiseTotal  = 100;
    s.heroStack           = 19950;
    s.villainStack        = 19900;
    s.heroStreetBet       = 50;
    s.villainStreetBet    = 100;
    s.potBase             = 0;
    s.betBeforeRaise      = 0;
    s.currentPlayer       = 1;
    s.street              = 0;
    s.raiseCount          = 0;
    s.isTerminal          = false;
    s.foldedPlayer        = -1;
    s.streetHasCheck      = false;
    s.bucketId            = 0;
    s.historyHash         = 0;
    return s;
}
 
// card indices for known hands
// assumes rank*4+suit encoding: Ac=48,Ad=49,Ah=50,As=51, 2c=0,2d=1,2h=2,2s=3
static const std::array<int,2> HAND_AA  = {50, 51};  // AhAs
static const std::array<int,2> HAND_72o = {25, 6};   // 7s 2h
static const std::array<int,5> BOARD_BLANK = {0, 4, 8, 12, 16}; // 2c 3c 4c 5c 6c
 
// =========================================================================
// LEVEL 1 — STRUCTURAL INVARIANTS
// =========================================================================
 
void test_level1_structural(MCCFR::Trainer& trainer, int iterations) {
    std::cout << "\n=== Level 1: Structural Invariants (after "
              << iterations << " iterations) ===\n";
 
    auto& map = trainer.getInfosetMap();
 
    // 1a. Infosets discovered
    EXPECT(map.size() > 0, "infosetMap is non-empty");
 
    int regretViolations   = 0;
    int strategyViolations = 0;
    int uninitializedNodes = 0;
    int checkedNodes       = 0;
 
    for (auto& [key, infoset] : map) {
        checkedNodes++;
 
        // 1b. numActions must be initialized
        if (infoset.numActions == 0) {
            uninitializedNodes++;
            continue;
        }
 
        // 1c. regretSum >= 0 for all actions
        for (int i = 0; i < infoset.numActions; ++i) {
            if (infoset.regretSum[i] < 0.0f) regretViolations++;
        }
 
        // 1d. getStrategy() sums to 1.0
        float strategy[MCCFR::MAX_ACTIONS];
        infoset.getStrategy(strategy);
        float sum = 0.0f;
        for (int i = 0; i < infoset.numActions; ++i) sum += strategy[i];
        if (std::fabs(sum - 1.0f) > 1e-4f) strategyViolations++;
    }
 
    EXPECT(uninitializedNodes == 0,
           "no uninitialized infosets (numActions==0 after visit)");
    EXPECT(regretViolations == 0,
           "regretSum >= 0 for all actions in all infosets (" +
           std::to_string(regretViolations) + " violations)");
    EXPECT(strategyViolations == 0,
           "strategy sums to 1.0 for all infosets (" +
           std::to_string(strategyViolations) + " violations)");
 
    std::cout << "  (checked " << checkedNodes << " infosets)\n";
}
 
// =========================================================================
// LEVEL 2 — CONVERGENCE SANITY
// =========================================================================
 
void test_level2_convergence(MCCFR::Trainer& trainer) {
    std::cout << "\n=== Level 2: Convergence Sanity ===\n";
 
    // 2a. AA preflop should have higher aggression than 72o
    // We look up the infoset for each hand at the root state and compare
    // average strategy fold frequency — AA should fold less
 
    MCCFRState root = makeRootState();
 
    // bucket for AA
    int32_t bucket_AA  = trainer.getBucketIdPublic(root, HAND_AA,  BOARD_BLANK);
    int32_t bucket_72o = trainer.getBucketIdPublic(root, HAND_72o, BOARD_BLANK);
 
    EXPECT(bucket_AA != bucket_72o,
           "AA and 72o map to different preflop buckets");
 
    MCCFR::InfosetKey key_AA {root.historyHash, bucket_AA};
    MCCFR::InfosetKey key_72o{root.historyHash, bucket_72o};
 
    auto& map = trainer.getInfosetMap();
 
    auto it_AA  = map.find(key_AA);
    auto it_72o = map.find(key_72o);
 
    EXPECT(it_AA  != map.end(), "AA infoset exists in map");
    EXPECT(it_72o != map.end(), "72o infoset exists in map");
 
    if (it_AA != map.end() && it_72o != map.end()) {
        float strat_AA[MCCFR::MAX_ACTIONS];
        float strat_72o[MCCFR::MAX_ACTIONS];
        it_AA->second.getAverageStrategy(strat_AA);
        it_72o->second.getAverageStrategy(strat_72o);
 
        // action 0 = fold
        float fold_AA  = strat_AA[0];
        float fold_72o = strat_72o[0];
 
        std::cout << "  AA  fold frequency: " << fold_AA  << "\n";
        std::cout << "  72o fold frequency: " << fold_72o << "\n";
 
        EXPECT(fold_AA < fold_72o,
               "AA folds less often than 72o preflop");
 
        // strategySum should be non-zero (delayed averaging ended)
        float sum_AA = 0.0f;
        for (int i = 0; i < it_AA->second.numActions; ++i)
            sum_AA += it_AA->second.strategySum[i];
        EXPECT(sum_AA > 0.0f, "AA strategySum is non-zero (delayed averaging ended)");
    }
 
    // 2b. infoset count should be in a reasonable range
    size_t n = map.size();
    std::cout << "  Total infosets discovered: " << n << "\n";
    EXPECT(n > 100,   "discovered more than 100 infosets");
    EXPECT(n < 100000000ULL, "discovered fewer than 100M infosets (no runaway)");
}
 
// =========================================================================
// LEVEL 3 — SINGLE TRAVERSAL TRACE
// =========================================================================
 
void test_level3_trace() {
    std::cout << "\n=== Level 3: Single Traversal Trace ===\n";
 
    // Run exactly 1 iteration with known cards and print everything
    // AA vs 72o, blank board
 
    Zobrist::init();
    MCCFR::Trainer traceTrainer;
    traceTrainer.setTraceMode(true);  // enables per-node debug prints inside traversal
    traceTrainer.train(1);
    traceTrainer.setTraceMode(false);
 
    auto& map = traceTrainer.getInfosetMap();
    std::cout << "  Infosets after 1 iteration: " << map.size() << "\n";
    EXPECT(map.size() > 0, "at least one infoset discovered in single traversal");
 
    // verify terminal payoff for a known fold sequence
    // construct a terminal fold state manually
    MCCFRState foldState = makeRootState();
    foldState.isTerminal  = true;
    foldState.foldedPlayer = 1;  // SB folded preflop
 
    std::array<int,2> p0h = HAND_AA;
    std::array<int,2> p1h = HAND_72o;
    std::array<int,5> brd = BOARD_BLANK;
 
    int payoff = GameEngine::getPayoff(foldState, p0h, p1h, brd);
    std::cout << "  Payoff when SB folds preflop (BB wins): " << payoff << "\n";
    EXPECT(payoff == 50, "BB wins SB's 50 chip post when SB folds preflop");
}

int lvl3testing() {
    std::cout << "=== MCCFR Test Suite ===\n";
    
 
    
    Bucketer::initialize();
    std::cout << "Bucketer initialized\n";
    Bucketer::IsomorphismEngine isoEngine;
    isoEngine.initialize();
    Zobrist::init();
    Eval::initialize();
    // --- Level 3 first (single trace, before any real training) ---
    test_level3_trace();
 
    // --- Run a short training pass ---
    std::cout << "\nRunning 10,000 iterations for Level 1 + 2 tests...\n";
    MCCFR::Trainer trainer;
    trainer.train(10000);
 
    // --- Level 1 ---
    test_level1_structural(trainer, 10000);
 
    // --- Level 2 ---
    test_level2_convergence(trainer);
 
    // --- Summary ---
    std::cout << "\n=== Results: " << passed << " passed, "
              << failed << " failed ===\n";
 
    return failed > 0 ? 1 : 0;
}

int main() {
    std::cout << "Working dir: " << std::filesystem::current_path() << "\n";


    std::cout << "--- START TRAINING ---\n";
    
    lvl3testing();
/*
    MCCFR::Trainer trainer;

    trainer.train(3);

    std::cout << "--- DONE ---\n";
    std::cout << "Infosets: " << trainer.getNumInfosets() << "\n";
    return 0;
}
*/

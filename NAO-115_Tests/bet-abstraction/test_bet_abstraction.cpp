#include <gtest/gtest.h>
#include "bet-abstraction/bet_sequence.hpp"
#include "bet-abstraction/bet_utils.hpp"
#include "cfr/game_engine.hpp"
#include "cfr/mccfr_state.hpp"
#include "eval/evaluator.hpp"
#include "eval/tables.hpp"
#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <algorithm>
#include <string>
#include <cassert>

struct TreeStats {
    int nodes[4] = {0,0,0,0}; // streets 0..3
};

// Recursive tree builder
void buildActionTree(const MCCFRState& state, TreeStats& stats) {
    // Increment node count for current street
    stats.nodes[state.street]++;

    // Terminal node check
    if (GameEngine::isGamestateTerminal(state)) return;

    // Generate legal actions
    auto actions = BetAbstraction::getLegalActions(state);

    for (int i = 0; i < actions.count; ++i) {
        MCCFRState nextState = state;

        // Apply the action
        nextState = GameEngine::applyAction(nextState, actions.actions[i]);

        buildActionTree(nextState, stats);
    }
}

int main() {
    // --- ROOT STATE ---
    MCCFRState root{};
    root.bigBlind = 100;
    root.player0Contribution = 100;
    root.player1Contribution = 50;
    root.heroStack = 20000;
    root.villainStack = 20000;
    root.heroStreetBet = 0;
    root.villainStreetBet = 0;
    root.previousRaiseTotal = 0;
    root.betBeforeRaise = 0;
    root.currentPlayer = 1;
    root.street = 1; // flop
    root.raiseCount = 0;
    root.isTerminal = false;
    root.foldedPlayer = -1;
    root.streetHasCheck = false;
    root.bucketId = 0;
    root.historyHash = 0;

    TreeStats stats{};
    buildActionTree(root, stats);

    std::cout << "Node counts per street:\n";
    for (int s = 1; s <= 3; ++s) {
        std::cout << "Street " << s << ": " << stats.nodes[s] << " nodes\n";
    }
}

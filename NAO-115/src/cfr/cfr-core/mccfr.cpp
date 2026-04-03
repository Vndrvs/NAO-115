#include "mccfr.hpp"
#include "game_engine.hpp"
#include "bet-abstraction/bet_sequence.hpp"
#include "../include/bucket-lookups/lut_indexer.hpp"
#include "../utils/zobrist.hpp"
#include <iostream>
#include <random>

namespace MCCFR {

Trainer::Trainer(uint64_t seed) :
    targetNodeBudget(0),
    nodesTouched(0),
    rng(seed),
    dist(0.0f, 1.0f),
    traceMode(false)
{
    mappingEngine.initialize();
}

int32_t Trainer::getBucketId(const MCCFRState& state,
                             const std::array<int, 2>& hand,
                             const std::array<int, 5>& board) {
    int boardSize = 0;
    
    // Map the internal street (0-3) to the number of cards on board
    if (state.street == 1) {        // Flop
        boardSize = 3;
    } else if (state.street == 2) { // Turn
        boardSize = 4;
    } else if (state.street == 3) { // River
        boardSize = 5;
    }
    // state.street is 0 (preflop) -> boardSize remains 0
    return Bucketer::lookup_bucket(mappingEngine, hand.data(), board.data(), boardSize);
}

// traverse function
float Trainer::traverseExternalSampling(const MCCFRState& state,
                                        int updatePlayer,
                                        const std::array<int, 2>& p0_hand,
                                        const std::array<int, 2>& p1_hand,
                                        const std::array<int, 5>& board) {
    
    nodesTouched++;
    
    // check whether game is terminal
    if (GameEngine::isGamestateTerminal(state)) {
        // Calculate chips won/lost from Player 0's perspective
        int payoff0 = GameEngine::getPayoff(state, p0_hand, p1_hand, board);
        // Return payoff relative to the player we are currently updating
        if (updatePlayer == 0) {
            return static_cast<float>(payoff0);
        } else {
            return static_cast<float>(-payoff0);
        }
    }
    
    // calculate bucketing locally
    int32_t currentBucket = 0;
    if (state.currentPlayer == 0) {
        currentBucket = getBucketId(state, p0_hand, board);
    } else {
        currentBucket = getBucketId(state, p1_hand, board);
    }
    
    // create infoset key
    InfosetKey key{state.historyHash, currentBucket};
    Infoset& infoset = infosetMap[key];
    
    BetAbstraction::ActionList legalActions = BetAbstraction::getLegalActions(state);
    
    
    if (infoset.numActions == 0) {
        infoset.initialize(legalActions.count);
    }
    
    float strategy[MAX_ACTIONS];
    infoset.getStrategy(strategy);
    
    // DEBUG
    if (traceMode) {
        std::cout << "[STRAT] street=" << (int)state.street
        << " player=" << (int)state.currentPlayer
        << " raiseCount=" << (int)state.raiseCount
        << " bucket=" << currentBucket
        << " actions=" << (int)legalActions.count << "\n";
        float sum = 0.0f;
        for (int i = 0; i < legalActions.count; ++i) {
            std::cout << "  a[" << i << "] = " << strategy[i] << "\n";
            sum += strategy[i];
        }
        std::cout << "  sum = " << sum << "\n";
    }
    
    // opponent's turn (external sampling)
    if (state.currentPlayer != updatePlayer) {
        uint64_t warmup = targetNodeBudget / 10;
        float weight = (iterations < warmup)
            ? 0.0f
            : static_cast<float>(iterations);
        infoset.updateStrategy(weight, 1.0f, strategy);
        
        float r = dist(rng);
        int chosenIndex = legalActions.count - 1;  // default to last action
        float cumulative = 0.0f;
        
        for (int i = 0; i < legalActions.count; ++i) {
            cumulative += strategy[i];
            if (r < cumulative) {
                chosenIndex = i;
                break;
            }
        }
        
        MCCFRState nextState = state;
        BetAbstraction::AbstractAction action = legalActions.actions[chosenIndex];
        nextState.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][chosenIndex];
        nextState = GameEngine::applyAction(nextState, action);
        
        return traverseExternalSampling(nextState, updatePlayer, p0_hand, p1_hand, board);
    }
    
    float actionEVs[MAX_ACTIONS] = {0.0f};
    float nodeEV = 0.0f;
    
    for (int i = 0; i < legalActions.count; ++i) {
        MCCFRState nextState = state;
        
        nextState.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][i];
        nextState = GameEngine::applyAction(nextState, legalActions.actions[i]);
        actionEVs[i] = traverseExternalSampling(nextState, updatePlayer, p0_hand, p1_hand, board);
        nodeEV += strategy[i] * actionEVs[i];
    }
    
    float regrets[MAX_ACTIONS] = {0.0f};
    for (int i = 0; i < legalActions.count; ++i) {
        regrets[i] = actionEVs[i] - nodeEV;
    }
    
    if (traceMode) {
        std::cout << "[REGRET]\n";
        for (int i = 0; i < legalActions.count; ++i) {
            std::cout << "  r[" << i << "] = " << regrets[i] << "\n";
        }
    }
    
    infosetMap[key].updateRegrets(regrets);
    
    return nodeEV;
}

void Trainer::train(uint64_t nodeBudget) {
    targetNodeBudget = nodeBudget;
    nodesTouched = 0;
    iterations = 0;

    int handsPlayed = 0;
    
    std::array<int, 52> deck;
    for (int i = 0; i < 52; ++i) deck[i] = i;
    
    while (nodesTouched < targetNodeBudget) {
        for (int j = 0; j < 9; ++j) {
            int k = j + rng() % (52 - j);
            std::swap(deck[j], deck[k]);
        }
        
        std::array<int, 2> p0_hand = {deck[0], deck[1]};
        std::array<int, 2> p1_hand = {deck[2], deck[3]};
        std::array<int, 5> board = {deck[4], deck[5], deck[6], deck[7], deck[8]};
        
        MCCFRState rootState{};
        rootState.bigBlind = 100;
        rootState.player0Contribution = 1000;
        rootState.player1Contribution = 1000;
        rootState.previousRaiseTotal = 0;
        rootState.heroStack = 9000;
        rootState.villainStack = 9000;
        rootState.heroStreetBet = 0;
        rootState.villainStreetBet = 0;
        rootState.potBase = 2000;
        rootState.betBeforeRaise = 0;
        rootState.currentPlayer = 0;
        rootState.street = 1;
        rootState.raiseCount = 0;
        rootState.isTerminal = false;
        rootState.foldedPlayer = -1;
        rootState.streetHasCheck = false;
        rootState.bucketId = 0;
        rootState.historyHash = 0;
        
        int updatePlayer = handsPlayed % 2;
        
        traverseExternalSampling(rootState, updatePlayer, p0_hand, p1_hand, board);
        
        handsPlayed++;
        iterations++;
    }
    
    //std::cout << "Training Complete. Total Infosets: " << infosetMap.size() << "\n";
}

}

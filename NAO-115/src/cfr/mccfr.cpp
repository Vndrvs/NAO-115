#include "mccfr.hpp"
#include "game_engine.hpp"
#include "bet-abstraction/bet_sequence.hpp"
#include "zobrist.hpp"
#include <iostream>
#include <random>

namespace MCCFR {

int32_t Trainer::getBucketId(const MCCFRState& state,
                             const std::array<int, 2>& p0_hand,
                             const std::array<int, 2>& p1_hand,
                             const std::array<int, 5>& board) {

    return 0;
}

float Trainer::traverseExternalSampling(MCCFRState state,
                                        int updatePlayer,
                                        const std::array<int, 2>& p0_hand,
                                        const std::array<int, 2>& p1_hand,
                                        const std::array<int, 5>& board) {
    
    if (GameEngine::isGamestateTerminal(state)) {
        int payoff0 = GameEngine::getPayoff(state, p0_hand, p1_hand, board);
        return (updatePlayer == 0) ? payoff0 : -payoff0;
    }


    state.bucketId = getBucketId(state, p0_hand, p1_hand, board);
    
    uint64_t infosetKey = state.historyHash ^ (static_cast<uint64_t>(state.bucketId) << 32);

    InfosetKey key{state.historyHash, state.bucketId};
    Infoset& infoset = infosetMap[key];
    BetAbstraction::ActionList legalActions = BetAbstraction::getLegalActions(state);

    if (infoset.numActions == 0) {
        infoset.initialize(legalActions.count);
    }

    float strategy[MAX_ACTIONS];
    infoset.getStrategy(strategy);

    if (state.currentPlayer != updatePlayer) {
       
        infoset.updateStrategy(1.0f, 1.0f, strategy);

        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float cumulative = 0.0f;
        int chosenIndex = 0;
        
        for (int i = 0; i < legalActions.count; ++i) {
            cumulative += strategy[i];
            if (r < cumulative) {
                chosenIndex = i;
                break;
            }
        }

        BetAbstraction::AbstractAction action = legalActions.actions[chosenIndex];
        state.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][chosenIndex];
        
        MCCFRState nextState = GameEngine::applyAction(state, action);
        return traverseExternalSampling(nextState, updatePlayer, p0_hand, p1_hand, board);
    }


    float actionEVs[MAX_ACTIONS] = {0.0f};
    float nodeEV = 0.0f;

    for (int i = 0; i < legalActions.count; ++i) {
        BetAbstraction::AbstractAction action = legalActions.actions[i];
        
        MCCFRState nextState = state;
        nextState.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][i];
        nextState = GameEngine::applyAction(nextState, action);

        actionEVs[i] = traverseExternalSampling(nextState, updatePlayer, p0_hand, p1_hand, board);
        
        nodeEV += strategy[i] * actionEVs[i];
    }

    float regrets[MAX_ACTIONS] = {0.0f};
    for (int i = 0; i < legalActions.count; ++i) {
        regrets[i] = actionEVs[i] - nodeEV;
    }
    infoset.updateRegrets(regrets);

    return nodeEV;
}

void Trainer::train(int iterations) {
    std::cout << "Starting MCCFR Training for " << iterations << " iterations...\n";

    std::vector<int> deck(52);
    for(int i=0; i<52; ++i) deck[i] = i;
    std::mt19937 rng(1337);

    for (int i = 0; i < iterations; ++i) {
        std::shuffle(deck.begin(), deck.end(), rng);
        std::array<int, 2> p0_hand = {deck[0], deck[1]};
        std::array<int, 2> p1_hand = {deck[2], deck[3]};
        std::array<int, 5> board   = {deck[4], deck[5], deck[6], deck[7], deck[8]};

        MCCFRState rootState{};
        rootState.heroStack = 19950;
        rootState.villainStack = 19900;
        rootState.heroStreetBet = 50;
        rootState.villainStreetBet = 100;
        rootState.potBase = 0;
        rootState.currentPlayer = 1;
        rootState.street = 0;
        rootState.raiseCount = 0;
        rootState.isTerminal = false;
        rootState.foldedPlayer = -1;
        rootState.streetHasCheck = false;
        rootState.historyHash = 0;

        int updatePlayer = i % 2;

        traverseExternalSampling(rootState, updatePlayer, p0_hand, p1_hand, board);

        if ((i + 1) % 10000 == 0) {
            std::cout << "Iteration " << (i + 1) << " | Infosets Discovered: " << infosetMap.size() << "\n";
        }
    }
    
    std::cout << "Training Complete. Total Infosets: " << infosetMap.size() << "\n";
}

} // namespace MCCFR

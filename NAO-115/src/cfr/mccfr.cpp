#include "mccfr.hpp"
#include "game_engine.hpp"
#include "bet-abstraction/bet_sequence.hpp"
#include "zobrist.hpp"
#include <iostream>
#include <random>

namespace MCCFR {

// Placeholder: Link this to your actual Bucketer
int32_t Trainer::getBucketId(const MCCFRState& state,
                             const std::array<int, 2>& p0_hand,
                             const std::array<int, 2>& p1_hand,
                             const std::array<int, 5>& board) {
    // For now, return 0. You will replace this with:
    // return Bucketer::getBucket(state.street, current_hand, board);
    return 0;
}

float Trainer::traverseExternalSampling(MCCFRState state,
                                        int updatePlayer,
                                        const std::array<int, 2>& p0_hand,
                                        const std::array<int, 2>& p1_hand,
                                        const std::array<int, 5>& board) {
    
    // 1. BASE CASE: Terminal Node -> Return the exact chip payoff for the updating player
    if (GameEngine::isGamestateTerminal(state)) {
        int payoff0 = GameEngine::getPayoff(state, p0_hand, p1_hand, board);
        return (updatePlayer == 0) ? payoff0 : -payoff0;
    }

    // 2. STATE SETUP
    // Assign the bucket ID based on the current player's cards and the board
    state.bucketId = getBucketId(state, p0_hand, p1_hand, board);
    
    // Combine betting history hash and bucket ID to get the unique Infoset Key
    uint64_t infosetKey = state.historyHash ^ (static_cast<uint64_t>(state.bucketId) << 32);

    // Fetch or create the Infoset
    InfosetKey key{state.historyHash, state.bucketId};
    Infoset& infoset = infosetMap[key];
    BetAbstraction::ActionList legalActions = BetAbstraction::getLegalActions(state);

    // Initialize the infoset if it's the first time visiting
    if (infoset.numActions == 0) {
        infoset.initialize(legalActions.count);
    }

    // Get current strategy based on positive regrets
    float strategy[MAX_ACTIONS];
    infoset.getStrategy(strategy);

    // 3. OPPONENT'S TURN (Sampling Node)
    if (state.currentPlayer != updatePlayer) {
        // External sampling: Update the average strategy of the opponent
        // (Reach probability is implicitly 1.0 because we reached this node via the Traverser's choices)
        infoset.updateStrategy(1.0f, 1.0f, strategy);

        // Sample ONE action based on the strategy distribution
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

        // Apply action, update hash, and continue down the single branch
        BetAbstraction::AbstractAction action = legalActions.actions[chosenIndex];
        state.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][chosenIndex];
        
        MCCFRState nextState = GameEngine::applyAction(state, action);
        return traverseExternalSampling(nextState, updatePlayer, p0_hand, p1_hand, board);
    }

    // 4. TRAVERSER'S TURN (Exploration Node)
    // We explore EVERY legal action, find their EVs, and update regrets.
    float actionEVs[MAX_ACTIONS] = {0.0f};
    float nodeEV = 0.0f;

    for (int i = 0; i < legalActions.count; ++i) {
        BetAbstraction::AbstractAction action = legalActions.actions[i];
        
        // Clone state, apply hash, apply action
        MCCFRState nextState = state;
        nextState.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][i];
        nextState = GameEngine::applyAction(nextState, action);

        // Recursively evaluate this branch
        actionEVs[i] = traverseExternalSampling(nextState, updatePlayer, p0_hand, p1_hand, board);
        
        // Node EV is the strategy-weighted sum of the action EVs
        nodeEV += strategy[i] * actionEVs[i];
    }

    // Compute regrets and update the Infoset
    float regrets[MAX_ACTIONS] = {0.0f};
    for (int i = 0; i < legalActions.count; ++i) {
        // Regret = EV of choosing this action - EV of the current strategy
        regrets[i] = actionEVs[i] - nodeEV;
    }
    infoset.updateRegrets(regrets);

    return nodeEV;
}

void Trainer::train(int iterations) {
    std::cout << "Starting MCCFR Training for " << iterations << " iterations...\n";

    // Setup a dummy deck (0-51) and RNG
    std::vector<int> deck(52);
    for(int i=0; i<52; ++i) deck[i] = i;
    std::mt19937 rng(1337);

    for (int i = 0; i < iterations; ++i) {
        // 1. Shuffle and Deal 9 cards (Chance Sampling)
        std::shuffle(deck.begin(), deck.end(), rng);
        std::array<int, 2> p0_hand = {deck[0], deck[1]};
        std::array<int, 2> p1_hand = {deck[2], deck[3]};
        std::array<int, 5> board   = {deck[4], deck[5], deck[6], deck[7], deck[8]};

        // 2. Setup the Root Game State
        MCCFRState rootState{};
        rootState.heroStack = 19950;
        rootState.villainStack = 19900;
        rootState.heroStreetBet = 50;
        rootState.villainStreetBet = 100;
        rootState.potBase = 0;
        rootState.currentPlayer = 1; // SB acts first preflop
        rootState.street = 0;
        rootState.raiseCount = 0;
        rootState.isTerminal = false;
        rootState.foldedPlayer = -1;
        rootState.streetHasCheck = false;
        rootState.historyHash = 0; // Starts at 0, Zobrist XORs from here

        // 3. Pick Traverser (Alternate between P0 and P1 each iteration)
        int updatePlayer = i % 2;

        // 4. Traverse
        traverseExternalSampling(rootState, updatePlayer, p0_hand, p1_hand, board);

        if ((i + 1) % 10000 == 0) {
            std::cout << "Iteration " << (i + 1) << " | Infosets Discovered: " << infosetMap.size() << "\n";
        }
    }
    
    std::cout << "Training Complete. Total Infosets: " << infosetMap.size() << "\n";
}

} // namespace MCCFR

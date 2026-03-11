#include "bet_sequence.hpp"
#include <iostream>

// should be set to 0 for mccfr training & set to 1 when running gtest
#define DEBUG_BET_SEQUENCE 0

#if DEBUG_BET_SEQUENCE
    #define DEBUG_PRINT(x) std::cout << "[BetAbst] " << x << "\n"
#else
    #define DEBUG_PRINT(x)
#endif

namespace BetAbstraction {

ActionList getLegalActions(const MCCFRState& state) {
    ActionList list;
    
    int32_t effectiveAllIn = state.effectiveAllIn();
    int32_t toCall = state.villainStreetBet - state.heroStreetBet;
    
    DEBUG_PRINT("Node context | Preflop: " << state.isPreflop()
                << " | toCall: " << toCall
                << " | raiseCount: " << (int)state.raiseCount);
    
    // BASELINE ACTIONS
    
    // facing bet
    if (toCall > 0) {
        // always add Fold and Call
        DEBUG_PRINT("Facing bet of " << toCall << ", adding Fold and Call.");
        list.add(ActionType::FOLD, 0);
        list.add(ActionType::CALL, 0);
            
        // game is all-in / the bet we are facing puts us all-in, we cannot raise
        if (state.anyAllIn() || effectiveAllIn <= state.villainStreetBet) {
            DEBUG_PRINT("Stack capped, returning early.");
            return list;
        }
        // not facing bet
    } else {
        // If the game is all-in but the street just started, return an empty list.
        // The future game engine must catch this and fast-forward to showdown.
        if (state.anyAllIn()) {
            DEBUG_PRINT("Game is all-in. Returning empty list.");
            return list;
        }
            
        // Facing no bet and stacks are deep: always add Check
        DEBUG_PRINT("Facing no bet, adding Check.");
        list.add(ActionType::CHECK, 0);
    }
    
    // BET AND RAISE ACTIONS
    if (state.raiseCount >= MAX_RAISES) {
        DEBUG_PRINT("Raise cap hit, now returning.");
        return list;
    }
    
    if (state.raiseCount == 3) {
        DEBUG_PRINT("4-bet, only adding All-in.");
        list.add(ActionType::RAISE, effectiveAllIn);
        return list;
    }
    
    int32_t minRaise = 0;
    if (toCall > 0) {
        minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
    } else {
        minRaise = state.bigBlind; // absolute min raise is BB amount in Poker
    }
    
    // Select the correct size arrays based on state
    const int32_t* numerators = nullptr;
    const int32_t* denominators = nullptr;
    int arraySize = 0;
    
    if (state.raiseCount <= 1) {
        // Open (0) OR First Raise (1) uses the street's base sizes
        if (state.isPreflop()) {
            numerators = PREFLOP_BET_NUMERATORS;
            denominators = PREFLOP_BET_DENOMINATORS;
            arraySize = PREFLOP_BET_COUNT;
        } else {
            numerators = POSTFLOP_BET_NUMERATORS;
            denominators = POSTFLOP_BET_DENOMINATORS;
            arraySize = POSTFLOP_BET_COUNT;
        }
    } else {
        // 3-Bet (state.raiseCount == 2) uses the universal 0.8x size
        numerators = THREE_BET_NUMERATORS;
        denominators = THREE_BET_DENOMINATORS;
        arraySize = THREE_BET_COUNT;
    }
    
    // RAISE SIZING & O(N) DEDUPLICATION
    int32_t lastAddedAmount = -1;
    // Open (0) OR First Raise (1) uses the street's base sizes
    for (int i = 0; i < arraySize; ++i) {
        int32_t amount = computeAmount(
                                    numerators[i], denominators[i],
                                    state.potBase, state.villainStreetBet,
                                    state.heroStreetBet, effectiveAllIn
        );
    
        // Filter invalid sizes instantly
        if (amount < minRaise) continue;
        if (amount == lastAddedAmount) continue;
        
        list.add(ActionType::RAISE, amount);
        lastAddedAmount = amount;
        
        // If we naturally hit all-in during generation, halt generation
        if (amount == effectiveAllIn) {
            break;
        }
    }
    
    if (lastAddedAmount != effectiveAllIn) {
        list.add(ActionType::RAISE, effectiveAllIn);
    }
    
    return list;
}

}

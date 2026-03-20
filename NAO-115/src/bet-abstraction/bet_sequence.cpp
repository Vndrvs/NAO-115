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
 
    // need to get effective all in amount first to raise with actual stack, not full default one
    int32_t effectiveAllIn = state.effectiveAllIn();
    int32_t toCall = state.villainStreetBet - state.heroStreetBet;
 
    DEBUG_PRINT("Node context | Preflop: " << state.isPreflop()
                << " | toCall: " << toCall
                << " | raiseCount: " << (int)state.raiseCount);
 
    // this block is responsible for giving the player a chance to react after raisecount has been maxed out
    if (state.raiseCount >= MAX_RAISES) {
        if (toCall > 0) {
            list.add(ActionType::FOLD, 0);
            list.add(ActionType::CALL, 0);
        } else {
            list.add(ActionType::CHECK, 0);
        }
        return list;
    }
 
    // raisecount == 3 - all-in only case
    if (state.raiseCount == 3) {
        if (toCall > 0) {
            list.add(ActionType::FOLD, 0);
            list.add(ActionType::CALL, 0);
            list.add(ActionType::RAISE, effectiveAllIn);
        } else {
            // this case can basically never occur
            list.add(ActionType::CHECK, 0);
            list.add(ActionType::RAISE, effectiveAllIn);
        }
        return list;
    }
 
    // baseline actions that can be added in any scenario
    if (toCall > 0) {
        list.add(ActionType::FOLD, 0);
        list.add(ActionType::CALL, 0);
    } else {
        list.add(ActionType::CHECK, 0);
    }
 
    // select the correct raise sizings relative to street
    const int32_t* numerators   = nullptr;
    const int32_t* denominators = nullptr;
    int arraySize = 0;
 
    if (state.raiseCount <= 1) {
        if (state.isPreflop()) {
            numerators   = PREFLOP_BET_NUMERATORS;
            denominators = PREFLOP_BET_DENOMINATORS;
            arraySize    = PREFLOP_BET_COUNT;
        } else {
            numerators   = POSTFLOP_BET_NUMERATORS;
            denominators = POSTFLOP_BET_DENOMINATORS;
            arraySize    = POSTFLOP_BET_COUNT;
        }
    } else {
        // raiseCount is 2: need 3-bet sizings
        numerators   = THREE_BET_NUMERATORS;
        denominators = THREE_BET_DENOMINATORS;
        arraySize    = THREE_BET_COUNT;
    }
 
    // compute legal min raise
    int32_t minRaise = 0;
    if (toCall > 0) {
        minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
    } else {
        minRaise = state.bigBlind;
    }
 
    // Sizing slots — fixed count, amounts capped on all-in and floored to min-raise
    for (int i = 0; i < arraySize; ++i) {
        int32_t amount = computeAmount(
            numerators[i], denominators[i],
            state.potBase, state.villainStreetBet,
            state.heroStreetBet, effectiveAllIn
        );
        amount = std::max(amount, minRaise);
        amount = std::min(amount, effectiveAllIn);
        list.add(ActionType::RAISE, amount);
    }
 
    // all-in is always Nao's final dedicated slot
    list.add(ActionType::RAISE, effectiveAllIn);
 
    return list;
}
 
}

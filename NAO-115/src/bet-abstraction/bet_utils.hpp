// refactor of the previous bet maths logic to be more compiler and logic friendly

#pragma once
#include <cstdint>
#include <algorithm>

namespace BetAbstraction {

#define DEBUG_BET_ABSTRACTION 0

#if DEBUG_BET_ABSTRACTION
    #define DEBUG_PRINT(x) std::cout << "[BetUtil] " << x << "\n"
#else
    #define DEBUG_PRINT(x)
#endif

// Core bet sequence build constraints:
// using integer arithmetic (numerator/denominator) instead of floats for speed

// Preflop opens: 2x, 3x, 5x pot
constexpr int32_t PREFLOP_BET_NUMERATORS[] = {2, 3, 5};
constexpr int32_t PREFLOP_BET_DENOMINATORS[] = {1, 1, 1};
constexpr int PREFLOP_BET_COUNT = 3;

// Postflop opens: 0.5x, 1x, 2x pot
constexpr int32_t POSTFLOP_BET_NUMERATORS[] = {1, 1, 2};
constexpr int32_t POSTFLOP_BET_DENOMINATORS[] = {2, 1, 1};
constexpr int POSTFLOP_BET_COUNT = 3;

// Postflop raises (facing a bet): 0.5x, 1x, 2x pot
constexpr int32_t RAISE_NUMERATORS[] = {1, 1, 2};
constexpr int32_t RAISE_DENOMINATORS[] = {2, 1, 1};
constexpr int RAISE_COUNT = 3;

// 3-bets (facing a raise): 0.8x pot
constexpr int32_t THREE_BET_NUMERATORS[] = {4};
constexpr int32_t THREE_BET_DENOMINATORS[] = {5}; // 4/5 = 0.8
constexpr int THREE_BET_COUNT = 1;

// Maximum actions per street (0=bet, 1=raise, 2=3bet, 3=4bet)
// 4-bet is restricted to All-In only. No 5-bets allowed.
constexpr int MAX_RAISES = 4;

//Computes a bet or raise amount using call-first geometry
inline int32_t computeAmount(int32_t numerator, int32_t denominator,
                             int32_t pot, int32_t villainBet,
                             int32_t heroCurrentBet, int32_t currentStack) {
    
    // 1. how much to call to match villain?
    int32_t callAmount = std::max(0, villainBet - heroCurrentBet);
    
    // 2. what would the pot be after the call?
    int32_t potAfterCall = pot + heroCurrentBet + villainBet + callAmount;
    
    // 3. calculate raise amount based on the pot * (num/denom)
    int32_t raiseAmount = (potAfterCall * numerator) / denominator;
    
    // 4. total chips we commit = our current bet + call + raise
    int32_t totalBet = heroCurrentBet + callAmount + raiseAmount;
    
    // 5. cap at our all-in size (stack)
    return std::min(totalBet, currentStack);
}

/*
Returns the minimum legal raise total.
 in NLHE, a raise must be at least as large as the previous raise increment.
 minRaise = previousRaiseTotal + previousRaiseIncrement
 where previousRaiseIncrement = previousRaiseTotal - betBeforeRaise
*/
inline int32_t computeMinRaise(int32_t previousRaiseTotal, int32_t betBeforeRaise) {
    int32_t previousIncrement = previousRaiseTotal - betBeforeRaise;
    return previousRaiseTotal + previousIncrement;
}

// Returns true if amount >= currentStack (player would be all-in)
inline bool isAllIn(int32_t amount, int32_t currentStack) {
    return amount >= currentStack;
}

}

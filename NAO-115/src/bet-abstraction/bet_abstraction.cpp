#include "bet_abstraction.hpp"
#include "bet_maths.hpp"
#include "cfr/mccfr_state.hpp"
#include <vector>

namespace BetAbstraction {

/*
 Preflop open sizes as BB multipliers.
 SB acts first preflop and can open to these sizes.
 BB can also raise to these sizes after SB limps.
 */
static const float PREFLOP_OPEN_SIZES[] = { 2.0f, 3.0f };
static const int PREFLOP_OPEN_COUNT   = 2;

/*
 Preflop reraise multiplier.
 Applied to previousRaiseTotal to compute 3-bet and 4-bet sizes.
 Example: SB opens 3BB, BB 3-bets to 2.5 * 3BB = 7.5BB
 */
static const float PREFLOP_RERAISE_MULTIPLIER = 2.5f;

/*
 Post-flop initial bet sizes as fractions of totalPot().
 Used when no bet is facing the current player.
 */
static const float POSTFLOP_BET_SIZES[]  = { 0.33f, 0.75f, 1.50f };
static const int POSTFLOP_BET_COUNT    = 3;

/*
 Post-flop raise sizes as fractions of totalPot() using call-first geometry.
 Used when facing a bet (raiseCount 1 -> 2).
 */
static const float POSTFLOP_RAISE_SIZES[] = { 0.75f, 1.50f };
static const int POSTFLOP_RAISE_COUNT = 2;

/*
 Post-flop 3-bet sizes as fractions of totalPot() using call-first geometry.
 Used when facing a raise (raiseCount 2 -> 3).
 */
static const float POSTFLOP_3BET_SIZES[] = { 1.00f };
static const int POSTFLOP_3BET_COUNT = 1;

/*
 Maximum raises per street before only call/fold is allowed.
 raiseCount 0: initial bet
 raiseCount 1: raise
 raiseCount 2: 3-bet
 raiseCount 3: 4-bet (all-in only)
 raiseCount 4: call/fold only
 */
static const int MAX_RAISES = 4;

std::vector<AbstractAction> getLegalActions(const MCCFRState& state) {
    std::vector<AbstractAction> actions;
    actions.reserve(7); // max possible actions at any node
    // fold - call - raise (3 sizes postflop) - reraise - reraise - all-in
    
    // get the current stack size of the player
    const int effectiveStack = state.effectiveStack();
    
    const int effectiveAllIn = state.effectiveAllIn();
    
    // get total pot for this game so far
    const int totalPot = state.potBase;
    
    // calculate whether we are facing a bet currently
    const int toCall = state.villainStreetBet - state.heroStreetBet;
    
    
    // Special case: Someone is all-in
    // if either player has no chips left, no betting is possible
    // only fold or call remaining
    if (state.anyAllIn() && toCall > 0) {
        actions.push_back({ 0, 0 }); // fold, amount 0
        actions.push_back({ 2, 0 }); // call, amount 0
        return actions;
    }
    // PREFLOP
    if (state.isPreflop()) {
        // no bet faced
        if (toCall == 0) {
            actions.push_back({ 1, 0 }); // check
            
            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                
                if (state.raiseCount == 0) {
                    // BB option after limp — use open sizes
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        amounts.push_back(computePreflopOpen(
                                                             PREFLOP_OPEN_SIZES[i], state.bigBlind, effectiveStack));
                    }
                    amounts = filterAndDeduplicate(amounts, 1, effectiveAllIn);
                } else {
                    // re-raise after both players equalized — use 2.5x
                    amounts.push_back(computePreflopReraise(
                                                            PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveStack));
                    int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                    amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
                }
                
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
            } else {
                actions.push_back({ 3, effectiveAllIn });
            }
        } else {
            // we are facing a bet
            actions.push_back({ 0, 0 }); // fold
            actions.push_back({ 2, 0 }); // call
            
            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                
                if (state.raiseCount == 0) {
                    // SB first action, BB is live bet — use open sizes
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        amounts.push_back(computePreflopOpen(
                                                             PREFLOP_OPEN_SIZES[i], state.bigBlind, effectiveStack));
                    }
                    amounts = filterAndDeduplicate(amounts, 1, effectiveAllIn);
                } else {
                    // facing actual raise — use 2.5x
                    amounts.push_back(computePreflopReraise(
                                                            PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveStack));
                    int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                    amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
                }
                
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
                // we reached the max raise limit (only all-in remains)
            } else {
                actions.push_back({ 3, effectiveAllIn }); // raise, all-in
            }
            
        }
        // return legal actions for preflop
        return actions;
    }
    // POST-FLOP (Flop, Turn, River)
    // no bet faced - check or bet possible
    if (toCall == 0) {
        actions.push_back({ 1, 0 }); // check, amount 0
        if (state.raiseCount < MAX_RAISES) {
            std::vector<int> amounts;
            
            for (int i = 0; i < POSTFLOP_BET_COUNT; i++) {
                int amount = computePostflopAmount(POSTFLOP_BET_SIZES[i], totalPot, 0, 0, effectiveStack);
                amounts.push_back(amount);
            }
            // opening bet: minimum is 1 chip
            amounts = filterAndDeduplicate(amounts, 1, effectiveAllIn);
            
            for (int amount : amounts) {
                actions.push_back({ 3, amount }); // bet
            }
        } else {
            // raise cap — all-in only
            actions.push_back({ 3, effectiveAllIn });
        }
    } else {
        // facing a bet - fold, call, raise or possible
        actions.push_back({ 0, 0 }); // fold
        actions.push_back({ 2, 0 }); // call (may be all-in)
        
        if (state.raiseCount >= MAX_RAISES) {
            // raise cap hit — no more raises allowed
            return actions;
        }
        
        if (state.raiseCount >= 3) {
            // abstract 4-bet: all-in only
            actions.push_back({ 3, effectiveAllIn });
            return actions;
        }
        
        const float* raiseSizes = nullptr;
        int raiseCountLocal = 0;
        
        if (state.raiseCount == 1) {
            raiseSizes = POSTFLOP_RAISE_SIZES;
            raiseCountLocal = POSTFLOP_RAISE_COUNT;
        } else if (state.raiseCount == 2) {
            raiseSizes = POSTFLOP_3BET_SIZES;
            raiseCountLocal = POSTFLOP_3BET_COUNT;
        }
        
        std::vector<int> amounts;
        for (int i = 0; i < raiseCountLocal; i++) {
            int amount = computePostflopAmount(
                                               raiseSizes[i],
                                               totalPot,
                                               state.villainStreetBet,
                                               state.heroStreetBet,
                                               effectiveStack
                                               );
            amounts.push_back(amount);
        }
        
        int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
        amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
        
        for (int amount : amounts) {
            actions.push_back({ 3, amount }); // raise with valid amounts
        }
    }
    // return legal actions for postflop
    return actions;
}

}

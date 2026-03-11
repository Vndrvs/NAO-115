#include "bet_abstraction.hpp"
#include "bet_maths.hpp"
#include "cfr/mccfr_state.hpp"
#include <vector>
#include <iostream>
/*
namespace BetAbstraction {


std::vector<AbstractAction> getLegalActions(const MCCFRState& state) {
    std::vector<AbstractAction> actions;
    actions.reserve(7); // max possible actions at any node
    // fold - call - raise (3 sizes postflop) - reraise - reraise - all-in
    
    // get the current stack size of the player
    const int effectiveStack = state.effectiveStack();
    
    const int effectiveAllIn = state.effectiveAllIn();
    
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
            
            if (state.raiseCount == 3) {
                actions.push_back({ 3, effectiveAllIn });
                return actions;
            }
            
            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                
                if (state.raiseCount == 0) {
                    // BB option after limp — use open sizes
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        amounts.push_back(computePreflopOpen(PREFLOP_OPEN_SIZES[i], state.bigBlind, effectiveAllIn));
                    }
                    amounts = filterAndDeduplicate(amounts, state.villainStreetBet + 1, effectiveAllIn);
                    
                    printf("getLegalActions P%d | raiseCount=%d | amounts after filter: ",
                            state.currentPlayer, state.raiseCount);
                        for (int a : amounts) printf("%d ", a);
                        printf("\n");
                } else {
                    // re-raise after both players equalized — use 2.5x
                    amounts.push_back(computePreflopReraise(PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveAllIn));
                    int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                    amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
                    printf("getLegalActions P%d | raiseCount=%d | amounts after filter: ",
                            state.currentPlayer, state.raiseCount);
                        for (int a : amounts) printf("%d ", a);
                        printf("\n");
                }
                
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
            } 
        } else {
            // we are facing a bet
            actions.push_back({ 0, 0 }); // fold
            actions.push_back({ 2, 0 }); // call
            
            if (state.effectiveAllIn() <= state.villainStreetBet) {
                return actions;
            }
            
            if (state.raiseCount == 3) {
                actions.push_back({ 3, effectiveAllIn });
                return actions;
            }
            
            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                
                if (state.raiseCount == 0) {
                    // SB first action, BB is live bet — use open sizes
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        amounts.push_back(computePreflopOpen(PREFLOP_OPEN_SIZES[i], state.bigBlind, effectiveAllIn));
                    }
                    amounts = filterAndDeduplicate(amounts, state.villainStreetBet + 1, effectiveAllIn);
                } else {
                    // facing actual raise — use 2.5x
                    amounts.push_back(computePreflopReraise(PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveAllIn));
                    int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                    amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
                }
                
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
                // we reached the max raise limit (only all-in remains)
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
                int additional = computePostflopAmount(POSTFLOP_BET_SIZES[i], state.potBase, 0, 0, effectiveStack);
                int totalContribution = additional + state.heroStreetBet;
                amounts.push_back(totalContribution);
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
            int additional = computePostflopAmount(
                                               raiseSizes[i],
                                               state.potBase,
                                               state.villainStreetBet,
                                               state.heroStreetBet,
                                               effectiveStack
                                               );
            int totalContribution = additional + state.heroStreetBet;
            amounts.push_back(totalContribution);
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


// bet logic: initial open bets possible: PREFLOP: 2x, 3x, 5x pot, POSTFLOP: 0.5x, 1x, 2x pot
// first reraise possible: (with valid geometry) 0.5x, 1x, 2x pot
// 3bet possible: 0.8x pot
// 4bet possible: 0.8x pot
// then only all-in or fold

// static const float PREFLOP_RAISE_SIZES[] = { 2.0f, 3.0f, 5.0f };
// static const float RERAISE_MULTIPLIER = 0.80f;
enum ActionCode {
    FOLD = 0,
    CHECK = 1,
    CALL = 2,
    RAISE = 3
};

std::vector<AbstractAction> getLegalActionsOriginal(const MCCFRState& state) {
    std::vector<AbstractAction> actions;
    actions.reserve(7); // are we sure this is true with current metrics?
    
    const int effectiveStack = state.effectiveStack();
    const int effectiveAllIn = state.effectiveAllIn();
    
    const int toCall = state.villainStreetBet - state.heroStreetBet;
    
    // are we sure this if is ok this way? i mean if anyone is all in, but we have less pot than the all-in, how can we call?
    // well we can call with our all-in amount but should we count that as all-in or call?
    if (state.anyAllIn() && toCall > 0) {
        actions.push_back({ FOLD, 0 });
        actions.push_back({ CALL, 0 });
        return actions;
    }
    
    // PREFLOP
    /*
     4 situations:
    SB first action (raiseCount=0, toCall=50) — always faces BB's live bet. Actions: fold, call, raise.
    BB option after SB limp (raiseCount=0, toCall=0) — SB called, BB can check or raise. Actions: check, raise.
    Facing a raise (raiseCount≥1, toCall>0) — someone raised. Actions: fold, call, raise (if raiseCount < MAX_RAISES).
    After equalizing at raiseCount≥1 — this shouldn't exist preflop. After any raise gets called, street transitions. So toCall=0 with raiseCount≥1 preflop only happens at the BB option after a limp, which is case 2.
   
    if (state.isPreflop()) {
        
        // no bet faced
        if (toCall == 0) {
            actions.push_back({ 1, 0 }); // check
            
            if (state.raiseCount == 3) {
                actions.push_back({ 3, effectiveAllIn });
                return actions;
            }
            
            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                
                if (state.raiseCount == 0) {
                    // BB option after limp — use open sizes
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        amounts.push_back(computePreflopOpen(PREFLOP_OPEN_SIZES[i], state.bigBlind, effectiveAllIn));
                    }
                    amounts = filterAndDeduplicate(amounts, state.villainStreetBet + 1, effectiveAllIn);
                    
                    printf("getLegalActions P%d | raiseCount=%d | amounts after filter: ",
                            state.currentPlayer, state.raiseCount);
                        for (int a : amounts) printf("%d ", a);
                        printf("\n");
                } else {
                    // re-raise after both players equalized — use 2.5x
                    amounts.push_back(computePreflopReraise(PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveAllIn));
                    int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                    amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
                    printf("getLegalActions P%d | raiseCount=%d | amounts after filter: ",
                            state.currentPlayer, state.raiseCount);
                        for (int a : amounts) printf("%d ", a);
                        printf("\n");
                }
                
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
            }
        } else {
            // we are facing a bet
            actions.push_back({ 0, 0 }); // fold
            actions.push_back({ 2, 0 }); // call
            
            if (state.effectiveAllIn() <= state.villainStreetBet) {
                return actions;
            }
            
            if (state.raiseCount == 3) {
                actions.push_back({ 3, effectiveAllIn });
                return actions;
            }
            
            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                
                if (state.raiseCount == 0) {
                    // SB first action, BB is live bet — use open sizes
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        amounts.push_back(computePreflopOpen(PREFLOP_OPEN_SIZES[i], state.bigBlind, effectiveAllIn));
                    }
                    amounts = filterAndDeduplicate(amounts, state.villainStreetBet + 1, effectiveAllIn);
                } else {
                    // facing actual raise — use 2.5x
                    amounts.push_back(computePreflopReraise(PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveAllIn));
                    int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                    amounts = filterAndDeduplicate(amounts, minRaise, effectiveAllIn);
                }
                
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
                // we reached the max raise limit (only all-in remains)
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
                int additional = computePostflopAmount(POSTFLOP_BET_SIZES[i], state.potBase, 0, 0, effectiveStack);
                int totalContribution = additional + state.heroStreetBet;
                amounts.push_back(totalContribution);
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
            int additional = computePostflopAmount(
                                               raiseSizes[i],
                                               state.potBase,
                                               state.villainStreetBet,
                                               state.heroStreetBet,
                                               effectiveStack
                                               );
            int totalContribution = additional + state.heroStreetBet;
            amounts.push_back(totalContribution);
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
 */

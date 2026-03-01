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
static const int   PREFLOP_OPEN_COUNT   = 2;

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
static const int   POSTFLOP_BET_COUNT    = 3;

/*
Post-flop raise sizes as fractions of totalPot() using call-first geometry.
 Used when facing a bet (raiseCount 1 -> 2).
*/
static const float POSTFLOP_RAISE_SIZES[] = { 0.75f, 1.50f };
static const int   POSTFLOP_RAISE_COUNT = 2;

/*
Post-flop 3-bet sizes as fractions of totalPot() using call-first geometry.
 Used when facing a raise (raiseCount 2 -> 3).
*/
static const float POSTFLOP_3BET_SIZES[] = { 1.00f };
static const int   POSTFLOP_3BET_COUNT = 1;

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

    const int effectiveStack = state.currentStack();
    const int totalPot = state.totalPot();

    // Special case: Someone is all-in
    // if either player has no chips left, no betting is possible
    // only fold or call remaining
    if (state.anyAllIn() && state.facingBet) {
        actions.push_back({ 0, 0 }); // fold, amount 0
        actions.push_back({ 2, 0 }); // call, amount 0
        return actions;
    }
    // PREFLOP
    if (state.isPreflop()) {
        // no raise incoming
        if (!state.facingBet) {
            if (state.currentPlayer == 0) {
                // BB option after SB limp — check or raise, NO fold
                actions.push_back({ 1, 0 }); // check
                // raise options
                if (state.raiseCount < MAX_RAISES) {
                    std::vector<int> amounts;
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        // get preflop open amounts
                        int amount = computePreflopOpen(PREFLOP_OPEN_SIZES[i], state.bigBlind, state.currentStack());
                        amounts.push_back(amount);
                    }
                    // validate amounts
                    int minRaise = computeMinRaise(state.bigBlind, 0);
                    amounts = filterAndDeduplicate(amounts, minRaise, state.currentStack());
                    
                    for (int amount : amounts) {
                        actions.push_back({ 3, amount });
                    }
                // we reached the max raise limit
                } else {
                    // we can only do all-in
                    actions.push_back({ 3, state.currentStack() });
                }
            // SB route
            } else {
                // SB first action — fold, call, raise (NO check)
                actions.push_back({ 0, 0 }); // fold
                actions.push_back({ 2, 0 }); // call
                // raise options
                if (state.raiseCount < MAX_RAISES) {
                    std::vector<int> amounts;
                    for (int i = 0; i < PREFLOP_OPEN_COUNT; i++) {
                        // get preflop open amounts
                        int amount = computePreflopOpen(PREFLOP_OPEN_SIZES[i], state.bigBlind, state.currentStack());
                        amounts.push_back(amount);
                    }
                    int minRaise = computeMinRaise(state.bigBlind, 0);
                    // validate amounts
                    amounts = filterAndDeduplicate(amounts, minRaise, state.currentStack());
                    for (int amount : amounts) {
                        actions.push_back({ 3, amount });
                    }
                // we reached the max raise limit
                } else {
                    // we can only do all-in
                    actions.push_back({ 3, state.currentStack() });
                }
            }
        } else {
            // facing a preflop raise — fold, call, reraise
            actions.push_back({ 0, 0 }); // fold
            actions.push_back({ 2, 0 }); // call

            if (state.raiseCount < MAX_RAISES) {
                std::vector<int> amounts;
                // 2.5x reraise on previousRaiseTotal
                int amount = computePreflopReraise(PREFLOP_RERAISE_MULTIPLIER, state.previousRaiseTotal, effectiveStack);
                amounts.push_back(amount);

                // calculate the amount of minimum valid raise
                int minRaise = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
                // validate amounts
                amounts = filterAndDeduplicate(amounts, minRaise, effectiveStack);
                for (int amount : amounts) {
                    actions.push_back({ 3, amount });
                }
                
            // we reached the max raise limit
            } else {
                // we can only do all-in
                actions.push_back({ 3, effectiveStack });
            }
        }

        return actions;
    }

    // POST-FLOP (Flop, Turn, River)
    if (!state.facingBet) {
        // no bet facing — check or bet
        actions.push_back({ 1, 0 }); // check
        // bet route
        if (state.raiseCount < MAX_RAISES) {
            // select bet sizes based on raise level
            // raiseCount 0: initial bet sizes (33%, 75%, 150%)
            // any deeper level shouldn't reach here (facingBet would be true)
            const float* betSizes = POSTFLOP_BET_SIZES;
            int betCount = POSTFLOP_BET_COUNT;

            std::vector<int> amounts;
            for (int i = 0; i < betCount; i++) {
                int amount = computePostflopAmount(betSizes[i], totalPot, 0, 0, effectiveStack);
                amounts.push_back(amount);
            }

            int minRaise = computeMinRaise(
                state.previousRaiseTotal > 0 ? state.previousRaiseTotal : 0,
                state.betBeforeRaise
            );

            amounts = filterAndDeduplicate(amounts, 1, effectiveStack);
            for (int amount : amounts) {
                actions.push_back({ 3, amount });
            }
            // we reached the max raise limit
        } else {
            // raise cap — all-in only
            actions.push_back({ 3, effectiveStack });
        }
    } else {
        // facing a bet — fold, call, raise
        actions.push_back({ 0, 0 }); // fold
        actions.push_back({ 2, 0 }); // call

        if (state.raiseCount < MAX_RAISES) {
            // select raise sizes based on current raise level
            const float* raiseSizes;
            int raiseCount;

            if (state.raiseCount == 1) {
                // facing initial bet → raise sizes: 75%, 150%
                raiseSizes = POSTFLOP_RAISE_SIZES;
                raiseCount = POSTFLOP_RAISE_COUNT;
            } else if (state.raiseCount == 2) {
                // facing a raise → 3-bet sizes: 100%
                raiseSizes = POSTFLOP_3BET_SIZES;
                raiseCount = POSTFLOP_3BET_COUNT;
            } else {
                // facing a 3-bet (raiseCount == 3) → 4-bet: all-in only
                // handled below by filterAndDeduplicate always appending all-in
                raiseSizes = nullptr;
                raiseCount = 0;
            }

            std::vector<int> amounts;
            for (int i = 0; i < raiseCount; i++) {
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

            amounts = filterAndDeduplicate(amounts, minRaise, effectiveStack);

            for (int amount : amounts) {
                actions.push_back({ 3, amount });
            }

        } else {
            // raise cap hit — all-in only
            actions.push_back({ 3, effectiveStack });
        }
    }

    return actions;
}

}

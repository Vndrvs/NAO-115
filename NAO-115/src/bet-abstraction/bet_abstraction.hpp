#pragma once

#include <vector>
#include <cstdint>
#include "cfr/mccfr_state.hpp"


/*
Given an MCCFRState, returns the list of legal abstract actions at this node.
- called at every single node during MCCFR traversal — it is on the
- hottest code path in the simulation, speed matters a lot here
 
Action convention:
 0 = fold
 1 = check
 2 = call
 3 = bet/raise (amount > 0)

Amount convention:
 fold/check/call: amount = 0
 bet/raise: amount = total chips committed this street after this action (not the additional chips, the total)
*/

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

struct AbstractAction {
    uint8_t type;      // 0=fold, 1=check, 2=call, 3=bet/raise
    int32_t amount;    // 0 for fold/check/call, total bet amount for bet/raise
};

/*
Returns all legal abstract actions for the current player at this node.
 
Handles:
 - Preflop opens (2BB, 3BB, all-in)
 - Preflop reraises (2.5x multiplier)
 - Post-flop bets (33%, 75%, 150%, all-in)
 - Post-flop raises (75%, 150%, all-in)
 - Post-flop 3-bets (100%, all-in)
 - Post-flop 4-bets (all-in only)
 - All-in situations (fold/call only)
 - Raise cap limiting (max raiseCount = 4)
 - Minimum raise filtering
 - Removal of duplicates in case of collapsed sizes
 */
std::vector<AbstractAction> getLegalActions(const MCCFRState& state);

}

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

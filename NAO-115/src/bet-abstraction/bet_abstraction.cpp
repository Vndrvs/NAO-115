#include "bet_abstraction.hpp"
#include "core/action.hpp"
#include "core/state.hpp"

std::vector<AbstractAction> getLegalActions(const AbstractionContext& context) {
    
    std::vector<AbstractAction> legalActions;
    // if preflop:
    if (context.street == Game::Street::Preflop) {
        // if villain didn't bet
        if (context.facingBet == false) {
            
        }
            
    } else {
        // if villain didn't bet
        if (context.facingBet == false) {
            
        }
    }
    
    
    // if postflop:
    
}
/*
 if preflop:
 if not facingBet:
 opens: 2BB, 3BB, all-in + call + fold
 if facingBet:
 if raiseCount < 4:
 reraises via computePreflopReraise
 fold + call + all-in
 
 if postflop:
 if not facingBet:
 check
 if raiseCount < 4:
 bets via computePostflopAmount
 all-in
 if facingBet:
 fold + call
 if raiseCount < 4:
 raises via computePostflopAmount
 all-in
 */

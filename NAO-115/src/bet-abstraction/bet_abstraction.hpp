#pragma once
#include "core/action.hpp"
#include "core/state.hpp"
#include <vector>

namespace BetMaths {

struct AbstractionContext {
    Game::Street street;
    
    int pot;                 // total pot before hero action
    
    int heroStreetBet;       // hero chips committed on this street
    int villainStreetBet;    // villain chips committed on this street
    int streetLastBetTo;     // max(heroStreetBet, villainStreetBet)
    
    int heroStack;
    int villainStack;
    int effectiveStack;
    
    int raiseCount;          // raises so far on this street
    bool facingBet;          // villainStreetBet > heroStreetBet
    
    int bigBlind;            // preflop only
    
    int previousRaiseTotal;  // last raise-to amount (street)
    int betBeforeRaise;      // street bet before last raise
};

struct AbstractAction {
    Game::ActionType type;    // fold, call, check, bet
    int amount;         // 0 for fold/check/call
};

std::vector<AbstractAction> getLegalActions(const AbstractionContext& ctx);
}

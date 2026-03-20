#pragma once

#include <cstdint>
#include "cfr/mccfr_state.hpp"
#include "bet_utils.hpp"

namespace BetAbstraction {

// action convention: 0 = fold, 1 = check, 2 = call, 3 = bet/raise (amount > 0)
struct AbstractAction {
    uint8_t type;
    int32_t amount;
};

// enum for readability
enum ActionType : uint8_t {
    FOLD = 0,
    CHECK = 1,
    CALL = 2,
    RAISE = 3  // used for both Bets and Raises
};

// trying to use stack allocated list to avoid vectors in hot path c:
struct ActionList {
    AbstractAction actions[6]; // one decision point - max 6 choices for Nao
    uint8_t count = 0;
    
    // append actions
    inline void add(uint8_t type, int32_t amount) {
        if (count >= 6) {
            printf("OVERFLOW: tried to add action %d amount %d at count %d\n",
                   type, amount, count);
            std::abort();
        }
        actions[count++] = {type, amount};
    }
};

ActionList getLegalActions(const MCCFRState& state);

}

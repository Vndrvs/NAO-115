#include "cfr/mccfr_state.hpp"
#include "bet-abstraction/bet_sequence.hpp"
#include "cfr/game_engine.hpp"

#include <cstdio>
#include <cstdint>
#include <stack>
#include <map>
#include <tuple>

int main() {
    
    int nodesVisited = 0;
    
    // key: (street, currentPlayer, raiseCount, toCall>0)
    std::map<std::tuple<int,int,int,int>, int> seenCounts;
    int mismatches = 0;
    
    MCCFRState root{};
    root.bigBlind            = 100;
    root.street              = 0;
    root.raiseCount          = 0;
    root.currentPlayer       = 1;
    root.isTerminal          = false;
    root.foldedPlayer        = -1;
    root.potBase             = 0;
    root.heroStreetBet       = 50;
    root.villainStreetBet    = 100;
    root.heroStack           = 19950;
    root.villainStack        = 19900;
    root.previousRaiseTotal  = 100;
    root.betBeforeRaise      = 0;
    root.historyHash         = 0;
    root.bucketId            = 0;
    root.streetHasCheck      = false;
    root.player0Contribution = 100;
    root.player1Contribution = 50;
    
    std::stack<MCCFRState> stk;
    stk.push(root);
    
    while (!stk.empty()) {
        MCCFRState state = stk.top();
        stk.pop();
        
        if (state.isTerminal) continue;
        
        nodesVisited++;
        
        auto actions = BetAbstraction::getLegalActions(state);
        int toCallFlag = (state.villainStreetBet - state.heroStreetBet) > 0 ? 1 : 0;
        auto key = std::make_tuple(
            (int)state.street,
            (int)state.currentPlayer,
            (int)state.raiseCount,
            toCallFlag
        );

        auto it = seenCounts.find(key);
        if (it == seenCounts.end()) {
            seenCounts[key] = actions.count;
        } else if (it->second != actions.count) {
            printf("MISMATCH: street=%d player=%d raiseCount=%d toCall=%d | "
                   "first seen with %d actions, now got %d\n",
                   (int)state.street, (int)state.currentPlayer,
                   (int)state.raiseCount, toCallFlag,
                   it->second, (int)actions.count);
            mismatches++;
        }

        for (int i = 0; i < actions.count; ++i)
            stk.push(GameEngine::applyAction(state, actions.actions[i]));
    }
    
    printf("Total nodes visited: %d\n", nodesVisited);

    if (mismatches == 0)
        printf("OK — no action count mismatches found\n");
    else
        printf("TOTAL MISMATCHES: %d\n", mismatches);
    
    return mismatches > 0 ? 1 : 0;
}

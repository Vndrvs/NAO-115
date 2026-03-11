#include "mccfr_state.hpp"
#include "bet-abstraction/bet_abstraction.hpp"
#include "game_engine.hpp"

#include <cstdio>
#include <cstdint>
#include <array>
#include <stack>


// =========================================================================
// BUCKET COUNTS PER STREET
// =========================================================================

static constexpr uint64_t BUCKETS[4] = { 169, 1000, 1000, 1000 };

// =========================================================================
// TREE WALK
// =========================================================================

struct TreeStats {
    uint64_t decisionNodes[2][4] = {};
};


static void walk(const MCCFRState& initialState, TreeStats& stats) {
    std::stack<MCCFRState> stk;
    stk.push(initialState);
    int nodeIndex = 0;

    while (!stk.empty()) {
        MCCFRState state = stk.top();
        stk.pop();

        if (state.isTerminal) continue;
        if (state.street > 0) continue;

        nodeIndex++;
        stats.decisionNodes[state.currentPlayer][state.street]++;

        auto actions = BetAbstraction::getLegalActions(state);

        printf("\n--- Node %d ---\n", nodeIndex);
        printf("  player         : %d (%s)\n", state.currentPlayer, state.currentPlayer == 0 ? "BB" : "SB");
        printf("  street         : %d\n", state.street);
        printf("  raiseCount     : %d\n", state.raiseCount);
        printf("  heroStreetBet  : %d\n", state.heroStreetBet);
        printf("  villainStreetBet: %d\n", state.villainStreetBet);
        printf("  toCall         : %d\n", state.villainStreetBet - state.heroStreetBet);
        printf("  heroStack      : %d\n", state.heroStack);
        printf("  villainStack   : %d\n", state.villainStack);
        printf("  potBase        : %d\n", state.potBase);
        printf("  totalPot       : %d\n", state.totalPot());
        printf("  effectiveStack : %d\n", state.effectiveStack());
        printf("  effectiveAllIn : %d\n", state.effectiveAllIn());
        printf("  streetHasCheck : %d\n", state.streetHasCheck);
        printf("  previousRaise  : %d\n", state.previousRaiseTotal);
        printf("  betBeforeRaise : %d\n", state.betBeforeRaise);
        printf("  p0Contribution : %d\n", state.player0Contribution);
        printf("  p1Contribution : %d\n", state.player1Contribution);
        printf("  legal actions  : %zu\n", actions.size());
        for (size_t i = 0; i < actions.size(); i++) {
            const char* typeName = "";
            switch (actions[i].type) {
                case 0: typeName = "fold";  break;
                case 1: typeName = "check"; break;
                case 2: typeName = "call";  break;
                case 3: typeName = "bet/raise"; break;
            }
            printf("    [%zu] type=%d (%s) amount=%d\n", i, actions[i].type, typeName, actions[i].amount);
        }

        for (const auto& action : actions)
            stk.push(GameEngine::applyAction(state, action));
    }

    printf("\n=== Preflop summary: P0=%llu P1=%llu total=%llu ===\n",
           (unsigned long long)stats.decisionNodes[0][0],
           (unsigned long long)stats.decisionNodes[1][0],
           (unsigned long long)(stats.decisionNodes[0][0] + stats.decisionNodes[1][0]));
}

// =========================================================================
// MAIN
// =========================================================================

int main() {
    MCCFRState state;
    
    state.currentPlayer = 1;
    state.street              = 0;
    state.raiseCount          = 0;
    state.isTerminal          = false;
    state.foldedPlayer        = -1;
    state.potBase             = 0;
    state.heroStreetBet       = 50;
    state.villainStreetBet    = 100;
    state.heroStack           = 19950;
    state.villainStack        = 19900;
    state.previousRaiseTotal  = 100;
    state.betBeforeRaise      = 0;
    state.bigBlind            = 100;
    state.historyHash         = 0;
    state.bucketId            = 0;
    state.streetHasCheck      = false;
    state.player0Contribution = 100;
    state.player1Contribution = 50;

    TreeStats stats;

    const char* streets[4] = { "Preflop", "Flop", "Turn", "River" };

    uint64_t totalNodes    = 0;
    uint64_t totalInfosets = 0;
    
    walk(state, stats);

    for (int s = 0; s < 4; ++s)
        for (int p = 0; p < 2; ++p)
            if (stats.decisionNodes[p][s] > 0)
                printf("street %d player %d: %llu nodes\n", s, p,
                       (unsigned long long)stats.decisionNodes[p][s]);
    
    printf("\n%-10s  %12s  %12s  %12s  %14s\n",
           "Street", "P0 nodes", "P1 nodes", "Total nodes", "Infosets");
    printf("%-10s  %12s  %12s  %12s  %14s\n",
           "------", "--------", "--------", "-----------", "--------");

    for (int s = 0; s < 4; ++s) {
        uint64_t p0    = stats.decisionNodes[0][s];
        uint64_t p1    = stats.decisionNodes[1][s];
        uint64_t nodes = p0 + p1;
        uint64_t info  = nodes * BUCKETS[s];
        totalNodes    += nodes;
        totalInfosets += info;
        printf("%-10s  %12llu  %12llu  %12llu  %14llu\n",
               streets[s],
               (unsigned long long)p0, (unsigned long long)p1,
               (unsigned long long)nodes, (unsigned long long)info);
    }

    double rawMB  = totalInfosets * 64.0 / (1024.0 * 1024.0);
    double withOH = rawMB * 1.25;

    printf("\nTotal decision nodes : %llu\n",  (unsigned long long)totalNodes);
    printf("Total infosets       : %llu\n",    (unsigned long long)totalInfosets);
    printf("Raw Infoset memory   : %.1f MB\n", rawMB);
    printf("With hash overhead   : %.1f MB\n", withOH);

    return 0;
}

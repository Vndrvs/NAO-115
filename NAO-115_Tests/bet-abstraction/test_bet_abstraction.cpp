#include <gtest/gtest.h>
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/hand_abstraction.hpp"
#include "bet-abstraction/bet_abstraction.hpp"
#include "bet-abstraction/bet_maths.hpp"
#include "cfr/mccfr_state.hpp"
#include "eval/evaluator.hpp"
#include "eval/tables.hpp"
#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <algorithm>
#include <string>

using namespace BetAbstraction;

class BetAbstractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Eval::initialize();
    }
};

// bet maths modules

TEST_F(BetAbstractionTest, PostflopAmount_InitialBet_33pct) {
    // potBase=1000, 33% bet = 330
    int result = computePostflopAmount(0.33f, 1000, 0, 0, 20000);
    EXPECT_EQ(result, 330);
}

TEST_F(BetAbstractionTest, PostflopAmount_InitialBet_75pct) {
    int result = computePostflopAmount(0.75f, 1000, 0, 0, 20000);
    EXPECT_EQ(result, 750);
}

TEST_F(BetAbstractionTest, PostflopAmount_RaiseGeometry) {
    // potBase=500, villain bet=200, hero has 0 in
    // callAmount = 200, potAfterCall = 500 + 200 + 200 = 900
    // 75% of 900 = 675, totalBet = 0 + 200 + 675 = 875
    int result = computePostflopAmount(0.75f, 500, 200, 0, 20000);
    EXPECT_EQ(result, 875);
}

TEST_F(BetAbstractionTest, PostflopAmount_RaiseGeometry_HeroHasChipsIn) {
    // potBase=500, villain raised to 600, hero had 200 in
    // callAmount = 600 - 200 = 400
    // potAfterCall = 500 + 600 + 400 = 1500
    // 75% of 1500 = 1125, totalBet = 200 + 400 + 1125 = 1725
    int result = computePostflopAmount(0.75f, 500, 600, 200, 20000);
    EXPECT_EQ(result, 1725);
}

TEST_F(BetAbstractionTest, PostflopAmount_CappedAtStack) {
    // result would exceed stack
    int result = computePostflopAmount(1.50f, 1000, 0, 0, 500);
    EXPECT_EQ(result, 500);
}

TEST_F(BetAbstractionTest, MinRaise_Basic) {
    // villain bets 200 (betBeforeRaise=0, previousRaiseTotal=200)
    // increment = 200, minReraise = 400
    int result = computeMinRaise(200, 0);
    EXPECT_EQ(result, 400);
}

TEST_F(BetAbstractionTest, MinRaise_AfterRaise) {
    // villain raised to 600, bet before was 200
    // increment = 400, minReraise = 1000
    int result = computeMinRaise(600, 200);
    EXPECT_EQ(result, 1000);
}

TEST_F(BetAbstractionTest, FilterDeduplicate_RemovesBelowMin) {
    std::vector<int> amounts = { 100, 300, 500 };
    auto result = filterAndDeduplicate(amounts, 250, 20000);
    // 100 removed, 300 and 500 kept, all-in appended
    EXPECT_EQ(result[0], 300);
    EXPECT_EQ(result[1], 500);
    EXPECT_EQ(result.back(), 20000);
}

TEST_F(BetAbstractionTest, FilterDeduplicate_CollapsesToAllIn) {
    // 150% and all-in both collapse to 800
    std::vector<int> amounts = { 330, 900, 1500 };
    auto result = filterAndDeduplicate(amounts, 1, 800);
    EXPECT_EQ(result[0], 330);
    EXPECT_EQ(result[1], 800); // 900 and 1500 both capped to 800
    EXPECT_EQ(result.size(), 2);
}

TEST_F(BetAbstractionTest, FilterDeduplicate_AllFilteredAllInStillPresent) {

    std::vector<int> amounts = { 100, 200, 900, 1800, 1800 };
    
    auto result = filterAndDeduplicate(amounts, 1000, 20000);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[1], 20000);
}

TEST_F(BetAbstractionTest, MCCFRState_TotalPot) {
    MCCFRState state{};
    state.potBase = 500;
    state.heroStreetBet = 100;
    state.villainStreetBet = 200;
    EXPECT_EQ(state.totalPot(), 800);
}

TEST_F(BetAbstractionTest, MCCFRState_EffectiveAllIn) {
    MCCFRState state{};
    state.heroStack = 19000;
    state.heroStreetBet = 1000;
    state.villainStack = 20000;
    state.villainStreetBet = 0;
    EXPECT_EQ(state.effectiveAllIn(), 20000);
}

TEST_F(BetAbstractionTest, MCCFRState_EffectiveAllIn_Asymmetric) {
    MCCFRState state{};
    state.heroStack = 19000;
    state.heroStreetBet = 1000;
    state.villainStack = 14000;
    state.villainStreetBet = 1000;
    EXPECT_EQ(state.effectiveAllIn(), 15000);
}

TEST_F(BetAbstractionTest, PostflopAmount_HeroAlreadyAhead_NoNegativeCall) {
    // villainBet < heroCurrentBet
    // callAmount must be limited to 0
    int result = computePostflopAmount(0.5f, 1000, 200, 400, 20000);

    // callAmount = max(0, 200 - 400) = 0
    // potAfterCall = 1000 + 200 + 0 = 1200
    // raiseAmount = 600
    // totalBet = 400 + 0 + 600 = 1000
    EXPECT_EQ(result, 1000);
}

TEST_F(BetAbstractionTest, PostflopAmount_ZeroPot) {
    int result = computePostflopAmount(0.75f, 0, 0, 0, 20000);
    EXPECT_EQ(result, 0); // 75% of 0 is 0
}

TEST_F(BetAbstractionTest, PostflopAmount_ZeroFraction) {
    int result = computePostflopAmount(0.0f, 1000, 200, 0, 20000);

    // callAmount = 200
    // potAfterCall = 1000 + 200 + 200 = 1400
    // raiseAmount = 0
    // totalBet = 200
    EXPECT_EQ(result, 200);
}

TEST_F(BetAbstractionTest, PostflopAmount_ExactStackHit) {
    int result = computePostflopAmount(1.0f, 1000, 0, 0, 1000);
    EXPECT_EQ(result, 1000);
}

TEST_F(BetAbstractionTest, MinRaise_InvalidState_NonNegative) {
    int result = computeMinRaise(200, 300);
    // previousIncrement = -100
    // minRaise = 100
    EXPECT_GE(result, 0);
}

TEST_F(BetAbstractionTest, FilterDeduplicate_DoesNotDuplicateAllIn) {
    std::vector<int> amounts = { 500, 1000, 20000 };
    auto result = filterAndDeduplicate(amounts, 1, 20000);
    EXPECT_EQ(result.back(), 20000);
    EXPECT_EQ(std::count(result.begin(), result.end(), 20000), 1);
}

TEST_F(BetAbstractionTest, FilterDeduplicate_AllAboveStack) {
    std::vector<int> amounts = { 50000, 60000 };
    auto result = filterAndDeduplicate(amounts, 1, 20000);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 20000);
}

TEST_F(BetAbstractionTest, FilterDeduplicate_MinRaiseAboveStack) {
    std::vector<int> amounts = { 100, 200, 300 };
    auto result = filterAndDeduplicate(amounts, 50000, 20000);

    // all-in must exist
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 20000);
}

TEST_F(BetAbstractionTest, IsAllIn_Exact) {
    EXPECT_TRUE(isAllIn(20000, 20000));
}

TEST_F(BetAbstractionTest, IsAllIn_Over) {
    EXPECT_TRUE(isAllIn(25000, 20000));
}

TEST_F(BetAbstractionTest, IsAllIn_NotAllIn) {
    EXPECT_FALSE(isAllIn(19999, 20000));
}

TEST_F(BetAbstractionTest, FilterDeduplicate_Invariants) {
    std::vector<int> amounts = { 100, 1000, 50000, 1000, 10 };
    int minRaise = 200;
    int stack = 20000;

    auto result = filterAndDeduplicate(amounts, minRaise, stack);

    // sorted
    EXPECT_TRUE(std::is_sorted(result.begin(), result.end()));

    // unique
    EXPECT_EQ(result.size(),
              std::distance(result.begin(),
              std::unique(result.begin(), result.end())));

    // last is all-in
    EXPECT_EQ(result.back(), stack);

    // all valid
    for (int a : result) {
        EXPECT_TRUE(a >= minRaise || a == stack);
    }
}

// bet abstraction modules

// prints game status quo to console
std::string stateToString(const MCCFRState& s) {
    std::ostringstream ss;
    ss << "\n--- MCCFRState ---\n"
       << "  street:             " << (int)s.street << "\n"
       << "  raiseCount:         " << (int)s.raiseCount << "\n"
       << "  bigBlind:           " << s.bigBlind << "\n"
       << "  potBase:            " << s.potBase << "\n"
       << "  heroStreetBet:      " << s.heroStreetBet << "\n"
       << "  villainStreetBet:   " << s.villainStreetBet << "\n"
       << "  heroStack:          " << s.heroStack << "\n"
       << "  villainStack:       " << s.villainStack << "\n"
       << "  previousRaiseTotal: " << s.previousRaiseTotal << "\n"
       << "  betBeforeRaise:     " << s.betBeforeRaise << "\n"
       << "  toCall:             " << (s.villainStreetBet - s.heroStreetBet) << "\n"
       << "  effectiveStack:     " << s.effectiveStack() << "\n"
       << "  effectiveAllIn:     " << s.effectiveAllIn() << "\n"
       << "  anyAllIn:           " << s.anyAllIn() << "\n"
       << "------------------\n";
    return ss.str();
}

// prints action type and amount (if applicable) to console
std::string actionsToString(const std::vector<AbstractAction>& actions) {
    std::ostringstream ss;
    ss << "Actions (" << actions.size() << "):\n";
    for (const auto& a : actions) {
        ss << "  type=" << (int)a.type << " amount=" << a.amount << "\n";
    }
    return ss.str();
}

// Build an MCCFRState by simulating a sequence of actions
MCCFRState buildSimulationState(std::mt19937& rng) {
    MCCFRState state{}; // instance of MCCFR state object
    state.bigBlind = 100; // set big blind to 100 according to our training model
    state.potBase = 0;   // pot should be 0 on start
    // both hero and villain start the game with 20k chips (mirroring Slumbot's gameplay)
    state.heroStack = 20000;
    state.villainStack = 20000;
    // current street bet amounts, 0 on start
    state.heroStreetBet = 0;
    state.villainStreetBet = 0;
    // bet history amounts, 0 on start
    state.previousRaiseTotal = 0;
    state.betBeforeRaise = 0;
    state.raiseCount = 0;   // counter to track raise counts and verify caps
    state.foldedPlayer = -1; // did anyone fold? (-1: not, 0-1: players)
    state.isTerminal = false;   // did the game end?

    // randomizes the simulated street in question
    std::uniform_int_distribution<int> streetDist(0, 3);
    state.street = streetDist(rng);
    
    std::uniform_int_distribution<int> randomGen(0, 1);
    bool isHeroSmallBlind = (randomGen(rng) == 0);
    
    // preflop blind status
    if (state.street == 0) {
        if (isHeroSmallBlind) {
            state.heroStreetBet = 50; // hero is small blind
            state.villainStreetBet = 100; // villain is big blind
            state.heroStack -= 50; // deduct the sb from hero's stack
            state.villainStack -= 100; // deduct the bb from villain's stack
        } else {
            state.villainStreetBet = 50; // villain is small blind
            state.heroStreetBet = 100; // hero is big blind
            state.villainStack -= 50; // deduct the sb from villain's stack
            state.heroStack -= 100; // deduct the bb from hero's stack
        }
    }
    
    if (state.street > 0) { // randomized postflop pot size
        std::uniform_int_distribution<int> potDist(0, 5000);
        state.potBase = potDist(rng) * 2;
        // deduct the pot's halves from the available stacks
        int eachAlreadyContributed = state.potBase / 2;
        state.heroStack -= eachAlreadyContributed;
        state.villainStack -= eachAlreadyContributed;
    }
    
    // simulate 0-3 raises randomly
    std::uniform_int_distribution<int> numRaisesDist(0, 3);
    // range inside of which raises are randomized
    int numRaises = numRaisesDist(rng);

    // on each raise action
    for (int r = 0; r < numRaises; r++) {
        // aggressor is villain on even raises, hero on odd
        // villain always raises and hero calls to match after each raise
        
        assert(state.heroStreetBet == state.villainStreetBet && "street bets should be equal at start of each raise iteration");
        int currentMax = state.heroStreetBet; // amount of chips already contributed on this street
        int pot = state.potBase + state.heroStreetBet + state.villainStreetBet;
        
        int totalBet;
        
        // pick random raise size according to our rules
        if (state.street == 0) {
            if (state.raiseCount == 0) {
                // preflop open options - 2 & 3 BB
                std::uniform_int_distribution<int> sizeDist(0, PREFLOP_OPEN_COUNT - 1);
                totalBet = static_cast<int>(PREFLOP_OPEN_SIZES[sizeDist(rng)] * state.bigBlind);
            } else {
                // preflop can only reraise by 2.5x
                totalBet = static_cast<int>(PREFLOP_RERAISE_MULTIPLIER * state.previousRaiseTotal);
            }
        } else {
            // postflop: pick appropriate size
            const float* sizes;
            int count;
            if (state.raiseCount == 0) {
                sizes = POSTFLOP_BET_SIZES;
                count = POSTFLOP_BET_COUNT;
            } else if (state.raiseCount == 1) {
                sizes = POSTFLOP_RAISE_SIZES;
                count = POSTFLOP_RAISE_COUNT;
            } else {
                sizes = POSTFLOP_3BET_SIZES;
                count = POSTFLOP_3BET_COUNT;
            }
            
            std::uniform_int_distribution<int> sizeDist(0, count - 1);
            totalBet = currentMax + static_cast<int>(pot * sizes[sizeDist(rng)]);
        }
        
        int minimumBet;

        if (state.previousRaiseTotal > 0) {
            minimumBet = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
        } else {
            minimumBet = currentMax + state.bigBlind;
        }

        totalBet = std::max(totalBet, minimumBet);
        bool villainActs = (r % 2 == 0); // alternating the one who raises
 
        // even raise No. in order, villain acts
        if (villainActs) {
            // verify whether the raise or bet amount is affordable using villain's stack
            int villainAllIn = state.villainStack + state.villainStreetBet;
            totalBet = std::min(totalBet, villainAllIn); // cap the bet at villain's all-in amount

            if (totalBet <= currentMax) {
                break; // if raise is higher than villain all-in amount, early exit
            }
            // record street bet amount before current raise happens (used in computing raise amount)
            state.betBeforeRaise = state.villainStreetBet;
            // calculate how many new chips go into the pot
            int additional = totalBet - state.villainStreetBet;
            // deduct these chips from villain's stack
            state.villainStack -= additional;
            // update street contribution with the newly raised amount
            state.villainStreetBet = totalBet;
            // record this raise total for the next player's minimum raise calculation
            state.previousRaiseTotal = totalBet;
        }
        // odd raise No. in order, hero acts, variables and logic all match
        else {
            int heroAllIn = state.heroStack + state.heroStreetBet;
            totalBet = std::min(totalBet, heroAllIn);

            if (totalBet <= currentMax) {
                break;
            }

            state.betBeforeRaise = state.heroStreetBet;
            int additional = totalBet - state.heroStreetBet;
            state.heroStack -= additional;
            state.heroStreetBet = totalBet;
            state.previousRaiseTotal = totalBet;
        }

        state.raiseCount++; // increment the raise counter

        if (state.heroStack <= 0 || state.villainStack <= 0) {
            break; // if the stack of any player runs out, the game is over, exit
        }

        if (villainActs) { // hero calls to keep state consistent for next raise
            int heroAdditional = state.villainStreetBet - state.heroStreetBet;
            if (heroAdditional > state.heroStack) {
                break;
            }
            
            state.heroStack -= heroAdditional;
            state.heroStreetBet = state.villainStreetBet;
        } else { // after hero raises, villain calls
            int villainAdditional = state.heroStreetBet - state.villainStreetBet;
            if (villainAdditional > state.villainStack) {
                break;
            }
            
            state.villainStack -= villainAdditional;
            state.villainStreetBet = state.heroStreetBet;
        }
    }
    

    // let's simulate a scenario where the last action is a raise to be called
    
    bool isThereUncalledRaise = (randomGen(rng) == 0);
    bool heroMakesUncalledRaise = (randomGen(rng) == 0);
    
    if (isThereUncalledRaise && state.raiseCount < 4) {
        if (heroMakesUncalledRaise && state.heroStack > 0) {
            int heroAllIn = state.heroStack + state.heroStreetBet;
            int minBet;
            
            if (state.previousRaiseTotal > 0) {
                minBet = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
            } else {
                int currentMax = std::max(state.heroStreetBet, state.villainStreetBet);
                minBet = currentMax + state.bigBlind;
            }
            
            if (minBet < heroAllIn) {
                std::uniform_int_distribution<int> betDist(minBet, heroAllIn);
                int totalBet = betDist(rng);
                state.betBeforeRaise = state.heroStreetBet;
                int additional = totalBet - state.heroStreetBet;
                state.heroStack -= additional;
                state.heroStreetBet = totalBet;
                state.previousRaiseTotal = totalBet;
                state.raiseCount++;
            }
        } else if (state.villainStack > 0) {
            int villainAllIn = state.villainStack + state.villainStreetBet;
            int minBet;
            
            if (state.previousRaiseTotal > 0) {
                minBet = computeMinRaise(state.previousRaiseTotal, state.betBeforeRaise);
            } else {
                minBet = state.villainStreetBet + state.bigBlind;
            }
            
            if (minBet < villainAllIn) {
                std::uniform_int_distribution<int> betDist(minBet, villainAllIn);
                int totalBet = betDist(rng);
                state.betBeforeRaise = state.villainStreetBet;
                int additional = totalBet - state.villainStreetBet;
                state.villainStack -= additional;
                state.villainStreetBet = totalBet;
                state.previousRaiseTotal = totalBet;
                state.raiseCount++;
            }
        }
    }

    return state;
}

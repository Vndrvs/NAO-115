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
#include <iostream>

using namespace BetAbstraction;

class BetAbstractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Eval::initialize();
    }
};

TEST_F(BetAbstractionTest, PostflopAmount_InitialBet_33pct) {
    // simple initial bet, no villain bet
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
    // everything below minRaise, only all-in survives
    std::vector<int> amounts = { 100, 200, 300 };
    auto result = filterAndDeduplicate(amounts, 1000, 20000);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 20000);
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
    // heroAllIn = 20000, villainAllIn = 20000
    EXPECT_EQ(state.effectiveAllIn(), 20000);
}

TEST_F(BetAbstractionTest, MCCFRState_EffectiveAllIn_Asymmetric) {
    MCCFRState state{};
    state.heroStack = 19000;
    state.heroStreetBet = 1000;
    state.villainStack = 14000;
    state.villainStreetBet = 1000;
    // heroAllIn = 20000, villainAllIn = 15000
    EXPECT_EQ(state.effectiveAllIn(), 15000);
}

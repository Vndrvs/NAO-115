#include <gtest/gtest.h>
#include "eval/tables.hpp"
#include "eval/evaluator.hpp"
#include <random>
#include <algorithm>
#include <numeric>

using namespace Eval;

class EvaluatorHeavyTest : public ::testing::Test {
protected:
    void SetUp() override {
        Eval::initialize();
    }
};

TEST_F(EvaluatorHeavyTest, VerifyAgainstOfficialRankTable) {
    // 1. Royal Flush (Rank 1) -> 7463 - 1 = 7462
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Ks"), Eval::parseCard("Qs"), Eval::parseCard("Js"), Eval::parseCard("Ts")}), 7462);

    // 2. Best Quads: Aces with King kicker (Rank 11) -> 7463 - 11 = 7452
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Ah"), Eval::parseCard("Ad"), Eval::parseCard("Ac"), Eval::parseCard("Ks")}), 7452);

    // 3. Worst Quads: Aces with 2 kicker (Rank 22) -> 7463 - 22 = 7441
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Ah"), Eval::parseCard("Ad"), Eval::parseCard("Ac"), Eval::parseCard("2s")}), 7441);

    // 4. Strongest Full House: Aces full of Kings (Rank 167) -> 7463 - 167 = 7296
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Ah"), Eval::parseCard("Ad"), Eval::parseCard("Ks"), Eval::parseCard("Kh")}), 7296);

    // 5. Strongest Flush: A-K-Q-J-9 (Rank 323) -> 7463 - 323 = 7140
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Ks"), Eval::parseCard("Qs"), Eval::parseCard("Js"), Eval::parseCard("9s")}), 7140);

    // 6. Strongest Straight: A-K-Q-J-T (Rank 1600) -> 7463 - 1600 = 5863
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Kh"), Eval::parseCard("Qd"), Eval::parseCard("Js"), Eval::parseCard("Tc")}), 5863);

    // 7. Specific Two Pair: J-J-T-T-2 (Rank 2841) -> 7463 - 2841 = 4622
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("Js"), Eval::parseCard("Jh"), Eval::parseCard("Ts"), Eval::parseCard("Th"), Eval::parseCard("2s")}), 4622);
    
    // 8. Absolute Worst Hand: High Card 7-5-4-3-2 (Rank 7462) -> 7463 - 7462 = 1
    EXPECT_EQ(Eval::evaluate5({Eval::parseCard("7s"), Eval::parseCard("5h"), Eval::parseCard("4d"), Eval::parseCard("3c"), Eval::parseCard("2s")}), 1);
}

TEST_F(EvaluatorHeavyTest, RanksCompared) {
    // second has pair
    int a1 = Eval::evaluate5({Eval::parseCard("7s"), Eval::parseCard("5h"), Eval::parseCard("4d"), Eval::parseCard("3c"), Eval::parseCard("2s")});
    int a2 = Eval::evaluate5({Eval::parseCard("7s"), Eval::parseCard("5h"), Eval::parseCard("4d"), Eval::parseCard("3c"), Eval::parseCard("3s")});
    EXPECT_TRUE(a1 < a2);
    
    // first is flush, second has pair
    int b1 = Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("Ts"), Eval::parseCard("4s"), Eval::parseCard("3s"), Eval::parseCard("2s")});
    int b2 = Eval::evaluate5({Eval::parseCard("7s"), Eval::parseCard("5h"), Eval::parseCard("4d"), Eval::parseCard("3c"), Eval::parseCard("3s")});
    EXPECT_TRUE(b1 > b2);
    
    // first wheel str vs second 2 pairs
    int c1 = Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("2h"), Eval::parseCard("4s"), Eval::parseCard("3d"), Eval::parseCard("5s")});
    int c2 = Eval::evaluate5({Eval::parseCard("7s"), Eval::parseCard("7h"), Eval::parseCard("4d"), Eval::parseCard("3c"), Eval::parseCard("3s")});
    EXPECT_TRUE(b1 > b2);
    
    // first is wheel flush, second is ace high flush
    int d1 = Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("2s"), Eval::parseCard("4s"), Eval::parseCard("3s"), Eval::parseCard("5s")});
    int d2 = Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("7s"), Eval::parseCard("4s"), Eval::parseCard("3s"), Eval::parseCard("2s")});
    EXPECT_TRUE(b1 > b2);
    
    // first is wheel flush, second is 2-6 flush
    int e1 = Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("2s"), Eval::parseCard("4s"), Eval::parseCard("3s"), Eval::parseCard("5s")});
    int e2 = Eval::evaluate5({Eval::parseCard("2s"), Eval::parseCard("3s"), Eval::parseCard("4s"), Eval::parseCard("5s"), Eval::parseCard("6s")});
    EXPECT_TRUE(e1 < e2);
}

TEST_F(EvaluatorHeavyTest, StraightFlushHierarchy) {
    int wheelFlush = Eval::evaluate5({Eval::parseCard("As"), Eval::parseCard("2s"),
                                      Eval::parseCard("3s"), Eval::parseCard("4s"), Eval::parseCard("5s")});
    int sixFlush = Eval::evaluate5({Eval::parseCard("2s"), Eval::parseCard("3s"),
                                    Eval::parseCard("4s"), Eval::parseCard("5s"), Eval::parseCard("6s")});
    
    EXPECT_EQ(wheelFlush, 7453);
    EXPECT_EQ(sixFlush, 7454);
    EXPECT_LT(wheelFlush, sixFlush);
}

// test early exit firing

TEST_F(EvaluatorHeavyTest, TriggerNoPairsShortcut) {
    // 7 unique ranks: A, K, Q, J, 9, 8, 7.
    // Mixed suits so it's NOT a flush.
    // It should find the A-K-Q-J-9 high card hand.
    int score = Eval::eval_7(
        Eval::deck[Eval::parseCard("As")], Eval::deck[Eval::parseCard("Kh")],
        Eval::deck[Eval::parseCard("Qd")], Eval::deck[Eval::parseCard("Jc")],
        Eval::deck[Eval::parseCard("9s")], Eval::deck[Eval::parseCard("8h")],
        Eval::deck[Eval::parseCard("7d")]
    );

    // Ace-high (A-K-Q-J-9) is Cactus Kev Rank 6186.
    // Your Score: 7463 - 6186 = 1277.
    EXPECT_EQ(score, 1277);
}

/*
TEST_F(EvaluatorHeavyTest, TriggerFlushEarlyExit) {
    // 6 Spades, no straight. Should trigger the high-to-low walk.
    // Hand: As, Ks, Qs, Js, 9s, 2s (spades) + 3d (diamond)
    int score = Eval::eval_7(
        Eval::deck[Eval::parseCard("As")], Eval::deck[Eval::parseCard("Ks")],
        Eval::deck[Eval::parseCard("Qs")], Eval::deck[Eval::parseCard("Js")],
        Eval::deck[Eval::parseCard("9s")], Eval::deck[Eval::parseCard("2s")],
        Eval::deck[Eval::parseCard("3d")]
    );
    
    // Ace-high flush with 9 kicker (Kev Rank 323).
    // Your Score: 7463 - 323 = 7140
    EXPECT_EQ(score, 7140);
}
 

TEST_F(EvaluatorHeavyTest, TriggerStraightFlushEarlyExit) {
    // 7-high Straight Flush in Hearts + 2 junk cards
    // Hand: 7h, 6h, 5h, 4h, 3h (Hearts) + Kc, Qd
    int score = Eval::eval_7(
        Eval::deck[Eval::parseCard("7h")], Eval::deck[Eval::parseCard("6h")],
        Eval::deck[Eval::parseCard("5h")], Eval::deck[Eval::parseCard("4h")],
        Eval::deck[Eval::parseCard("3h")], Eval::deck[Eval::parseCard("Kc")],
        Eval::deck[Eval::parseCard("Qd")]
    );
    
    // 7-high Straight Flush (Kev Rank 8).
    // Your Score: 7463 - 8 = 7455
    EXPECT_EQ(score, 7455);
}



TEST_F(EvaluatorHeavyTest, TriggerWheelStraightFlushEarlyExit) {
    // Ace-low Straight Flush in Diamonds
    // Hand: Ad, 2d, 3d, 4d, 5d + Ks, Qh
    int score = Eval::eval_7(
        Eval::deck[Eval::parseCard("Ad")], Eval::deck[Eval::parseCard("2d")],
        Eval::deck[Eval::parseCard("3d")], Eval::deck[Eval::parseCard("4d")],
        Eval::deck[Eval::parseCard("5d")], Eval::deck[Eval::parseCard("Ks")],
        Eval::deck[Eval::parseCard("Qh")]
    );
    
    // 5-high Straight Flush (Kev Rank 10).
    // Your Score: 7463 - 10 = 7453
    EXPECT_EQ(score, 7453);
}



TEST_F(EvaluatorHeavyTest, MonteCarloDifferentialStressTest) {
    std::mt19937 rng(1337);
    std::vector<int> deck_indices(52);
    std::iota(deck_indices.begin(), deck_indices.end(), 0);

    const int SAMPLES = 1000000;
    
    for (int i = 0; i < SAMPLES; ++i) {
        std::shuffle(deck_indices.begin(), deck_indices.end(), rng);

        int c[7];
        for(int j=0; j<7; j++) c[j] = Eval::deck[deck_indices[j]];

        int brute_force_max = 0;
        for (int p = 0; p < 21; p++) {
            int score = Eval::eval_5(
                c[Eval::permutations[p][0]], c[Eval::permutations[p][1]],
                c[Eval::permutations[p][2]], c[Eval::permutations[p][3]],
                c[Eval::permutations[p][4]]
            );
            if (score > brute_force_max) brute_force_max = score;
        }

        int optimized_result = Eval::eval_7(c[0], c[1], c[2], c[3], c[4], c[5], c[6]);

        ASSERT_EQ(optimized_result, brute_force_max)
            << "Optimized eval_7 failed to find the absolute best 5-card combination on iteration " << i;
            
        int sub_5_score = Eval::eval_5(c[0], c[1], c[2], c[3], c[4]);
        ASSERT_GE(optimized_result, sub_5_score);
    }
}
*/

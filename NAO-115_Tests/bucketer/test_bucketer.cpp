#include <gtest/gtest.h>
#include "bucketing/bucketer.hpp"
#include "bucketing/hand_abstraction.hpp"
#include "eval/evaluator.hpp"
#include "eval/tables.hpp"
#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>

using namespace Bucketer;

/*
TEST(ApplyZTest, BasicNormalizationSinglePoint)
{
    std::vector<std::array<float,4>> data = {
        {2.f, 4.f, 6.f, 8.f}
    };

    std::vector<std::array<float,2>> stats = {
        {1.f, 1.f},
        {2.f, 2.f},
        {3.f, 3.f},
        {4.f, 4.f}
    };

    apply_z(data, stats);

    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(data[0][i], 1.f);
}

TEST(ApplyZTest, MultiplePoints)
{
    std::vector<std::array<float,4>> data = {
        {2.f, 4.f, 6.f, 8.f},
        {3.f, 6.f, 9.f, 12.f}
    };

    std::vector<std::array<float,2>> stats = {
        {1.f, 1.f},
        {2.f, 2.f},
        {3.f, 3.f},
        {4.f, 4.f}
    };

    apply_z(data, stats);

    EXPECT_FLOAT_EQ(data[0][0], 1.f);
    EXPECT_FLOAT_EQ(data[1][0], 2.f);

    EXPECT_FLOAT_EQ(data[0][1], 1.f);
    EXPECT_FLOAT_EQ(data[1][1], 2.f);

    EXPECT_FLOAT_EQ(data[0][2], 1.f);
    EXPECT_FLOAT_EQ(data[1][2], 2.f);

    EXPECT_FLOAT_EQ(data[0][3], 1.f);
    EXPECT_FLOAT_EQ(data[1][3], 2.f);
}

TEST(ApplyZTest, ZeroStdDevDoesNotModify)
{
    std::vector<std::array<float,4>> data = {
        {5.f, 6.f, 7.f, 8.f}
    };

    std::vector<std::array<float,2>> stats = {
        {1.f, 0.f},
        {2.f, 1e-10f},
        {3.f, 1.f},
        {4.f, 2.f}
    };

    apply_z(data, stats);

    EXPECT_FLOAT_EQ(data[0][0], 5.f);
    EXPECT_FLOAT_EQ(data[0][1], 6.f);

    EXPECT_FLOAT_EQ(data[0][2], (7.f - 3.f) / 1.f);
    EXPECT_FLOAT_EQ(data[0][3], (8.f - 4.f) / 2.f);
}

TEST(BucketerStats, CalculatesMeanAndStdDev) {
    std::vector<std::array<float,4>> data = {
        {1.f, 2.f, 3.f, 4.f},
        {5.f, 6.f, 7.f, 8.f},
        {9.f, 10.f, 11.f, 12.f}
    };

    std::vector<std::array<float,2>> stats;
    compute_stats(data, stats);

    EXPECT_FLOAT_EQ(stats[0][0], 5.f);
    EXPECT_FLOAT_EQ(stats[1][0], 6.f);

    float expected_sd = std::sqrt(32.0f / 3.0f);
    
    EXPECT_NEAR(stats[0][1], expected_sd, 1e-5);
    EXPECT_NEAR(stats[1][1], expected_sd, 1e-5);
    EXPECT_NEAR(stats[2][1], expected_sd, 1e-5);
    EXPECT_NEAR(stats[3][1], expected_sd, 1e-5);
}

TEST(BucketerStats, ZeroVariance) {

    std::vector<std::array<float,4>> data = {
        {10.f, 20.f, 30.f, 40.f},
        {10.f, 20.f, 30.f, 40.f},
        {10.f, 20.f, 30.f, 40.f}
    };

    std::vector<std::array<float,2>> stats;
    compute_stats(data, stats);

    EXPECT_FLOAT_EQ(stats[0][0], 10.f);
    
    EXPECT_FLOAT_EQ(stats[0][1], 0.f);
    EXPECT_FLOAT_EQ(stats[1][1], 0.f);
}

// test kmeans
TEST(KMeansTest, SingleClusterReturnsMean)
{
    std::vector<std::array<float,4>> data = {
        {1.f, 2.f, 3.f, 4.f},
        {5.f, 6.f, 7.f, 8.f}
    };

    auto centroids = kmeans(data, 1, 10);

    ASSERT_EQ(centroids.size(), 1);

    EXPECT_FLOAT_EQ(centroids[0][0], 3.f);
    EXPECT_FLOAT_EQ(centroids[0][1], 4.f);
    EXPECT_FLOAT_EQ(centroids[0][2], 5.f);
    EXPECT_FLOAT_EQ(centroids[0][3], 6.f);
}

TEST(KMeansTest, TwoClearlySeparatedClusters)
{
    std::vector<std::array<float,4>> data = {
        {0.f, 0.f, 0.f, 0.f},
        {0.1f, 0.f, 0.f, 0.f},
        {10.f, 10.f, 10.f, 10.f},
        {10.1f, 10.f, 10.f, 10.f}
    };

    auto centroids = kmeans(data, 2, 20);

    ASSERT_EQ(centroids.size(), 2);

    bool found_small = false;
    bool found_large = false;

    for (const auto& c : centroids) {
        float dist_to_small = 0.f;
        float dist_to_large = 0.f;

        for (int i = 0; i < 4; ++i) {
            dist_to_small += (c[i] - 0.05f) * (c[i] - 0.05f);
            dist_to_large += (c[i] - 10.05f) * (c[i] - 10.05f);
        }

        if (dist_to_small < 1e-2f)
            found_small = true;
        if (dist_to_large < 1e-2f)
            found_large = true;
    }

    EXPECT_TRUE(found_small);
    EXPECT_TRUE(found_large);
}

TEST(KMeansTest, IdenticalPoints)
{
    std::vector<std::array<float,4>> data(5, {3.f, 3.f, 3.f, 3.f});

    auto centroids = kmeans(data, 1, 5);

    ASSERT_EQ(centroids.size(), 1);

    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(centroids[0][i], 3.f);
}

TEST(KMeansTest, DeterministicAcrossRuns)
{
    std::vector<std::array<float,4>> data = {
        {0.f,0.f,0.f,0.f},
        {0.1f,0.f,0.f,0.f},
        {10.f,10.f,10.f,10.f},
        {10.1f,10.f,10.f,10.f}
    };

    auto c1 = kmeans(data, 2, 20);
    auto c2 = kmeans(data, 2, 20);

    ASSERT_EQ(c1.size(), c2.size());

    for (size_t i = 0; i < c1.size(); ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_FLOAT_EQ(c1[i][j], c2[i][j]);
}

TEST(KMeansTest, KEqualsN)
{
    std::vector<std::array<float,4>> data = {
        {1.f,2.f,3.f,4.f},
        {5.f,6.f,7.f,8.f},
        {9.f,10.f,11.f,12.f}
    };

    auto centroids = kmeans(data, 3, 10);

    ASSERT_EQ(centroids.size(), 3);

    for (const auto& point : data) {
        bool found = false;

        for (const auto& c : centroids) {
            bool equal = true;
            for (int i = 0; i < 4; ++i)
                if (std::fabs(point[i] - c[i]) > 1e-6f)
                    equal = false;

            if (equal)
                found = true;
        }

        EXPECT_TRUE(found);
    }
}

TEST(KMeansTest, ThrowsWhenKExceedsN)
{
    std::vector<std::array<float,4>> data = {
        {0.f,0.f,0.f,0.f},
        {10.f,10.f,10.f,10.f}
    };

    EXPECT_THROW(kmeans(data, 3, 15), std::invalid_argument);
}

TEST(KMeansTest, ZeroIterationsReturnsInitializedCentroids)
{
    std::vector<std::array<float,4>> data = {
        {1.f,1.f,1.f,1.f},
        {2.f,2.f,2.f,2.f}
    };

    auto centroids = kmeans(data, 1, 0);

    ASSERT_EQ(centroids.size(), 1);

    bool matches = false;
    for (const auto& p : data) {
        bool equal = true;
        for (int i = 0; i < 4; ++i)
            if (std::fabs(p[i] - centroids[0][i]) > 1e-6f)
                equal = false;

        if (equal)
            matches = true;
    }

    EXPECT_TRUE(matches);
}
*/

class BucketerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Eval::initialize();
        for (int i = 0; i < 3; ++i) {
            centroids[i].clear();
            feature_stats[i].clear();
        }
    }

    void assertNoDegeneration(const Eval::FlopFeatures& f) {
        EXPECT_FALSE(std::isnan(f.ehs));
        EXPECT_FALSE(std::isnan(f.asymmetry));
        EXPECT_FALSE(std::isnan(f.volatility));
        EXPECT_FALSE(std::isnan(f.exclusivity));

        EXPECT_FALSE(std::isinf(f.ehs));
        EXPECT_FALSE(std::isinf(f.asymmetry));
        EXPECT_FALSE(std::isinf(f.volatility));
        EXPECT_FALSE(std::isinf(f.exclusivity));
    }

    void assertBounded(const Eval::FlopFeatures& f) {
        EXPECT_GE(f.ehs,         0.f);
        EXPECT_LE(f.ehs,         1.f);
        EXPECT_GE(f.volatility,  0.f);
        EXPECT_LE(f.volatility,  1.f);
        EXPECT_GE(f.exclusivity, 0.f);
        EXPECT_LE(f.exclusivity, 1.f);
        EXPECT_GE(f.asymmetry,  -1.f);
        EXPECT_LE(f.asymmetry,   1.f);
    }
};

TEST_F(BucketerTest, Flop_NutFlushDraw_DryBoard) {
    // Hand: A♥ 2♥  Board: 7♥ 3♥ K♠
    // Correctly a flush draw — only 3 hearts on board+hand combined
    // A♥ blocks nut flush for villain, high exclusivity expected
    std::array<int, 2> hand  = {49, 1};  // A♥ 2♥
    std::array<int, 3> board = {25, 9, 44}; // 7♥ 3♥ K♠

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    assertNoDegeneration(f);
    assertBounded(f);

    EXPECT_GT(f.ehs,         0.40f);
    EXPECT_GT(f.asymmetry,   0.0f);  // upside dominates
    EXPECT_GT(f.volatility,  0.15f);
    EXPECT_GT(f.exclusivity, 0.40f);
}

TEST_F(BucketerTest, Flop_OESD_Behind_LowExclusivity) {
    // Hand: 8♠ 7♥  Board: 9♦ 6♣ 2♥
    // OESD — straights complete with 5 or T, shared with villain range
    // HS is actually high (8-high beats unpaired hands), Npot significant
    std::array<int, 2> hand  = {24, 21};
    std::array<int, 3> board = {26, 19, 5};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    assertNoDegeneration(f);
    assertBounded(f);

    EXPECT_GT(f.ehs,         0.60f);
    EXPECT_LT(f.asymmetry,   0.0f);  // currently strong, faces significant Npot
    EXPECT_GT(f.volatility,  0.08f);
    EXPECT_LT(f.exclusivity, 0.40f); // straight outs shared with villain
}

TEST_F(BucketerTest, Flop_CompleteAir_MonotoneBoard) {
    // Hand: 9♠ 4♥  Board: A♠ K♠ Q♠
    // No spade, broadway draws available, EHS pulled up by straight outs
    // High exclusivity because runouts where hero wins tend to be exclusive
    std::array<int, 2> hand  = {28, 13};
    std::array<int, 3> board = {48, 44, 40};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    assertNoDegeneration(f);
    assertBounded(f);

    EXPECT_LT(f.ehs,         0.60f);
    EXPECT_GT(f.asymmetry,   0.0f);
    EXPECT_GT(f.exclusivity, 0.60f);
}
TEST_F(BucketerTest, Flop_BottomTwoPair_WetBoard) {
    // Hand: 3♠ 2♥  Board: 3♥ 2♦ J♠
    // Bottom two pair on a board with straight and trips danger
    // Expect: decent HS but EHS pulled down by high Npot,
    //         negative asymmetry, high volatility
    std::array<int, 2> hand  = {4, 1};
    std::array<int, 3> board = {5, 2, 36};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    assertNoDegeneration(f);
    assertBounded(f);

    EXPECT_GT(f.ehs,        0.40f);
    EXPECT_LT(f.asymmetry,  0.0f);  // downside risk dominates
    EXPECT_GT(f.volatility, 0.10f);
}


TEST_F(BucketerTest, Flop_TopFullHouse_VeryStrong) {
    // Hand: J♠ J♥  Board: J♦ 7♠ 7♥
    // Flopped top full house — only quads 7s and runner-runner straight flush beat us
    // Expect: very high EHS, very low volatility, negative asymmetry
    std::array<int, 2> hand  = {36, 37};
    std::array<int, 3> board = {38, 24, 25};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    assertNoDegeneration(f);
    assertBounded(f);

    EXPECT_GT(f.ehs,        0.95f);
    EXPECT_LT(f.volatility, 0.08f);
    EXPECT_LT(f.asymmetry,  0.0f);
}

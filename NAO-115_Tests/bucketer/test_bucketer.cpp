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
};

TEST_F(BucketerTest, FlopFeatures_NutStraight_LowVolatility) {
    // Hand: 9♠ 8♠
    // Board: 7♦ 6♣ 5♥  → nut straight, very static
    std::array<int, 2> hand  = {30, 26};
    std::array<int, 3> board = {22, 18, 14};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    // Already crushing
    EXPECT_GT(f.ehs, 0.85f);

    // Little downside left
    EXPECT_LT(f.asymmetry, 0.1f);

    // Board is static → low volatility
    EXPECT_LT(f.volatility, 0.05f);

    // Even vs top range, should hold well
    EXPECT_GT(f.equityUnderPressure, 0.7f);
}

TEST_F(BucketerTest, FlopFeatures_MonsterDraw_HighUpside) {
    // Hand: A♠ K♠
    // Board: Q♠ J♦ 2♣  → nut straight + nut flush draw
    std::array<int, 2> hand  = {51, 47};
    std::array<int, 3> board = {43, 39, 2};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    // Not there yet
    EXPECT_GT(f.ehs, 0.35f);
    EXPECT_LT(f.ehs, 0.6f);

    // Huge upside
    EXPECT_GT(f.asymmetry, 0.3f);

    // Many turn/river realizations
    EXPECT_GT(f.volatility, 0.12f);

    // Under pressure, still decent due to nut potential
    EXPECT_GT(f.equityUnderPressure, 0.25f);
}

TEST_F(BucketerTest, FlopFeatures_WeakTopPair_LowEUP) {
    // Hand: A♣ 9♦
    // Board: A♥ K♠ Q♦  → top pair, terrible texture
    std::array<int, 2> hand  = {50, 33};
    std::array<int, 3> board = {48, 45, 42};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    // Ahead often now, but fragile
    EXPECT_GT(f.ehs, 0.4f);

    // Little upside, mostly downside
    EXPECT_LT(f.asymmetry, 0.0f);

    // Many bad runouts
    EXPECT_GT(f.volatility, 0.08f);

    // Against top range, this hand suffers
    EXPECT_LT(f.equityUnderPressure, 0.35f);
}

TEST_F(BucketerTest, FlopFeatures_Air_BadEverywhere) {
    // Hand: 7♣ 2♦
    // Board: A♠ K♦ Q♥
    std::array<int, 2> hand  = {19, 2};
    std::array<int, 3> board = {51, 45, 43};

    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);

    EXPECT_LT(f.ehs, 0.25f);
    EXPECT_LT(f.asymmetry, 0.0f);
    EXPECT_LT(f.equityUnderPressure, 0.15f);
}

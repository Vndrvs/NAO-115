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

TEST_F(BucketerTest, FlopFeaturesStrategicSanity) {
    // Indices for 9s, 8s, 7s, 6s, 2d
    // These indices MUST be converted to the evaluator's internal format
    std::array<int, 2> hand = {30, 26};
    std::array<int, 3> board = {22, 18, 2};
    
    // Ensure you are using the optimized function that handles deck lookups
    auto f = Eval::calculateFlopFeaturesTwoAhead(hand, board);
    
    // Strategic expectations for a massive draw:
    // 1. hs: Should be moderate (~0.45) because it hasn't hit yet
    EXPECT_NEAR(f.ehs, 0.45f, 0.15f);
    
    // 2. asymmetry: Should be positive (upside potential)
    EXPECT_GT(f.asymmetry, 0.2f);
    
    // 3. volatility: Should be high (many cards change the outcome)
    EXPECT_GT(f.volatility, 0.1f);
}

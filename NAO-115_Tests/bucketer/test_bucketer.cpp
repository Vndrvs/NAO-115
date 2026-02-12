#include <gtest/gtest.h>
#include "eval/bucketer.hpp"
#include <cmath>

using namespace Bucketer;

TEST(BucketerStats, CalculatesMeanAndStdDev) {
    std::vector<std::array<float,4>> data = {
        {1.f, 2.f, 3.f, 4.f},
        {5.f, 6.f, 7.f, 8.f},
        {9.f, 10.f, 11.f, 12.f}
    };

    std::vector<std::array<float,2>> stats;
    compute_stats(data, stats);

    // 1. Check Means (Index 0)
    EXPECT_FLOAT_EQ(stats[0][0], 5.f);
    EXPECT_FLOAT_EQ(stats[1][0], 6.f);

    // 2. Check Standard Deviations (Index 1)
    // We expect sqrt(32/3) ≈ 3.265986
    float expected_sd = std::sqrt(32.0f / 3.0f);
    
    EXPECT_NEAR(stats[0][1], expected_sd, 1e-5);
    EXPECT_NEAR(stats[1][1], expected_sd, 1e-5);
    EXPECT_NEAR(stats[2][1], expected_sd, 1e-5);
    EXPECT_NEAR(stats[3][1], expected_sd, 1e-5);
}

TEST(BucketerStats, ZeroVariance) {
    // If all points are identical, SD must be 0
    std::vector<std::array<float,4>> data = {
        {10.f, 20.f, 30.f, 40.f},
        {10.f, 20.f, 30.f, 40.f},
        {10.f, 20.f, 30.f, 40.f}
    };

    std::vector<std::array<float,2>> stats;
    compute_stats(data, stats);

    // Mean should be the number itself
    EXPECT_FLOAT_EQ(stats[0][0], 10.f);
    
    // Std Dev should be exactly 0
    EXPECT_FLOAT_EQ(stats[0][1], 0.f);
    EXPECT_FLOAT_EQ(stats[1][1], 0.f);
}

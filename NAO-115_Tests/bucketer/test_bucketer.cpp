#include <gtest/gtest.h>
#include "eval/bucketer.hpp"

using namespace Bucketer;

TEST(BucketerStats, SimpleData) {
    std::vector<std::array<float,4>> data = {
        {1.f, 2.f, 3.f, 4.f},
        {5.f, 6.f, 7.f, 8.f},
        {9.f, 10.f, 11.f, 12.f}
    };

    std::vector<std::array<float,2>> stats;
    compute_stats(data, stats);

    EXPECT_FLOAT_EQ(stats[0][0], 5.f);
    EXPECT_FLOAT_EQ(stats[1][0], 6.f);
    EXPECT_FLOAT_EQ(stats[2][0], 7.f);
    EXPECT_FLOAT_EQ(stats[3][0], 8.f);
}

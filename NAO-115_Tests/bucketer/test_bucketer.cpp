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

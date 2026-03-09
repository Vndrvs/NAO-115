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

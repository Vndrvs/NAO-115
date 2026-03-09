#include <gtest/gtest.h>
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/hand_abstraction.hpp"
#include "eval/evaluator.hpp"
#include "eval/tables.hpp"
#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>
#include <filesystem>

using namespace Bucketer;

class BucketerRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        Eval::initialize();
        Bucketer::initialize();
    }
};

#pragma once

#include <vector>
#include <array>
#include <cstdint>

namespace Eval {

/**
 * 4-float feature vector for flop abstraction using values:
 * E: Expected Hand Strength
 * E2: Hand Strength Square
 * PPot: Positive Potential
 * NPot: Negative Potential
 */

struct FlopFeatures {
    float e;
    float e2;
    float ppot;
    float npot;
};

// calculates the features

double calculateHandStrength(const std::array<int, 2>& hand,
                           const std::array<int, 3>& board);

double calculateFlopFeatures(const std::array<int, 2>& hand,
                           const std::array<int, 3>& board);

}

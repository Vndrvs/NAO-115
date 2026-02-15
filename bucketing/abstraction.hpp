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

struct BaseFeatures {
    float e;
    float e2;
    float ppot;
    float npot;
};

struct RiverFeatures {
    float eVsRandom;
    float eVsTop;
    float eVsMid;
    float eVsBot;
};

// calculates the features

float calculateFlopHandStrength(const std::array<int, 2>& hand,
                           const std::array<int, 3>& board);

float calculateTurnHandStrength(const std::array<int, 2>& hand,
                                const std::array<int, 4>& board);

BaseFeatures calculateFlopFeaturesTwoAhead(const std::array<int, 2>& hand,
                           const std::array<int, 3>& board);

BaseFeatures calculateFlopFeaturesFast(const std::array<int, 2>& hand,
                           const std::array<int, 3>& board);

BaseFeatures calculateTurnFeaturesFast(const std::array<int, 2>& hand,
                           const std::array<int, 4>& board);

RiverFeatures calculateRiverFeatures(const std::array<int, 2>& hand,
                                        const std::array<int, 5>& board);

}

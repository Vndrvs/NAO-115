#pragma once

namespace Eval {

/**
 * 4-float feature vector for flop abstraction using values:
 * E: Expected Hand Strength
 * E2: Hand Strength Square
 * PPot: Positive Potential
 * NPot: Negative Potential
 */

struct StrengthTransitions {
    float hs;
    float ppot;
    float npot;
    float volatility;
};

struct RiverFeatures {
    float eVsRandom;
    float eVsTop;
    float eVsMid;
    float eVsBot;
};

float calculateEHS(const std::array<int,2>& hand,
                   const std::array<int,3>& board);

float calculateEHS(const std::array<int,2>& hand,
                   const std::array<int,4>& board);

StrengthPotentials calculateFlopFeaturesFast(const std::array<int, 2>& hand,
                                       const std::array<int, 3>& board);

StrengthPotentials calculateTurnFeaturesFast(const std::array<int, 2>& hand,
                                       const std::array<int, 4>& board);

}

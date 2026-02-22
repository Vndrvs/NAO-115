#pragma once

namespace Eval {

/**
Feature vectors for abstraction using values:
- HS: Expected Hand Strength
- PPot: Positive Potential
- NPot: Negative Potential
- Volatility:
- Exclusivity:
 */

struct FlopFeatures {
    float hs;
    float ppot;
    float npot;
    float volatility;
};

struct TurnFeatures {
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

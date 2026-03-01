#pragma once
#include <array>

namespace Eval {

/**
Feature vectors for abstraction using values:
- HS: Expected Hand Strength
- Asymmetry
- Volatility
- EquityUnderPressure
 */

struct FlopFeatures {
    float ehs;
    float asymmetry;
    float nutPotential;
};

struct TurnFeatures {
    float ehs;
    float asymmetry;
    float nutPotential;
};

struct RiverFeatures {
    float equityTotal;
    float equityVsStrong;
    float equityVsWeak;
    float blockerIndex;
};

FlopFeatures calculateFlopFeaturesTwoAhead(const std::array<int, 2>& hand,
                                       const std::array<int, 3>& board);

TurnFeatures calculateTurnFeatures(const std::array<int, 2>& hand,
                                       const std::array<int, 4>& board);

RiverFeatures calculateRiverFeatures(const std::array<int, 2>& hand,
                                       const std::array<int, 5>& board);

}

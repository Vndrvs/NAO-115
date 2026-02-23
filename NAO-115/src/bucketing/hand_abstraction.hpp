#pragma once

namespace Eval {

/**
Feature vectors for abstraction using values:
- HS: Expected Hand Strength
- Asymmetry
- Volatility
- EquityUnderPressure
 */

struct FlopFeatures {
    float hs;
    float asymmetry;
    float volatility;
    float equityUnderPressure;
};

struct TurnFeatures {
    float hs;
    float asymmetry;
    float volatility;
    float equityUnderPressure;
};

struct RiverFeatures {
    float eVsRandom;
    float eVsTop;
    float eVsMid;
    float eVsBot;
};

FlopFeatures calculateFlopFeaturesTwoAhead(const std::array<int, 2>& hand,
                                       const std::array<int, 3>& board);

TurnFeatures calculateTurnFeatures(const std::array<int, 2>& hand,
                                       const std::array<int, 4>& board);

}

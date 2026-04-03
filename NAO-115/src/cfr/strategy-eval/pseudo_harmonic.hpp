#pragma once

/*
Pseudo-Harmonic action translation mapping technique proposed by Ganzfried & Sandholm (IJCAI 2013)
 -> research source: https://www.cs.cmu.edu/~sandholm/reverse%20mapping.ijcai13.pdf

When a strategy trained on abstraction A faces a bet from opponent B that is not present in A's action sequence options
 this maps x to a probability distribution over the two actions in A
 
 Formula (randomized version):
 f_A,B(x) = (B - x)(1 + A) / ((B - A)(1 + x))
 
 Where:
 lowerBet = largest abstract action <= x (pot fraction)
 higherBet = smallest abstract action >= x (pot fraction)
 villainBet = opponent's actual bet (pot fraction)
 probLower = probability of mapping to A
 1-f = probability of mapping to B

(all values must be expressed as pot fractions, not chip amounts)

 Edge cases:
 - x <= A_min: map to A_min with probability 1
 - x >= A_max: map to A_max with probability 1 (all-in)
 - A == B: map to A with probability 1 (x is in abstraction)
 */

#include <cstdint>
#include <array>
#include <cassert>

namespace PHM {

/*
Compute probability of mapping opponent bet x to lower bracket A.
 - values in pot fractions.
 - returns probability between 0-1
 */
inline float mappingProbability(float lowerBet, float higherBet, float realSize) {
    // special: exact match or bad interval
    if (lowerBet >= higherBet - 1e-6f) {
        return 1.0f;
    }

    // safety clamping (should already work anyways)
    if (realSize <= lowerBet) {
        return 1.0f;
    }
    
    if (realSize >= higherBet) {
        return 0.0f;
    }

    // pseudo-harmonic mapping formula
    float probLower = (higherBet - realSize) * (1.0f + lowerBet) / ((higherBet - lowerBet) * (1.0f + realSize));

    // PHM formula can produce slight FP overshoot (e.g. -1e-7, 1.0000001)
    if (probLower < 0.0f) {
        probLower = 0.0f;
    }
    
    if (probLower > 1.0f) {
        probLower = 1.0f;
    }

    return probLower;
}

/*
Given an opponent's bet in chips and the current pot, find the bracketing abstract actions and sample one using PHM function
 - returns index into abstractSizes of the chosen abstract action.
 */

inline int translateBet(const float* abstractBetSizes, int numberOfBetSizes, float villainBet,float potSize, float random01) {
    if (numberOfBetSizes <= 0) {
        return -1;
    }
    #ifndef NDEBUG
    for (int i = 1; i < numberOfBetSizes; ++i) {
        assert(abstractBetSizes[i - 1] <= abstractBetSizes[i]);
    }
    #endif
    
    // clamp random number to 0-1
    if (random01 < 0.0f) {
        random01 = 0.0f;
    }
    
    if (random01 >= 1.0f) {
        random01 = 0.999999f;
    }
    
    // invalid pot, fallback to largest size
    if (potSize <= 0.0f) {
        return numberOfBetSizes - 1;
    }

    float betFraction = villainBet / potSize;

    // find bracketing abstract actions
    int lowerIndex = -1;
    int higherIndex = -1;

    for (int i = 0; i < numberOfBetSizes; ++i) {
        if (abstractBetSizes[i] <= betFraction) {
            lowerIndex = i;
        }

        if (abstractBetSizes[i] >= betFraction && higherIndex == -1) {
            higherIndex = i;
        }
    }

    // if it's below abstraction, it's mapped to the smallest
    if (lowerIndex == -1) {
        return 0;
    }

    // if it's above abstraction, it's mapped to the largest
    if (higherIndex == -1) {
        return numberOfBetSizes - 1;
    }

    // if it's an exact match, randomness is not needed
    if (lowerIndex == higherIndex) {
        return lowerIndex;
    }

    // PHM: sampling between lower and upper bets
    float lowerBet = abstractBetSizes[lowerIndex];
    float higherBet = abstractBetSizes[higherIndex];
    float probLower = mappingProbability(lowerBet, higherBet, betFraction);

    if (random01 < probLower) {
        return lowerIndex;
    }
    
    return higherIndex;
}

}

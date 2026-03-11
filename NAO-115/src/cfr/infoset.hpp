// Defines Nao's Infoset struct — one entry in the MCCFR strategy table. (CFR+)
#pragma once

#include <cstdint>
#include <cstring>
 
static constexpr int MAX_ACTIONS = 6;

// as per my current cpp knowledge, adding manual padding would give false signal to the compiler
// therefore I trust alignas to do the trick
struct alignas(64) Infoset {

    // I'll zero initialize the whole arrays
    
    // Cumulative regret for each action (always >= 0, floored after every updateRegrets() call)
    float regretSum[MAX_ACTIONS] = {0.0f}; // 6 x 4 = 24 bytes

     // Cumulative weighted strategy for each action
    float strategySum[MAX_ACTIONS] = {0.0f}; // 6 x 4 = 24 bytes

    // Number of legal actions at this node (can be: 2 - MAX_ACTIONS)
    uint8_t numActions = 0; // 1 byte

    // Set number ofa ctions on first visit, must be called exactly once (n must be in: 2 - MAX_ACTIONS)
    void initialize(int n) {
        numActions = static_cast<uint8_t>(n);
    }

    // regret matching
    void getStrategy(float* out) const {
        float total = 0.0f;
        for (int i = 0; i < numActions; ++i) {
            total += regretSum[i];
        }

        if (total > 0.0f) {
            float inv = 1.0f / total;
            for (int i = 0; i < numActions; ++i) {
                out[i] = regretSum[i] * inv;
            }
        } else {
            float uniform = 1.0f / static_cast<float>(numActions);
            for (int i = 0; i < numActions; ++i) {
                out[i] = uniform;
            }
        }
    }

    // update methods
    void updateRegrets(const float* regrets) {
        for (int i = 0; i < numActions; ++i) {
            regretSum[i] += regrets[i];
            if (regretSum[i] < 0.0f) {
                regretSum[i] = 0.0f;
            }
        }
    }


    // Accumulate weighted reach-probability-scaled strategy
    void updateStrategy(float weight, float reachProbability, const float* strategy) {
        if (weight <= 0.0f) {
            return;
        }
        float w = weight * reachProbability;
        for (int i = 0; i < numActions; ++i) {
            strategySum[i] += w * strategy[i];
        }
    }

    // Final policy extration (called after training completes, not during traversal)
    void getAverageStrategy(float* out) const {
        float total = 0.0f;
        for (int i = 0; i < numActions; ++i) {
            total += strategySum[i];
        }

        if (total > 0.0f) {
            float inv = 1.0f / total;
            for (int i = 0; i < numActions; ++i) {
                out[i] = strategySum[i] * inv;
            }
        } else {
            float uniform = 1.0f / static_cast<float>(numActions);
            for (int i = 0; i < numActions; ++i) {
                out[i] = uniform;
            }
        }
    }
};

static_assert(sizeof(Infoset) == 64, "Infoset must be exactly 64 bytes");
static_assert(alignof(Infoset) == 64, "Infoset must be 64-byte aligned");

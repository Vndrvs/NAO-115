#pragma once
#include <cstdint>
#include <cstddef>

// the evaluator will send these values as a signal back to the BO after running
struct BOSignal {
    double ev_mean;
    double ev_variance_of_mean;
};


struct BOConfig {
    float flop[3];
    float turn[3];
    float river[3];
    // uint64_t node_budget; node budget will be forced and constant
    int threads; // should be context aware, auto
    int samples; // should be fixed
    uint64_t seed; // should be randomized
};

struct BOResult {
    double exploitability;
    double br_p0;
    double br_p1;
    size_t infosets;
    double training_sec;
    double eval_sec;
};

BOResult evaluateNaoPostflopBO_Test(const BOConfig& config);
void applyBOBetSizing(const BOConfig& cfg);

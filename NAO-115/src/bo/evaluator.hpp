#pragma once
#include <cstdint>
#include <cstddef>
#include <omp.h>

// NOTE: if you want to limit the number of threads used for training, reset the variable here!
// NAO discovers the number of training-available cores
inline int threads = omp_get_max_threads();

// set node budget variable here
constexpr uint64_t nodeBudget = 200000000;
// set duplicate hands amount run in heads up module here
constexpr uint64_t duplicateHands = 2000000;

// constant seeds to minimize training and evaluation noise
const uint64_t baseSeed = 42;
const uint64_t trainingSeed = baseSeed;
const uint64_t evaluatorSeed = baseSeed + 1337;

// the evaluator will send these values as a signal back to the BO after running
struct BOSignal {
    double ev_mean;
    double ev_variance_of_mean;
};

// the BO will propose a 9-dimension bet size vector, which will then be converted
// to this struct (only for code flow clarity)
struct BOParams {
    double flopBetSizes[3];
    double turnBetSizes[3];
    double riverBetSizes[3];
};

// the evaluator will also output a set of logged values that describe
// the specific evaluation
struct LoggedValues {
    uint32_t boIteration; // number of iteration inside BO process
    BOParams params; // bet sizes used in flop-turn-river evaluation runs
    // total number of MCCFR infosets in the game tree which was constructed
    // using the bet sizes proposed by the BO which were then passed to the bet sequence module
    uint64_t infosetsCount;
    uint64_t totalNodesTouched; // how many nodes were touched across the whole MCCFR run
    float averageRunPerNode; // totalNodesTouched / infosetCount - ratio number
    float avgAbsRegret;  // mean of the absolute values of all cumulative regrets across all nodes
    float maxAbsRegret; // single "worst" decision point in the entire game tree
    double mccfrTrainingSeconds; // how much time did it take to run the MCCFR on the game tree
    double evaluationSeconds; // time needed to evaluate the strategy with the heads up module
    uint64_t duplicateHands; // how many duplicate hands were used in the heads up simulation
    double stdDevHandEV; // standard deviation of the N hand outcomes
    double standardError; // stdDev / sqrt(n)
    double evP0; // mean EV of P0 in chips (evP0 = -evP1)
    double evP0_bb100; // industry standard: Big Blinds per 100 hands
    double cumulativeBestEV; // highest EV found up to the current evaluation iteration
    uint64_t trainerSeed; // seed used for MCCFR training
    uint64_t evaluatorSeed; // seed used for heads up evaluation run
};

// we will sort the bet size triplets coming from the BO with this function
inline void sort3(double& a, double& b, double& c) {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
}

#include <gtest/gtest.h>
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/hand_abstraction.hpp"
#include "eval/evaluator.hpp"
#include "eval/tables.hpp"
#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>

using namespace Bucketer;

class BucketerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Eval::initialize();
        for (int i = 0; i < 3; ++i) {
            centroids[i].clear();
            feature_stats[i].clear();
        }
    }
};

int main() {
    Eval::initialize();
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 51);
    
    constexpr int N = 100; // small enough to finish quickly
    
    // flop timing
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < N; ++i) {
        std::array<int,2> hand;
        std::array<int,3> board;
        // draw unique cards
        uint64_t used = 0;
        int fill = 0;
        while (fill < 5) {
            int c = dist(rng);
            if (!(used & (1ULL << c))) {
                used |= (1ULL << c);
                if (fill < 2) hand[fill] = c;
                else board[fill-2] = c;
                fill++;
            }
        }
        Eval::calculateFlopFeaturesTwoAhead(hand, board);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double per_sample = total_ms / N;
    
    std::cout << "Flop: " << N << " samples in " << total_ms << "ms" << std::endl;
    std::cout << "      " << per_sample << "ms per sample" << std::endl;
    std::cout << "      projected 50k samples: " << (per_sample * 50000 / 1000 / 60) << " minutes" << std::endl;
    
    return 0;
}

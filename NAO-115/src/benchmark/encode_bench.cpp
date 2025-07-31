#include "preflop_encoder.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <random>

using namespace Preflop;

int main() {
    // some sample hands we'll keep picking randomly
    std::vector<std::string> sampleHands = {
        "AsKs", "AhKd", "7c7d", "2c3c", "KdAh",
        "QsJs", "9h8h", "Tc9d", "4c4s", "5d6h"
    };

    // random engine to pick hands
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, static_cast<int>(sampleHands.size()) - 1);

    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();
    auto end = start;

    int totalProcessed = 0;

    // run for roughly one second
    do {
        // grab a random hand from the list
        std::string hand = sampleHands[dist(rng)];

        // convert it and if good, get its index
        auto encoded = convertHandFormat(hand);
        if (encoded) {
            volatile int index = handToIndex(*encoded);
            (void)index; // stop compiler from optimizing away
        }

        ++totalProcessed;
        end = Clock::now();

    } while (std::chrono::duration<double>(end - start).count() < 1.0);

    // total microseconds elapsed
    auto elapsed = std::chrono::duration<double, std::micro>(end - start).count();

    // avg time per hand in microseconds
    double avgMicroseconds = elapsed / totalProcessed;

    std::cout << "processed " << totalProcessed << " hands in one second\n";
    std::cout << "avg time per hand: " << avgMicroseconds << "μs\n";

    return 0;
}

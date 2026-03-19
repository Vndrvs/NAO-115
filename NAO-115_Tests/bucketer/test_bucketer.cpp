#include <iostream>
#include <vector>
#include <random>
#include "hand-bucketing/mapping_engine.hpp"
#include "hand-bucketing/bucketer.hpp"
#include "../include/bucket-lookups/lut_manager.hpp"
#include "../include/bucket-lookups/lut_indexer.hpp"
#include "eval/evaluator.hpp"

using namespace Bucketer;

void run_parity_test(int num_trials) {
    IsomorphismEngine isoEngine;
    isoEngine.initialize();
    
    // 1. Initialize original bucketer (loads centroids.dat)
    Bucketer::initialize();

    // 2. Load LUTs into RAM (Update paths to where your .lut files are)
    if (!load_luts("output/data/luts/flop_buckets.lut",
                   "output/data/luts/turn_buckets.lut")) {
        std::cerr << "Failed to load LUT files for parity test.\n";
        return;
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 51);

    int flop_mismatches = 0;
    int turn_mismatches = 0;

    std::cout << "Starting Parity Test (" << num_trials << " random hands)...\n";

    for (int i = 0; i < num_trials; ++i) {
        // --- FLOP TEST ---
        std::array<int, 2> hand_f;
        std::array<int, 3> board_f;
        
        // Use existing drawing logic from bucketer.cpp or inline here
        auto drawCards = [&](int count, std::vector<int>& out) {
            uint64_t used = 0;
            out.clear();
            while(out.size() < count) {
                int c = dist(rng);
                if (!(used & (1ULL << c))) {
                    used |= (1ULL << c);
                    out.push_back(c);
                }
            }
        };

        std::vector<int> cards;
        drawCards(5, cards);
        hand_f = {cards[0], cards[1]};
        board_f = {cards[2], cards[3], cards[4]};

        int dynamic_f = get_flop_bucket(hand_f, board_f);
        int lut_f     = lookup_bucket(isoEngine, hand_f.data(), board_f.data(), 3);

        if (dynamic_f != lut_f) flop_mismatches++;

        // --- TURN TEST ---
        drawCards(6, cards);
        std::array<int, 2> hand_t = {cards[0], cards[1]};
        std::array<int, 4> board_t = {cards[2], cards[3], cards[4], cards[5]};

        int dynamic_t = get_turn_bucket(hand_t, board_t);
        int lut_t     = lookup_bucket(isoEngine, hand_t.data(), board_t.data(), 4);

        if (dynamic_t != lut_t) turn_mismatches++;

        if (i % (num_trials / 10) == 0) std::cout << "Progress: " << i << " / " << num_trials << "...\n";
    }

    std::cout << "\n--- TEST RESULTS ---\n";
    std::cout << "Flop Mismatches: " << flop_mismatches << " / " << num_trials << "\n";
    std::cout << "Turn Mismatches: " << turn_mismatches << " / " << num_trials << "\n";

    if (flop_mismatches == 0 && turn_mismatches == 0) {
        std::cout << "SUCCESS: LUT and Dynamic Bucketer are in perfect parity.\n";
    } else {
        std::cout << "FAILURE: Mismatches detected. Check indexing/isomorphism logic.\n";
    }
}

int main() {
    Eval::initialize();
    run_parity_test(100000);
    return 0;
}

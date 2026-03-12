#include <gtest/gtest.h>
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/hand_abstraction.hpp"
#include "hand-bucketing/generate_luts.hpp"

#include "eval/evaluator.hpp"
#include "eval/tables.hpp"

#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>
#include <filesystem>
#include <limits>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cassert>
#include <atomic>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace Bucketer;

void testDynamicBucketer(IsomorphismEngine& isoEngine) {
    std::cout << "\n--- STEP 1: PRE-TESTING DYNAMIC BUCKETER ---\n";
    
    uint64_t test_idx = 500000;
    std::array<uint8_t, 5> waugh_cards = isoEngine.unindexFlop(test_idx);
    
    std::array<int, 2> hand = { static_cast<int>(waugh_cards[0]), static_cast<int>(waugh_cards[1]) };
    std::array<int, 3> board = { static_cast<int>(waugh_cards[2]), static_cast<int>(waugh_cards[3]), static_cast<int>(waugh_cards[4]) };
    
    std::cout << "Testing Waugh Index: " << test_idx << "\n";
    std::cout << "Hand (0-51 ints) : [" << hand[0] << ", " << hand[1] << "]\n";
    std::cout << "Board (0-51 ints): [" << board[0] << ", " << board[1] << ", " << board[2] << "]\n";
    
    uint16_t bucket_id = static_cast<uint16_t>(Bucketer::get_flop_bucket(hand, board));
    std::cout << "SUCCESS! Dynamically Assigned Flop Bucket: " << bucket_id << "\n";
}

void generateFlopLUT(IsomorphismEngine& isoEngine) {
    uint64_t max_idx = isoEngine.getFlopCombinations();
    std::cout << "\n--- STEP 2: GENERATING FLOP LUT (" << max_idx << " combinations) ---\n";
    
    std::atomic<int> processed_count(0);
    std::vector<uint16_t> lut(max_idx);
    
#pragma omp parallel for
    for (uint64_t i = 0; i < max_idx; ++i) {
        std::array<uint8_t, 5> waugh_cards = isoEngine.unindexFlop(i);
        
        std::array<int, 2> hand = { static_cast<int>(waugh_cards[0]), static_cast<int>(waugh_cards[1]) };
        std::array<int, 3> board = { static_cast<int>(waugh_cards[2]), static_cast<int>(waugh_cards[3]), static_cast<int>(waugh_cards[4]) };
        
        lut[i] = static_cast<uint16_t>(Bucketer::get_flop_bucket(hand, board));
        
        int current_count = ++processed_count;
        if (i % 1000 == 0) {
#pragma omp critical
            std::cout << "Processed " << i << " / " << max_idx << "\n";
        }
    }
    
    std::ofstream out("flop_buckets.lut", std::ios::binary);
    out.write(reinterpret_cast<const char*>(lut.data()), lut.size() * sizeof(uint16_t));
    out.close();
    std::cout << "SUCCESS: Saved 'flop_buckets.lut' to disk!\n";
}

void verifyLUT(IsomorphismEngine& isoEngine) {
    std::cout << "\n--- STEP 3: VERIFYING LUT MATCHES DYNAMIC CALCULATION ---\n";
    
    std::ifstream in("flop_buckets.lut", std::ios::binary);
    if (!in) {
        std::cout << "ERROR: LUT file not found!\n";
        return;
    }
    
    // Determine file size and load into memory
    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint16_t> lut(size / sizeof(uint16_t));
    in.read(reinterpret_cast<char*>(lut.data()), size);
    in.close();
    
    // Check a few edge cases and random indices
    std::vector<uint64_t> test_indices = {0, 100, 500000, 1000000, isoEngine.getFlopCombinations() - 1};
    
    bool all_passed = true;
    for (uint64_t idx : test_indices) {
        std::array<uint8_t, 5> waugh_cards = isoEngine.unindexFlop(idx);
        std::array<int, 2> hand = { static_cast<int>(waugh_cards[0]), static_cast<int>(waugh_cards[1]) };
        std::array<int, 3> board = { static_cast<int>(waugh_cards[2]), static_cast<int>(waugh_cards[3]), static_cast<int>(waugh_cards[4]) };
        
        uint16_t dynamic_bucket = static_cast<uint16_t>(Bucketer::get_flop_bucket(hand, board));
        uint16_t lut_bucket = lut[idx];
        
        if (dynamic_bucket != lut_bucket) {
            std::cout << "MISMATCH at index " << idx << "! Dynamic: " << dynamic_bucket << " | LUT: " << lut_bucket << "\n";
            all_passed = false;
        } else {
            std::cout << "Index " << idx << " verified perfectly. (Bucket: " << lut_bucket << ")\n";
        }
    }
    
    if (all_passed) {
        std::cout << "SUCCESS: LUT lookups perfectly match dynamic calculations!\n\n";
    }
}

int main() {
    // 1. Initialize global states
    Eval::initialize();
    Bucketer::initialize();
    
    // 2. Initialize Waugh's isomorphism combinatorics
    IsomorphismEngine isoEngine;
    isoEngine.initialize();
    
    // 3. Run the pipeline
    testDynamicBucketer(isoEngine);
    generateFlopLUT(isoEngine);
    verifyLUT(isoEngine);
    
    return 0;
}

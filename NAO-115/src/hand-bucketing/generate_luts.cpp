/**
 * This lookup table generator uses Kevin Waugh's 'Poker Hand Isomorphism' C library for poker hand mapping.
 * Copyright notice is included in the README of my repo, the license of my repo and on top of all source files in the 'external folder'.
 * The included source materials are available through this link: https://github.com/kdub0/hand-isomorphism
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <atomic>
#include <omp.h>
#include <string>

#include "mapping_engine.hpp"
#include "bucketer.hpp"
#include "eval/evaluator.hpp"

using namespace Bucketer;

void generateFlopLUT(IsomorphismEngine& mappingEngine) {
    uint64_t max_idx = mappingEngine.getFlopCombinations();
    std::cout << "Generating Flop LUT for " << max_idx << " combinations...\n";
    
    std::vector<uint16_t> lut(max_idx);
    std::atomic<uint64_t> progress{0};
    
#pragma omp parallel for schedule(static)
    for (uint64_t i = 0; i < max_idx; ++i) {
        
        std::array<uint8_t, 5> waugh_cards = mappingEngine.unindexFlop(i);
        
        std::array<int, 2> hand = {
            static_cast<int>(waugh_cards[0]),
            static_cast<int>(waugh_cards[1])
        };
        
        std::array<int, 3> board = {
            static_cast<int>(waugh_cards[2]),
            static_cast<int>(waugh_cards[3]),
            static_cast<int>(waugh_cards[4])
        };
        
        uint16_t bucket_id = static_cast<uint16_t>(Bucketer::get_flop_bucket(hand, board));
        
        lut[i] = bucket_id;
        
        uint64_t p = ++progress;
        if (p % 100000 == 0) {
#pragma omp critical
            {
                std::cout << "Processed " << p << " / " << max_idx << " flops\n";
            }
        }
    }
    
    std::ofstream out("flop_buckets.lut", std::ios::binary);
    out.write(reinterpret_cast<const char*>(lut.data()), lut.size() * sizeof(uint16_t));
    out.close();
    
    std::cout << "Saved Flop LUT to disk.\n";
}

void generateTurnLUT(IsomorphismEngine& mappingEngine) {
    
    uint64_t max_idx = mappingEngine.getTurnCombinations();
    std::cout << "Generating Turn LUT for " << max_idx << " combinations...\n";
    
    std::vector<uint16_t> lut(max_idx);
    std::atomic<uint64_t> progress{0};
    
#pragma omp parallel for schedule(static)
    for (uint64_t i = 0; i < max_idx; ++i) {
        
        std::array<uint8_t, 6> waugh_cards = mappingEngine.unindexTurn(i);
        
        std::array<int, 2> hand = {
            static_cast<int>(waugh_cards[0]),
            static_cast<int>(waugh_cards[1])
        };
        
        std::array<int, 4> board = {
            static_cast<int>(waugh_cards[2]),
            static_cast<int>(waugh_cards[3]),
            static_cast<int>(waugh_cards[4]),
            static_cast<int>(waugh_cards[5])
        };
        
        uint16_t bucket_id = static_cast<uint16_t>(Bucketer::get_turn_bucket(hand, board));
        
        lut[i] = bucket_id;
        
        uint64_t p = ++progress;
        
        if (p % 1000000 == 0) {
#pragma omp critical
            {
                std::cout << "Processed " << p << " / " << max_idx << " turns\n";
            }
        }
    }
    
    std::ofstream out("turn_buckets.lut", std::ios::binary);
    out.write(reinterpret_cast<const char*>(lut.data()), lut.size() * sizeof(uint16_t));
    out.close();
    
    std::cout << "Saved Turn LUT to disk.\n";
}


int main() {
    Eval::initialize();
    Bucketer::initialize();
    IsomorphismEngine mappingEngine;
    mappingEngine.initialize();
    // uncomment the function that needs to be run ->
    //generateFlopLUT(mappingEngine);
    //generateTurnLUT(mappingEngine);
    return 0;
}

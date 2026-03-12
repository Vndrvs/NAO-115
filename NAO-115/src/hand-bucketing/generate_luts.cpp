/**
 * This lookup table generator uses Kevin Waugh's 'Poker Hand Isomorphism' C library for poker hand mapping.
 * Copyright notice is included in the README of my repo, the license of my repo and on top of all source files in the 'external folder'.
 * The included source materials are available through this link: https://github.com/kdub0/hand-isomorphism
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include "generate_luts.hpp"
#include "bucketer.hpp"

using namespace Bucketer;

void generateFlopLUT(IsomorphismEngine& isomorphismEngine) {
    uint64_t max_idx = isomorphismEngine.getFlopCombinations();
    std::cout << "Generating Flop LUT for " << max_idx << " isomorphic combinations...\n";

    std::vector<uint16_t> lut(max_idx);

    for (uint64_t i = 0; i < max_idx; ++i) {
        std::array<uint8_t, 5> waugh_cards = isomorphismEngine.unindexFlop(i);
        
        // 1:1 map cards
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

        if ((i + 1) % 1000 == 0) {
            std::cout << "Processed " << (i + 1) << " / " << max_idx << " flops...\n";
        }
    }

    std::ofstream out("flop_buckets.lut", std::ios::binary);
    out.write(reinterpret_cast<const char*>(lut.data()), lut.size() * sizeof(uint16_t));
    out.close();
    std::cout << "SUCCESS: Saved Flop LUT (2.5 MB) to disk!\n";
}

int main() {
    Bucketer::initialize();
    IsomorphismEngine isomorphismEngine;
    isomorphismEngine.initialize();

    generateFlopLUT(isomorphismEngine);
    return 0;
}

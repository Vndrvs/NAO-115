/**
 * This lookup table generator uses Kevin Waugh's 'Poker Hand Isomorphism' C library for poker hand mapping.
 * Copyright notice is included in the README of my repo, the license of my repo and on top of all source files in the 'external folder'.
 * The included source materials are available through this link: https://github.com/kdub0/hand-isomorphism
 */

#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <stdexcept>

// include the library from external
extern "C" {
    #define _Bool bool
    #include "external/hand_index.h"
}

namespace Bucketer {

class IsomorphismEngine {
private:
    hand_indexer_t flop_indexer;
    hand_indexer_t turn_indexer;
    
    bool is_initialized = false;

public:
    IsomorphismEngine() = default;
    ~IsomorphismEngine() {
        if (is_initialized) {
            hand_indexer_free(&flop_indexer);
            hand_indexer_free(&turn_indexer);
        }
    }

    // combinatorics table
    void initialize() {
        if (is_initialized) return;

        // flop - 2 hole cards, 3 board cards
        uint8_t flop_cards_per_round[] = {2, 3};
        if (!hand_indexer_init(2, flop_cards_per_round, &flop_indexer)) {
            throw std::runtime_error("Flop indexer initializing failed");
        }

        // turn - 2 hole cards, 4 board cards
        uint8_t turn_cards_per_round[] = {2, 4};
        if (!hand_indexer_init(2, turn_cards_per_round, &turn_indexer)) {
            throw std::runtime_error("Turn indexer initializing failed");
        }

        is_initialized = true;
    }

    // get the maximum size of the tables
    uint64_t getFlopCombinations() const {
        return is_initialized ? hand_indexer_size(&flop_indexer, 1) : 0;
    }

    uint64_t getTurnCombinations() const {
        return is_initialized ? hand_indexer_size(&turn_indexer, 1) : 0;
    }

    std::array<uint8_t, 5> unindexFlop(uint64_t idx) const {
        std::array<uint8_t, 5> cards;
        hand_unindex(&flop_indexer, 1, idx, cards.data());
        return cards;
    }

    // Convert Waugh's index back into physical cards for the Turn (6 cards)
    std::array<uint8_t, 6> unindexTurn(uint64_t idx) const {
        std::array<uint8_t, 6> cards;
        hand_unindex(&turn_indexer, 1, idx, cards.data());
        return cards;
    }
};

}

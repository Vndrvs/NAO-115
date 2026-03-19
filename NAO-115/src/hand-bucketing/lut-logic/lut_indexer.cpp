#include "../include/bucket-lookups/lut_indexer.hpp"
#include "../include/bucket-lookups/lut_manager.hpp"
#include "../bucketer.hpp"
#include <iostream>
#include <array>

namespace Bucketer {

// pointer versions of the original centroid.dat lookups from bucketer.cpp, preflop and river stage

int get_preflop_bucket_raw(const int* hand) {
    std::array<int, 2> handArray = {hand[0], hand[1]};
    return get_preflop_bucket(handArray);
}

int get_river_bucket_raw(const int* hand, const int* board) {
    std::array<int, 2> handArray = {hand[0], hand[1]};
    std::array<int, 5> boardArray = {board[0], board[1], board[2], board[3], board[4]};
    return get_river_bucket(handArray, boardArray);
}

// combined lookup using LUT system in flop and turn stages
int lookup_bucket(IsomorphismEngine& mappingEngine, const int* hand, const int* board, int boardSize) {
    switch (boardSize) {
        // preflop route
        case 0: {
            return get_preflop_bucket_raw(hand);
        }
        // flop route
        case 3: {
            std::array<uint8_t, 5> cards = {
                static_cast<uint8_t>(hand[0]),
                static_cast<uint8_t>(hand[1]),
                static_cast<uint8_t>(board[0]),
                static_cast<uint8_t>(board[1]),
                static_cast<uint8_t>(board[2])
            };
            uint64_t idx = mappingEngine.getFlopIndex(cards);
            if (idx >= FLOP_COMBOS) {
                std::cout << "OOB FLOP INDEX: " << idx << "\n";
                std::cout << "cards: "
                          << (int)cards[0] << " "
                          << (int)cards[1] << " | "
                          << (int)cards[2] << " "
                          << (int)cards[3] << " "
                          << (int)cards[4] << "\n";
                std::abort();
            }

            if (!flop_lut) {
                std::cout << "FLOP LUT POINTER IS NULL\n";
                std::abort();
            }
            return flop_lut[mappingEngine.getFlopIndex(cards)];
        }
        // turn route
        case 4: {
            std::array<uint8_t, 6> cards = {
                static_cast<uint8_t>(hand[0]),
                static_cast<uint8_t>(hand[1]),
                static_cast<uint8_t>(board[0]),
                static_cast<uint8_t>(board[1]),
                static_cast<uint8_t>(board[2]),
                static_cast<uint8_t>(board[3])
            };
            
            uint64_t idx = mappingEngine.getTurnIndex(cards);

                if (idx >= TURN_COMBOS) {
                    std::cout << "OOB TURN INDEX: " << idx << "\n";
                    std::cout << "Expected max: " << TURN_COMBOS << "\n";
                    std::cout << "Cards: ";
                    for (auto c : cards) std::cout << (int)c << " ";
                    std::cout << "\n";
                    std::abort();
                }
            return turn_lut[mappingEngine.getTurnIndex(cards)];
        }
        // river route
        case 5: {
            return get_river_bucket_raw(hand, board);
        }
        default: {
            std::cerr << "Error: Invalid board size passed to lookup_bucket: " << boardSize << "\n";
            return -1;
        }
    }
}

}

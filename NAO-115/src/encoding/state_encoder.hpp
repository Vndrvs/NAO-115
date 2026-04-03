#pragma once

#include <array>
#include <cstdint>
#include "cfr/cfr-core/mccfr_state.hpp"

namespace StateEncoder {

/*
Maps a 32-bit encoded card to its deck index (0-51) - builds board bitmasks for network input encoding.
 index = rank * 4 + suit
 suit: 0 = clubs, 1 = diamonds, 2 = hearts, 3 = spades
*/
inline int cardToIndex(int encodedCard) {
    int rank = (encodedCard >> 8) & 0xF;
    int suitBits = (encodedCard >> 12) & 0xF;
    int suit = 0;
    if      (suitBits & 0x1) suit = 0; // clubs
    else if (suitBits & 0x2) suit = 1; // diamonds
    else if (suitBits & 0x4) suit = 2; // hearts
    else                     suit = 3; // spades
    return rank * 4 + suit;
}
/*
Builds a 52-bit bitmask from an array of encoded cards - tracks board cards across streets.
 -> bit i is set if deck[i] is present in the array
*/
inline uint64_t cardsToBitmask(const int* cards, int numCards) {
    uint64_t mask = 0;
    for (int i = 0; i < numCards; i++) {
        mask |= (1ULL << cardToIndex(cards[i]));
    }
    return mask;
}

/*
Encodes a public game state into a 65-dimensional float vector suitable for neural network input.
 
Layout:
 [0-3]      street one-hot
 [4]        raiseCount / 4.0 (normalized)
 [5]        heroIsSB (0 or 1)
 [6]        potBase / 20000 (normalized)
 [7]        heroStreetBet / 20000 (normalized)
 [8]        villainStreetBet / 20000 (normalized)
 [9]        heroStack / 20000 (normalized)
 [10]       villainStack / 20000 (normalized)
 [11]       previousRaiseTotal / 20000 (normalized)
 [12]       betBeforeRaise / 20000 (normalized)
 [13-64]    board card bitmask (52 binary values)
*/
std::array<float, 65> encode(const MCCFRState& state, uint64_t boardMask);

}

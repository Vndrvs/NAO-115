#include "state_encoder.hpp"
#include "cfr/cfr-core/mccfr_state.hpp"
#include <array>

namespace StateEncoder {

constexpr float MAX_RAISES = 4.0f;
constexpr float MAX_CHIPS_IN_PLAY = 40000.0f;

std::array<float, 65> encode(const MCCFRState& state, uint64_t boardMask) {
    // Zero-initializes all 65 elements to 0.0f
    std::array<float, 65> input = {};

    // [0-3] Street one-hot (assuming state.street is 0=Preflop, 1=Flop, 2=Turn, 3=River)
    input[state.street] = 1.0f;

    // [4] Raise count normalized to [0.0, 1.0]
    input[4] = state.raiseCount / MAX_RAISES;

    // [5] Hero position
    input[5] = state.heroIsSB() ? 1.0f : 0.0f;

    // [6-12] Chip values normalized by total chips in the entire game
    input[6]  = state.potBase            / MAX_CHIPS_IN_PLAY;
    input[7]  = state.heroStreetBet      / MAX_CHIPS_IN_PLAY;
    input[8]  = state.villainStreetBet   / MAX_CHIPS_IN_PLAY;
    input[9]  = state.heroStack          / MAX_CHIPS_IN_PLAY;
    input[10] = state.villainStack       / MAX_CHIPS_IN_PLAY;
    input[11] = state.previousRaiseTotal / MAX_CHIPS_IN_PLAY;
    input[12] = state.betBeforeRaise     / MAX_CHIPS_IN_PLAY;

    // [13-64] Board card bitmask (52 dimensions)
    for (int i = 0; i < 52; i++) {
        input[13 + i] = ((boardMask >> i) & 1ULL) ? 1.0f : 0.0f;
    }

    return input;
}

}

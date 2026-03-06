#include "game_engine.hpp"

MCCFRState switchPlayer(MCCFRState state) {
    std::swap(state.heroStack,      state.villainStack);
    std::swap(state.heroStreetBet,  state.villainStreetBet);
    state.currentPlayer = 1 - state.currentPlayer;
    return state;
}

#pragma once

#include <string>
#include <cstdint>
#include <array>

namespace Game {

enum class Street : uint8_t {
    Preflop = 0,
    Flop = 1,
    Turn = 2,
    River = 3
};

struct PlayerState {
    int stack;        // remaining total chip count of the specific player
    int currentBet;   // chip count bet on the current state
};

struct GameState {
    int pot = 0; // total chips in the pot
    std::array<PlayerState, 2> players; // we use the playerstate struct to get the 2 types of chip data
    int currentPlayer = 1; // 0: Big Blind, 1: Small Blind
    Street street = Street::Preflop; // current street, updated when we see a '/' in the actionstring
    // 0: preflop, 1: flop, 2: turn, 3: river
    bool isTerminal = false; // 'isTerminal' is a variable which signals
    // that we don't expect any player action after the last action string we receive or send
    // eg. Fold -> someone folded, the games immediately
    // All in bet and a player calls on it -> this can happen at any street, no further actions can be taken
    // Check - Check on River -> (onon-all-in terminal situation, game simply ends)
    std::string actionHistory; // full action history from both players, matching Slumbot's formatting
    int streetToInt() const {
        return static_cast<int>(street);
    }
};

void applyAction(GameState& state, const std::string& actionStr);

}

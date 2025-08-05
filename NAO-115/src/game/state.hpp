#pragma once

#include <string>

struct GameState {
    int pot; // total chips in the pot
    int playerChips[2]; // total chips remaining per players (we also call this stack)
    int playerBets[2]; // current street chip bets per players
    int currentPlayer; // 0: Big Blind, 1: Small Blind
    int street; // current street, updated when we see a '/' in the actionstring
                // 0: preflop, 1: flop, 2: turn, 3: river
    bool isTerminal; // I'll try to explain this. 'isTerminal' is a custom variable which signals
                     // that we don't expect any player action after the last action string we receive or send
                     // eg. Fold -> someone folded, the games immediately
                     // All in bet and a player calls on it -> this can happen at any street, no further actions can be taken
                     // Check - Check on River -> (onon-all-in terminal situation, game simply ends)
    std::string actionHistory; // full action history from both players, matching Slumbot's formatting
};
